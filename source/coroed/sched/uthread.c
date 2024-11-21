#include "uthread.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define PAGE_SIZE (size_t)(4 * 1024)
#define THREAD_SIZE (size_t)(256 * PAGE_SIZE)
#define STACK_SIZE (size_t)(THREAD_SIZE - 8 * PAGE_SIZE)

struct [[gnu::packed]] switch_frame {
  // Callee-saved registers
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
  struct uthread* thread = mmap(
      /* addr   */ NULL,
      /* len    */ THREAD_SIZE,
      /* prot   */ PROT_READ | PROT_WRITE,
      /* flags  */ MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
      /* fd     */ -1,
      /* offset */ 0
  );
  if (!thread) {
    return NULL;
  }

  uthread_reset(thread);

  return thread;
}

void uthread_free(struct uthread* thread) {
  assert(thread != NULL);
  uthread_reset(thread);
  int code = munmap(thread, THREAD_SIZE);
  assert(code == 0);
}

void uthread_reset(struct uthread* thread) {
  thread->context = (uint8_t*)thread + STACK_SIZE;

  struct switch_frame* frame = (struct switch_frame*)(thread->context);
  memset(frame, 0, sizeof(struct switch_frame));
  uthread_set_entry(thread, NULL);
  uthread_set_arg_0(thread, NULL);
  uthread_set_arg_1(thread, NULL);
}

void uthread_set_entry(struct uthread* thread, uthread_routine entry) {
  uthread_frame(thread)->rip = (uint64_t)(entry);
}

void uthread_set_arg_0(struct uthread* thread, void* value) {
  uthread_frame(thread)->r15 = (uint64_t)(value);
}

void uthread_set_arg_1(struct uthread* thread, void* value) {
  uthread_frame(thread)->r14 = (uint64_t)(value);
}
