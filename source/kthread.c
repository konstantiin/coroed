#include "kthread.h"

#include <assert.h>
#include <threads.h>
#include <unistd.h>

#ifdef KTHREAD_CLONE

#include <linux/sched.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/wait.h>

#endif

kthread_id_t kthread_id() {
  return getpid();
}

#ifdef KTHREAD_STDLIB

enum kthread_status kthread_status_from(int thrd_status) {
  return thrd_status == thrd_success ? KTHREAD_SUCCESS : KTHREAD_FAILURE;
}

enum kthread_status kthread_create(
    struct kthread* kthread, kthread_routine routine, void* argument
) {
  int code = thrd_create(&kthread->thrd, routine, /* arg = */ argument);
  return kthread_status_from(code);
}

enum kthread_status kthread_join(struct kthread* kthread) {
  int status = 0;
  int code = thrd_join(kthread->thrd, &status);
  assert(code != thrd_success || status == 0);
  return kthread_status_from(code);
}

#endif

#ifdef KTHREAD_CLONE

enum kthread_status kthread_create(
    struct kthread* kthread, kthread_routine routine, void* argument
) {
  kthread->stack = mmap(
      /* addr   */ NULL,
      /* len    */ KTHREAD_CLONE_STACK_SIZE,
      /* prot   */ PROT_READ | PROT_WRITE,
      /* flags  */ MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
      /* fd     */ -1,
      /* offset */ 0
  );
  assert(kthread->stack != MAP_FAILED);

  kthread->pid = clone(
      /* fn          */ routine,
      /* child_stack */ kthread->stack + KTHREAD_CLONE_STACK_SIZE,
      /* flags       */ SIGCHLD | CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_IO,
      /* arg         */ argument,
      /* parent_tid  */ NULL,
      /* tls         */ NULL,
      /* child_tid   */ NULL
  );
  assert(0 < kthread->pid);

  return KTHREAD_SUCCESS;
}

enum kthread_status kthread_join(struct kthread* kthread) {
  int status = 0;
  int code = waitpid(kthread->pid, &status, /* options */ 0);
  assert(code != -1 && status == 0);

  code = munmap(kthread->stack, KTHREAD_CLONE_STACK_SIZE);
  assert(code == 0);

  return KTHREAD_SUCCESS;
}

#endif
