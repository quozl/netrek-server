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
#include "draft.h"
#include "planet.h"
#include "util.h"

/* BUGs 2008-04-17 ... a draft can occur without a second captain somehow, workaround is to change a slot team using xtkill and ask them to uncaptain and captain.  Enforced weapon and lock state causes delegation to every player. */

/* draft */

/* http://en.wikipedia.org/wiki/Draft_%28sports%29 "A sports draft is
   the process by which professional sports teams select players not
   contracted to any team, often from colleges or amateur ranks." */

/* testing this code ... review the tests/inl-draft script, use
   tools/setgame to trigger a draft manually, use tools/setgame to
   monitor status->gameup, use tools/setship to monitor slots involved
   in a draft. */

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

/* whether a slot is to be ignored for a draft */
static int inl_draft_ignore(struct player *j)
{
  if (j->p_status == PFREE) return 1;
  if (j->p_flags & PFROBOT) return 1;
  if (j->p_flags & PFOBSERV) return 1;
  return 0;
}

static int inl_draft_count_by_state(int p_inl_draft)
{
  int h, k;
  struct player *j;

  for (h = 0, j = &players[0], k = 0; h < MAXPLAYER; h++, j++) {
    if (inl_draft_ignore(j)) continue;
    if (j->p_inl_draft == p_inl_draft) k++;
  }
  return k;
}

/* measure the size of the pool */
static int inl_draft_pool_size()
{
  int h, k;
  struct player *j;

  for (h = 0, j = &players[0], k = 0; h < MAXPLAYER; h++, j++) {
    if (inl_draft_ignore(j)) continue;
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
    if (inl_draft_ignore(j)) continue;
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
      /* captains 10% right and left */
      int dx = DRAFT_W / 10;

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

      j->p_inl_x = x - dx + j->p_inl_pool * (dx / 18) + 1000;
      j->p_inl_y = y;
    }
    break;

  case INL_DRAFT_STYLE_BOTTOM_TO_TOP:
    {
      int dy = DRAFT_W / 5;
      int dx = DRAFT_W / 2;

      j->p_inl_x = x - (dx / 2) + j->p_inl_pool * (dx / 14);
      j->p_inl_y = y + dy;
    }
    break;

  case INL_DRAFT_STYLE_CENTRE_OUTWARDS:
    {
      int n = context->inl_pool;

      j->p_inl_y = y;
      if (n == 0) {
        j->p_inl_x = x;
      } else {
        int dx = DRAFT_W / 20;

        j->p_inl_x = x - (dx * n / 2) + j->p_inl_pool * dx;
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
      
      j->p_inl_x = x + j->p_inl_pick * dx;
      j->p_inl_y = y;
    }
    break;

  case INL_DRAFT_STYLE_BOTTOM_TO_TOP:
    {
      int dx = 0;
      int dy = 0;

      if (j->p_team == ROM) {
        dx = -(DRAFT_W / 8) - (DRAFT_W / 10 * (j->p_inl_pick / 3));
        dy =  (DRAFT_W / 8) + (DRAFT_W / 10 * (j->p_inl_pick % 3));
      } else if (j->p_team == FED) {
        dx =  (DRAFT_W / 8) + (DRAFT_W / 10 * (j->p_inl_pick / 3));
        dy =  (DRAFT_W / 8) + (DRAFT_W / 10 * (j->p_inl_pick % 3));
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
        n = context->inl_home_pick;
      }
      if (j->p_team == ROM) {
        j->p_inl_y = y - dy;
        n = context->inl_away_pick;
      }

      if (n == 0) {
        j->p_inl_x = x;
      } else {
        int dx = DRAFT_W / 20;
        j->p_inl_x = x - (dx * n / 2) + j->p_inl_pick * dx + dx / 2;
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

static void inl_draft_place_end(struct player *j)
{
  pl_pick_home_offset(j->p_team, &j->p_inl_x, &j->p_inl_y);
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
  case INL_DRAFT_MOVING_TO_HOME:
  case INL_DRAFT_END:
    break;
  }
}

/* highlight the captain with the up */
static void inl_draft_highlight_up(struct player *k)
{
  getship(&k->p_ship, BATTLESHIP);
  god(k->p_no, "Draft captain, your turn to pick a player.");
}

/* highlight the captain who is waiting */
static void inl_draft_highlight_down(struct player *k)
{
  getship(&k->p_ship, SCOUT);
  god(k->p_no, "Draft captain, the other captain has the pick, standby.");
}

/* everybody else */
static void inl_draft_highlight_off(struct player *k)
{
  getship(&k->p_ship, CRUISER);
}

static void inl_draft_assign_to_pool(struct player *j)
{
  j->p_inl_draft = INL_DRAFT_MOVING_TO_POOL;
  j->p_inl_pick = 0;
  if (j->p_inl_captain) return; /* captains don't get put in the pool */

  j->p_inl_pool = context->inl_pool++;

  /* separate pool on client sorted player list */
  change_team_quietly(j->p_no, NOBODY, j->p_team);
}

void inl_draft_begin()
{
  int h;
  struct player *j;

  status->gameup &= ~GU_INL_DRAFTED;

  context->inl_draft = INL_DRAFT_MOVING_TO_POOL;
  context->inl_pool = 0;
  context->inl_home_pick = 0;
  context->inl_away_pick = 0;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (inl_draft_ignore(j)) continue;
    j->p_desspeed = 0;
    bay_release(j);
    j->p_flags &= ~(PFREPAIR | PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN |
                    PFCLOAK);
    j->p_flags |= PFSEEN;
    inl_draft_highlight_off(j);
    inl_draft_assign_to_pool(j);
    inl_draft_place(j);
  }

  status->gameup |= GU_INL_DRAFTING;
  pmessage(0, MALL, "GOD->ALL", "The captains have agreed to hold a draft.");
}

void inl_draft_done()
{
  int h;
  struct player *j;

  context->inl_draft = INL_DRAFT_MOVING_TO_HOME;
  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (inl_draft_ignore(j)) continue;
    j->p_inl_draft = INL_DRAFT_MOVING_TO_HOME;
    inl_draft_highlight_off(j);
    inl_draft_place_end(j);
  }
  if (status->gameup & GU_INROBOT)
    status->gameup |= GU_INL_DRAFTED;
}

void inl_draft_end()
{
  int h;
  struct player *j;

  context->inl_draft = INL_DRAFT_OFF;
  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    j->p_inl_draft = INL_DRAFT_OFF;
    p_heal(j);
  }
  pmessage(0, MALL, "GOD->ALL", "The draft has completed.");
  status->gameup &= ~GU_INL_DRAFTING;
}

static void inl_draft_arrival_captain(struct player *k)
{
  int other_team = k->p_team == ROM ? FED : ROM;
  struct player *other_captain = inl_draft_team_to_captain(other_team);

  /* captains are admirals */
  if (status->gameup & GU_INROBOT)
    k->p_stats.st_rank = RANK_ADMIRAL;

  /* arrival without another captain */
  if (other_captain == NULL) {
    k->p_inl_draft = INL_DRAFT_CAPTAIN_UP;
    inl_draft_highlight_up(k);
    return;
  }
  /* arrival with a captain who has the up */
  if (other_captain->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
    k->p_inl_draft = INL_DRAFT_CAPTAIN_DOWN;
    inl_draft_highlight_down(k);
    return;
  }
  k->p_inl_draft = INL_DRAFT_CAPTAIN_UP;
  inl_draft_highlight_up(k);
  /* therefore captain closest to draft gets the first choice */
}

static void inl_draft_arrival_pool(struct player *j)
{
  j->p_inl_draft = INL_DRAFT_POOLED;
}

static void inl_draft_arrival_pick(struct player *j)
{
  j->p_inl_draft = INL_DRAFT_PICKED;
}

static void inl_draft_arrival_home(struct player *j)
{
  j->p_inl_draft = INL_DRAFT_END;
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
  case INL_DRAFT_MOVING_TO_HOME : /* draft ended, going home */
    inl_draft_arrival_home(j);
    break;
  }
}

/* animate alive ships during draft */
void inl_draft_update()
{
  int h;
  struct player *j;

  /* tumbling transwarp-like animation for alive ships involved in draft */
  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    int dx, dy;
    if (inl_draft_ignore(j)) continue;
    if (j->p_status != PALIVE) continue;
    /* newly arriving players are forced into the pool */
    if (context->inl_draft == INL_DRAFT_MOVING_TO_POOL &&
        j->p_inl_draft == INL_DRAFT_OFF) {
      pmessage(0, MALL, "GOD->ALL", "%s has joined, and is ready to be drafted", j->p_mapchars);
      inl_draft_assign_to_pool(j);
    }
    inl_draft_place(j);
    dx = j->p_x - j->p_inl_x;
    dy = j->p_y - j->p_inl_y;
    if ((abs(dx) + abs(dy)) > 750) {
      p_x_y_go(j, j->p_x - (dx / 10), j->p_y - (dy / 10));
      j->p_desdir = j->p_dir = (u_char) rint(((int)j->p_dir + 24) % 256);
    } else {
      p_x_y_go(j, j->p_inl_x, j->p_inl_y);
      inl_draft_arrival(j);
    }
  }

  switch (context->inl_draft) {
  case INL_DRAFT_MOVING_TO_POOL:
    if (inl_draft_pool_size() == 0)
      inl_draft_done();
    break;
  case INL_DRAFT_MOVING_TO_HOME:
    if (inl_draft_count_by_state(INL_DRAFT_MOVING_TO_HOME) == 0)
      inl_draft_end();
    break;
  }
}

static int inl_draft_next(struct player *k)
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j == k) continue;
    if (inl_draft_ignore(j)) continue;
    if (!j->p_inl_captain) continue;
    j->p_inl_draft = INL_DRAFT_CAPTAIN_UP;
    inl_draft_highlight_up(j);
    k->p_inl_draft = INL_DRAFT_CAPTAIN_DOWN;
    inl_draft_highlight_down(k);
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
    j->p_inl_pick = context->inl_home_pick++;
  }

  if (j->p_team == ROM) {
    j->p_inl_pick = context->inl_away_pick++;
  }

  j->p_inl_draft = INL_DRAFT_MOVING_TO_PICK;

  pmessage(0, MALL, "GOD->ALL", "Selection #%d: %s (%s) drafts %s (%s).",
           context->inl_home_pick + context->inl_away_pick,
           k->p_mapchars, j->p_team == FED ? "HOME" : "AWAY", j->p_mapchars,
           j->p_name);
  /* set rank of player depending on pick position */
  if (status->gameup & GU_INROBOT)
    j->p_stats.st_rank =
      NUMRANKS - (context->inl_home_pick + context->inl_away_pick - 1) / 2 - 2;
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
        pmessage(0, MALL, "GOD->ALL", "%s passes this draft pick.",
                 k->p_mapchars);
      }
      /* TODO: this can result in an imbalance */
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
        god(j->p_no, "Draft selector, your captain has chosen you to pick.");
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
        god(j->p_no, "Draft selector, your captain has withdrawn your duty to pick.");
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
    break;
  case INL_DRAFT_CAPTAIN_UP     : /* captain with right to select */
  case INL_DRAFT_CAPTAIN_DOWN   : /* captain without right to select */
    if (me->p_inl_draft == INL_DRAFT_CAPTAIN_UP ||
        me->p_inl_draft == INL_DRAFT_CAPTAIN_DOWN) {
        pmessage(0, MALL, "GOD->ALL",
                 "Captain %s slaps captain %s around with a dead trout.",
                 me->p_mapchars, j->p_mapchars);
    }
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
  case INL_DRAFT_MOVING_TO_HOME  : return "_MOVING_TO_HOME";
  case INL_DRAFT_END             : return "_END";
  }
  return "UNKNOWN";
}

void inl_draft_watch()
{
  int h;
  struct player *j;

  for (;;) {
    usleep(500000);
    fprintf(stderr, "\033[f_draft_style=%d _draft=%02d _pool=%02d "
            "_home_pick=%02d _away_pick=%02d\n",
            inl_draft_style, context->inl_draft, context->inl_pool,
            context->inl_home_pick, context->inl_away_pick);
    for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
      if (inl_draft_ignore(j)) continue;
      fprintf(stderr, "%s C=%d D=%d O=%02d P=%02d X=%08d Y=%08d %s\n",
              j->p_mapchars, j->p_inl_captain, j->p_inl_draft,
              j->p_inl_pool, j->p_inl_pick,
              j->p_inl_x, j->p_inl_y, inl_draft_name(j->p_inl_draft));
    }
  }
}
