#include "clock.h"

#include <assert.h>
#include <bits/time.h>
#include <stdint.h>
#include <time.h>

static unsigned long long timespec2ms(const struct timespec* spec) {
  const time_t ms_in_s = 1000;
  return (unsigned long long)spec->tv_sec * ms_in_s + spec->tv_sec / (ms_in_s * ms_in_s);
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
