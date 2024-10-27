#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "clock.h"
#include "shed.h"

#define DELAY 200

static void routine1() {
  for (;;) {
    printf("In routine 1\n");
    clock_delay(DELAY);
  }
}

static void routine2() {
  for (;;) {
    printf("In routine 2\n");
    clock_delay(DELAY);
  }
}

int main() {
  shed_init();

  shed_run(&routine1);
  shed_run(&routine2);

  shed_routine();

  return 0;
}
