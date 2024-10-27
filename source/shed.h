#pragma once

struct thread;

void shed_init();

void shed_run(void (*entry)());

void shed_routine();

void shed_destroy();
