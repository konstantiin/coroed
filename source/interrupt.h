#pragma once

#include "alarm.h"
#include "kthread.h"

#ifdef KTHREAD_STDLIB

inline void interrupt_setup(alarm_callback callback) {
  // Do nothing
}

inline void interrupt_on() {
  // Do nothing
}

inline void interrupt_off() {
  // Do nothing
}

#endif

#ifdef KTHREAD_CLONE

inline void interrupt_setup(alarm_callback callback) {
  alarm_setup(callback);
}

inline void interrupt_on() {
  alarm_on();
}

inline void interrupt_off() {
  alarm_off();
}

#endif
