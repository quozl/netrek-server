#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include "defs.h"
#include "alarm.h"


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
  (void) signal(SIGALRM, alarm_handler);
}

void alarm_ignore()
{
  (void) signal(SIGALRM, SIG_IGN);
}

void alarm_prevent_inheritance()
{
  (void) signal(SIGALRM, SIG_DFL);
}

void alarm_handler(int signum)
{
  alarm_count++;
  signal(SIGALRM, alarm_handler);
}

void alarm_wait_for()
{
  while (1) {
    pause();
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

void alarm_setitimer(int distortion, int fps)
{
  struct itimerval udt;
  int tv_usec = 1000000 / distortion * 10 / fps;
  int stat;

  udt.it_interval.tv_sec = 0;
  udt.it_interval.tv_usec = tv_usec;
  udt.it_value.tv_sec = 0;
  udt.it_value.tv_usec = tv_usec;

  fprintf(stderr, "alarm_setitimer: interval timer %d microseconds\n", tv_usec);
  stat = setitimer(ITIMER_REAL, &udt, (struct itimerval *) NULL);
  if (stat == 0) return;

  perror("alarm_setitimer");
  exit(1);
}
