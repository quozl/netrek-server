#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "ltd_stats.h"
#include "util.h"

extern int openmem(int);

int mode = 0;
static char *ships[] = {"SC", "DD", "CA", "BB", "AS", "SB"};
static char *statnames[] = {"F", "O", "A", "E", "D", "Q", "V"};
static int fd = 1;        /* stdout for non-socket connections */

/* forward function declarations */
static void udp();
static void output(const char *fmt, ...);
static char *name_fix(const char *name);
static char *str(int n, char *s);

int main(int argc, char **argv)
{
    int i;
    char monitor[17];
    int teams[9];
    float totalRatings;
    float hours;
    char *fixed_name;
 
    if (argc>1) {
	mode = *(argv[1]);
	/* if stdin is not a tty, assume it is a socket */
	if (!isatty(0)) fd = 0;
    }

    if (mode == 'u') { udp(); exit(0); }

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
	output("pid   pl avrt std   in   ou who\n");
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
                   ltd_defense_rating(&(players[i]))
#else
		   bombingRating(players+i),
		   planetRating(players+i),
		   offenseRating(players+i),
		   defenseRating(players+i)
#endif
		   );
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
		   "%4d %3d %4.1f %4.1f "
		   "%s:%s@%s %c",
		   players[i].p_process,
		   players[i].p_mapchars, 
		   players[i].p_avrt,
		   players[i].p_stdv,
		   players[i].p_pkls_c_s,
		   players[i].p_pkls_s_c,
		   (fixed_name) ? fixed_name : players[i].p_name,
		   players[i].p_login,
		   players[i].p_full_hostname,
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
    if (mode == 'v') return 0;  /* TODO: dump queue hosts someday */

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

/* server comment */
static char *comment_get() {
#define MAXPATH 256
  char name[MAXPATH];
  snprintf(name, MAXPATH, "%s/%s", SYSCONFDIR, "comment");
  FILE *file = fopen(name, "r");
  if (file == NULL) return NULL;
  static char text[80];
  char *res = fgets(text, 80, file);
  fclose(file);
  if (res == NULL) return NULL;
  res[strlen(res)-1] = '\0';
  return res;
}

/* player count on a queue */
static int pc(int queue) {
  int j, n = 0;
  for (j=queues[QU_PICKUP].low_slot;j < queues[QU_PICKUP].high_slot;j++)
    if (players[j].p_status != PFREE)
      if (!is_robot(&players[j])) n++;
  return n;
}

/* queue length */
static int ql(int queue) {
  return queues[queue].count;
}

/* respond to udp multicast request from clients on a LAN */
static void udp()
{
  char buf[2];
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int sock = 0;
  int stat = recvfrom(sock, buf, 1, MSG_TRUNC, (struct sockaddr *) &addr, &addrlen);
  if (stat < 0 && errno == ENOTSOCK)
    fprintf(stderr, "players: must be called by netrekd with UDP file descriptor setup\n");
  if (stat < 0) { perror("players: recvfrom"); return; }
  if (stat == 0) return;
  
  /* if it isn't a standard query, ignore */
  if (buf[0] != '?') return;
  
  /* compose a reply packet */
  char packet[128];
  
  /* default the comment */
  char *comment = comment_get();
  if (comment == NULL) {
    char *ip = inet_ntoa(addr.sin_addr);
    if (!strcmp(ip,"127.0.0.1")) {
      comment = "server on this computer";
    } else {
      comment = "a nearby netrek server";
    }
  }

  /* s,type,comment,ports,port,players,queue[,port,players,queue] */
  /* if server isn't running, send simple reply */
  if (!openmem(-1)) {
    /* we don't have sysdefaults, so give an invalid type of unknown */
    sprintf(packet, "s,unknown,%s,1,2592,%d,%d\n", comment, 0, 0);
  } else {
    char *type = my_metaserver_type();
    if (inl_mode) {
      int q1 = QU_HOME;
      int q2 = QU_AWAY;
      /* assume standard INL port numbers, we do not read etc/ports */
      sprintf(packet, "s,%s,%s,2,4566,%d,%d,4577,%d,%d\n",
              type, comment, pc(q1), ql(q1), pc(q2), ql(q2));
    } else {
      int q = QU_PICKUP;
      /* assume standard bronco port numbers, we do not read etc/ports */
      sprintf(packet, "s,%s,%s,1,2592,%d,%d\n",
              type, comment, pc(q), ql(q));
    }
  }
  
  /* send the reply */
  stat = sendto(sock, packet, strlen(packet), 0, (struct sockaddr *) &addr, addrlen);
  if (stat < 0) { perror("players: sendto"); return; }
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

  if (fd == 1) {
    vfprintf(stdout, fmt, args);
  } else {
    char      buf[512];
    vsnprintf(buf, 511, fmt, args);
    write(fd, buf, strlen(buf));
  }
  va_end(args);
}

