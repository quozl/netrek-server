#include <signal.h>
#include <sys/wait.h>
#include "defs.h"
#include "alarm.h"
#include INC_UNISTD

/* alarm signal functions */

sig_atomic_t alarm_count;

void alarm_set();
void alarm_handler(int signum);

void alarm_init()
{
  alarm_count = 0;
  alarm_set();
}

void alarm_set()
{
  (void) SIGNAL(SIGALRM, alarm_handler);
}

void alarm_ignore()
{
  (void) SIGNAL(SIGALRM, SIG_IGN);
}

void alarm_prevent_inheritance()
{
  (void) SIGNAL(SIGALRM, SIG_DFL);
}

void alarm_handler(int signum)
{
  alarm_count++;
  HANDLE_SIG(SIGALRM, alarm_handler);
}

void alarm_wait_for()
{
  while (1) {
    PAUSE(SIGALRM);
    if (alarm_count) {
      alarm_count--;
      return;
    }
  }
}

int alarm_send(pid_t pid)
{
  return kill(pid, SIGALRM);
}
