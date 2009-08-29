/*! @file setgame.c
    Affect game state from command line, pause, resume and terminate,
    plus test functions.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "util.h"
#include "draft.h"

static void usage(void)
{
  fprintf(stderr, "\
Usage: setgame COMMAND [COMMAND ...]\n\
\n\
COMMAND is one of:\n\
\n\
pause               pause the game\n\
resume              resume the game after a pause\n\
terminate           terminate the game\n\
wait-for-terminate  wait for the game to terminate\n\
set-test-counter n  set the number of frames to step in test mode\n\
test-mode           turn on test mode\n\
wait-for-pause      wait until a pause occurs\n\
single-step         do one simulation step during a pause\n\
no-test-mode        turn off test mode\n\
show-context        show the context structure\n\
inl-draft-on        activate INL draft mode\n\
inl-draft-off       deactivate INL draft mode\n\
\n\
In test mode the simulation engine will decrement the frame counter\n\
defined by set-test-counter and pause the game when the counter\n\
reaches zero.  To use it, set up what you need, then pause, test-mode,\n\
set-test-counter, set up what you want to measure, resume,\n\
wait-for-pause, then measure what you want to.\n\
");
}

/* send message to players */
static void say(const char *fmt, ...)
{
  va_list args;
  char buf[80];

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  amessage(buf, 0, MALL);
  va_end(args);
}

int main(int argc, char **argv)
{
    int i, verbose = 0;

    if (argc == 1) { usage(); return 1; }
    openmem(0);

    i = 0;

 state_0:
    if (++i == argc) return 0;

    if (!strcmp(argv[i], "verbose")) {
      verbose++;
      goto state_0;
    }

    if (!strcmp(argv[i], "restart")) {
      int pid = context->daemon;
      if (pid < 2) {
        fprintf (stderr, "setgame: context->daemon invalid\n");
        exit(1);
      }
      if (kill (pid, SIGHUP) != 0) {
        perror("setgame: kill");
        exit (1);
      }
      if (verbose) say("Game restarted");
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

    if (!strcmp(argv[i], "wait-for-inl-start")) {
      while (status->gameup & (GU_GAMEOK|GU_INROBOT)) {
        sleep(1);
        if (!(status->gameup & (GU_CHAOS|GU_PRACTICE))) break;
      }
      goto state_0;
    }

    if (!strcmp(argv[i], "wait-for-inl-end")) {
      while (status->gameup & (GU_GAMEOK|GU_INROBOT)) {
        sleep(1);
        if (status->gameup & (GU_CHAOS|GU_PRACTICE)) break;
      }
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

    if (!strcmp(argv[i], "single-step")) {
      if (!(status->gameup & GU_PAUSED)) {
        fprintf(stderr, "setgame: single-step: only valid during GU_PAUSED\n");
        exit(1);
      }
      if (context->frame_test_mode) {
        fprintf(stderr, "setgame: single-step: only valid "
                "if not already in test mode\n");
        exit(1);
      }
      context->frame_test_counter = 1;
      context->frame_test_mode = 1;
      status->gameup &= ~GU_PAUSED;
      while (!(status->gameup & GU_PAUSED)) usleep(20000);
      goto state_0;
    }

    if (!strcmp(argv[i], "show-context")) {
      printf("daemon: %d\n", context->daemon);
      printf("frame: %d\n", context->frame);
      printf("frame_tourn_start: %d\n", context->frame_tourn_start);
      printf("frame_tourn_end: %d\n", context->frame_tourn_end);
      printf("frame_test_mode: %d\n", context->frame_test_mode);
      printf("frame_test_counter: %d\n", context->frame_test_counter);
      printf("quorum[2]: %d %d (%s vs %s)\n",
             context->quorum[0], context->quorum[1],
             team_code(context->quorum[0]), team_code(context->quorum[1]));
      printf("blog_pickup_game_full: %d\n", context->blog_pickup_game_full);
      printf("blog_pickup_queue_full: %d\n", context->blog_pickup_queue_full);
      goto state_0;
    }

    if (!strcmp(argv[i], "watch-gameup")) {
      int old, new;
      old = -1;
      for(;;) {
        new = status->gameup;
        if (new != old) {
          fprintf(stderr, "%s%s%s%s%s%s%s%s%s%s%s%s%s$\n",
                  new & GU_GAMEOK       ? "_GAMEOK      ":"             ",
                  new & GU_PRACTICE     ? "_PRACTICE    ":"             ",
                  new & GU_CHAOS        ? "_CHAOS       ":"             ",
                  new & GU_PAUSED       ? "_PAUSED      ":"             ",
                  new & GU_INROBOT      ? "_INROBOT     ":"             ",
                  new & GU_NEWBIE       ? "_NEWBIE      ":"             ",
                  new & GU_PRET         ? "_PRET        ":"             ",
                  new & GU_BOT_IN_GAME  ? "_BOT_IN_GAME ":"             ",
                  new & GU_CONQUER      ? "_CONQUER     ":"             ",
                  new & GU_PUCK         ? "_PUCK        ":"             ",
                  new & GU_DOG          ? "_DOG         ":"             ",
                  new & GU_INL_DRAFTING ? "_INL_DRAFTING":"             ",
                  new & GU_INL_DRAFTED  ? "_INL_DRAFTED ":"             ");
          old = new;
        }
        usleep(100000);
      }
    }

    if (!strcmp(argv[i], "lock-on")) {
      lock_on(LOCK_SETGAME);
      goto state_0;
    }

    if (!strcmp(argv[i], "lock-off")) {
      lock_off(LOCK_SETGAME);
      goto state_0;
    }

    if (!strcmp(argv[i], "lock-show")) {
      lock_show(LOCK_SETGAME);
      goto state_0;
    }

    if (!strcmp(argv[i], "lock-dump")) {
      int i;
      for (i=0; i<NLOCKS; i++) {
        lock_show(i);
      }
      goto state_0;
    }

    if (!strcmp(argv[i], "sleep")) {
      if (++i == argc) return 0;
      sleep(atoi(argv[i]));
      goto state_0;
    }

    if (!strcmp(argv[i], "inl-draft-on")) {
      inl_draft_begin();
      goto state_0;
    }

    if (!strcmp(argv[i], "inl-draft-off")) {
      inl_draft_end();
      goto state_0;
    }

    if (!strcmp(argv[i], "watch-draft")) {
      inl_draft_watch();
      goto state_0;
    }

    if (!strcmp(argv[i], "inl-drafted-off")) {
      status->gameup &= ~GU_INL_DRAFTED;
      goto state_0;
    }

    if (!strcmp(argv[i], "conquer-trigger")) {
      context->conquer_trigger++;
      status->gameup |= GU_PAUSED;
      goto state_0;
    }

    goto state_0;
}
