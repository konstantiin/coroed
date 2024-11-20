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

#include "spinlock.h"
#include "task.h"
#include "uthread.h"

#define SCHED_THREADS_LIMIT 64
#define SCHED_WORKERS_COUNT (size_t)(1)
#define SCHED_NEXT_MAX_ATTEMPTS (size_t)(8)

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
  struct uthread sched_thread;
  size_t curr_index;
  struct {
    size_t steps;
    size_t cancelled;
  } statistics;
};

struct task tasks[SCHED_THREADS_LIMIT];

struct worker workers[SCHED_WORKERS_COUNT];
thrd_t threads[SCHED_WORKERS_COUNT];

void sched_task_init(struct task* task) {
  task->thread = NULL;
  task->worker = NULL;
  task->state = UTHREAD_ZOMBIE;
  spinlock_init(&task->lock);
}

void sched_worker_init(struct worker* worker) {
  worker->sched_thread.context = NULL;
  worker->curr_index = 0;
  worker->statistics.steps = 0;
  worker->statistics.cancelled = 0;
}

void sched_init() {
  for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
    sched_task_init(&tasks[i]);
  }
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    sched_worker_init(&workers[i]);
  }
}

void sched_switch_to_scheduler(struct task* task) {
  struct uthread* sched = &task->worker->sched_thread;
  task->worker = NULL;
  uthread_switch(task->thread, sched);
}

void sched_switch_to(struct worker* worker, struct task* task) {
  assert(task->thread != &worker->sched_thread);

  task->worker = worker;

  struct uthread* sched = &worker->sched_thread;
  uthread_switch(sched, task->thread);
}

struct task* sched_next(struct worker* worker);

int sched_loop(void* argument) {
  struct worker* worker = argument;

  for (;;) {
    struct task* task = sched_next(worker);
    if (task == NULL) {
      break;
    }

    task->state = UTHREAD_RUNNING;

    sched_switch_to(worker, task);

    if (task->state == UTHREAD_CANCELLED) {
      uthread_reset(task->thread);
      task->state = UTHREAD_ZOMBIE;
      worker->statistics.cancelled += 1;
    } else if (task->state == UTHREAD_RUNNING) {
      task->state = UTHREAD_RUNNABLE;
    } else {
      assert(false);
    }

    spinlock_unlock(&task->lock);

    worker->statistics.steps += 1;
  }

  return 0;
}

struct task* sched_next(struct worker* worker) {
  for (size_t attempt = 0; attempt < SCHED_NEXT_MAX_ATTEMPTS; ++attempt) {
    for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
      struct task* task = &tasks[worker->curr_index];
      if (!spinlock_try_lock(&task->lock)) {
        continue;
      }

      worker->curr_index = (worker->curr_index + 1) % SCHED_THREADS_LIMIT;
      if (task->thread != NULL && task->state == UTHREAD_RUNNABLE) {
        return task;
      }

      spinlock_unlock(&task->lock);
    }
    sleep(1);
  }

  return NULL;
}

void sched_cancel(struct task* task) {
  task->state = UTHREAD_CANCELLED;
}

void task_yield(struct task* task) {
  sched_switch_to_scheduler(task);
}

void task_exit(struct task* task) {
  sched_cancel(task);
  task_yield(task);
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
    int code = thrd_create(&threads[i], sched_loop, /* arg = */ &workers[i]);
    assert(code == thrd_success);
  }
}

void sched_wait() {
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    int status = 0;
    int code = thrd_join(threads[i], &status);
    assert(code == thrd_success);
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
    printf("|- worker %zu\n", i);
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
