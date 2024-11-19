// Linux
#define _GNU_SOURCE

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

// Linux
#include <linux/sched.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "spinlock.h"
#include "task.h"
#include "uthread.h"

#define THREAD_COUNT_LIMIT 8

#define SCHED_WORKER_STACK_SIZE (size_t)(1024 * 1024)
#define SCHED_WORKERS_COUNT (size_t)(4)

#define SCHED_NEXT_MAX_ATTEMPTS (size_t)(8)

struct task {
  struct uthread* thread;
  enum {
    UTHREAD_RUNNABLE,
    UTHREAD_RUNNING,
    UTHREAD_CANCELLED,
    UTHREAD_ZOMBIE,
  } state;
  struct spinlock lock;
};

struct task tasks[THREAD_COUNT_LIMIT];

thread_local struct task* sched_task = NULL;
thread_local struct task* curr_task = NULL;
thread_local size_t curr_index = 0;

thread_local struct {
  size_t steps;
} statistics = {
    .steps = 0,
};

void sched_init() {
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    struct task* task = &tasks[i];
    task->thread = NULL;
    task->state = UTHREAD_ZOMBIE;
    spinlock_init(&task->lock);
  }
}

void sched_switch_to_scheduler() {
  uthread_switch(curr_task->thread, sched_task->thread);
}

void sched_switch_to(struct task* task) {
  assert(task != sched_task);
  curr_task = task;
  uthread_switch(sched_task->thread, curr_task->thread);
}

struct task* sched_next();

int sched_loop(void* argument) {
  (void)argument;

  /* Setup sched thread */
  struct uthread thread = {.context = NULL};
  struct task sched = {.thread = &thread};
  sched_task = &sched;

  /* Event loop */
  for (;;) {
    struct task* task = sched_next();
    if (task == NULL) {
      break;
    }

    task->state = UTHREAD_RUNNING;

    sched_switch_to(task);

    printf(" ");
    fflush(stdout);  // NOLINT

    if (task->state == UTHREAD_CANCELLED) {
      uthread_reset(task->thread);
      task->state = UTHREAD_ZOMBIE;
    } else if (task->state == UTHREAD_RUNNING) {
      task->state = UTHREAD_RUNNABLE;
    } else {
      assert(false);
    }

    spinlock_unlock(&task->lock);

    statistics.steps += 1;
  }

  printf("[coroed] Worker statitics: steps = %zu\n", statistics.steps);

  return 0;
}

void sched_start() {
  printf("[coroed] Starting the runtime...\n");

  thrd_t workers[SCHED_WORKERS_COUNT];
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    int code = thrd_create(&workers[i], sched_loop, /* arg = */ NULL);
    assert(code == thrd_success);
  }

  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    int status = 0;
    int code = thrd_join(workers[i], &status);
    assert(code == thrd_success);
  }
}

struct task* sched_next() {
  for (size_t attempt = 0; attempt < SCHED_NEXT_MAX_ATTEMPTS; ++attempt) {
    for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
      struct task* task = &tasks[curr_index];
      if (!spinlock_try_lock(&task->lock)) {
        continue;
      }

      curr_index = (curr_index + 1) % THREAD_COUNT_LIMIT;
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

struct task* task_current() {
  return curr_task;
}

void task_yield() {
  sched_switch_to_scheduler();
}

void* task_argument(struct task* task) {
  return uthread_arg_0(task->thread);
}

void task_exit() {
  sched_cancel(task_current());
  task_yield();
}

void sched_submit(void (*entry)(), void* argument) {
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
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
      uthread_set_arg_0(task->thread, argument);
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

void sched_destroy() {
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    struct task* task = &tasks[i];
    spinlock_lock(&task->lock);
    if (task->thread != NULL) {
      free(task->thread);
    }
    spinlock_unlock(&task->lock);
  }
}
