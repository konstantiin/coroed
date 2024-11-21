#pragma once

#define KTHREAD_STDLIB

#include <stdint.h>
#include <unistd.h>

#ifdef KTHREAD_STDLIB
#include <threads.h>
#endif

struct kthread;

enum kthread_status {
  KTHREAD_SUCCESS,
  KTHREAD_FAILURE,
};

typedef int (*kthread_routine)(void*);

typedef size_t kthread_id_t;

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
