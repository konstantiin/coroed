#include "thread.h"

#include <string.h>

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

struct thread;

void thread_switch(struct thread* prev, struct thread* next) {
  void __switch_threads(void** prev, void* next);  // NOLINT
  __switch_threads(&prev->context, next->context);
}

struct thread* thread_create(size_t stack_size, void (*entry)()) {
  const size_t size = stack_size + sizeof(struct thread);

  struct thread* thread = (struct thread*)(malloc(size));
  if (!thread) {
    return NULL;
  }

  struct switch_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.rip = (uint64_t)entry;

  thread->context = (uint8_t*)thread + size - sizeof(frame);
  memcpy(thread->context, &frame, sizeof(frame));

  return thread;
}
