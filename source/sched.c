#include "sched.h"

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#include "alarm.h"
#include "spinlock.h"
#include "task.h"
#include "uthread.h"

#define THREAD_COUNT_LIMIT 8

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

void sched_loop() {
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
  }
}

struct task* sched_next() {
  static size_t curr_index = 0;
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    struct task* task = &tasks[curr_index];
    curr_index = (curr_index + 1) % THREAD_COUNT_LIMIT;
    if (task->thread != NULL && task->state == UTHREAD_RUNNABLE) {
      return task;
    }
  }
  return NULL;
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

    if (task->thread == NULL) {
      task->thread = uthread_allocate();
      assert(task->thread != NULL);
      task->state = UTHREAD_ZOMBIE;
    }

    if (task->state == UTHREAD_ZOMBIE) {
      uthread_reset(task->thread, entry, argument);
      task->state = UTHREAD_RUNNABLE;
      return;
    }
  }

  printf("Threads exhausted");
  exit(1);
}

void sched_cancel(struct task* task) {
  task->state = UTHREAD_CANCELLED;
}

void sched_destroy() {
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    struct task* task = &tasks[i];
    if (task->thread != NULL) {
      free(task->thread);
    }
  }
}
