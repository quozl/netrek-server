#include "copyright.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

void blog_file(char *class, char *file)
{
  char blog[256];

  if (!blogging) return;
  snprintf(blog, 256-1, "%s/blog", LIBDIR);

  if (fork() == 0) {
    execl(blog, blog, class, file, NULL);
    perror(blog);
    _exit(1);
  }
}

void blog_printf(char *class, const char *fmt, ...)
{
  va_list args;
  char name[MSG_LEN];
  struct timeval tv;
  FILE *file;

  if (!blogging) return;
  gettimeofday(&tv, (struct timezone *) 0);
  sprintf(name, "%s.%d.txt", class, (int) tv.tv_sec);
  file = fopen(name, "w");
  va_start(args, fmt);
  vfprintf(file, fmt, args);
  va_end(args);
  fclose(file);
  blog_file(class, name);
}

void blog_pickup_game_full()
{
  if (context->blog_pickup_game_full) return;
  blog_printf("queue", "Pickup game full.\n");
  context->blog_pickup_game_full = 1;
}

void blog_pickup_game_not_full()
{
  context->blog_pickup_game_full = 0;
}

void blog_pickup_queue_full()
{
  if (context->blog_pickup_queue_full) return;
  blog_printf("queue", "Pickup queue full.\n");
  context->blog_pickup_queue_full = 1;
}

void blog_pickup_queue_not_full()
{
  context->blog_pickup_queue_full = 0;
}

void blog_game_over(struct status *was, struct status *is)
{
  int np;

  if (!blogging) return;
  np = (is->planets - was->planets);
  if (np < 1) return;

  blog_printf("daemon", "Game over\n"
              "Players have left, %d planets taken, %d armies bombed, "
              "%d deaths, %d kills, over %.1f hours of t-mode play.\n",
              np,
              (int) (is->armsbomb - was->armsbomb),
              (int) (is->kills - was->kills),
              (int) (is->losses - was->losses),
              (float) (is->time - was->time) / reality / 60.0
              );
}

void blog_base_loss(struct player *j)
{
  blog_printf("racial", 
              "%s lost their starbase, with %d armies\n",
              team_name(j->p_team), j->p_armies);
}
