#pragma once

#include "uthread.h"

struct task;

void sched_init();

void sched_submit(uthread_routine entry, void* argument);

void sched_start();

void sched_wait();

void sched_print_statistics();

void sched_destroy();
