#include <sys/wait.h>
#include "defs.h"
#include "alarm.h"
#include INC_UNISTD

/* alarm signal functions */

int alarm_count;

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
  alarm_set();
}

void alarm_wait_for()
{
  while (1) {
    PAUSE(SIGALRM);
    if (alarm_count--) return;
  }
}
