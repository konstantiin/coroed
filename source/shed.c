#include "shed.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "thread.h"

#define THREAD_COUNT_LIMIT 64

static struct thread* threads[THREAD_COUNT_LIMIT];
static int threads_count = 0;

static thread_local struct thread* shed_thread;
static thread_local struct thread* curr_thread;

static void shed_interrupt(int signo);

void shed_init() {
  /* Setup an "interrupt" handler */
  struct sigaction action;
  action.sa_handler = &shed_interrupt;
  action.sa_flags = SA_NODEFER;

  sigaction(SIGALRM, &action, /* oldact = */ NULL);
}

static void shed_interrupt(int signo) {
  assert(signo == SIGALRM);

  printf("interrupt\n");
  thread_switch(curr_thread, shed_thread);
}

void shed_routine() {
  int current_thread = 0;

  struct thread shed;
  shed_thread = &shed;

  for (;;) {
    if (threads_count == 0) {
      continue;
    }

    curr_thread = threads[current_thread++ % threads_count];

    alarm(1);

    thread_switch(shed_thread, curr_thread);
  }
}

void shed_run(void (*entry)()) {
  const size_t default_stack_size = 16384;
  threads[threads_count++] = thread_create(default_stack_size, entry);
}

void shed_destroy() {
  for (int i = 0; i < threads_count; ++i) {
    free(threads[i]);
  }
}
