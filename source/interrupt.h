#pragma once

#include <assert.h>
#include <stdint.h>

#include "alarm.h"

void interrupt_setup(alarm_callback callback);
void interrupt_on();
void interrupt_off();

struct interrupt_stack {
  int64_t depth;
};

void interrupt_stack_init(struct interrupt_stack* stack);
void interrupt_received(struct interrupt_stack* stack);
void interrupt_off_push(struct interrupt_stack* stack);
void interrupt_off_pop(struct interrupt_stack* stack);
