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
#include "alarm.h"
#include "util.h"

void blog_file(char *class, char *file)
{
  char blog[256];

  if (!blogging) return;
  snprintf(blog, 256-1, "%s/blog-file", LIBDIR);

  if (fork() == 0) {
    alarm_prevent_inheritance();
    nice(1);
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
  /* TODO: watch out for choosing the same file name */
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
  blog_printf("queue", "Pickup game full\n\n");
  context->blog_pickup_game_full = 1;
}

void blog_pickup_game_not_full()
{
  context->blog_pickup_game_full = 0;
}

void blog_pickup_queue_full()
{
  if (context->blog_pickup_queue_full) return;
  blog_printf("queue", "Pickup queue full\n\n");
  context->blog_pickup_queue_full = 1;
}

void blog_pickup_queue_not_full()
{
  context->blog_pickup_queue_full = 0;
}

void blog_game_over(struct status *was, struct status *is)
{
  int np;
  float hours;

  if (!blogging) return;
  np = (is->planets - was->planets);
  if (np < 1) return;

  /* BUG: 50 fps change impacts */
  hours = (is->time - was->time) / (1000000 / distortion) / 60.0 / 60.0;
  blog_printf("daemon", "Game over\n\n"
              "Players have left, %d planets taken, %d armies bombed, "
              "%d deaths, %d kills, over %.1f hours of t-mode play.\n",
              np,
              (int) (is->armsbomb - was->armsbomb),
              (int) (is->kills - was->kills),
              (int) (is->losses - was->losses),
              hours
              );
}

void blog_base_loss(struct player *j)
{
  blog_printf("racial", 
              "%s lost their starbase\n\n"
              "Starbase with %d armies, piloted by %s, "
              "reported destroyed in valiant battle with enemy forces.  "
              "Reconstruction is underway, due in %d minutes.  "
              "Long live the %s.\n",
              team_name(j->p_team), j->p_armies, j->p_name, 
              starbase_rebuild_time, team_name(j->p_team));
}
