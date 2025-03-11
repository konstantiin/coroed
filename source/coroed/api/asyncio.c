#include "asyncio.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "coroed/api/task.h"
#include "coroed/sched/schedy.h"
#include "coroed/sched/kthread.h"

void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * Добавляет файловый дескриптор в epoll и усыпляет файбер.
 */
static void asyncio_wait(struct task* self, int fd, uint32_t events) {
  struct epoll_event ev = {.events = events, .data.ptr = self};
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

  task_block(self, fd);
}

/**
 * Реализация асинхронного чтения.
 */
ssize_t async_read(struct task* self, int fd, void* buf, size_t count) {
  ssize_t total = 0;

  while (count > 0) {
    ssize_t result = read(fd, buf, count);

    if (result > 0) {
      buf = (char*)buf + result;
      count -= result;
      total += result;
      continue;
    }

    if (result == 0) {
      return total;
    }

    if (result == -1 && errno == EINTR) {
      continue;
    }

    if (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if (total > 0) {
        return total;
      }
      asyncio_wait(self, fd, EPOLLIN);
      task_yield(self);
      continue;
    }

    return -errno;
  }

  return total;
}

/**
 * Реализация асинхронной записи.
 */
ssize_t async_write(struct task* self, int fd, const void* buf, size_t count) {
  ssize_t total = 0;

  while (count > 0) {
    ssize_t result = write(fd, buf, count);

    if (result > 0) {
      buf = (const char*)buf + result;
      count -= result;
      total += result;
      continue;
    }

    if (result == -1 && errno == EINTR) {
      continue;
    }

    if (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if (total > 0) {
        return total;
      }
      asyncio_wait(self, fd, EPOLLOUT);
      task_yield(self);
      continue;
    }

    return -errno;
  }

  return total;
}