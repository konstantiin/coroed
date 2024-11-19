#pragma once

#include <stdatomic.h>
#include <stdbool.h>

struct spinlock {
  atomic_bool is_locked;
};

void spinlock_init(struct spinlock* lock);
void spinlock_lock(struct spinlock* lock);
bool spinlock_try_lock(struct spinlock* lock);
void spinlock_unlock(struct spinlock* lock);
