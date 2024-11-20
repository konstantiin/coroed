#include "schedy.h"

#include <assert.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#include "interrupt.h"
#include "kthread.h"
#include "spinlock.h"
#include "task.h"
#include "uthread.h"

#define SCHED_THREADS_LIMIT 512
#define SCHED_WORKERS_COUNT (size_t)(8)
#define SCHED_NEXT_MAX_ATTEMPTS (size_t)(4)

#define INTR_ENTER(worker) interrupt_received(&(worker)->interrupt_stack)
#define INTR_OFF(worker) interrupt_off_push(&(worker)->interrupt_stack)
#define INTR_ON(worker) interrupt_off_pop(&(worker)->interrupt_stack)

struct task {
  struct uthread* thread;
  struct worker* worker;
  enum {
    UTHREAD_RUNNABLE,
    UTHREAD_RUNNING,
    UTHREAD_CANCELLED,
    UTHREAD_ZOMBIE,
  } state;
  struct spinlock lock;
};

struct worker {
  size_t index;
  struct kthread kthread;
  struct interrupt_stack interrupt_stack;
  struct uthread sched_thread;
  struct task* running_task;
  struct {
    size_t steps;
    size_t cancelled;
  } statistics;
};

static struct spinlock tasks_lock;
static size_t next_task_index = 0;
static struct task tasks[SCHED_THREADS_LIMIT];

static kthread_id_t kthread_ids[SCHED_WORKERS_COUNT];
static struct worker workers[SCHED_WORKERS_COUNT];

void sched_task_init(struct task* task) {
  task->thread = NULL;
  task->worker = NULL;
  task->state = UTHREAD_ZOMBIE;
  spinlock_init(&task->lock);
}

void sched_worker_init(struct worker* worker, size_t index) {
  worker->index = index;
  interrupt_stack_init(&worker->interrupt_stack);
  worker->sched_thread.context = NULL;
  worker->running_task = NULL;
  worker->statistics.steps = 0;
  worker->statistics.cancelled = 0;
}

void sched_init() {
  spinlock_init(&tasks_lock);
  for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
    sched_task_init(&tasks[i]);
  }
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    kthread_ids[i] = 0;
    sched_worker_init(&workers[i], i);
  }
}

void sched_switch_to_scheduler(struct task* task) {
  struct uthread* sched = &task->worker->sched_thread;
  task->worker = NULL;
  uthread_switch(task->thread, sched);
}

void sched_switch_to(struct worker* worker, struct task* task) {
  assert(task->thread != &worker->sched_thread);

  task->state = UTHREAD_RUNNING;

  task->worker = worker;
  worker->running_task = task;

  INTR_ON(worker);

  struct uthread* sched = &worker->sched_thread;
  uthread_switch(sched, task->thread);
}

struct task* sched_acquire_next();
void sched_release(struct task* task);

struct worker* sched_worker() {
  kthread_id_t tid = kthread_id();
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    if (kthread_ids[i] == tid) {
      return &workers[i];
    }
  }
  return NULL;
}

void sched_preempt() {
  struct worker* worker = sched_worker();
  INTR_ENTER(worker);
  sched_switch_to_scheduler(worker->running_task);
}

int sched_loop(void* argument) {
  struct worker* worker = argument;
  kthread_ids[worker->index] = kthread_id();

  interrupt_setup(sched_preempt);

  for (;;) {
    struct task* task = sched_acquire_next();
    if (task == NULL) {
      break;
    }

    sched_switch_to(worker, task);

    worker->statistics.steps += 1;
    if (task->state == UTHREAD_CANCELLED) {
      worker->statistics.cancelled += 1;
    }

    sched_release(task);
  }

  return 0;
}

struct task* sched_acquire_next() {
  for (size_t attempt = 0; attempt < SCHED_NEXT_MAX_ATTEMPTS; ++attempt) {
    spinlock_lock(&tasks_lock);
    for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
      struct task* task = &tasks[next_task_index];
      if (!spinlock_try_lock(&task->lock)) {
        continue;
      }

      next_task_index = (next_task_index + 1) % SCHED_THREADS_LIMIT;
      if (task->thread != NULL && task->state == UTHREAD_RUNNABLE) {
        spinlock_unlock(&tasks_lock);
        return task;
      }

      spinlock_unlock(&task->lock);
    }
    spinlock_unlock(&tasks_lock);

    sleep(1);
  }

  return NULL;
}

void sched_release(struct task* task) {
  task->worker = NULL;
  if (task->state == UTHREAD_CANCELLED) {
    uthread_reset(task->thread);
    task->state = UTHREAD_ZOMBIE;
  } else if (task->state == UTHREAD_RUNNING) {
    task->state = UTHREAD_RUNNABLE;
  } else {
    assert(false);
  }
  spinlock_unlock(&task->lock);
}

void sched_cancel(struct task* task) {
  task->state = UTHREAD_CANCELLED;
}

void task_yield(struct task* task) {
  INTR_OFF(task->worker);
  sched_switch_to_scheduler(task);
}

void task_exit(struct task* task) {
  sched_cancel(task);
  task_yield(task);
}

void task_submit(struct task* parent, uthread_routine entry, void* argument) {
  struct worker* worker = NULL;
  for (;;) {
    worker = parent->worker;
    INTR_OFF(worker);
    if (worker == parent->worker) {
      break;
    }
  }

  sched_submit(*entry, argument);
  INTR_ON(worker);
}

void sched_submit(void (*entry)(), void* argument) {
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
    }

    spinlock_unlock(&task->lock);
    if (is_submitted) {
      return;
    }
  }

  printf("Threads exhausted");
  exit(1);
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
    tasks_count += worker->statistics.cancelled;
    steps_count += worker->statistics.steps;
  }

  printf("|- tasks executed %zu\n", tasks_count);
  printf("|- steps done     %zu\n", steps_count);

  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    printf("|- worker %zu %zu\n", i, kthread_ids[i]);
    printf("   |- steps     %zu\n", worker->statistics.steps);
    printf("   |- cancelled %zu\n", worker->statistics.cancelled);
  }
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
