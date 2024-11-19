#pragma once

struct task;

struct task* task_current();

void* unsafe_task_argument(struct task* task);
void unsafe_task_exit();

#define DECLARE_TASK(name, type, argument) \
  void task_body_##name(type* argument);   \
  void name()

#define DEFINE_TASK(name, type, argument)                   \
  DECLARE_TASK(name, type, argument);                       \
                                                            \
  void name() {                                             \
    struct task* self = task_current();                     \
    type* __argument = (type*)(unsafe_task_argument(self)); \
    task_body_##name(__argument);                           \
    unsafe_task_exit();                                            \
  }                                                         \
                                                            \
  void task_body_##name(type* argument)
