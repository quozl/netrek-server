#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/file.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "ltd_stats.h"

int mode=0;

extern int openmem(int);

static char *ships[] = {"SC", "DD", "CA", "BB", "AS", "SB"};

static char *statnames[] = {"F", "O", "A", "E", "D"};

static void output(const char *fmt, ...);
static char *name_fix(const char *name);
static char *str(int n, char *s);

static int out = 1;        /* stdout for non-socket connections */

int main(int argc, char **argv)
{
    int i;
    char monitor[17];
    int teams[9];
    float totalRatings;
    float hours;
    char *fixed_name;
 
    if (argc>1) {
	mode= *(argv[1]);
	out = 0;
    }

    if (!openmem(-1)) {
	output("\nNo one playing.\n");
	exit(0);
    }
	
    for (i=0; i<9; i++) teams[i]=0;

    output("<>=======================================================================<>\n");

    switch(mode) {
      case 'l':
	break;
      case 'd':
	output("  Pl: Name             Type  Kills Damage Shields Armies   Fuel\n");
	break;
      case 'r':
	output("  Pl: Name             Status Type Bombing Planets Offense Defense\n");
	break;

      case 'm':
	output("  Pl: Rank       Name             Login      Host name                Type\n");
	break;

      case 'v':
	output("pid   pl avrt std in ou who\n");
	break;

      default:
	output("  Pl: Rank       Name              Login      Host name        Ratings   DI\n");
	break;
    }
    output("<>=======================================================================<>\n");
    for (i=0; i<MAXPLAYER; i++) {
#ifdef LTD_STATS
 	if (players[i].p_status == PFREE || ltd_ticks(&(players[i]), LTD_SB) == 0)
	    continue;
	teams[players[i].p_team]++;
        totalRatings = ltd_total_rating(&(players[i]));
        hours = (float) ltd_ticks(&(players[i]), LTD_TOTAL) / (10*60*60);
     
#else
	if (players[i].p_status == PFREE || players[i].p_stats.st_tticks==0)
	    continue;
	teams[players[i].p_team]++;
	totalRatings = bombingRating(players+i)+ planetRating(players+i)+
		offenseRating(players+i);
	hours = (float) players[i].p_stats.st_tticks/(10*60*60);
#endif
	switch(mode) {
	  case 'd':
	    output("  %2s: %-16s   %2s %6.2f %6d %7d %6d %6d\n",
		   players[i].p_mapchars, players[i].p_name,
		   ships[players[i].p_ship.s_type],
		   players[i].p_kills,
		   players[i].p_damage,
		   players[i].p_shield,
		   players[i].p_armies,
		   players[i].p_fuel);
	    break;
	  case 'r':
	    output("  %2s: %-16s %s/%4d  %2s %7.2f %7.2f %7.2f %7.2f\n", 
		   players[i].p_mapchars,
		   players[i].p_name, 
		   statnames[players[i].p_status],
		   players[i].p_ghostbuster,
		   ships[players[i].p_ship.s_type],
#ifdef LTD_STATS
                   ltd_bombing_rating(&(players[i])),
                   ltd_planet_rating(&(players[i])),
                   ltd_offense_rating(&(players[i])),
                   ltd_defense_rating(&(players[i])));
#else
		   bombingRating(players+i),
		   planetRating(players+i),
		   offenseRating(players+i),
		   defenseRating(players+i));
#endif
	    break;
	  case 'm':
            fixed_name = name_fix(players[i].p_name);
	    output("  %2s: %-10s %-16s %-10s %-24s  %2s\n",
		   players[i].p_mapchars, 
		   ranks[players[i].p_stats.st_rank].name,
		   (fixed_name) ? fixed_name : players[i].p_name,
		   str(10,players[i].p_login),
		   str(-24, players[i].p_monitor),
		   ships[players[i].p_ship.s_type]);
            free(fixed_name);
	    break;
	  case 'v':
            fixed_name = name_fix(players[i].p_name);
	    output("%5d %2s "
#ifdef PING
		   "%4d %3d %2d %2d "
#endif
		   "%s:%s@%s %c",
		   players[i].p_process,
		   players[i].p_mapchars, 
#ifdef PING
		   players[i].p_avrt,
		   players[i].p_stdv,
		   players[i].p_pkls_c_s,
		   players[i].p_pkls_s_c,
#endif
		   (fixed_name) ? fixed_name : players[i].p_name,
		   players[i].p_login,
#ifdef FULL_HOSTNAMES
		   players[i].p_full_hostname,
#else
		   players[i].p_monitor,
#endif
		   '\n');
            free(fixed_name);
	    break;
	  default:
	    strncpy(monitor, players[i].p_monitor, NAME_LEN);
	    monitor[NAME_LEN-1]=0;
	    output(" %c%2s: %-10s %-16s  %-10s %-16s %5.2f %8.2f\n",
		   (players[i].p_stats.st_flags & ST_CYBORG) ? '*' : ' ',
		   players[i].p_mapchars,
		   ranks[players[i].p_stats.st_rank].name,
		   players[i].p_name,
		   players[i].p_login,
		   monitor,
		   totalRatings,
		   totalRatings * hours);
	    break;
	}
    }
    if (mode == 'v') return 0;  /* dump queue hosts someday */

    output("<>=======================================================================<>\n");
    output("  Feds: %d   Roms: %d   Kli: %d   Ori: %d\n", 
	   teams[FED], teams[ROM], teams[KLI], teams[ORI]);

    if(queues[QU_PICKUP].count > 0)
	output("  Wait queue: %d\n", queues[QU_PICKUP].count);
    else
	output("  No wait queue.\n");

    output("<>=======================================================================<>\n");
    return 0;		/* satisfy lint */
}


static char *name_fix(const char *name)
{
   char *new = strdup(name);                    /* xx, never freed */
   register
   char *r = new;

   if(!new) return new;                         /* don't play with null ptr */

   while(*name){
      *r++ = (*name <= 32)?'_':*name;
      name++;
   }
   *r = 0;
   return new;
}


static char *str(int n, char *s)
{
   char                 *new;
   register int         i = 0, j;
   register char        *r;

   if(n > 0){
      j = strlen(s);
      if(j <= n)
         return s;
      else {
         new = (char *) malloc(n);              /* xx, never freed */
         r = new;
         while(*s && i < n){
            *r++ = *s++;
            i++;
         }
         *r = 0;
         return new;
      }
   }
   else if(n < 0){
      n = -n;
      j = strlen(s);
      if(j <= n)
         return s;
      else{
         new = (char *) malloc(n);              /* xx, never freed */
         r = new;
         s += (j-n);
         while(*s){
            *r++ = *s++;
         }
         *r = 0;
         new[0] = '+';  /* indicate that something missing */
         return new;
      }
   }
   else
      return NULL;
}


static void output(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);

   if(out == 1){
      vfprintf(stdout, fmt, args);
   }
   else {
      char      buf[512];
      vsprintf(buf, fmt, args);
      write(out, buf, strlen(buf));
   }
   va_end(args);
}

