#pragma once

#include <threads.h>

typedef void (*alarm_callback)();

void alarm_setup(alarm_callback callback);

void alarm_on();

void alarm_off();
