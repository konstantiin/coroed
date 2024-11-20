#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "log.h"
#include "schedy.h"
#include "task.h"

enum {
  DELAY = 50,
  COUNT = 8,
};

static atomic_int_least64_t actual_count = 0;
static int_least64_t expected_count = 0;

TASK_DEFINE(increment, void, ignored) {
  atomic_fetch_add(&actual_count, 1);
}

TASK_DEFINE(decrement, void, ignored) {
  atomic_fetch_add(&actual_count, -1);
}

TASK_DEFINE(adder, void, argument) {
  int64_t value = (int64_t)(argument);
  if (0 < value) {
    for (int64_t i = 1; i <= value; ++i) {
      GO(increment, NULL);
    }
  } else if (value < 0) {
    for (int64_t i = -1; i >= value; ++i) {
      GO(decrement, NULL);
    }
  }
}

TASK_DEFINE(print_loop, const char, message) {
  const int delay = atoi(message) * DELAY;  // NOLINT
  for (int i = 0; i < COUNT; ++i) {
    char* msg = malloc(sizeof(char) * (strlen(message) + 1));
    strcpy(msg, message);

    LOG_INFO_FLUSHED("%s", msg);

    YIELD;

    free(msg);
    clock_delay(delay);
  }
}

TASK_DEFINE(spammer, void, ignored) {
  for (int i = 0; i < COUNT / 2; ++i) {
    GO(&print_loop, "1");
    GO(&print_loop, "2");
  }
}

void add(int64_t value) {
  expected_count += value;
  sched_submit(&adder, (void*)(value));
}

int randint() {
  return rand() % 32;  // NOLINT
}

int main() {
  log_init();
  sched_init();

  srand(231);  // NOLINT

  sched_submit(&spammer, NULL);
  add(randint());
  add(randint());
  add(randint());
  add(randint());
  add(randint());
  add(randint());
  add(randint());
  add(randint());

  sched_start();

  sched_wait();

  sched_print_statistics();

  assert(atomic_load(&actual_count) == expected_count);

  sched_destroy();

  return 0;
}
