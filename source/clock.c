#include "clock.h"

#include <assert.h>
#include <time.h>

static unsigned long long timespec2ms(const struct timespec* spec) {
  return (unsigned long long)spec->tv_sec * 1000 + spec->tv_sec / (1000 * 1000);
}

uint64_t clock_now() {
  struct timespec spec;

  int code = clock_gettime(CLOCK_MONOTONIC, &spec);
  assert(code == 0);

  return timespec2ms(&spec);
}

void clock_delay(uint64_t milliseconds) {
  const unsigned long long start = clock_now();
  while (clock_now() - start < milliseconds) {
    // Do nothing
  }
}
