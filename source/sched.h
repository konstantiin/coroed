#pragma once

#include <uthread.h>

struct uthread* sched_current();

void sched_submit(uthread_routine entry, void* argument);

void sched_cancel(struct uthread* thread);

void sched_yield();

void sched_exit();

void sched_start();

void sched_destroy();
