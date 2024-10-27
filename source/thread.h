#pragma once

#include <stdint.h>
#include <stdlib.h>

struct thread {
  void* context;
};

void thread_switch(struct thread* prev, struct thread* next);

struct thread* thread_create(size_t stack_size, void (*entry)());
