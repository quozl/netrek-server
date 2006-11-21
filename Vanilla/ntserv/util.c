/*
 * util.c
 */
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

#ifdef FULL_HOSTNAMES
  p = &players[0];
  for(i=j;i<MAXPLAYER;i++) {
    if ( (p->p_status != PFREE) && (!(p->p_flags & PFROBOT))) {
      if (!strcmp(host, p->p_full_hostname)) return i;
    }
    p++;
  }
#endif
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
