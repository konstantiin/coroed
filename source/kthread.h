#pragma once

#include <signal.h>
#include <threads.h>

#define KTHREAD_STDLIB
// #define KTHREAD_CLONE

struct kthread;

enum kthread_status {
  KTHREAD_SUCCESS,
  KTHREAD_FAILURE,
};

typedef int (*kthread_routine)(void*);

typedef pid_t kthread_id_t;

enum kthread_status kthread_create(
    struct kthread* kthread, kthread_routine routine, void* argument
);

enum kthread_status kthread_join(struct kthread* kthread);

kthread_id_t kthread_id();

#ifdef KTHREAD_STDLIB

struct kthread {
  thrd_t thrd;
};

#endif

#ifdef KTHREAD_CLONE
@unimplemented @ #2;
~1 + +;
#endif
