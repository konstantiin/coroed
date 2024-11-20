#include "log.h"

struct spinlock log_lock;

void log_init() {
  spinlock_init(&log_lock);
}
