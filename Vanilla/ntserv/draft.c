#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include "copyright.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "daemon.h"
#include "draft.h"
#include "util.h"

/* draft */

/* http://en.wikipedia.org/wiki/Draft_%28sports%29 "A sports draft is
the process by which professional sports teams select players not
contracted to any team, often from colleges or amateur ranks." */

/* TODO: initial draft mode declaration by captains, requires INL
   robot voting commands to be added.  */
#define INL_DRAFT_STYLE_BOTTOM_TO_TOP 2

/* Rich Hansen

+--------------------------------------------------+
|                                                  |
|                                                  |
|                   P               P              | 
|                                                  |
|                 P P P           P P P            | 
|                                                  |
|                 P P P           P P P            |
|                                                  |
|                                                  |
|                     C   {}   C                   |
|                                                  |
|                                                  |
|                                                  |
|          O O O O O O O O O O O O O O O O O       |
|                                                  |
|                                                  |
|                                                  |
+--------------------------------------------------+

Key: {} = centre, O = pool players, C = captains, P = picked team

Desired offsets from center:

Player pool -- 20% down
Captains -- 20% right/left
Picked players -- 20%, 30%, 40% up, 25%, 30%, 35% right (or left)

*/

#define INL_DRAFT_STYLE_LEFT_TO_RIGHT 1

/* James Cameron

+--------------------------------------------------+
|                                                  |
|                           P P P P                |
|                         C                        |
|                                                  |
| O O O O O O O O O O     {}                       |
|                                                  |
|                         C                        |
|                           P P P P P              |
|                                                  |
+--------------------------------------------------+

Key: {} = centre, O = pool players, C = captains, P = picked team

*/

#define INL_DRAFT_STYLE_CENTRE_OUTWARDS 0

/* James Cameron

+--------------------------------------------------+
|                 P P P P P P P P P                | -4000
|                                                  |
|                         C S                      | -2000
|                                                  |
|  O O O O O O O O O O O O{}O O O O O O O O O O O  | +0
|                                                  |
|                       S C                        | +2000
|                                                  |
|                 P P P P P P P P P                | +4000
+--------------------------------------------------+

Key:
{} = centre, O = pool players, C = captains, P = picked team, S = selector

*/

/* position of galactic to use for draft */
#define DRAFT_X (GWIDTH/2)
#define DRAFT_Y (GWIDTH/2)

/* width of tactical */
#define DRAFT_W (GWIDTH/5)

/* measure the size of the pool */
static int inl_draft_pool_size()
{
  int h, k;
  struct player *j;

  for (h = 0, j = &players[0], k = 0; h < MAXPLAYER; h++, j++) {
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    if (j->p_inl_draft == INL_DRAFT_MOVING_TO_POOL ||
        j->p_inl_draft == INL_DRAFT_POOLED ||
        j->p_inl_draft == INL_DRAFT_MOVING_TO_PICK) k++;
  }
  return k;
}

/* given a team, find their draft captain */
static struct player *inl_draft_team_to_captain(int p_team)
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    if (!j->p_inl_captain) continue;
    if (j->p_team != p_team) continue;
    if (j->p_inl_draft == INL_DRAFT_CAPTAIN_UP ||
        j->p_inl_draft == INL_DRAFT_CAPTAIN_DOWN) return j;
  }
  return NULL;
}

/* given a pick player, find their draft captain */
static struct player *inl_draft_pick_to_captain(struct player *k)
{
  return inl_draft_team_to_captain(k->p_team);
}

static void inl_draft_place_captain(struct player *j)
{
  int x = DRAFT_X;
  int y = DRAFT_Y;

  switch (inl_draft_style) {

  case INL_DRAFT_STYLE_LEFT_TO_RIGHT:
    {
      int offset = 2000;

      if (j->p_team == FED) { y += offset; }
      if (j->p_team == ROM) { y -= offset; }
      j->p_inl_x = x;
      j->p_inl_y = y;
    }
    break;

  case INL_DRAFT_STYLE_BOTTOM_TO_TOP:
    {
      /* captains 20% right and left */
      int dx = DRAFT_W / 5;

      if (j->p_team == FED) { x += dx; }
      if (j->p_team == ROM) { x -= dx; }
      j->p_inl_x = x;
      j->p_inl_y = y;
    }
    break;

  case INL_DRAFT_STYLE_CENTRE_OUTWARDS:
    {
      int dy = 2000;;

      j->p_inl_x = x;
      if (j->p_team == FED) { j->p_inl_y = y + dy; }
      if (j->p_team == ROM) { j->p_inl_y = y - dy; }
    }
    break;
  }
}

static void inl_draft_place_pool(struct player *j)
{
  int x = DRAFT_X;
  int y = DRAFT_Y;

  switch (inl_draft_style) {
  case INL_DRAFT_STYLE_LEFT_TO_RIGHT:
    {
      int dx = 10000;

      j->p_inl_x = x - dx + j->p_inl_pool_sequence * (dx / 18) + 1000;
      j->p_inl_y = y;
    }
    break;

  case INL_DRAFT_STYLE_BOTTOM_TO_TOP:
    {
      int dy = DRAFT_W / 5;
      int dx = DRAFT_W / 2;

      j->p_inl_x = x - (dx / 2) + j->p_inl_pool_sequence * (dx / 14);
      j->p_inl_y = y + dy;
    }
    break;

  case INL_DRAFT_STYLE_CENTRE_OUTWARDS:
    {
      int n = context->inl_pool_sequence;

      j->p_inl_y = y;
      if (n == 0) {
        j->p_inl_x = x;
      } else {
        int dx = DRAFT_W / 20;

        j->p_inl_x = x - (dx * n / 2) + j->p_inl_pool_sequence * dx;
      }
    }
    break;
  }
}

static void inl_draft_place_pick(struct player *j)
{
  int x = DRAFT_X;
  int y = DRAFT_Y;

  switch (inl_draft_style) {
  case INL_DRAFT_STYLE_LEFT_TO_RIGHT:
    {
      int dx = DRAFT_W / 20;
      int dy = 4000;

      if (j->p_team == FED) { y += dy; }
      if (j->p_team == ROM) { y -= dy; }
      
      j->p_inl_x = x + j->p_inl_pick_sequence * dx;
      j->p_inl_y = y;
    }
    break;

  case INL_DRAFT_STYLE_BOTTOM_TO_TOP:
    {
      int dx = 0;
      int dy = 0;

      if (j->p_team == ROM) {
        dx = (DRAFT_W / 4) + (DRAFT_W / 10 * (j->p_inl_pick_sequence / 3));
        dy = (DRAFT_W / 4) + (DRAFT_W / 10 * (j->p_inl_pick_sequence % 3));
      } else if (j->p_team == FED) {
        dx = (DRAFT_W / 4) - (DRAFT_W / 10 * (j->p_inl_pick_sequence / 3));
        dy = (DRAFT_W / 4) + (DRAFT_W / 10 * (j->p_inl_pick_sequence % 3));
      }
      
      j->p_inl_x = x + dx;
      j->p_inl_y = y - dy;
    }
    break;

  case INL_DRAFT_STYLE_CENTRE_OUTWARDS:
    {
      int n = 0, dy = 4000;

      if (j->p_team == FED) {
        j->p_inl_y = y + dy;
        n = context->inl_home_pick_sequence;
      }
      if (j->p_team == ROM) {
        j->p_inl_y = y - dy;
        n = context->inl_away_pick_sequence;
      }

      if (n == 0) {
        j->p_inl_x = x;
      } else {
        int dx = DRAFT_W / 20;
        j->p_inl_x = x - (dx * n / 2) + j->p_inl_pick_sequence * dx + dx / 2;
      }
    }
    break;
  }
}

static void inl_draft_place_selector(struct player *j)
{
  inl_draft_place_captain(j);
  switch (inl_draft_style) {
  case INL_DRAFT_STYLE_BOTTOM_TO_TOP:
    j->p_inl_y -= 1000;
    break;
  case INL_DRAFT_STYLE_LEFT_TO_RIGHT:
    j->p_inl_x -= 2000;
    break;
  case INL_DRAFT_STYLE_CENTRE_OUTWARDS:
    if (j->p_team == FED) { j->p_inl_x -= 2000; }
    if (j->p_team == ROM) { j->p_inl_x += 2000; }
    break;
  }
}

static void inl_draft_place(struct player *j)
{
  switch (j->p_inl_draft) {
  case INL_DRAFT_OFF            : /* not involved */
    break;
  case INL_DRAFT_CAPTAIN_UP     : /* captain with right to select */
  case INL_DRAFT_CAPTAIN_DOWN   : /* captain without right to select */
    inl_draft_place_captain(j);
    break;
  case INL_DRAFT_MOVING_TO_POOL : /* in transit to pool */
  case INL_DRAFT_POOLED         : /* in pool of players to be chosen */
    inl_draft_place_pool(j);
    break;
  case INL_DRAFT_MOVING_TO_PICK : /* has been chosen, in transit to team */
  case INL_DRAFT_PICKED         : /* has been chosen by a captain */
    inl_draft_place_pick(j);
    break;
  case INL_DRAFT_PICKED_SELECTOR:
    inl_draft_place_selector(j);
    break;
  }
}

static void inl_draft_assign_to_pool(struct player *j)
{
  j->p_inl_draft = INL_DRAFT_MOVING_TO_POOL;
  if (j->p_inl_captain) return; /* captains don't get put in the pool */

  j->p_inl_pool_sequence = context->inl_pool_sequence++;
}

void inl_draft_begin()
{
  int h, i;
  struct player *j;

  context->inl_pool_sequence = 0;
  context->inl_home_pick_sequence = 0;
  context->inl_away_pick_sequence = 0;

  for (h = 0, i = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    inl_draft_assign_to_pool(j);
    inl_draft_place(j);
    /* TODO: set course and speed, with a speed proportional to
    distance to target, rather than step into position */
    j->p_desspeed = 0;
  }
  
  status->gameup |= GU_INL_DRAFT;
  pmessage(0, MALL, "GOD->ALL", "The Captains have agreed to hold a draft.");
}

void inl_draft_end()
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    j->p_inl_draft = INL_DRAFT_OFF;
  }
  status->gameup &= ~(GU_INL_DRAFT);
  /* TODO: send the players back home? or let them fight here? */
  /* TODO: turn off confine during a draft? see if it has an impact */
  pmessage(0, MALL, "GOD->ALL", "The draft has completed!");
}

static void inl_draft_arrival_captain(struct player *k)
{
  int other_team = k->p_team == ROM ? FED : ROM;
  struct player *other_captain = inl_draft_team_to_captain(other_team);

  /* arrival without another captain */
  if (other_captain == NULL) {
    k->p_inl_draft = INL_DRAFT_CAPTAIN_UP;
    return;
  }
  /* arrival with a captain who has the up */
  if (other_captain->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
    k->p_inl_draft = INL_DRAFT_CAPTAIN_DOWN;
    return;
  }
  k->p_inl_draft = INL_DRAFT_CAPTAIN_UP;
  /* therefore captain closest to draft gets the first choice */
  /* TODO: indicate up captain using some graphical element, but do
  not reduce body language opportunities ... e.g. use plasma torps not
  shields */
}

static void inl_draft_arrival_pool(struct player *j)
{
  j->p_inl_draft = INL_DRAFT_POOLED;
}

static void inl_draft_arrival_pick(struct player *j)
{
  j->p_inl_draft = INL_DRAFT_PICKED;
}

/* a ship has arrived at the nominated position */
static void inl_draft_arrival(struct player *j)
{
  switch (j->p_inl_draft) {
  case INL_DRAFT_MOVING_TO_POOL : /* in transit to pool */
    if (j->p_inl_captain) {
      inl_draft_arrival_captain(j);
      return;
    }
    inl_draft_arrival_pool(j);
    break;
  case INL_DRAFT_MOVING_TO_PICK : /* has been chosen, in transit to team */
    inl_draft_arrival_pick(j);
    break;
  case INL_DRAFT_POOLED:
  case INL_DRAFT_PICKED:
    if (j->p_inl_captain) {
      inl_draft_arrival_captain(j);
    }
    break;
  }
}

/* animate alive ships during draft */
void inl_draft_update()
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    int dx, dy;
    if (j->p_status != PALIVE) continue;
    if (j->p_flags & PFROBOT) continue;
    /* newly arriving players are forced into the pool */
    if (j->p_inl_draft == INL_DRAFT_OFF) {
      pmessage(0, MALL, "GOD->ALL", "%s has joined, and is ready to be drafted", j->p_mapchars);
      inl_draft_assign_to_pool(j);
    }
    inl_draft_place(j);
    dx = j->p_x - j->p_inl_x;
    dy = j->p_y - j->p_inl_y;
    if ((abs(dx) + abs(dy)) > 750) {
      p_x_y_go(j, j->p_x - (dx / 4), j->p_y - (dy / 4));
      j->p_dir = ((u_char) nint(atan2(
                                      (double) (j->p_inl_x - j->p_x),
                                      (double) (j->p_y - j->p_inl_y))
                                / 3.14159 * 128.));
      /* TODO: factorise the above formula into util.c */
      /* spin the ship */
      /* has no effect, no idea why, - quozl */
      /* j->p_dir = ((u_char) nint((j->p_dir + 1) % 128)); */
    } else {
      p_x_y_go(j, j->p_inl_x, j->p_inl_y);
      inl_draft_arrival(j);
    }
  }
  if (inl_draft_pool_size() == 0) {
    inl_draft_end();
  }
}

static int inl_draft_next(struct player *k)
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j == k) continue;
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    if (!j->p_inl_captain) continue;
    j->p_inl_draft = INL_DRAFT_CAPTAIN_UP;
    k->p_inl_draft = INL_DRAFT_CAPTAIN_DOWN;
    return 1;
  }
  /* TODO: test that a captain who leaves and returns can allow draft
  to continue, test that a replacement captain can take the role */
  pmessage(0, MALL, "GOD->ALL", "Draft stalled, no captain of other team.");
  return 0;
}

static void inl_draft_pick(struct player *j, struct player *k)
{
  /* TODO: draw a phaser from captain or selector to pick? */
  if (j->p_team != k->p_team) {
    change_team_quietly(j->p_no, k->p_team, j->p_team);
  }

  if (j->p_team == FED) {
    j->p_inl_pick_sequence = context->inl_home_pick_sequence++;
  }
  if (j->p_team == ROM) {
    j->p_inl_pick_sequence = context->inl_away_pick_sequence++;
  }

  j->p_inl_draft = INL_DRAFT_MOVING_TO_PICK;

  /* pmessage(0, MALL, "GOD->ALL", "Draft pick of %s by %s.", j->p_mapchars,
           k->p_mapchars); */

  pmessage(0, MALL, "GOD->ALL", "%s Pick # %d: %s drafts %s.",
           j->p_team == FED ? "HOME" : "AWAY", j->p_inl_pick_sequence,
           k->p_mapchars, j->p_mapchars);
}

void inl_draft_select(int n)
{
  struct player *j, *k = me;      /* j: pick-ee, k: pick-er */
  if ((n < 0) || (n > MAXPLAYER)) return;
  j = &players[n];
  switch (j->p_inl_draft) {
  case INL_DRAFT_OFF            : /* not involved */
    break;
  case INL_DRAFT_CAPTAIN_UP     : /* captain with right to select */
    break;
  case INL_DRAFT_CAPTAIN_DOWN   : /* captain without right to select */
    if (k->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain fingers fellow captain */
      /* meaning: pass */
      if (inl_draft_next(k)) {
        pmessage(0, MALL, "GOD->ALL", "Draft pick declined by %s.",
                 k->p_mapchars);
      }
    }
    break;
  case INL_DRAFT_MOVING_TO_POOL : /* in transit to pool */
  case INL_DRAFT_POOLED         : /* in pool of players to be chosen */
    if (k->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain fingers a pool player */
      /* meaning: pool player is picked, next captain to pick */
      if (!j->p_inl_captain) {
        if (inl_draft_next(k)) {
          inl_draft_pick(j, k);
        }
      }
    } else if (k->p_inl_draft == INL_DRAFT_PICKED_SELECTOR) {
      /* selector fingers a pool player */
      /* meaning: pool player is picked, next captain to pick */
      k = inl_draft_pick_to_captain(me);
      if (k == NULL) {
        pmessage(0, MALL, "GOD->ALL", "Draft error, %s has no captain.",
                 me->p_mapchars);
        return;
      }
      if (k->p_inl_draft != INL_DRAFT_CAPTAIN_UP) {
        pmessage(0, MALL, "GOD->ALL",
                 "Draft error, pick by %s ignored because captain is not up.",
                 me->p_mapchars);
        return;
      }
      if (!j->p_inl_captain) {
        if (inl_draft_next(k)) {
          inl_draft_pick(j, k);
        }
      }
    } else if (k->p_inl_draft == INL_DRAFT_PICKED) {
      /* non-captain pick fingers a pool player */
      /* meaning: team signaling to captain their preference */
      /* TODO: distort pool position according to how many fingerings */
    }
    break;
  case INL_DRAFT_MOVING_TO_PICK : /* has been chosen, in transit to team */
  case INL_DRAFT_PICKED         : /* has been chosen by a captain */
    if (k->p_inl_draft == INL_DRAFT_CAPTAIN_UP ||
        k->p_inl_draft == INL_DRAFT_CAPTAIN_DOWN) {
      /* captain fingers a picked player */
      /* meaning: delegation of pick duty */
      if (j->p_team == k->p_team) {
        j->p_inl_draft = INL_DRAFT_PICKED_SELECTOR;
      }
    }
    break;
  case INL_DRAFT_PICKED_SELECTOR:
    if (k->p_inl_draft == INL_DRAFT_CAPTAIN_UP ||
        k->p_inl_draft == INL_DRAFT_CAPTAIN_DOWN) {
      /* captain fingers a selector */
      /* meaning: cancel delegation of pick duty */
      if (j->p_team == k->p_team) {
        j->p_inl_draft = INL_DRAFT_PICKED;
      }
    }
    break;
  }
}

void inl_draft_reject(int n)
{
  struct player *j;
  if ((n < 0) || (n > MAXPLAYER)) return;
  j = &players[n];
  switch (j->p_inl_draft) {
  case INL_DRAFT_OFF            : /* not involved */
  case INL_DRAFT_MOVING_TO_POOL : /* in transit to pool */
  case INL_DRAFT_POOLED         : /* in pool of players to be chosen */
  case INL_DRAFT_CAPTAIN_UP     : /* captain with right to select */
  case INL_DRAFT_CAPTAIN_DOWN   : /* captain without right to select */
    break;
  case INL_DRAFT_MOVING_TO_PICK : /* has been chosen, in transit to team */
  case INL_DRAFT_PICKED         : /* has been chosen by a captain */
    if (me->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain flicks a picked player */
      /* meaning: undo pick */
      /* TODO: sysdef policy default DRAFT_UNPICK=0 */
      /* TODO: verify pick is on same team as captain */
      /* TODO: throw the pick back to the pool */
    }
    break;
  }
}

char *inl_draft_name(int x)
{
  switch (x) {
  case INL_DRAFT_OFF             : return "_OFF";
  case INL_DRAFT_MOVING_TO_POOL  : return "_MOVING_TO_POOL";
  case INL_DRAFT_CAPTAIN_UP      : return "_CAPTAIN_UP";
  case INL_DRAFT_CAPTAIN_DOWN    : return "_CAPTAIN_DOWN";
  case INL_DRAFT_POOLED          : return "_POOLED";
  case INL_DRAFT_MOVING_TO_PICK  : return "_MOVING_TO_PICK";
  case INL_DRAFT_PICKED          : return "_PICKED";
  case INL_DRAFT_PICKED_SELECTOR : return "_PICKED_SELECTOR";
  }
  return "UNKNOWN";
}

void inl_draft_watch()
{
  int h;
  struct player *j;

  for (;;) {
    usleep(500000);
    fprintf(stderr, "\033[fS=%d        O=%02d H=%02d A=%02d\n",
            inl_draft_style,
            context->inl_pool_sequence,
            context->inl_home_pick_sequence, context->inl_away_pick_sequence);
    for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
      if (j->p_status == PFREE) continue;
      if (j->p_flags & PFROBOT) continue;
      fprintf(stderr, "%s C=%d D=%d O=%02d P=%02d X=%08d Y=%08d %s\n",
              j->p_mapchars, j->p_inl_captain, j->p_inl_draft,
              j->p_inl_pool_sequence, j->p_inl_pick_sequence,
              j->p_inl_x, j->p_inl_y, inl_draft_name(j->p_inl_draft));
    }
  }
}
