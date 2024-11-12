#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "clock.h"
#include "shed.h"
#include "task.h"

#define DELAY 100

DEFINE_TASK(print_loop, const char, message) {
  for (;;) {
    printf("%s", message);
    clock_delay(DELAY);
  }
}

int main() {
  shed_submit(&print_loop, "1");
  shed_submit(&print_loop, "2");
  shed_submit(&print_loop, "3");
  shed_start();
  return 0;
}
