#include "schedy.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include "coroed/api/task.h"
#include "coroed/core/relax.h"
#include "coroed/core/spinlock.h"
#include "kthread.h"
#include "uthread.h"

enum {
  /**
   * Максимальное количество файберов, которое может
   * одновременно выполняться на планировщике.
   */
  SCHED_THREADS_LIMIT = 512,

  /**
   * Количество рабочих потоков для исполнения файберов.
   */
  SCHED_WORKERS_COUNT = (size_t)(8),

  /**
   * Обеспечивает костыль для ретрая операций
   * `submit` и `acquire_next` при высокой
   * конкуренции на спинлоках файберов.
   */
  SCHED_NEXT_MAX_ATTEMPTS = (size_t)(16),
};

/**
 * Задача, выполняющаяся на планировщике.
 *
 * Hint: также тут можно хранить список задач, ожидающих
 *       завершения этой, чтобы уведомить их о данном
 *       событии.
 */
struct task {
  /**
   * Контекст исполнения.
   */
  struct uthread* thread;

  /**
   * Установлен при `UTHREAD_RUNNING`. Указывает на
   * рабочий поток, на котором исполняется задача.
   * Необходим для получения контекста локального
   * планировщика при операциях `yield`, `await`, `submit`.
   */
  struct worker* worker;

  enum {
    /** Готова к исполнению. */
    UTHREAD_RUNNABLE,

    /** Прямо сейчас выполняется. */
    UTHREAD_RUNNING,

    /** Завершена и скоро станет зомби. */
    UTHREAD_FINISHED,

    /** Отработала и может быть переиспользования. */
    UTHREAD_ZOMBIE,

    /** Заблокирована вводом-выводом или оижданием события. */
    UTHREAD_BLOCKED,
  } state;  // Текущее состояние задачи

  /**
   * Защищает поля структуры от неупорядоченного доступа.
   */
  struct spinlock lock;

  int id;
  int blocked_by_fd;
  /**
   * Список задач, которые ожидают выполнения этой.
   * Пока что используется только в событиях.
   */
  struct task* waiting;
  uint64_t last_timestamp;
};

/**
 * Рабочий поток, исполняющий файберы.
 */
struct worker {
  /**
   * Идентификатор рабочего потока.
   */
  size_t index;

  /**
   * Поток операционной системы, на котором
   * исполняется рабочий.
   */
  struct kthread kthread;

  /**
   * Контекст планировщика. Нужно переключиться на него,
   * чтобы вернуться в планировщик.
   */
  struct uthread sched_thread;

  /**
   * В данный момент исполняемая на рабочем
   * задача. Может быть `NULL`.
   */
  struct task* running_task;

  struct {
    size_t steps;     // Сколько шагов было выполнено
    size_t finished;  // Сколько задач было завершено
  } statistics;       // Локальная статистика работяги
};

struct statistics {
  uint64_t running;
  uint64_t runnable;
  uint64_t blocked;
};

struct statistics stats = {
    .running = 0,
    .runnable = 0,
    .blocked = 0,
};

struct statistics stats_dump = {
    .running = 0,
    .runnable = 0,
    .blocked = 0,
};

static struct spinlock tasks_lock;              // Защищает список задач
static size_t next_task_index = 0;              // Для планирования round-robin
static struct task tasks[SCHED_THREADS_LIMIT];  // Список всех задач
int epoll_fd;                                   //
struct spinlock sched_poll_io_lock;
struct spinlock stats_lock;
struct spinlock stats_dump_lock;

struct spinlock task_id_lock;
int last_task_id = 0;

struct spinlock blocked_task_counter_lock;
int blocked_task_counter = 0;

// Hint: для реализации более сложных схем управления, вам
//       вам могут понадобиться связаные списки (`core/list.h`).

static kthread_id_t kthread_ids[SCHED_WORKERS_COUNT];
static struct worker workers[SCHED_WORKERS_COUNT];

uint64_t get_time_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

void update_stats(struct task* task) {
  spinlock_lock(&stats_lock);
  uint64_t prev = task->last_timestamp;
  uint64_t now = get_time_ms();
  task->last_timestamp = now;
  switch (task->state) {
    case UTHREAD_BLOCKED:
      stats.blocked += now - prev;
      break;
    case UTHREAD_RUNNING:
      stats.running += now - prev;
      break;
    case UTHREAD_RUNNABLE:
      stats.runnable += now - prev;
      break;
    default:
      break;
  }
  if (spinlock_try_lock(&stats_dump_lock)) {
    stats_dump.running = stats.running;
    stats_dump.runnable = stats.runnable;
    stats_dump.blocked = stats.blocked;
    spinlock_unlock(&stats_dump_lock);
  }
  spinlock_unlock(&stats_lock);
}

void task_unblock_queue(struct task* task) {
  while (task) {
    spinlock_lock(&task->lock);
    if (task->state != UTHREAD_BLOCKED) {
      spinlock_unlock(&task->lock);
      return;
    }
    update_stats(task);
    struct task* next = task->waiting;
    task->state = UTHREAD_RUNNABLE;
    spinlock_lock(&blocked_task_counter_lock);
    blocked_task_counter--;
    spinlock_unlock(&blocked_task_counter_lock);
    spinlock_unlock(&task->lock);
    task = next;
  }
}

// new_task->lock should be acquired
void insert_blocked_task(struct task* new_task, struct task* queue) {
  new_task->waiting = queue;
  update_stats(new_task);
  new_task->state = UTHREAD_BLOCKED;
  spinlock_lock(&blocked_task_counter_lock);
  blocked_task_counter++;
  spinlock_unlock(&blocked_task_counter_lock);
}
/**
 * Проверяем, какие корутины готовы продолжить работу
 */
void sched_poll_io() {
  struct epoll_event events[MAX_EVENTS];
  int n = epoll_wait(epoll_fd, events, MAX_EVENTS, 0);
  for (int i = 0; i < n; i++) {
    struct task* task = events[i].data.ptr;

    spinlock_lock(&task->lock);
    if (task->state == UTHREAD_BLOCKED) {
      update_stats(task);
      task->state = UTHREAD_RUNNABLE;
      spinlock_lock(&blocked_task_counter_lock);
      blocked_task_counter--;
      spinlock_unlock(&blocked_task_counter_lock);
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, task->blocked_by_fd, NULL);
    spinlock_unlock(&task->lock);
  }
}

/**
 * Установить задачу в пустое состояние.
 */
void sched_task_init(struct task* task) {
  task->thread = NULL;
  task->worker = NULL;
  task->state = UTHREAD_ZOMBIE;
  spinlock_init(&task->lock);
}

/**
 * Установить рабочего в пустое состояние.
 */
void sched_worker_init(struct worker* worker, size_t index) {
  worker->index = index;
  worker->sched_thread.context = NULL;
  worker->running_task = NULL;
  worker->statistics.steps = 0;
  worker->statistics.finished = 0;
}

void sched_init() {
  epoll_fd = epoll_create1(0);
  assert(epoll_fd >= 0);
  spinlock_init(&tasks_lock);
  spinlock_init(&task_id_lock);
  spinlock_init(&sched_poll_io_lock);
  spinlock_init(&stats_lock);
  spinlock_init(&stats_dump_lock);
  spinlock_init(&blocked_task_counter_lock);
  for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
    sched_task_init(&tasks[i]);
  }
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    kthread_ids[i] = 0;
    sched_worker_init(&workers[i], i);
  }
}

/**
 * Находясь в контексте задачи `task`, переключиться
 * в контекст планировщика.
 */
void sched_switch_to_scheduler(struct task* task) {
  struct uthread* sched = &task->worker->sched_thread;
  task->worker = NULL;
  uthread_switch(task->thread, sched);
}

/**
 * Находясь в контексте планировщика, переключиться
 * в контекст задачи `task`. Выполнять ее до следующего
 * невынужденного возвращения в планировщик.
 */
void sched_switch_to(struct worker* worker, struct task* task) {
  assert(task->thread != &worker->sched_thread);

  update_stats(task);

  task->state = UTHREAD_RUNNING;

  task->worker = worker;
  worker->running_task = task;

  struct uthread* sched = &worker->sched_thread;
  uthread_switch(sched, task->thread);
}

/**
 * Получить следующую задачу на исполнение в
 * соответствии с текущим алгоритмом планирования.
 *
 * Вызывающему передается владение задачей, а также
 * захваченный лок `task->lock`.
 */
struct task* sched_acquire_next();

/**
 * Вернуть задачу в очередь планирования.
 *
 * Очереди передается владение задачей,
 * а также она отпустит лок `task->lock`.
 */
void sched_release(struct task* task);

/**
 * Цикл планировщика. Выполняется, пока есть задачи.
 */
int sched_loop(void* argument) {
  struct worker* worker = argument;
  kthread_ids[worker->index] = kthread_id();

  for (;;) {
    if (spinlock_try_lock(&sched_poll_io_lock)) {
      sched_poll_io();
      spinlock_unlock(&sched_poll_io_lock);
    }
    struct task* task = sched_acquire_next();
    if (task == NULL) {
      if (blocked_task_counter != 0) {
        continue;
      }
      break;
    }

    sched_switch_to(worker, task);

    worker->statistics.steps += 1;
    if (task->state == UTHREAD_FINISHED) {
      worker->statistics.finished += 1;
    }

    sched_release(task);

    // Hint: где-то здесь можно было бы опросить
    //       механизмы для неблокирующего ввода-вывода
    //       и перевести удовлетворенные BLOCKED
    //       потоки в RUNNABLE состояние.
  }

  return 0;
}

struct task* sched_acquire_next() {
  // На всякий случай пытаемся найти задачу несколько раз,
  // так как какие-то `task->lock` могли быть отпущены.

  for (size_t attempt = 0; attempt < SCHED_NEXT_MAX_ATTEMPTS; ++attempt) {
    spinlock_lock(&tasks_lock);  // Защитим `next_task_index`

    for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
      struct task* task = &tasks[next_task_index];
      if (!spinlock_try_lock(&task->lock)) {
        continue;
      }

      // Планирование round-robin
      next_task_index = (next_task_index + 1) % SCHED_THREADS_LIMIT;

      if (task->thread != NULL && task->state == UTHREAD_RUNNABLE) {
        spinlock_unlock(&tasks_lock);
        return task;
      }

      spinlock_unlock(&task->lock);
    }

    spinlock_unlock(&tasks_lock);
    SPINLOOP(2 * attempt);
  }

  return NULL;
}

void sched_release(struct task* task) {
  task->worker = NULL;
  if (task->state == UTHREAD_FINISHED) {
    // Отправляем задачу на кладбище, а могли бы
    // еще, например, разблокировать зависимые задачи.
    uthread_reset(task->thread);
    task->state = UTHREAD_ZOMBIE;
  } else if (task->state == UTHREAD_RUNNING) {
    update_stats(task);
    task->state = UTHREAD_RUNNABLE;
  } else /* if (task->state == UTHREAD_BLOCKED) */ {
    // assert(false && "Not implemented");
    // task->state = UTHREAD_RUNNABLE;
  }
  spinlock_unlock(&task->lock);
}

/**
 * Отметить задачу завершенной.
 */
void sched_finish(struct task* task) {
  task->state = UTHREAD_FINISHED;
}

void sched_block(struct task* task) {
  update_stats(task);
  task->state = UTHREAD_BLOCKED;
  spinlock_lock(&blocked_task_counter_lock);
  blocked_task_counter++;
  spinlock_unlock(&blocked_task_counter_lock);
}

void task_yield(struct task* caller) {
  sched_switch_to_scheduler(caller);
}

void task_exit(struct task* caller) {
  sched_finish(caller);
  task_yield(caller);
}

void task_block(struct task* caller, int fd) {
  caller->blocked_by_fd = fd;
  sched_block(caller);
  task_yield(caller);
}

task_t task_submit(struct task* caller, uthread_routine entry, void* argument) {
  (void)caller;  // Может быть полезно.
  task_t child = sched_submit(*entry, argument);
  return child;
}

task_t sched_try_submit(void (*entry)(), void* argument) {
  for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
    struct task* task = &tasks[i];

    if (!spinlock_try_lock(&task->lock)) {
      continue;
    }

    if (task->thread == NULL) {
      task->thread = uthread_allocate();
      assert(task->thread != NULL);
      task->state = UTHREAD_ZOMBIE;
    }

    const bool is_submitted = task->state == UTHREAD_ZOMBIE;

    if (task->state == UTHREAD_ZOMBIE) {
      uthread_reset(task->thread);
      uthread_set_entry(task->thread, entry);
      uthread_set_arg_0(task->thread, task);
      uthread_set_arg_1(task->thread, argument);
      task->state = UTHREAD_RUNNABLE;
      spinlock_lock(&task_id_lock);
      task->id = last_task_id++;
      task->waiting = NULL;
      task->last_timestamp = get_time_ms();
      spinlock_unlock(&task_id_lock);
    }

    spinlock_unlock(&task->lock);
    if (is_submitted) {
      return (task_t){.task = task};
    }
  }

  return (task_t){.task = NULL};
}

task_t sched_submit(void (*entry)(), void* argument) {
  for (size_t attempt = 0; attempt < SCHED_NEXT_MAX_ATTEMPTS; ++attempt) {
    task_t handle = sched_try_submit(entry, argument);
    if (handle.task != NULL) {
      return handle;
    }
    SPINLOOP(2 * attempt);
  }

  assert(false && "Can't create a task");
}

void sched_start() {
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    enum kthread_status status = kthread_create(&worker->kthread, sched_loop, worker);
    assert(status == KTHREAD_SUCCESS);
  }
}

void sched_wait() {
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    enum kthread_status status = kthread_join(&worker->kthread);
    assert(status == KTHREAD_SUCCESS);
  }
}

void sched_print_statistics() {
  printf("\nsched statistics\n");

  size_t tasks_count = 0;
  size_t steps_count = 0;
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    tasks_count += worker->statistics.finished;
    steps_count += worker->statistics.steps;
  }

  printf("|- tasks executed %zu\n", tasks_count);
  printf("|- steps done     %zu\n", steps_count);

  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    printf("|- worker %zu %zu\n", i, kthread_ids[i]);
    printf("   |- steps     %zu\n", worker->statistics.steps);
    printf("   |- finished  %zu\n", worker->statistics.finished);
  }
}

void sched_print_stats() {
  printf("Running: %lu\n", stats.running);
  printf("Runnable: %lu\n", stats.runnable);
  printf("Blocked: %lu\n", stats.blocked);
}

void sched_print_dump_stats() {
  spinlock_lock(&stats_dump_lock);
  printf("Running: %lu\n", stats_dump.running);
  printf("Runnable: %lu\n", stats_dump.runnable);
  printf("Blocked: %lu\n", stats_dump.blocked);
  spinlock_unlock(&stats_dump_lock);
}

void sched_destroy() {
  for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
    struct task* task = &tasks[i];
    spinlock_lock(&task->lock);
    if (task->thread != NULL) {
      uthread_free(task->thread);
    }
    spinlock_unlock(&task->lock);
  }
}
