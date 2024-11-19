#pragma once

#include <uthread.h>

struct task;

struct task* sched_current();

void* task_argument(struct task* task);

void sched_submit(uthread_routine entry, void* argument);

void sched_cancel(struct task* thread);

void sched_yield();

void sched_exit();

void sched_start();

void sched_destroy();
