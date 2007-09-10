#include <stdlib.h>
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
    /* Not sure how we can position independly of player number
       There's going to be two ugly holes where the captains would be
       May need to assign a new variable
    */
    j->p_inl_x = x - xoffset + j->p_no * (xoffset / 14);
    j->p_inl_y = y - yoffset;
  }
}

static void inl_draft_place_pick(struct player *j)
{
  int x = GWIDTH / 2;
  int y = GWIDTH / 2;
  int offset_x = ( GWIDTH / 5 ) / 2; /* half of tactical */
  int offset_y = ( GWIDTH / 5 ) / 6;
  if (j->p_team == FED) { y += offset_y; }
  if (j->p_team == ROM) { y -= offset_y; }
  /* TODO: position independently of player number */
  j->p_inl_x = x + offset_x - j->p_no * (offset_x / 18) ;
  j->p_inl_y = y;
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
  }
}

void inl_draft_begin()
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    j->p_inl_draft = INL_DRAFT_MOVING_TO_POOL;
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
  pmessage(0, MALL, "GOD->ALL", "Draft ends.");
}

/* for _MOVING_TO_POOL animate position, and on arrival choose whether
they are _POOLED or _CAPTAIN_UP or _CAPTAIN_DOWN */
static void inl_draft_arrival_pool(struct player *j)
{
  if (!j->p_inl_captain) {
    j->p_inl_draft = INL_DRAFT_POOLED;
    return;
  }
  /* TODO: determine which captain should get first pick, default FED */
  if (j->p_team == FED) j->p_inl_draft = INL_DRAFT_CAPTAIN_UP;
  if (j->p_team == ROM) j->p_inl_draft = INL_DRAFT_CAPTAIN_DOWN;
}

static void inl_draft_arrival_pick(struct player *j)
{
  j->p_inl_draft = INL_DRAFT_PICKED;
  /* TODO: count slots in INL_DRAFT_MOVING_TO_POOL or INL_DRAFT_POOLED */
  /* TODO: terminate draft if no slots remain in pool */
}

/* a ship has arrived at the nominated position */
static void inl_draft_arrival(struct player *j)
{
  switch (j->p_inl_draft) {
  case INL_DRAFT_MOVING_TO_POOL : /* in transit to pool */
    inl_draft_arrival_pool(j);
    break;
  case INL_DRAFT_MOVING_TO_PICK : /* has been chosen, in transit to team */
    inl_draft_arrival_pick(j);
    break;
  }
}

/* animate alive ships during draft */
void inl_draft_update()
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    int dx, dy, nx, ny;
    if (j->p_status != PALIVE) continue;
    if (j->p_flags & PFROBOT) continue;
    /* newly arriving players are forced into the pool */
    if (j->p_inl_draft == INL_DRAFT_OFF) {
      j->p_inl_draft = INL_DRAFT_MOVING_TO_POOL;
      pmessage(0, MALL, "GOD->ALL", "Draft pool addition, new ship joined.");
    }
    /* TODO: captain could quit and rejoin */
    inl_draft_place(j);
    dx = j->p_x - j->p_inl_x;
    dy = j->p_y - j->p_inl_y;
    if ((abs(dx) + abs(dy)) > 200) {
      nx = j->p_x - (dx / 10);
      ny = j->p_y - (dy / 10);
      p_x_y_go(j, nx, ny);
    } else {
      p_x_y_go(j, j->p_inl_x, j->p_inl_y);
      inl_draft_arrival(j);
    }
  }
}

static int inl_draft_next()
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j == me) continue;
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    if (!j->p_inl_captain) continue;
    j->p_inl_draft = INL_DRAFT_CAPTAIN_UP;
    me->p_inl_draft = INL_DRAFT_CAPTAIN_DOWN;
    return 1;
  }
  /* TODO: cannot proceed with next pick, no captain of other team */
  pmessage(0, MALL, "GOD->ALL", "Draft stalled, no captain of other team.");
  return 0;
}

static void inl_draft_pick(struct player *j)
{
  /* TODO: draw a phaser from captain to pick? */

  if (j->p_team != me->p_team) {
    change_team_quietly(j->p_no, me->p_team, j->p_team);
  }

  j->p_inl_draft = INL_DRAFT_MOVING_TO_PICK;

  pmessage(0, MALL, "GOD->ALL", "Draft pick of %s by %s.", j->p_mapchars,
           me->p_mapchars);
}

void inl_draft_select(int n)
{
  struct player *j;
  if ((n < 0) || (n > MAXPLAYER)) return;
  j = &players[n];
  switch (j->p_inl_draft) {
  case INL_DRAFT_OFF            : /* not involved */
    break;
  case INL_DRAFT_CAPTAIN_UP     : /* captain with right to select */
    break;
  case INL_DRAFT_CAPTAIN_DOWN   : /* captain without right to select */
    if (me->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain fingers fellow captain */
      /* meaning: pass */
      if (inl_draft_next()) {
        pmessage(0, MALL, "GOD->ALL", "Draft pick declined by %s.",
                 me->p_mapchars);
      }
    }
    break;
  case INL_DRAFT_MOVING_TO_POOL : /* in transit to pool */
  case INL_DRAFT_POOLED         : /* in pool of players to be chosen */
    if (me->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain fingers a pool player */
      /* meaning: pool player is picked, next captain to pick */
      if (inl_draft_next()) {
        inl_draft_pick(j);
      }
    } else if (me->p_inl_draft == INL_DRAFT_PICKED) {
      /* non-captain pick fingers a pool player */
      /* meaning: team signaling to captain their preference */
      /* TODO: distort pool position according to how many fingerings */
    }
    break;
  case INL_DRAFT_MOVING_TO_PICK : /* has been chosen, in transit to team */
  case INL_DRAFT_PICKED         : /* has been chosen by a captain */
    if (me->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain fingers a picked player */
      /* meaning: delegation of pick duty */
      /* TODO: implement _PICKED_SELECTOR */
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
  case INL_DRAFT_OFF             : return "INL_DRAFT_OFF";
  case INL_DRAFT_MOVING_TO_POOL  : return "INL_DRAFT_MOVING_TO_POOL";
  case INL_DRAFT_CAPTAIN_UP      : return "INL_DRAFT_CAPTAIN_UP";
  case INL_DRAFT_CAPTAIN_DOWN    : return "INL_DRAFT_CAPTAIN_DOWN";
  case INL_DRAFT_POOLED          : return "INL_DRAFT_POOLED";
  case INL_DRAFT_MOVING_TO_PICK  : return "INL_DRAFT_MOVING_TO_PICK";
  case INL_DRAFT_PICKED          : return "INL_DRAFT_PICKED";
  case INL_DRAFT_PICKED_SELECTOR : return "INL_DRAFT_PICKED_SELECTOR";
  }
  return "UNKNOWN";
}
