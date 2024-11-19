#pragma once

struct task;

struct task* task_current();

void task_yield();

void* task_argument(struct task* task);
void task_exit();

#define DECLARE_TASK(name, type, argument)            \
  void task_body_##name(type* argument); /* NOLINT */ \
  void name()

#define DEFINE_TASK(name, type, argument)                         \
  DECLARE_TASK(name, type, argument);                             \
                                                                  \
  void name() {                                                   \
    struct task* self = task_current();                           \
    type* __argument = (type*)(task_argument(self)); /* NOLINT */ \
    task_body_##name(__argument);                                 \
    task_exit();                                                  \
  }                                                               \
                                                                  \
  void task_body_##name(type* argument) /* NOLINT */
