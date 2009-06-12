/*
 * util.c
 */

#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "copyright.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "planet.h"

#if !defined(strcasestr)
#define strcasestr strstr
#endif /* !defined(strcasestr) */

/*
** Provide the angular distance between two angles.
*/

int angdist(u_char x, u_char y)
{
    register u_char res;

    res = (u_char)ABS(x - y);
    if (res > 128)
	return(256 - (int) res);
    return((int) res);
}

#ifdef DEFINE_NINT
int nint(double x)
{
   return (int) x;              /* xx */
}

#endif /* DEFINE_NINT */


#ifndef HAVE_USLEEP
int microsleep(LONG microSeconds)
{
        u_int            Seconds, uSec;
        int                     nfds, readfds, writefds, exceptfds;
        struct  timeval         Timer;

        nfds = readfds = writefds = exceptfds = 0;

        if( (microSeconds == (U_LONG) 0)
                || microSeconds > (U_LONG) 4000000 )
        {
                errno = ERANGE;         /* value out of range */
                perror( "usleep time out of range ( 0 -> 4000000 ) " );
                return -1;
        }

        Seconds = microSeconds / (U_LONG) 1000000;
        uSec    = microSeconds % (U_LONG) 1000000;

        Timer.tv_sec            = Seconds;
        Timer.tv_usec           = uSec;

        if( select( nfds, &readfds, &writefds, &exceptfds, &Timer ) < 0 )
        {
		if (errno != EINTR) {
                	perror( "usleep (select) failed" );
                	return -1;
		}
        }

        return 0;
}
#endif

int realNumShips(int owner)
{
    int         i, num = 0;
    struct player       *p;

    for (i = 0, p = players; i < MAXPLAYER; i++, p++)
        if (p->p_status != PFREE && 
            !(p->p_flags & PFOBSERV) &&
            !(p->p_flags & PFROBOT) &&
            p->p_team == owner)
                num++;
    return (num);
}

int realNumShipsBots(int owner)
{
    int         i, num = 0;
    struct player       *p;

    for (i = 0, p = players; i < MAXPLAYER; i++, p++)
        if (p->p_status != PFREE && 
            !(p->p_flags & PFOBSERV) &&
            p->p_team == owner)
                num++;
    return (num);
}

#ifdef LTD_STATS

/* find out who the other T mode team is.  Its not the team I'm on */
/* LTD stats needs to know who the enemy is to determine what zone */
/* a player is in.  Time in each zone is kept track of in LTD      */

void setEnemy(int myteam, struct player *me)
{
  int i, other_team;
  
  other_team = 0;
  
  for(i = 0; i < MAXTEAM; i++) {
    if(i == myteam)
      continue;
    else {
      if (realNumShips(i) >= tournplayers)
	other_team = i;
    }
  }
  
  me->p_hist.enemy_team = other_team;
  /* BUG: this should use team_opposing() instead */

}

#endif /* LTD_STATS */

/* given a host name, return first matching player */
int find_slot_by_host(char *host, int j)
{
  struct player *p;
  int i;

  p = &players[0];
  for(i=j;i<MAXPLAYER;i++) {
    if ( (p->p_status != PFREE) && (!(p->p_flags & PFROBOT))) {
      if (!strcmp(host, p->p_full_hostname)) return i;
    }
    p++;
  }
  return -1;
}

/* given ip address, return first matching player */
int find_slot_by_ip(char *ip, int j)
{
  struct player *p;
  int i;

  p = &players[0];
  for(i=j;i<MAXPLAYER;i++) {
    if ( (p->p_status != PFREE) && (!(p->p_flags & PFROBOT))) {
      if (!strcmp(ip, p->p_ip)) return i;
    }
    p++;
  }
  return -1;
}

char *team_name(int team) {
  char *array[MAXTEAM+1] = { "0", "Federation", "Romulans", "3", "Klingons", 
			     "5", "6", "7", "Orions" };
  return array[team];
}

char *team_verb(int team) {
  char *array[MAXTEAM+1] = { "0", "has", "have", "3", "have",
			     "5", "6", "7", "have" };
  return array[team];
}

char *team_code(int team) {
  char *array[MAXTEAM+1] = { "0", "FED", "ROM", "3", "KLI",
			     "5", "6", "7", "ORI" };
  return array[team];
}

int team_find(char *name)
{
  if (!strncasecmp(name,team_name(FED),strlen(name))) return FED;
  if (!strncasecmp(name,team_name(ROM),strlen(name))) return ROM;
  if (!strncasecmp(name,team_name(KLI),strlen(name))) return KLI;
  if (!strncasecmp(name,team_name(ORI),strlen(name))) return ORI;
  return 0;
}

void orbit_release_by_planet(struct planet *pl) {
  int i;
  for (i=0; i<MAXPLAYER; i++) {
    struct player *me = &players[i];
    if (me->p_status & PFREE) continue;
    if (me->p_flags & PFOBSERV) continue;
    if (me->p_flags & PFORBIT) {
      if (me->p_planet == pl->pl_no) {
	me->p_flags &= ~(PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
      }
    }
  }
}

/* given a team mask, return the team mask of the opposing team */
int team_opposing(int team)
{
  if (!status->tourn) return NOBODY;
  if (team == context->quorum[0]) return context->quorum[1];
  if (team == context->quorum[1]) return context->quorum[0];
  return NOBODY;
}

/* given a team mask, return the team number */
int team_no(int team)
{
    if (team == FED) return 0;
    if (team == ROM) return 1;
    if (team == KLI) return 2;
    if (team == ORI) return 3;
    return -1;
}

/* return either me or the observed me */
struct player *my()
{
  if (Observer && (me->p_flags & PFPLOCK))
    return &players[me->p_playerl];
  return me;
}

int is_robot(const struct player *pl)
{
#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
  if (pl->p_flags & PFBPROBOT) return 1;
  if (is_robot_by_host && (!strcmp(pl->p_ip, robot_host))) return 1;
#endif
  if (pl->p_flags & PFROBOT) return 1;
  return 0;
}

int is_local(const struct player *p)
{
  /*
   * If hostname ends with 'localhost', consider it local.
   * Not very efficient, but fast enough and less intrusive than
   * adding a new flag.
   */
  size_t len;
  
  len = strlen(p->p_full_hostname);
  if (len >= 9 && 
      strstr(p->p_full_hostname, "localhost") == &p->p_full_hostname[len - 9])
    return 1;
  return 0;
}

/* if only one ship type is allowed, force the passed type to it */
int is_only_one_ship_type_allowed(int *type)
{
  int i, count = 0, seen = 0;
  for (i=0;i<NUM_TYPES;i++)
    if (shipsallowed[i]) { count++; seen=i; }
  if (count == 1) { *type = seen; return 1; }
  return 0;
}

/* return the metaserver type code for this server */
char *my_metaserver_type()
{
#ifdef STURGEON
  if (sturgeon) return "S";
#endif
  if (inl_mode) return "I";
  if (practice_mode) return "F";
  if (hockey_mode) return "H";
  return "B";
}

int is_guest(char *name)
{
  if (!strcmp(name, "guest")) return 1;
  if (!strcmp(name, "Guest")) return 1;
  return 0;
}

/* convert a player number to a player struct pointer */
struct player *p_no(int i)
{
  return &players[i];
}

/* convert a text player number to a player struct pointer */
struct player *player_by_number(char *name)
{
  int i = atoi(name);
  if ((i == 0) && (name[0] != '0')) {
    char c = name[0];
    if (c >= 'a' && c <= 'z')
      i = c - 'a' + 10;
    else
      return NULL;
  }
  if (i >= MAXPLAYER) return NULL;
  return &players[i];
}

void change_team_quietly(int p_no, int ours, int theirs)
{
    struct player *k = &players[p_no];
    int queue;

    if ((k->w_queue != QU_PICKUP) && (ours != NOBODY))
    {
        queue = ( ours == FED ) ? QU_HOME : QU_AWAY;
        queues[k->w_queue].free_slots++;
        k->w_queue = queue;
        queues[k->w_queue].free_slots--;
    }

    k->p_hostile |= theirs;
    k->p_swar    |= theirs;
    k->p_hostile &= ~ours;
    k->p_swar    &= ~ours;
    k->p_war      = (k->p_hostile | k->p_swar);
    k->p_team     =  ours;
    sprintf(k->p_mapchars, "%c%c", teamlet[k->p_team], shipnos[p_no]);
    sprintf(k->p_longname, "%s (%s)", k->p_name, k->p_mapchars);
}

/*
 **  Move a player to the specified team, if they are not yet there.
 **  Make them peaceful with the new team, and hostile/at war with the
 **  other team.
 */
void change_team(int p_no, int ours, int theirs)
{
    struct player *k = &players[p_no];

    change_team_quietly(p_no, ours, theirs);

    k->p_status = PEXPLODE;
    k->p_whydead = TOURNSTART;
    if (k->p_ship.s_type == STARBASE)
        k->p_explode = 2 * SBEXPVIEWS;
    else
        k->p_explode = 10;
    k->p_ntorp = 0;
    k->p_nplasmatorp = 0;
    k->p_hostile = (FED | ROM | ORI | KLI);
    k->p_war     = (k->p_hostile | k->p_swar);
}

char slot_char(int slotnum)
{
    /* Numbered slot */
    if ((slotnum >= 0) && (slotnum < 10))
        return('0' + slotnum);
    /* Alphabetical slot */
    if ((slotnum >= 10) && (slotnum < MAXPLAYER) && (slotnum < 36))
        return('a' + slotnum - 10);
    /* Valid slot, but higher than 'z' */
    if ((slotnum >= 36) && (slotnum < MAXPLAYER))
        return('?');
    return -1;
}

int slot_num(char slot)
{
    if ((slot >= '0') && (slot <= '9'))
        return(slot - '0');
    if ((tolower(slot) >= 'a') && (tolower(slot) <= 'z'))
        return(tolower(slot) - 'a' + 10);
    return -1;
}

/* set coordinates for a new ship, caller should be synchronised to daemon */
void p_x_y_go(struct player *pl, int p_x, int p_y)
{
  pl->p_x = p_x;
  pl->p_y = p_y;
  pl->p_x_internal = spi(p_x);
  pl->p_y_internal = spi(p_y);
  pl->p_x_y_set = 0;
}

/* cause a ship to be constrained in a box (x,y)-(x,y) */
void p_x_y_box(struct player *pl, int p_x_min, int p_y_min, int p_x_max, int p_y_max)
{
  pl->p_x_min = spi(p_x_min);
  pl->p_x_max = spi(p_x_max);
  pl->p_y_min = spi(p_y_min);
  pl->p_y_max = spi(p_y_max);
}

/* reset constraint to whole galactic */
void p_x_y_unbox(struct player *pl)
{
  pl->p_x_min = spi(0);
  pl->p_x_max = spi(GWIDTH);
  pl->p_y_min = spi(0);
  pl->p_y_max = spi(GWIDTH);
}

/* request new coordinates in unsynchronised fashion */
void p_x_y_set(struct player *j, int p_x, int p_y)
{
  j->p_x_input = spi(p_x);
  j->p_y_input = spi(p_y);
  j->p_x_y_set++;
  /* note: request not obeyed until ship is !(PFDOCK|PFORBIT) */
}

/* adopt new coordinates */
void p_x_y_commit(struct player *j)
{
  j->p_x_internal = j->p_x_input;
  j->p_y_internal = j->p_y_input;
  j->p_x_y_set = 0;
}

/* convert changed p_x to internal scaled representation */
void p_x_y_to_internal(struct player *j)
{
  j->p_x_internal = spi(j->p_x);
  j->p_y_internal = spi(j->p_y);
}

/* move one ship over the top of another (use case: transwarp) */
void p_x_y_join(struct player *j, struct player *pl)
{
  j->p_x = pl->p_x;
  j->p_y = pl->p_y;
  j->p_x_internal = pl->p_x_internal;
  j->p_y_internal = pl->p_y_internal;
}

void t_x_y_set(struct torp *k, int t_x, int t_y)
{
  k->t_x = t_x;
  k->t_y = t_y;
  k->t_x_internal = spi(t_x);
  k->t_y_internal = spi(t_y);
}

int p_ups_set(struct player *me, int client_ups)
{
  int fpu, ups = client_ups;

  /* keep requested updates per second within sysdef limits */
  if (ups < minups) ups = minups;
  if (ups > maxups) ups = maxups;
  if (ups > fps) ups = fps;

  /*
  Convert to a skip count then back to an updates per second that
  corresponds to the skip count, using integer math ... this results
  in an update rate that is most of the time not what the client asked
  for, so we will tell the client the update rate that they will get.
  The client is told the server frame rate, but not how the server
  implements the update rate.

  TODO: send client the actual rate based on fpu.

  For example, at 50 server frames per second, the updates per second
  rates are as follows:

  fps = 50, ups = 50, therefore fpu = 1
  fps = 50, ups = 25, therefore fpu = 2
  fps = 50, ups = 10, therefore fpu = 5
  fps = 50, ups =  1, therefore fpu = 50
  */

  fpu = fps / ups;
  ups = fps / fpu;

  /* if there is no effective change, do nothing */
  if ((me->p_fpu == fpu) && (me->p_ups == ups)) return 0;

  /* store the change */
  me->p_fpu = fpu;
  me->p_ups = ups;

  if (me->p_flags & PFPRACTR) return 1;
#ifdef SHORT_THRESHOLD
  /* I need the number of updates for the threshold handling  HW 93 */
  if (send_threshold != 0) { /* We need to recompute the threshold */
    actual_threshold = send_threshold / ups;
    if (actual_threshold < 60) { /* my low value */
      actual_threshold = 60; /* means: 1 SP_S_PLAYER+SP_S_YOU+36 bytes */
    }
  }
#endif
  return 1;
}

/* This function checks to see if a player is safe from damage. 
   Requires SAFE_IDLE sysdef option to be on, and player to be cloaked
   on a homeworld, not in t-mode, with 0 kills, and maximum fuel,
   shields and hull.
*/
int is_idle(struct player *victim)
{
    if (victim->p_inl_draft != INL_DRAFT_OFF) return 1;
    if (safe_idle
        && (!status->tourn)
        && ((victim->p_flags & PFCLOAK) && (victim->p_flags & PFORBIT)
            && (planets[victim->p_planet].pl_flags & PLHOME))
        && (victim->p_kills == 0.0)
        && (victim->p_ntorp == 0)
        && (victim->p_nplasmatorp == 0)
        && (victim->p_fuel == victim->p_ship.s_maxfuel)
        && (victim->p_shield == victim->p_ship.s_maxshield)
        && (victim->p_damage == 0))
        return 1;
    else
        return 0;
}

int is_invisible_due_idle(struct player *victim)
{
  if (status->gameup & GU_INL_DRAFTING) return 0;
  return is_idle(victim);
}

void p_x_y_go_home(struct player *k)
{
    int x, y;
    pl_pick_home_offset(k->p_team, &x, &y);
    p_x_y_go(k, x, y);
}

void p_heal(struct player *k)
{
  k->p_shield = k->p_ship.s_maxshield;
  k->p_subshield = 0;
  k->p_damage = k->p_subdamage = 0;
  k->p_fuel   = k->p_ship.s_maxfuel;
  k->p_etemp  = k->p_etime = 0;
  k->p_wtemp  = k->p_wtime = 0;
}

/* seconds to described units */
void s2du(int input, unsigned char *value, char *unit) {
  int x = input;

  if (x < 256) { *value = x; *unit = 's'; return; } /* seconds */

  x = input / 60;
  if (x < 256) { *value = x; *unit = 'm'; return; } /* minutes */

  x = input / 60 / 60;
  if (x < 256) { *value = x; *unit = 'h'; return; } /* hours */

  x = input / 60 / 60 / 24;
  if (x < 256) { *value = x; *unit = 'd'; return; } /* days */

  x = input / 60 / 60 / 24 / 7;
  if (x < 256) { *value = x; *unit = 'w'; return; } /* weeks */

  x = input / 60 / 60 / 24 / 365;
  if (x < 256) { *value = x; *unit = 'y'; return; } /* years */

  *value = input & 0xff; *unit = 'z'; return;
}

/*
   Convert an integer `val' to a null terminated string `result'.

   Only the `prec' most significant digits will be written out.
   If `val' can be expressed in fewer than `prec' digits then the
   number is padded out with zeros (if pad is true) or spaces
   (if pad is false).

   WARNING: val must be <= 100000000 (size < 9).

   [code copied from COW string_util.c]
*/
char *itoapad(int val, char *result, int pad, int prec)
{
  int     lead_digit = 0, i, j, too_big = 1, h = 1;

  if (prec != 0)
    result[prec] = '\0';


  /* Careful!!! maximum number convertable must be <=100000000. size < 9 */

  for (i = 100000000, j = 0; i && h <= prec; i /= 10, j++)
    {
      if ((9 - prec) > j && too_big)
        continue;
      else if (h)
        {
          j = 0;
          too_big = 0;
          h = 0;
        }

      result[j] = (val % (i * 10)) / i + '0';

      if (result[j] != '0' && !lead_digit)
        lead_digit = 1;

      if (!lead_digit && !pad)
        if ((result[j] = (val % (i * 10)) / i + '0') == '0')
          result[j] = ' ';
    }

  if (val == 0)
    result[prec - 1] = '0';

  return (result);
}

/*
   Convert a float `fval' to a null terminated string `result'.

   Only the `iprec' most significant whole digits and the `dprec'
   most significat fractional digits are printed.

   The integer part will be padded with zeros (if pad is true) or
   spaces (if pad is false) if it is shorter than `iprec' digits.

   The floating point part will always be padded with zeros.

   WARNING: The whole part of `fval' must be <= 100000000 (size < 9).

   [code copied from COW string_util.c]
*/
char *ftoa(float fval, char *result, int pad, int iprec, int dprec)
{
  int     i, ival;
  float   val = fval;

  if ((iprec + dprec) != 0)
    result[iprec + dprec + 1] = '\0';

  for (i = 0; i < dprec; i++)
    val *= 10.0;

  ival = val;
  itoapad(ival, result, pad, iprec + dprec);

  for (i = (iprec + dprec); i >= iprec; i--)
    if (result[i] == ' ')
      result[i + 1] = '0';
  else
    result[i + 1] = result[i];

  result[iprec] = '.';

  if (fval < 1.0)
    result[iprec - 1] = '0';

  return (result);
}
