#include "alarm.h"

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void alarm_setup(alarm_callback callback) {
  struct sigaction action;
  action.sa_handler = callback;
  action.sa_flags = SA_NODEFER;
  sigaction(SIGALRM, &action, /* oldact = */ NULL);
}

void alarm_on() {
  alarm(1);
}

void alarm_off() {
  alarm(0);
}
