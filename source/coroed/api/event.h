#pragma once

#include <stdatomic.h>

#include "coroed/api/task.h"

#define EVENT_WAIT(event) event_wait(__self, (event))

struct event {
  atomic_bool is_fired;
};

void event_init(struct event* event);

void event_wait(struct task* caller, struct event* event);

void event_fire(struct event* event);
