#include "copyright.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "alarm.h"

static pid_t pid = 0;

void advertise()
{
  int status;
  pid_t result;

  /* do not proceed unless advertising is enabled */
  if (!server_advertise_enable) return;

  /* if forked process jams up, we will simply wait for it to unjam */
  if (pid != 0) {
    result = waitpid(pid, &status, WNOHANG);
    if (result != pid) return;
    pid = 0;
  }

  /* fork a process to handle the task */  
  if ((pid = fork()) != 0) return;

  /* from here on we are executing in the context of the forked process */
  alarm_prevent_inheritance();

  pmessage(0, MALL, "GOD->ALL",
	   "ad: %s", server_advertise_filter);

  exit(0);
}
