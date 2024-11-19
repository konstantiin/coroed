#include "alarm.h"

#include <assert.h>
#include <signal.h>
#include <threads.h>
#include <unistd.h>

thread_local alarm_callback on_alarm = NULL;

static void signal_handler(int signo) {
  assert(signo == SIGALRM);
  on_alarm();
}

void alarm_setup(alarm_callback callback) {
  struct sigaction action;
  action.sa_handler = &signal_handler;
  action.sa_flags = SA_NODEFER;
  sigaction(SIGALRM, &action, /* oldact = */ NULL);

  on_alarm = callback;
}

void alarm_on() {
  alarm(1);
}

void alarm_off() {
  alarm(0);
}
