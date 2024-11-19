#pragma once

#include "uthread.h"

struct task;

void sched_submit(uthread_routine entry, void* argument);

void sched_cancel(struct task* task);

void sched_yield();

void sched_start();

void sched_destroy();
