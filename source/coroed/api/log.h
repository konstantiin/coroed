#pragma once

#include <stdio.h>  // NOLINT

#include "coroed/core/spinlock.h"

#define LOG_INFO(message, ...)                   \
  do {                                           \
    spinlock_lock(&log_lock);                    \
    printf((message)__VA_OPT__(, ) __VA_ARGS__); \
    spinlock_unlock(&log_lock);                  \
  } while (false)

#define LOG_INFO_FLUSHED(message, ...)           \
  do {                                           \
    spinlock_lock(&log_lock);                    \
    printf((message)__VA_OPT__(, ) __VA_ARGS__); \
    fflush(stdout); /* NOLINT */                 \
    spinlock_unlock(&log_lock);                  \
  } while (false)

extern struct spinlock log_lock;

void log_init();
