#pragma once

#include <stdint.h>

// TODO: rename to uthread
struct thread {
  void* context;
};

typedef void (*thread_routine)();

void thread_switch(struct thread* prev, struct thread* next);

struct thread* thread_allocate();

void thread_reset(struct thread* thread, thread_routine entry);

void* thread_ip(struct thread* thread);
