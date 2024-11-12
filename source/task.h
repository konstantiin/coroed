#pragma once

#include "sched.h"
#include "uthread.h"

#define DECLARE_TASK(name, type, argument) \
  void task_body_##name(type* argument);   \
  void name()

#define DEFINE_TASK(name, type, argument)               \
  DECLARE_TASK(name, type, argument);                   \
  void name() {                                         \
    struct uthread* self = sched_current();              \
    type* __argument = (type*)(uthread_argument(self)); \
    task_body_##name(__argument);                       \
    sched_exit();                                        \
  }                                                     \
  void task_body_##name(type* argument)
