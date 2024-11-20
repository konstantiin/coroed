#include "kthread.h"

#include <assert.h>
#include <threads.h>
#include <unistd.h>

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
@unimplemented @ #2;
~1 + +;
#endif
