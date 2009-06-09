#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "planet.h"
#include "util.h"
#include "draft.h"

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
set-inl-draft n             set INL draft mode state n (0=off)\n\
player                      change an observer to a player\n\
observer                    change a player to an observer\n\
captain                     mark a player as a draft captain\n\
damage n                    set damage to n\n\
shields n                   set shields remaining to n\n\
fuel n                      set fuel remaining to n\n\
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

int setship(char *cmds)
{
  const char delimiters[] = " ";
  char *token;
  struct player *me;

  token = strtok(cmds, delimiters);
  if (!token) { usage(); return 1; }
  openmem(0);

 state_0:
  me = player_by_number(token);
  if (me == NULL) {
    fprintf(stderr, "unknown slot\n");
    exit(1);
  }

 state_1:
  if (!(token = strtok (NULL, delimiters))) return 0;

  if (!strcmp(token, "show-position")) {
    printf("frame %d", context->frame);
    printf(" speed %d", me->p_speed);
    printf(" dir %d", me->p_dir);
    printf(" position %d %d", me->p_x, me->p_y);
    printf("\n");
    goto state_1;
  }

  if (!strcmp(token, "position")) {
    int p_x, p_y;
    if (!(token = strtok (NULL, delimiters))) return 0;
    p_x = atoi(token);
    if (!(token = strtok (NULL, delimiters))) return 0;
    p_y = atoi(token);
    p_x_y_set(me, p_x, p_y);
    goto state_1;
  }

  if (!strcmp(token, "dir")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
    me->p_dir = atoi(token);
    me->p_desdir = atoi(token);
    bay_release(me);
    me->p_flags &= ~(PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
    goto state_1;
  }

  if (!strcmp(token, "speed")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
    me->p_desspeed = atoi(token);
    me->p_flags &= ~(PFREPAIR | PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
    me->p_flags &= ~(PFPLOCK | PFPLLOCK);
    goto state_1;
  }

  if (!strcmp(token, "wait-for-stop")) {
    while (me->p_speed) usleep(20000);
    goto state_1;
  }

  if (!strcmp(token, "lock-planet")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
    struct planet *pl = planet_find(token);
    //* lock on, from lock_planet() in interface.c 
    me->p_flags |= PFPLLOCK;
    me->p_flags &= ~(PFPLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
    me->p_planet = pl->pl_no;
    goto state_1;
  }

  if (!strcmp(token, "wait-for-orbit")) {
    while (!(me->p_flags & PFORBIT)) usleep(20000);
    goto state_1;
  }

  if (!strcmp(token, "wobble")) {
    t_attribute |= TWOBBLE;
    goto state_1;
  }

  if (!strcmp(token, "no-wobble")) {
    t_attribute &= ~TWOBBLE;
    goto state_1;
  }

  if (!strcmp(token, "torp-speed")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
    t_torpspeed = atoi(token);
    goto state_1;
  }

  if (!strcmp(token, "fire-test-torpedo")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
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
    k->t_dir    = atoi(token);
    k->t_war    = me->p_war;
    k->t_team   = me->p_team;
    k->t_whodet = NODET;
    goto state_1;
  }

  if (!strcmp(token, "show-test-torpedo-position")) {
    struct torp *k = t_find(me, TMOVE);
    if (k != NULL) {
      printf("torp %d x %d y %d\n", k->t_dir, k->t_x, k->t_y);
    }
    goto state_1;
  }

  if (!strcmp(token, "destroy-test-torpedo")) {
    struct torp *k = t_find(me, TMOVE);
    if (k != NULL) {
      k->t_status = TOFF;
    }
    goto state_1;
  }

  if (!strcmp(token, "monitor-coordinates")) {
    for (;;) {
      printf("p_x %X p_y %X p_x_internal %X p_y_internal %X\n", me->p_x, me->p_y, me->p_x_internal, me->p_y_internal);
      usleep(20000);
    }
    goto state_1;
  }

  if (!strcmp(token, "monitor-docking")) {
    for (;;) {
      printf("p_flags & PFDOCK %X p_dock_with %X p_dock_bay %X\n", me->p_flags & PFDOCK, me->p_dock_with, me->p_dock_bay);
      usleep(20000);
    }
    goto state_1;
  }

  if (!strcmp(token, "sleep")) {
    if (!(token = strtok(NULL, delimiters))) return 0;
    sleep(atoi(token));
    goto state_1;
  }

  if (!strcmp(token, "set-inl-draft")) {
    if (!(token = strtok(NULL, delimiters))) return 0;
    me->p_inl_draft = atoi(token);
    goto state_1;
  }

  if (!strcmp(token, "monitor-inl-draft")) {
    for (;;) {
      printf("p_inl_captain %d p_inl_draft %d (%s) p_inl_x %d p_inl_y %d p_inl_pick %d\n", me->p_inl_captain, me->p_inl_draft, inl_draft_name(me->p_inl_draft), me->p_inl_x, me->p_inl_y, me->p_inl_pick);
      usleep(20000);
    }
    goto state_1;
  }

  if (!strcmp(token, "show-inl-draft")) {
    printf("p_inl_captain %d p_inl_draft %d (%s) p_inl_x %d p_inl_y %d p_inl_pick %d\n", me->p_inl_captain, me->p_inl_draft, inl_draft_name(me->p_inl_draft), me->p_inl_x, me->p_inl_y, me->p_inl_pick);
    goto state_1;
  }

  if (!strcmp(token, "wait-for-inl-draft-to-end")) {
    for (;;) {
      if (me->p_inl_draft == INL_DRAFT_OFF) exit(0);
      usleep(20000);
    }
    goto state_1;
  }

  if (!strcmp(token, "player")) {
    me->p_flags &= ~PFOBSERV;
    me->p_status = PALIVE;
    goto state_1;
  }
  if (!strcmp(token, "observer")) {
    me->p_flags |= PFOBSERV;
    me->p_status = POBSERV;
    goto state_1;
  }

  if (!strcmp(token, "captain")) {
    me->p_inl_captain = 1;
    goto state_1;
  }

  if (!strcmp(token, "no-captain")) {
    me->p_inl_captain = 0;
    goto state_1;
  }

  if (!strcmp(token, "damage")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
    me->p_damage = me->p_ship.s_maxdamage - atoi(token);
    goto state_1;
  }

  if (!strcmp(token, "shields")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
    me->p_shield = atoi(token);
    goto state_1;
  }

  if (!strcmp(token, "fuel")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
    me->p_fuel = atoi(token);
    goto state_1;
  }

  if (!strcmp(token, "disconnect")) {
    if (!(token = strtok (NULL, delimiters))) return 0;
    me->p_disconnect = atoi(token);
    goto state_1;
  }

  goto state_0;
}

int main(int argc, char **argv)
{
  int i, j, k;
  int length = 0;
  char *command;

  for (j=1; j < argc; j++){
    for (k=0; k < strlen(argv[j]); k++) { length++; }
    if (j != (argc-1)) { length++; }
  }

  command = malloc(length);

  i=0;
  for (j=1; j < argc; j++){
    for (k=0; k < strlen(argv[j]); k++)
      command[i++] = argv[j][k];
    if (j != (argc-1)) { command[i++] = ' '; }
  }

  setship(command);
  exit(0);
}
