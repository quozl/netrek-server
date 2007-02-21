#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

static void usage(void)
{
  fprintf(stderr, "\
Usage: setgame COMMAND [COMMAND ...]\n\
\n\
COMMAND is one of:\n\
\n\
pause         pause the game\n\
resume        resume the game after a pause\n\
terminate     terminate the game\n\
");
}

/* send message to players */
static void _pmessage(char *str, int recip, int group)
{
  struct message *cur;
  if (++(mctl->mc_current) >= MAXMESSAGE)
    mctl->mc_current = 0;
  cur = &messages[mctl->mc_current];
  cur->m_no = mctl->mc_current;
  cur->m_flags = group;
  cur->m_time = 0;
  cur->m_from = 255;
  cur->m_recpt = recip;
  (void) sprintf(cur->m_data, "%s", str);
  cur->m_flags |= MVALID;
}

/* send message to players */
static void say(const char *fmt, ...)
{
  va_list args;
  char buf[80];

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  _pmessage(buf, 0, MALL);
  va_end(args);
}

int main(int argc, char **argv)
{
    int i, verbose = 0;

    openmem(0);
    if (argc == 1) { usage(); return 1; }

    i = 0;

 state_0:
    if (++i == argc) return 0;

    if (!strcmp(argv[i], "verbose")) {
      verbose++;
      goto state_0;
    }

    if (!strcmp(argv[i], "pause")) {
      status->gameup |= GU_PAUSED;
      if (verbose) say("Game paused");
      goto state_0;
    }

    if (!strcmp(argv[i], "resume")) {
      status->gameup &= ~GU_PAUSED;
      if (verbose) say("Game resumed");
      goto state_0;
    }

    if (!strcmp(argv[i], "terminate")) {
      if (verbose) { say("Game terminated"); sleep(1); }
      status->gameup &= ~GU_GAMEOK;
      goto state_0;
    }

    if (!strcmp(argv[i], "wait-for-terminate")) {
      while (status->gameup & GU_GAMEOK) sleep(1);
      goto state_0;
    }

    if (!strcmp(argv[i], "no-test-mode")) {
      context->frame_test_mode = 0;
      goto state_0;
    }

    if (!strcmp(argv[i], "set-test-counter")) {
      if (++i == argc) return 0;
      context->frame_test_counter = atoi(argv[i]);
      goto state_0;
    }

    if (!strcmp(argv[i], "test-mode")) {
      context->frame_test_mode = 1;
      goto state_0;
    }

    if (!strcmp(argv[i], "wait-for-pause")) {
      while (!(status->gameup & GU_PAUSED)) usleep(20000);
      goto state_0;
    }

    /* expected use,
       [set up static test environment]
       pause test-mode set-test-counter n
       [set up dynamic test environment]
       resume wait-for-pause
       [examine result] */

    if (!strcmp(argv[i], "show-context")) {
      printf("daemon: %d\n", context->daemon);
      printf("frame: %d\n", context->frame);
      printf("frame_tourn_start: %d\n", context->frame_tourn_start);
      printf("frame_tourn_end: %d\n", context->frame_tourn_end);
      printf("frame_test_mode: %d\n", context->frame_test_mode);
      printf("frame_test_counter: %d\n", context->frame_test_counter);
      printf("quorum[2]: %d %d\n", context->quorum[0], context->quorum[1]);
      printf("blog_pickup_game_full: %d\n", context->blog_pickup_game_full);
      printf("blog_pickup_queue_full: %d\n", context->blog_pickup_queue_full);
      goto state_0;
    }

    goto state_0;
}
