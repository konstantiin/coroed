// Linux
#define _GNU_SOURCE

#include "schedy.h"

#include <assert.h>
#include <sched.h>
#include <signal.h>
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

#include "alarm.h"
#include "spinlock.h"
#include "task.h"
#include "uthread.h"

#define THREAD_COUNT_LIMIT 8

#define SCHED_WORKER_STACK_SIZE (size_t)(1024 * 1024)
#define SCHED_WORKERS_COUNT (size_t)(2)

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

static thread_local struct task* sched_task = NULL;
static thread_local struct task* curr_task = NULL;

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

  /* Setup an "interrupt" handler */
  alarm_setup(sched_switch_to_scheduler);

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

    alarm_on();
    sched_switch_to(task);

    printf(" ");
    fflush(stdout);  // NOLINT

    if (task->state == UTHREAD_CANCELLED) {
      uthread_reset(task->thread, /* entry = */ NULL, /* argument = */ NULL);
      task->state = UTHREAD_ZOMBIE;
    } else if (task->state == UTHREAD_RUNNING) {
      task->state = UTHREAD_RUNNABLE;
    } else {
      assert(false);
    }

    spinlock_unlock(&task->lock);
  }

  return 0;
}

void sched_start() {
  uint8_t* stacks[SCHED_WORKERS_COUNT] = {0};
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    stacks[i] = mmap(
        /* addr = */ NULL,
        SCHED_WORKER_STACK_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
        /* fd = */ -1,
        /* offset = */ 0
    );
    assert(stacks[i] != MAP_FAILED);
  }

  pid_t workers[SCHED_WORKERS_COUNT] = {0};
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    uint8_t* stack_top = stacks[i] + SCHED_WORKER_STACK_SIZE;
    const int flags = SIGCHLD | CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_IO;
    void* argument = NULL;
    workers[i] = clone(sched_loop, stack_top, flags, argument);
    assert(0 < workers[i]);
  }

  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    int status = waitpid(workers[i], NULL, 0);
    assert(status != -1);
  }
}

struct task* sched_next() {
  static thread_local size_t curr_index = 0;
  static const size_t max_attempts = 64 * THREAD_COUNT_LIMIT;

  for (size_t i = 0; i < max_attempts; ++i) {
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

  return NULL;
}

void sched_cancel(struct task* task) {
  task->state = UTHREAD_CANCELLED;
}

struct task* task_current() {
  return curr_task;
}

void task_yield() {
  alarm_off();
  sched_switch_to_scheduler();
}

void* unsafe_task_argument(struct task* task) {
  return uthread_argument(task->thread);
}

void unsafe_task_exit() {
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
      uthread_reset(task->thread, entry, argument);
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
