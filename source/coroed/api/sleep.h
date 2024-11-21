#pragma once

#include <stdint.h>

#include "task.h"

#define SLEEP(ms) task_sleep(__self, (ms))

void task_sleep(struct task* caller, uint64_t milliseconds);
