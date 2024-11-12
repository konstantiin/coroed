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

#include "uthread.h"

#define THREAD_COUNT_LIMIT 8

static struct uthread* threads[THREAD_COUNT_LIMIT] = {NULL};

static thread_local struct uthread* shed_thread = NULL;
static thread_local struct uthread* curr_thread = NULL;

static void shed_interrupt(int signo) {
  assert(signo == SIGALRM);
  printf(" ");
  fflush(stdout);
  uthread_switch(curr_thread, shed_thread);
}

static struct uthread* sched_next();

void shed_start() {
  /* Setup an "interrupt" handler */
  struct sigaction action;
  action.sa_handler = &shed_interrupt;
  action.sa_flags = SA_NODEFER;
  sigaction(SIGALRM, &action, /* oldact = */ NULL);

  /* Setup shed thread */
  struct uthread shed = {.context = NULL};
  shed_thread = &shed;

  /* Event loop */
  for (;;) {
    struct uthread* thread = sched_next();
    assert(thread);
    alarm(1);
    curr_thread = thread;
    uthread_switch(shed_thread, curr_thread);
  }
}

static struct uthread* sched_next() {
  static size_t curr_index = 0;
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    struct uthread* thread = threads[curr_index];
    curr_index = (curr_index + 1) % THREAD_COUNT_LIMIT;
    if (thread != NULL && uthread_ip(thread) != NULL) {
      return thread;
    }
  }
  return NULL;
}

void shed_submit(void (*entry)()) {
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    if (threads[i] == NULL) {
      threads[i] = uthread_allocate();
      assert(threads[i] != NULL);
    }

    if (uthread_ip(threads[i]) == NULL) {
      uthread_reset(threads[i], entry);
      return;
    }
  }

  printf("Threads exhausted");
  exit(1);
}

void shed_destroy() {
  for (size_t i = 0; i < THREAD_COUNT_LIMIT; ++i) {
    struct uthread* thread = threads[i];
    if (thread != NULL) {
      free(thread);
    }
  }
}
