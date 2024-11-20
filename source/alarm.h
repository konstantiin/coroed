#pragma once

typedef void (*alarm_callback)(int);

void alarm_setup(alarm_callback callback);

void alarm_on();

void alarm_off();
