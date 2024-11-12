#include "thread.h"

#include <stdlib.h>
#include <string.h>

#define STACK_SIZE 16384

struct [[gnu::packed]] switch_frame {
  uint64_t rflags;
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t rbp;
  uint64_t rbx;
  uint64_t rip;
};

void thread_switch(struct thread* prev, struct thread* next) {
  void __switch_threads(void** prev, void* next);  // NOLINT
  __switch_threads(&prev->context, next->context);
}

struct thread* thread_allocate() {
  const size_t size = STACK_SIZE + sizeof(struct thread);

  struct thread* thread = (struct thread*)(malloc(size));
  if (!thread) {
    return NULL;
  }

  thread_reset(thread, NULL);

  return thread;
}

void thread_reset(struct thread* thread, thread_routine entry) {
  thread->context = (uint8_t*)thread + STACK_SIZE - sizeof(struct switch_frame);

  struct switch_frame* frame = (struct switch_frame*)(thread->context);
  memset(frame, 0, sizeof(struct switch_frame));
  frame->rip = (uint64_t)entry;
}

void* thread_ip(struct thread* thread) {
  struct switch_frame* frame = (struct switch_frame*)(thread->context);
  return (void*)(frame->rip);
}
