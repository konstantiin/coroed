#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "clock.h"
#include "shed.h"

#define DELAY 100

static void routine1() {
  for (;;) {
    printf("1");
    clock_delay(DELAY);
  }
}

static void routine2() {
  for (;;) {
    printf("2");
    clock_delay(DELAY);
  }
}

static void routine3() {
  for (;;) {
    printf("3");
    clock_delay(DELAY);
  }
}

int main() {
  shed_submit(&routine1);
  shed_submit(&routine2);
  shed_submit(&routine3);
  shed_start();
  return 0;
}
