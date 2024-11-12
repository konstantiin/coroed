#pragma once

#include <uthread.h>

struct uthread* shed_current();

void shed_submit(uthread_routine entry, void* argument);

void shed_cancel(struct uthread* thread);

void shed_yield();

void shed_exit();

void shed_start();

void shed_destroy();
