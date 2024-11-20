#include "interrupt.h"

#include <assert.h>

#include "alarm.h"
#include "kthread.h"

#ifdef KTHREAD_STDLIB

void interrupt_setup(alarm_callback callback) {
  // Do nothing
}

void interrupt_on() {
  // Do nothing
}

void interrupt_off() {
  // Do nothing
}

#endif

#ifdef KTHREAD_CLONE

void interrupt_setup(alarm_callback callback) {
  alarm_setup(callback);
}

void interrupt_on() {
  alarm_on();
}

void interrupt_off() {
  alarm_off();
}

#endif

void interrupt_stack_init(struct interrupt_stack* stack) {
  stack->depth = 1;
}

void interrupt_received(struct interrupt_stack* stack) {
  stack->depth += 1;
}

void interrupt_off_push(struct interrupt_stack* stack) {
  interrupt_off();
  stack->depth += 1;
}

void interrupt_off_pop(struct interrupt_stack* stack) {
  assert(0 < (stack)->depth);
  (stack)->depth -= 1;
  if ((stack)->depth == 0) {
    interrupt_on();
  }
}
