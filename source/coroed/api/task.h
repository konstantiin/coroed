#pragma once

#include "coroed/sched/uthread.h"

#define YIELD task_yield(__self)

#define GO(entry, argument) task_submit(__self, (entry), (argument))

struct task;

typedef struct {
  struct task* task;
} task_t;

typedef uthread_routine task_body;

void tasks_init();
void tasks_submit(task_body body, void* argument);
void tasks_start();
void tasks_wait();
void tasks_print_statistics();
void tasks_destroy();

void task_yield(struct task* caller);
void task_exit(struct task* caller);
task_t task_submit(struct task* caller, uthread_routine entry, void* argument);

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
