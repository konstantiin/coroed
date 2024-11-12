#pragma once

#include <stdbool.h>
#include <stdint.h>

struct uthread {
  void* context;
};

typedef void (*uthread_routine)();

void uthread_switch(struct uthread* prev, struct uthread* next);

struct uthread* uthread_allocate();

void uthread_reset(struct uthread* thread, uthread_routine entry, void* argument);

bool uthread_is_finished(struct uthread* thread);

void* uthread_argument(struct uthread* thread);
