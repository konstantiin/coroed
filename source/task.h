#pragma once

#include "uthread.h"

#define YIELD           \
  do {                  \
    task_yield(__self); \
  } while (false)

#define GO(entry, argument)                   \
  do {                                        \
    task_submit(__self, (entry), (argument)); \
  } while (false)

struct task;

void task_yield(struct task* task);
void task_exit(struct task* task);
void task_submit(struct task* parent, uthread_routine entry, void* argument);

#define TASK_DECLARE(name, type, argument)                                 \
  void task_body_##name(struct task* __self, type* argument); /* NOLINT */ \
  void name()

#define TASK_DEFINE(name, type, argument)      \
  TASK_DECLARE(name, type, argument);          \
                                               \
  void name() {                                \
    struct task* self = NULL;                  \
    type* argument = NULL;        /* NOLINT */ \
    UTHREAD_READ_ARG_0(self);     /* NOLINT */ \
    UTHREAD_READ_ARG_1(argument); /* NOLINT */ \
    task_body_##name(self, argument);          \
    task_exit(self);                           \
  }                                            \
                                               \
  void task_body_##name(struct task* __self, type* argument) /* NOLINT */
