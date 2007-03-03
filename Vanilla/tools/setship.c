#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "util.h"

static void usage(void)
{
  fprintf(stderr, "\
Usage: setship SLOT get\n\
       setship SLOT COMMAND [COMMAND ...]\n\
\n\
position x y                move ship to x y coordinate\n\
dir n                       set course to n (0-255)\n\
speed n                     set speed to n (0-maxwarp)\n\
wobble                      set next torp standard wobble\n\
no-wobble                   set next torp without wobble\n\
fire-test-torpedo n         fire torp in direction n\n\
show-test-torpedo-position  show position of first torpedo owned\n\
destroy-test-torpedo        detonate torpedo\n\
sleep n                     sleep for n seconds\n\
");
}

struct torp *t_find(struct player *me, int status)
{
  struct torp *k = NULL;
  for (k = firstTorpOf(me); k <= lastTorpOf(me); k++)
    if (k->t_status == status)
      break;
  return k;
}

int t_attribute = TOWNERSAFE | TDETTEAMSAFE;
int t_torpspeed = -1;

int main(int argc, char **argv)
{
    int i, player, verbose = 0;
    struct player *me;

    if (argc == 1) { usage(); return 1; }
    openmem(0);

    i = 1;

 state_0:
    /* check for ship identifier */

  player=atoi(argv[1]);
  if ((player == 0) && (*argv[1] != '0')) {
    char c = *argv[1];
    if (c >= 'a' && c <= 'z')
      player = c - 'a' + 10;
    else {
      fprintf(stderr, "unknown slot\n");
    exit(1);
    }
  }
  if (player >= MAXPLAYER) {
    printf("MAXPLAYER is set to %d, and you want %d?\n", 
	   MAXPLAYER, player);
    exit(1);
  }
  me = &players[player];

 state_1:
    if (++i == argc) return 0;
    
    if (!strcmp(argv[i], "show-position")) {
      printf("frame %d", context->frame);
      printf(" speed %d", me->p_speed);
      printf(" dir %d", me->p_dir);
      printf(" position %d %d", me->p_x, me->p_y);
      printf("\n");
      goto state_1;
    }

    if (!strcmp(argv[i], "verbose")) {
      verbose++;
      goto state_1;
    }

    if (!strcmp(argv[i], "position")) {
      int p_x, p_y;
      if (++i == argc) return 0;
      p_x = atoi(argv[i]);
      if (++i == argc) return 0;
      p_y = atoi(argv[i]);
      p_x_y_set(&players[player], p_x, p_y);
      goto state_1;
    }

    if (!strcmp(argv[i], "dir")) {
      if (++i == argc) return 0;
      players[player].p_dir = atoi(argv[i]);
      me->p_desdir = atoi(argv[i]);
      bay_release(me);
      me->p_flags &= ~(PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
      goto state_1;
    }

    if (!strcmp(argv[i], "speed")) {
      if (++i == argc) return 0;
      me->p_desspeed = atoi(argv[i]);
      me->p_flags &= ~(PFREPAIR | PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
      me->p_flags &= ~(PFPLOCK | PFPLLOCK);
      goto state_1;
    }

    if (!strcmp(argv[i], "wait-for-stop")) {
      while (me->p_speed) usleep(20000);
      goto state_1;
    }

    if (!strcmp(argv[i], "lock-planet")) {
      if (++i == argc) return 0;
      struct planet *pl = planet_find(argv[i]);
      /* lock on, from lock_planet() in interface.c */
      me->p_flags |= PFPLLOCK;
      me->p_flags &= ~(PFPLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
      me->p_planet = pl->pl_no;
      goto state_1;
    }

    if (!strcmp(argv[i], "wait-for-orbit")) {
      while (!(me->p_flags & PFORBIT)) usleep(20000);
      goto state_1;
    }

    if (!strcmp(argv[i], "wobble")) {
      t_attribute |= TWOBBLE;
      goto state_1;
    }

    if (!strcmp(argv[i], "no-wobble")) {
      t_attribute &= ~TWOBBLE;
      goto state_1;
    }

    if (!strcmp(argv[i], "torp-speed")) {
      if (++i == argc) return 0;
      t_torpspeed = atoi(argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "fire-test-torpedo")) {
      if (++i == argc) return 0;
      struct ship *myship = &me->p_ship;
      struct torp *k = t_find(me, TFREE);
      me->p_ntorp++;
      k->t_status = TMOVE;
      k->t_type = TPLASMA;
      k->t_attribute = t_attribute;
      k->t_owner = me->p_no;
      t_x_y_set(k, me->p_x, me->p_y);
      k->t_turns  = myship->s_torpturns;
      k->t_damage = 0;
      k->t_gspeed = (t_torpspeed == -1 ? myship->s_torpspeed : t_torpspeed)
                    * WARP1;
      k->t_fuse   = 500;
      k->t_dir    = atoi(argv[i]);
      k->t_war    = me->p_war;
      k->t_team   = me->p_team;
      k->t_whodet = NODET;
      goto state_1;
    }

    if (!strcmp(argv[i], "show-test-torpedo-position")) {
      struct torp *k = t_find(me, TMOVE);
      if (k != NULL) {
        printf("torp %d x %d y %d\n", k->t_dir, k->t_x, k->t_y);
      }
      goto state_1;
    }

    if (!strcmp(argv[i], "destroy-test-torpedo")) {
      struct torp *k = t_find(me, TMOVE);
      if (k != NULL) {
        k->t_status = TOFF;
      }
      goto state_1;
    }

    if (!strcmp(argv[i], "monitor-coordinates")) {
      for (;;) {
        printf("p_x %X p_y %X p_x_internal %X p_y_internal %X\n", me->p_x, me->p_y, me->p_x_internal, me->p_y_internal);
        usleep(20000);
      }
      goto state_1;
    }

    if (!strcmp(argv[i], "monitor-docking")) {
      for (;;) {
        printf("p_flags & PFDOCK %X p_dock_with %X p_dock_bay %X\n", me->p_flags & PFDOCK, me->p_dock_with, me->p_dock_bay);
        usleep(20000);
      }
      goto state_1;
    }

    if (!strcmp(argv[i], "sleep")) {
      if (++i == argc) return 0;
      sleep(atoi(argv[i]));
      goto state_1;
    }

    goto state_0;
}
