#pragma once

#include <stdint.h>
#include <stdlib.h>

// TODO: make stack size constant and rely on it
// TODO: rename to uthread
struct thread {
  void* context;
  size_t stack_size;
};

typedef void (*thread_routine)();

void thread_switch(struct thread* prev, struct thread* next);

struct thread* thread_allocate(size_t stack_size);

void thread_reset(struct thread* thread, thread_routine entry);

void* thread_ip(struct thread* thread);
