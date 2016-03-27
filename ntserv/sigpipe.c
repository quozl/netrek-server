/* signal to pipe delivery implementation */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "sigpipe.h"

/* pipe private to process */
static int sigpipe[2];

/* create a signal pipe, returns 0 for success, -1 with errno for failure */
int sigpipe_create()
{
  int rc;
  
  rc = pipe(sigpipe);
  if (rc < 0) return rc;
  
  fcntl(sigpipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(sigpipe[1], F_SETFD, FD_CLOEXEC);
  
#ifdef O_NONBLOCK
#define FLAG_TO_SET O_NONBLOCK
#else
#ifdef SYSV
#define FLAG_TO_SET O_NDELAY
#else /* BSD */
#define FLAG_TO_SET FNDELAY
#endif
#endif
  
  rc = fcntl(sigpipe[1], F_GETFL);
  if (rc != -1)
    rc = fcntl(sigpipe[1], F_SETFL, rc | FLAG_TO_SET);
  if (rc < 0) return rc;
  return 0;
#undef FLAG_TO_SET
}

/* generic handler for signals, writes signal number to pipe */
void sigpipe_handler(int signum)
{
  write(sigpipe[1], &signum, sizeof(signum));
  sigpipe_assign(signum);
}

/* internal sigaction wrapper */
void sigpipe_sigaction(int signum, void (*handler)(int signum))
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigaction(signum, &sa, NULL);
}

/* assign a signal number to the pipe */
void sigpipe_assign(int signum)
{
  sigpipe_sigaction(signum, sigpipe_handler);
}

/* suspend signal delivery for a time */
void sigpipe_suspend(int signum)
{
  sigpipe_sigaction(signum, SIG_IGN);
}

/* resume signal delivery after suspend */
void sigpipe_resume(int signum)
{
  sigpipe_sigaction(signum, sigpipe_handler);
}

/* return the signal pipe read file descriptor for select(2) */
int sigpipe_fd()
{
  return sigpipe[0];
}

/* read and return the pending signal from the pipe */
int sigpipe_read()
{
  int signum;
  read(sigpipe[0], &signum, sizeof(signum));
  return signum;
}

void sigpipe_close()
{
  close(sigpipe[0]);
  sigpipe[0] = -1;
  close(sigpipe[1]);
  sigpipe[1] = -1;
}
