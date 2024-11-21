#pragma once

#define CPU_RELAX() __asm__ volatile("pause" ::: "memory") /* NOLINT */

#define SPINLOOP(spins)                 \
  do {                                  \
    for (int i = 0; i < (spins); ++i) { \
      CPU_RELAX();                      \
    }                                   \
  } while (false)
