#pragma once

#define KTHREAD_STDLIB
// #define KTHREAD_CLONE

#ifdef KTHREAD_CLONE
#define _GNU_SOURCE  // NOLINT
#endif

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

#ifdef KTHREAD_CLONE

#define KTHREAD_CLONE_PAGE_SIZE (size_t)(4 * 1024)
#define KTHREAD_CLONE_STACK_SIZE (size_t)(256 * KTHREAD_CLONE_PAGE_SIZE)

struct kthread {
  uint8_t* stack;
  pid_t pid;
};

#endif
