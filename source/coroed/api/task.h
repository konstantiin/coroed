#pragma once

#include "coroed/sched/uthread.h"

#define YIELD           \
  do {                  \
    task_yield(__self); \
  } while (false)

#define GO(entry, argument)                   \
  do {                                        \
    task_submit(__self, (entry), (argument)); \
  } while (false)

struct task;

typedef uthread_routine task_body;

void tasks_init();
void tasks_submit(task_body body, void* argument);
void tasks_start();
void tasks_wait();
void tasks_print_statistics();
void tasks_destroy();

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
