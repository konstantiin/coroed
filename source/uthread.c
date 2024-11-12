#include "uthread.h"

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

struct switch_frame* uthread_frame(struct uthread* thread) {
  return thread->context;
}

void uthread_switch(struct uthread* prev, struct uthread* next) {
  void __switch_threads(void** prev, void* next);  // NOLINT
  __switch_threads(&prev->context, next->context);
}

struct uthread* uthread_allocate() {
  const size_t size = STACK_SIZE + sizeof(struct uthread);

  struct uthread* thread = (struct uthread*)(malloc(size));
  if (!thread) {
    return NULL;
  }

  uthread_reset(thread, /* entry = */ NULL, /* argument = */ NULL);

  return thread;
}

void uthread_reset(struct uthread* thread, uthread_routine entry, void* argument) {
  thread->context = (uint8_t*)thread + STACK_SIZE - sizeof(struct switch_frame);

  struct switch_frame* frame = (struct switch_frame*)(thread->context);
  memset(frame, 0, sizeof(struct switch_frame));
  frame->rip = (uint64_t)entry;
  frame->r15 = (uint64_t)argument;
}

bool uthread_is_finished(struct uthread* thread) {
  return (void*)(uthread_frame(thread)->rip) == NULL;
}

void* uthread_argument(struct uthread* thread) {
  return (void*)(uthread_frame(thread)->r15);
}
