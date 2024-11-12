#pragma once

#include "shed.h"
#include "uthread.h"

#define DECLARE_TASK(name, type, argument) \
  void task_body_##name(type* argument);   \
  void name()

#define DEFINE_TASK(name, type, argument)               \
  DECLARE_TASK(name, type, argument);                   \
  void name() {                                         \
    struct uthread* self = shed_current();              \
    type* __argument = (type*)(uthread_argument(self)); \
    task_body_##name(__argument);                       \
    shed_exit();                                        \
  }                                                     \
  void task_body_##name(type* argument)
