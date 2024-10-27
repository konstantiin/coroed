#include "shed.h"

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

#include "thread.h"

#define THREAD_COUNT_LIMIT 8

static struct thread* threads[THREAD_COUNT_LIMIT] = {NULL};

static thread_local struct thread* shed_thread = NULL;
static thread_local struct thread* curr_thread = NULL;

static void shed_interrupt(int signo) {
  assert(signo == SIGALRM);
  printf("interrupt\n");
  thread_switch(curr_thread, shed_thread);
}

static struct thread* sched_next();

void shed_start() {
  /* Setup an "interrupt" handler */
  struct sigaction action;
  action.sa_handler = &shed_interrupt;
  action.sa_flags = SA_NODEFER;
  sigaction(SIGALRM, &action, /* oldact = */ NULL);

  /* Setup shed thread */
  struct thread shed = {.context = NULL, .stack_size = 0};
  shed_thread = &shed;

  /* Event loop */
  for (;;) {
    struct thread* thread = sched_next();
    alarm(1);
    curr_thread = thread;
    thread_switch(shed_thread, curr_thread);
  }
}

static struct thread* sched_next() {
  static size_t curr_index = 0;
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    struct thread* thread = threads[curr_index];
    curr_index = (curr_index + 1) % THREAD_COUNT_LIMIT;
    if (thread != NULL && thread_ip(thread) != NULL) {
      return thread;
    }
  }

  printf("No more threads to run");
  exit(0);
}

void shed_submit(void (*entry)()) {
  const size_t default_stack_size = 16384;

  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    if (threads[i] == NULL) {
      threads[i] = thread_allocate(default_stack_size);
      assert(threads[i] != NULL);
    }

    if (thread_ip(threads[i]) == NULL) {
      thread_reset(threads[i], entry);
      return;
    }
  }

  printf("Threads exhausted");
  exit(1);
}

void shed_destroy() {
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    struct thread* thread = threads[i];
    if (thread != NULL) {
      free(thread);
    }
  }
}
