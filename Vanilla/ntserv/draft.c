#include <stdlib.h>
#include <math.h>
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

/* positioning on tactical

+--------------------------------------------------+
|                                                  |
|                           P P P P                |
|                         C                        |
|                                                  |
| O O O O O O O O O O                              |
|                                                  |
|                         C                        |
|                           P P P P P              |
|                                                  |
+--------------------------------------------------+

Key: O = pool players, C = captains, P = picked team

*/

/* Modified tactical positioning-- will tractors reach?:
   (yes, tractor range is not checked on the code path used, it is up
   to the client whether it sends the packet, and both player lock and
   tractor trigger inl_draft_select ... probably worth emphasising
   player lock instead -- quozl)

+--------------------------------------------------+
|                                                  |
/                                                  /
/                   P               P              / 
/                                                  /
/                 P P P           P P P            / 
/                                                  /
/                 P P P           P P P            /
/                                                  /
/                                                  |
|                      C        C                  |
|                                                  |
|                                                  |
|                                                  |
|          O O O O O O O O O O O O O O O O O       |
|                                                  |
|                                                  |
/                                                  /
/                                                  /
/                                                  /
+--------------------------------------------------+

Key: O = pool players, C = captains, P = picked team

Desired offsets from center:

Player pool -- 20% down
Captains -- 20% right/left
Picked players-- 20%, 30%, 40% up, 25%, 30%, 35% right (or left)

*/

#define INL_DRAFT_STYLE_BOTTOM_TO_TOP 1
#define INL_DRAFT_STYLE_LEFT_TO_RIGHT 2

static int inl_draft_style = INL_DRAFT_STYLE_BOTTOM_TO_TOP;

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
  int x = GWIDTH / 2;
  int y = GWIDTH / 2;

  if (inl_draft_style == INL_DRAFT_STYLE_LEFT_TO_RIGHT) {
    int offset = ( GWIDTH / 5 ) / 4; 
    if (j->p_team == FED) { y += offset; }
    if (j->p_team == ROM) { y -= offset; }
    j->p_inl_x = x;
    j->p_inl_y = y;
  }

  if (inl_draft_style == INL_DRAFT_STYLE_BOTTOM_TO_TOP) {
    /* Going to put the captains 20% right and left */
    int xoffset = (GWIDTH / 5) / 5; /* one-fifth of tactical */
    if (j->p_team == FED) { x += xoffset; }
    if (j->p_team == ROM) { x -= xoffset; }
    j->p_inl_x = x;
    j->p_inl_y = y;
  }
}

static void inl_draft_place_pool(struct player *j)
{
  int x = GWIDTH / 2;
  int y = GWIDTH / 2;

  if (inl_draft_style == INL_DRAFT_STYLE_LEFT_TO_RIGHT) {
    int offset_x = ( GWIDTH / 5 ) / 2;
    /* TODO: position independently of player number */
    j->p_inl_x = x - offset_x + j->p_no * (offset_x / 18) + 1000;
    j->p_inl_y = y;
  }

  if (inl_draft_style == INL_DRAFT_STYLE_BOTTOM_TO_TOP) {
    int yoffset = (GWIDTH / 5) / 5; /* one-fifth of tactical */
    int xoffset = (GWIDTH / 5) / 2;
    j->p_inl_x = x - xoffset + j->p_inl_pool_sequence * (xoffset / 14);
    j->p_inl_y = y - yoffset;
  }
}

static void inl_draft_place_pick(struct player *j)
{
  int x = GWIDTH / 2;
  int y = GWIDTH / 2;

  if (inl_draft_style == INL_DRAFT_STYLE_LEFT_TO_RIGHT) {
    int offset_x = ( GWIDTH / 5 ) / 2; /* half of tactical */
    int offset_y = ( GWIDTH / 5 ) / 6;
    if (j->p_team == FED) { y += offset_y; }
    if (j->p_team == ROM) { y -= offset_y; }

    /* TODO: position independently of player number */
    j->p_inl_x = x + offset_x - j->p_no * (offset_x / 18) ;
    j->p_inl_y = y;
  }

  if (inl_draft_style == INL_DRAFT_STYLE_BOTTOM_TO_TOP) {
    /* Magic numbers set in inl_draft_pick()
		   0-99 		are pool places
			 100-199	are home picks
			 200-299	are away picks
    */
    if (j->p_inl_pick_sequence > 200) { /* Away pick */
      int xoffset = (( GWIDTH / 5 ) / 4) + ((( GWIDTH / 5) / 10) * ((j->p_inl_pick_sequence - 200) / 3));
      int yoffset = (( GWIDTH / 5 ) / 4) + ((( GWIDTH / 5) / 10) * ((j->p_inl_pick_sequence - 200) % 3));
    } else if (j->p_inl_pick_sequence > 100) { /* Home pick */
      int xoffset = (( GWIDTH / 5 ) / 4) - ((( GWIDTH / 5) / 10) * ((j->p_inl_pick_sequence - 100) / 3));
      int yoffset = (( GWIDTH / 5 ) / 4) + ((( GWIDTH / 5) / 10) * ((j->p_inl_pick_sequence - 100) % 3));
    } else { /* ERROR */
      int xoffset = 0;
      int yoffset = 0;
    }

    j->p_inl_x = x + xoffset;
    j->p_inl_y = y + yoffset;
  }
}

static void inl_draft_place_selector(struct player *j)
{
  inl_draft_place_captain(j);
  j->p_inl_x -= 2000;
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

void inl_draft_begin()
{
  int h, i;
  int pool_position_count = 0;
  struct player *j;

  for (h = 0, i = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    j->p_inl_draft = INL_DRAFT_MOVING_TO_POOL;
    if (!j->p_inl_captain) {
      j->p_inl_pool_sequence = i++;
      /* TODO: Handle ships who join mid-draft-- give a poolPosition somehow */
    }
    inl_draft_place(j);
    /* TODO: set course and speed, with a speed proportional to
    distance to target, rather than step into position */
    j->p_desspeed = 0;
  }
  
  status->gameup |= GU_INL_DRAFT;
  pmessage(0, MALL, "GOD->ALL", "Draft begins.");
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
  pmessage(0, MALL, "GOD->ALL", "Draft ends.");
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
      j->p_inl_draft = INL_DRAFT_MOVING_TO_POOL;
      pmessage(0, MALL, "GOD->ALL", "Draft pool addition, new ship joined.");
      /* TODO: Assign newcomer a pool position */
    }
    inl_draft_place(j);
    dx = j->p_x - j->p_inl_x;
    dy = j->p_y - j->p_inl_y;
    if ((abs(dx) + abs(dy)) > 500) {
      p_x_y_go(j, j->p_x - (dx / 10), j->p_y - (dy / 10));
      /* Face the way you move
      j->p_dir = ((u_char) nint(atan2((double) (j->p_inl_x -
            j->p_x), (double) (j->p_y - j->p_inl_y)) / 3.14159 *
            128.));
      */
      /* TODO: factorise the above formula into util.c */
      /* spin the ship */
      j->p_dir = ((u_char) nint((j->p_dir + 1) % 128));
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
  static int home_pick_sequence = 0;
  static int away_pick_sequence = 0;

  if (j->p_team != k->p_team) {
    change_team_quietly(j->p_no, k->p_team, j->p_team);
  }

  if (j->p_team == FED) {
    j->p_inl_pick_sequence = 100 + home_pick_sequence++;
  } else if (j->p_team == ROM) {
    j->p_inl_pick_sequence = 200 + away_pick_sequence++;
  }

  j->p_inl_draft = INL_DRAFT_MOVING_TO_PICK;

  pmessage(0, MALL, "GOD->ALL", "Draft pick of %s by %s.", j->p_mapchars,
           k->p_mapchars);
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
