#pragma once

#include <stdint.h>

struct uthread {
  void* context;
};

typedef void (*uthread_routine)();

void uthread_switch(struct uthread* prev, struct uthread* next);

struct uthread* uthread_allocate();

void uthread_reset(struct uthread* thread);

void uthread_set_entry(struct uthread* thread, uthread_routine entry);
void uthread_set_arg_0(struct uthread* thread, void* value);
void uthread_set_arg_1(struct uthread* thread, void* value);

#define UTHREAD_READ_ARG_0(r15) __asm__ volatile("movq %%r15, %0" : "=r"(r15) : : "r15");
#define UTHREAD_READ_ARG_1(r14) __asm__ volatile("movq %%r14, %0" : "=r"(r14) : : "r14");
