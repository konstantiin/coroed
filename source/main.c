#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "clock.h"
#include "schedy.h"
#include "task.h"

#define DELAY 500
#define COUNT 8

DEFINE_TASK(print_loop, const char, message) {
  const int delay = atoi(message) * DELAY;  // NOLINT
  for (int i = 0; i < COUNT; ++i) {
    printf("%s", message);
    task_yield();
    clock_delay(delay);
  }
}

int main() {
  sched_init();

  char* arg[] = {"1", "2", "3"};

  sched_submit(&print_loop, arg[0]);
  sched_submit(&print_loop, arg[1]);
  sched_submit(&print_loop, arg[2]);

  sched_start();

  sched_destroy();

  return 0;
}
