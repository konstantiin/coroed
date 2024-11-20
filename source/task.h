#pragma once

#include "uthread.h"

#define yield           \
  do {                  \
    task_yield(__self); \
  } while (false)

struct task;

void task_yield(struct task* task);
void task_exit(struct task* task);

#define DECLARE_TASK(name, type, argument)                                 \
  void task_body_##name(struct task* __self, type* argument); /* NOLINT */ \
  void name()

#define DEFINE_TASK(name, type, argument) \
  DECLARE_TASK(name, type, argument);     \
                                          \
  void name() {                           \
    struct task* self = NULL;             \
    type* argument = NULL; /* NOLINT */   \
    UTHREAD_READ_ARG_0(self);             \
    UTHREAD_READ_ARG_1(argument);         \
    task_body_##name(self, argument);     \
    task_exit(self);                      \
  }                                       \
                                          \
  void task_body_##name(struct task* __self, type* argument) /* NOLINT */
