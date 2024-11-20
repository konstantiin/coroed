#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "clock.h"
#include "log.h"
#include "schedy.h"
#include "task.h"

#define DELAY 500
#define COUNT 8

TASK_DEFINE(print_loop, const char, message) {
  const int delay = atoi(message) * DELAY;  // NOLINT
  for (int i = 0; i < COUNT; ++i) {
    char* msg = malloc(sizeof(char) * (strlen(message) + 1));
    strcpy(msg, message);

    LOG_INFO_FLUSHED("%s ", msg);
    yield;

    free(msg);
    clock_delay(delay);
  }
}

TASK_DEFINE(spammer, void, ignored) {
  for (int i = 0; i < COUNT; ++i) {
    sched_submit(&print_loop, "1");
  }
}

int main() {
  log_init();
  sched_init();

  sched_submit(&print_loop, "1");
  sched_submit(&spammer, NULL);
  sched_submit(&print_loop, "1");
  sched_submit(&spammer, NULL);
  sched_submit(&print_loop, "1");
  sched_submit(&spammer, NULL);
  sched_submit(&print_loop, "1");

  sched_start();

  sched_wait();

  sched_print_statistics();

  sched_destroy();

  return 0;
}
