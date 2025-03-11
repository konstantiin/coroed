#include "event.h"

#include <stddef.h>

#include "coroed/api/task.h"
#include "coroed/core/spinlock.h"

void event_init(struct event* event) {
  event->is_fired = false;
  event->task = NULL;
  spinlock_init(&event->lock);
}

void event_wait(struct task* caller, struct event* event) {
  // Заметим, что текущая реализация является блокирующей,
  // что может приводить к дедлокам при использовании некоторых
  // алгоритмов планирования (например, FIFO). Для решения этой
  // проблемы следует "парковать" файбер в планировщике, отправляя
  // его в а-ля BLOCKED состояние.
  spinlock_lock(&event->lock);
  if (!event->is_fired) {
    insert_blocked_task(caller, event->task);
    event->task = caller;
  }
  spinlock_unlock(&event->lock);
  task_yield(caller);
}

void event_fire(struct event* event) {
  spinlock_lock(&event->lock);
  event->is_fired = true;
  task_unblock_queue(event->task);
  spinlock_unlock(&event->lock);
}