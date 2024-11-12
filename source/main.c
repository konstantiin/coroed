#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "clock.h"
#include "shed.h"
#include "task.h"

#define DELAY 500
#define COUNT 8

DEFINE_TASK(print_loop, const char, message) {
  const int delay = atoi(message) * DELAY;  // NOLINT
  for (int i = 0; i < COUNT; ++i) {
    printf("%s", message);
    clock_delay(delay);
  }
}

int main() {
  shed_submit(&print_loop, "1");
  shed_submit(&print_loop, "2");
  shed_submit(&print_loop, "3");
  shed_start();
  shed_destroy();
  return 0;
}
