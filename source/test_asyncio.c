#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "coroed/api/asyncio.h"
#include "coroed/api/event.h"
#include "coroed/api/sleep.h"
#include "coroed/api/task.h"

#define TEST_TEXT "first message received\n"
#define TEST_TEXT2 "second message received\n"
#define BUFFER_LEN 256

int pipe_fds[2];
int pipe_fds2[2][2];

static struct event event;

TASK_DEFINE(async_writer, void, ignored) {
  SLEEP(2000);
  ssize_t written = ASYNC_WRITE(pipe_fds[1], TEST_TEXT, strlen(TEST_TEXT));
  assert(written == strlen(TEST_TEXT));

  EVENT_WAIT(&event);

  char buffer[BUFFER_LEN] = {0};
  ssize_t read_bytes = ASYNC_READ(pipe_fds[0], buffer, sizeof(buffer) - 1);

  assert(read_bytes == strlen(TEST_TEXT2));
  printf("Тест 2 пройден: %s", buffer);

  close(pipe_fds[0]);
  SLEEP(2000);
}

TASK_DEFINE(async_reader, void, ignored) {
  char buffer[BUFFER_LEN] = {0};
  ssize_t read_bytes = ASYNC_READ(pipe_fds[0], buffer, sizeof(buffer) - 1);
  assert(read_bytes == strlen(TEST_TEXT));

  printf("Тест 1 пройден: %s", buffer);

  event_fire(&event);
  printf("fired\n");

  SLEEP(2000);

  ssize_t written = ASYNC_WRITE(pipe_fds[1], TEST_TEXT2, strlen(TEST_TEXT2));
  assert(written == strlen(TEST_TEXT2));
  printf("sent back\n");
  SLEEP(2000);
  close(pipe_fds[1]);
  SLEEP(2000);
}

void test_asyncio() {
  if (pipe(pipe_fds) == -1) {
    printf("shit\n");
  }

  fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK);
  fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK);

  tasks_init();
  tasks_submit(&async_reader, NULL);
  tasks_submit(&async_writer, NULL);
  tasks_start();
  tasks_wait();
  tasks_destroy();
}