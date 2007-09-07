#include <stdlib.h>
#include "copyright.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "daemon.h"
#include "conquer.h"
#include "util.h"

/* draft */

/* http://en.wikipedia.org/wiki/Draft_%28sports%29 "A sports draft is
the process by which professional sports teams select players not
contracted to any team, often from colleges or amateur ranks." */

/* TODO: initial draft mode declaration by captains, mutual agreement,
   sets all players to appropriate p_inl_draft modes, and moves
   players into position. */

/* TODO: potential positioning on tactical

+--------------------------------------------------+
|                                                  |
|                           P P P P                |
|                         C                        |
| O O O O O O O O O O                              |
|                         C                        |
|                           P P P P P              |
|                                                  |
+--------------------------------------------------+

Key: O = pool players, C = captains, P = picked team

*/

/* TODO: during a draft if a slot joins, with INL_DRAFT_OFF, head them
   into the pool */

void inl_draft_begin()
{
  int h;
  struct player *j;

  for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
    if (j->p_status == PFREE) continue;
    if (j->p_flags & PFROBOT) continue;
    j->p_inl_draft = INL_DRAFT_MOVING_TO_POOL;
  }
  /* TODO: set course and speed, speed proportional to distance to target */
  status->gameup |= GU_INL_DRAFT;
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
}

/* TODO: call inl_draft_update from daemon during GU_INL_DRAFT mode */

void inl_draft_update()
{
  /* TODO: for INL_DRAFT_MOVING_TO_POOL animate position, and on
     arrival choose whether they are INL_DRAFT_POOLED or
     INL_DRAFT_CAPTAIN_UP or INL_DRAFT_CAPTAIN_DOWN */

  /* TODO: for INL_DRAFT_MOVING_TO_PICK animate position, and on
     arrival change to INL_DRAFT_PICKED */
}

void inl_draft_select(int n)
{
  struct player *player;
  if ((n < 0) || (n > MAXPLAYER)) return;
  player = &players[n];
  switch (player->p_inl_draft) {
  case INL_DRAFT_OFF            : /* not involved */
    break;
  case INL_DRAFT_CAPTAIN_UP     : /* captain with right to select */
    break;
  case INL_DRAFT_CAPTAIN_DOWN   : /* captain without right to select */
    if (me->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain fingers fellow captain */
      /* meaning: pass */
      /* TODO: identify fellow captain, set him to INL_DRAFT_CAPTAIN_UP */
      me->p_inl_draft = INL_DRAFT_CAPTAIN_DOWN;
    }
    break;
  case INL_DRAFT_MOVING_TO_POOL : /* in transit to pool */
  case INL_DRAFT_POOLED         : /* in pool of players to be chosen */
    if (me->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain fingers a pool player */
      /* meaning: pool player is picked, next captain to pick */
      player->p_inl_draft = INL_DRAFT_MOVING_TO_PICK;
      /* TODO: draw phaser from captain to pick */
      /* TODO: change pick to captain's team and fix war settings */
      /* TODO: set course, speed and move pick to team */
      /* TODO: upon arrival set INL_DRAFT_PICKED */

      /* TODO: count slots in INL_DRAFT_MOVING_TO_POOL or INL_DRAFT_POOLED */
      /* TODO: terminate draft if no slots remain in pool */
      /* TODO: identify fellow captain, set him to INL_DRAFT_CAPTAIN_UP */
      me->p_inl_draft = INL_DRAFT_CAPTAIN_DOWN;
    } else if (me->p_inl_draft == INL_DRAFT_PICKED) {
      /* non-captain fingers a pool player */
      /* TODO: distort position according to how many fingerings by team */
    }
    break;
  case INL_DRAFT_MOVING_TO_PICK : /* has been chosen, in transit to team */
  case INL_DRAFT_PICKED         : /* has been chosen by a captain */
    if (me->p_inl_draft == INL_DRAFT_CAPTAIN_UP) {
      /* captain fingers a picked player */
      /* meaning: undefined */
      /* TODO: verify pick is on same team as captain */
      /* TODO: throw the pick back to the pool */
    }
    break;
  }
}

void inl_draft_reject(int n)
{
  struct player *player;
  if ((n < 0) || (n > MAXPLAYER)) return;
  player = &players[n];
  switch (player->p_inl_draft) {
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

/*  Hey Emacs!
 * Local Variables:
 * c-file-style:"bsd"
 * End:
 */
