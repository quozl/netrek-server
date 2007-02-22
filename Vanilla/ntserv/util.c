/*
 * util.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include "copyright.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"

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
#ifdef OBSERVERS
            p->p_status != POBSERV &&
#endif
            !(p->p_flags & PFROBOT) &&
            p->p_team == owner)
                num++;
    return (num);
}

#ifdef LTD_STATS

/* find out who the other T mode team is.  Its not the team I'm on */
/* LTD stats needs to know who the enemy is to determine what zone */
/* a player is in.  Time in each zone is kept track of in LTD      */
/* tno == my current team                                          */

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

/* given a team number, return the team number of the opposing team */
int team_opposing(int team)
{
  if (!status->tourn) return NOBODY;
  if (team == context->quorum[0]) return context->quorum[1];
  if (team == context->quorum[1]) return context->quorum[0];
  return NOBODY;
}

/* return either me or the observed me */
struct player *my()
{
#ifdef OBSERVERS
  if (Observer && (me->p_flags & PFPLOCK))
    return &players[me->p_playerl];
#endif
  return me;
}

int is_robot(const struct player *pl)
{
  if (pl->p_flags & PFBPROBOT) return 1;
  if (pl->p_flags & PFROBOT) return 1;
  if (strcasestr(pl->p_login, "robot")) return 1;
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
void p_x_y_set(struct player *pl, int p_x, int p_y)
{
  if (p_x < 0) p_x = 0;
  if (p_y < 0) p_y = 0;
  if (p_x > GWIDTH) p_x = GWIDTH;
  if (p_y > GWIDTH) p_y = GWIDTH;
  pl->p_x_input = spi(p_x);
  pl->p_y_input = spi(p_y);
  pl->p_x_y_set++;
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
