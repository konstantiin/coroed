#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "coroed/api/event.h"
#include "coroed/api/log.h"
#include "coroed/api/sleep.h"
#include "coroed/api/task.h"

enum {
  DELAY = 50,
  COUNT = 8,
};

static atomic_int_least64_t actual_count = 0;
static int_least64_t expected_count = 0;

static atomic_int_least64_t state = 0;
static struct event event;

TASK_DEFINE(eventer, void, ignored) {
  SLEEP(1000);
  state = 1;
  event_fire(&event);
}

TASK_DEFINE(event_test, void, ignored) {
  assert(state == 0);
  GO(eventer, NULL);
  EVENT_WAIT(&event);
  assert(state == 1);
}

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
    SLEEP(delay);
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
  tasks_submit(&adder, (void*)(value));
}

int randint() {
  return rand() % 32;  // NOLINT
}

int main() {
  log_init();
  tasks_init();

  srand(231);  // NOLINT

  tasks_submit(&spammer, NULL);
  tasks_submit(&event_test, NULL);
  add(randint());
  add(randint());
  add(randint());
  add(randint());
  add(randint());
  add(randint());
  add(randint());
  add(randint());

  tasks_start();

  tasks_wait();

  tasks_print_statistics();

  assert(atomic_load(&actual_count) == expected_count);
  assert(state == 1);

  tasks_destroy();

  return 0;
}
