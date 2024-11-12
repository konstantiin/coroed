#pragma once

#include <shed.h>
#include <uthread.h>

#define DECLARE_TASK(name, type, argument) \
  void task_body_##name(type* argument);   \
  void name()

#define DEFINE_TASK(name, type, argument)                         \
  DECLARE_TASK(name, type, argument);                             \
  void name() {                                                   \
    type* __argument = (type*)(uthread_argument(shed_current())); \
    task_body_##name(__argument);                                 \
  }                                                               \
  void task_body_##name(type* argument)
