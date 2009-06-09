#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/file.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

/* There is a time bias towards people with at least 4 hours of play... */
#define TIMEBIAS 4

int daysold = 28;

extern double sqrt(double);

struct rawdesc;

extern void getpath(void);

void cumestats(struct rawdesc *new);
void fixkeymap(char *s);
void usage(void);
int ordered(struct rawdesc *num1, struct rawdesc *num2, char mode);
void printout(char *format, ...);

int             out = 1;        /* stdout for non-socket connections */

int numplayers = 0;
double sumoffense = 0.0;
double sumdefense = 0.0;
double sumplanets = 0.0;
double sumbombing = 0.0;
double sum2offense = 0.0;
double sum2defense = 0.0;
double sum2planets = 0.0;
double sum2bombing = 0.0;
double avgoffense = 0.0;
double avgdefense = 0.0;
double avgplanets = 0.0;
double avgbombing = 0.0;
double stdoffense = 0.0;
double stddefense = 0.0;
double stdplanets = 0.0;
double stdbombing = 0.0;

struct statentry play_entry;
struct rawdesc *output=NULL;
char mode;
int showall;
struct rawdesc {
    struct rawdesc *next;
    char name[NAME_LEN];
    int ttime;
    int rank;
    float bombing, planets, offense, defense;
    int nbomb, nplan, nkill, ndie;
    int sbkills, sblosses, sbtime;
    float sbmaxkills;
    LONG lastlogin;
#ifdef GENO_COUNT
    int genos;
#endif

};

void addNewPlayer(char *name, struct player *pl, char mode)
{
    struct rawdesc *temp;
    struct rawdesc **temp2;

    temp=(struct rawdesc *) malloc(sizeof(struct rawdesc));
    strcpy(temp->name, name);
    temp->lastlogin=pl->p_stats.st_lastlogin;
    temp->rank=pl->p_stats.st_rank;
#ifdef LTD_STATS
    temp->ttime=ltd_ticks(pl, LTD_TOTAL) / 10;
    temp->bombing=ltd_bombing_rating(pl);
    temp->planets=ltd_planet_rating(pl);
    temp->offense=ltd_offense_rating(pl);
    temp->defense=ltd_defense_rating(pl);
    temp->nbomb=ltd_armies_bombed(pl, LTD_TOTAL);
    temp->nplan=ltd_planets_taken(pl, LTD_TOTAL);
    temp->nkill=ltd_kills(pl, LTD_TOTAL);
    temp->ndie=ltd_deaths(pl, LTD_TOTAL);
    temp->sbtime=ltd_ticks(pl, LTD_SB) / 10;
    temp->sbkills=ltd_kills(pl, LTD_SB);
    temp->sblosses=ltd_deaths(pl, LTD_SB);
    temp->sbmaxkills=ltd_kills_max(pl, LTD_SB);
#else
    temp->ttime=pl->p_stats.st_tticks / 10;
    temp->bombing=bombingRating(pl);
    temp->planets=planetRating(pl);
    temp->offense=offenseRating(pl);
    temp->defense=defenseRating(pl);
    temp->nbomb=pl->p_stats.st_tarmsbomb;
    temp->nplan=pl->p_stats.st_tplanets;
    temp->nkill=pl->p_stats.st_tkills;
    temp->ndie=pl->p_stats.st_tlosses;
    temp->sbtime=pl->p_stats.st_sbticks / 10;
    temp->sbkills=pl->p_stats.st_sbkills;
    temp->sblosses=pl->p_stats.st_sblosses;
    temp->sbmaxkills=pl->p_stats.st_sbmaxkills;
#endif
#ifdef GENO_COUNT
    temp->genos=pl->p_stats.st_genos;
#endif
    temp2 = &output;

    if (mode=='n') cumestats(temp);

    for (;;) {
	if (((*temp2)==NULL) || ordered(temp, *temp2, mode)) {
	    temp->next=(*temp2);
	    (*temp2) = temp;
	    break;
	}
	temp2= &((*temp2)->next);
    }
}

int ordered(struct rawdesc *num1, struct rawdesc *num2, char mode)
{
    float temp, temp2;

    switch(mode) {
    case 'o':
	temp =num1->ttime*(num1->offense)/(num1->ttime/90 + 40 * TIMEBIAS);
	temp2=num2->ttime*(num2->offense)/(num2->ttime/90 + 40 * TIMEBIAS);

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}

    case 'd':
	temp =(float) num1->ttime/(num1->ndie + num1->ttime/90 + 40 * TIMEBIAS);
	temp2=(float) num2->ttime/(num2->ndie + num2->ttime/90 + 40 * TIMEBIAS);

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}
    case 'b':
	temp =num1->ttime*(num1->bombing)/(num1->ttime/90 + 40 * TIMEBIAS);
	temp2=num2->ttime*(num2->bombing)/(num2->ttime/90 + 40 * TIMEBIAS);

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}
    case 'p':
	temp =num1->ttime*(num1->planets)/(num1->ttime/90 + 40 * TIMEBIAS);
	temp2=num2->ttime*(num2->planets)/(num2->ttime/90 + 40 * TIMEBIAS);

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}
    case 't':
	temp =num1->ttime*(num1->planets+num1->bombing+num1->offense)/(num1->ttime/90 + 10 * TIMEBIAS);
	temp2=num2->ttime*(num2->planets+num1->bombing+num1->offense)/(num2->ttime/90 + 10 * TIMEBIAS);

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}
    case 'h':
	temp =num1->ttime;
	temp2=num2->ttime;

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}
#ifdef GENO_COUNT
    case 'g':
	if (num1->genos>=num2->genos) {
	  if (num1->genos==num2->genos) {
	    temp =num1->ttime*(num1->planets+num1->bombing+num1->offense);
	    temp2=num2->ttime*(num2->planets+num2->bombing+num2->offense);
	    if (temp>=temp2) {
	      return(1);
	    } else {
	     return(0);
	    }
	  }
	  return(1);    
	} else {
	  return(0);
	}
#endif
    case 'n':
    case 'a':
#ifdef GENO_COUNT
    case 'G':
#endif
	if (num1->rank > num2->rank) return(1);
	if (num2->rank > num1->rank) return(0);

	/* 4/2/89  Rank by DI within class.
	 */
    case 'D':
	temp =num1->ttime*(num1->planets+num1->bombing+num1->offense);
	temp2=num2->ttime*(num2->planets+num2->bombing+num2->offense);

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}
    case 's':
	if (num1->sblosses*10 < num1->sbkills) {
	  temp =((float) num1->sbkills/10.0 - (float) num1->sblosses) *
	        ((float) num1->sbkills * 3600.0 / (float) num1->sbtime) / 90.0;
	} else {
	  temp =((float) num1->sbkills/10.0 - (float) num1->sblosses) /
	        ((float) num1->sbkills * 3600.0 / (float) num1->sbtime) * 90.0;
	}

	if (num2->sblosses*10 < num2->sbkills) {
	  temp2 =((float) num2->sbkills/10.0 - (float) num2->sblosses) *
	        ((float) num2->sbkills * 3600.0 / (float) num2->sbtime) / 90.0;
	} else {
	  temp2 =((float) num2->sbkills/10.0 - (float) num2->sblosses) /
	        ((float) num2->sbkills * 3600.0 / (float) num2->sbtime) * 90.0;
	}

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}
    default:
	return(1);
    }
}
	    
int main(int argc, char **argv)
{
#ifdef LTD_STATS

    printf("scores: This program does not work with LTD_STATS\n");
    exit(1);

#else

    int fd;
    static struct player j;
    struct rawdesc *temp;
    int i;
    double totalTime=0;
    char namebuf[NAME_LEN+1];

    getpath();
    status=(struct status *) malloc(sizeof(struct status));

    showall=0; output=NULL;
    mode='a';
    if (argc>1){
	mode= *argv[1];

	if (argv[1][1]=='a') {
	    showall=1;
	}
    }

    if (mode=='O') {
	if (argc>2) daysold=atoi(argv[2]);
	if (argc > 3) out = 0;
    }
    else if (argc > 2) out = 0;

    fd = open(Global, O_RDONLY, 0777);
    if (fd < 0) {
	printout("Cannot open the global file!\n");
	exit(0);
    }
    read(fd, (char *) status, sizeof(struct status));
    close(fd);
    fd = open(PlayerFile, O_RDONLY, 0777);
    if (fd < 0) {
	perror(PlayerFile);
	exit(1);
    }
    switch(mode) {
    case 'O':
    case 'o':
    case 'r':
    case 'b':
    case 'p':
    case 'd':
    case 'a':
    case 'n':
    case 't':
	printout("    Name             Hours      Planets    Bombing    Offense    Defense\n");
	printout("    Totals           %7.2f%11d%11d%11d%11d\n",
	    status->time/36000.0, status->planets, status->armsbomb, 
	    status->kills, status->losses);
	break;
#ifdef GENO_COUNT
    case 'g':
	printout("    Name               Hours   Ratings   DI    Genos\n");
	break;
    case 'G':
	printout("    Name             Hours      Planets    Bombing    Offense    Defense  Genos\n");
	printout("    Totals           %7.2f%11d%11d%11d%11d\n",
	    status->time/36000.0, status->planets, status->armsbomb, 
	    status->kills, status->losses);
	break;
#endif
    case 'D':
    case 'h':
	printout("    Name               Hours   Ratings   DI    Last login\n");
	break;
    case 's':
	printout("    Name              Hours    Kills   Losses  Ratio   Maxkills Kills/hr Rating\n");
	break;
    case 'T':
	totalTime=status->timeprod/10;
	break;
    default:
	usage();
	break;
    case 'A':
    case 'X':
	printout("%10ld %10d %10d %10d %10d %10lf\n",
	    status->time,
	    status->planets,
	    status->armsbomb,
	    status->kills,
	    status->losses, 
	    status->timeprod);
	break;
    }

    i= -1;
    while (read(fd, (char *) &play_entry, sizeof(struct statentry)) == sizeof(struct statentry)) {
	i++;
	j.p_stats = play_entry.stats;
	if (j.p_stats.st_tticks<1) j.p_stats.st_tticks=1;
	if (play_entry.stats.st_tticks<1) play_entry.stats.st_tticks=1;
	fixkeymap(play_entry.stats.st_keymap);
	switch (mode) {
	case 'o':
	case 'b':
	case 'p':
	case 'd':
	case 'a':
	case 'n':
	case 't':
	case 'D':
	case 'h':
#ifdef GENO_COUNT
	case 'G':
	case 'g':
#endif
	    /* Throw away entries with less than 1 hour play time */
	    if (play_entry.stats.st_tticks < 36000) continue;
	case 's':
	    /* Throw away entries unused for over 4 weeks */
	    if (!showall && (mode!='n') && time(NULL) > play_entry.stats.st_lastlogin + 2419200) continue;
	    /* Throw away SB's with less than 1 hour play time */
	    if (mode=='s' && play_entry.stats.st_sbticks < 36000) continue;
#ifdef GENO_COUNT
            /* Don't list people with 0 genos */
            if (mode=='g' && play_entry.stats.st_genos == 0) continue;
#endif
	    addNewPlayer(play_entry.name, &(j), mode);
	    break;
	case 'O':
	    if (play_entry.stats.st_lastlogin + daysold*86400 > time(NULL)) break;
	case 'r':
	    printout("%3d %-16.16s %6.2f %6d %5.2f %5d %5.2f %5d %5.2f %5d %5.2f\n",
		i,
		play_entry.name,
		play_entry.stats.st_tticks / 36000.0,
		play_entry.stats.st_tplanets,
		planetRating(&j),
		play_entry.stats.st_tarmsbomb,
		bombingRating(&j),
		play_entry.stats.st_tkills,
		offenseRating(&j),
		play_entry.stats.st_tlosses,
		defenseRating(&j));
	    break;
	case 'T':
	    totalTime += play_entry.stats.st_ticks / 10 +
			 play_entry.stats.st_sbticks / 10;
	    break;
	default:
	case 'X':
	    {
		unsigned erase_password;
		for (erase_password = 0;
		     erase_password < sizeof (play_entry.password);
		     erase_password++)
		    play_entry.password[erase_password] = 'X';
		play_entry.password[sizeof (play_entry.password) - 1] = '\0';
	    }
	    // Fall through
	case 'A':
	    strcpy(namebuf, play_entry.name);
	    strcat(namebuf, "_");
#ifdef GENO_COUNT
	    printf("%-16.16s %-16.16s %-96.96s %1d %9.2lf %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %9.2lf %9ld %7d %7d\n",
#else
	    printout("%-16.16s %-16.16s %-96.96s %1d %9.2lf %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %9.2lf %9ld %7d\n",
#endif
		namebuf,
		play_entry.password,
		play_entry.stats.st_keymap,
		play_entry.stats.st_rank,
		play_entry.stats.st_maxkills,
		play_entry.stats.st_kills,
		play_entry.stats.st_losses,
		play_entry.stats.st_armsbomb,
		play_entry.stats.st_planets,
		play_entry.stats.st_ticks,
		play_entry.stats.st_tkills,
		play_entry.stats.st_tlosses,
		play_entry.stats.st_tarmsbomb,
		play_entry.stats.st_tplanets,
		play_entry.stats.st_tticks,
		play_entry.stats.st_sbkills,
		play_entry.stats.st_sblosses,
		play_entry.stats.st_sbticks,
		play_entry.stats.st_sbmaxkills,
		play_entry.stats.st_lastlogin,
#ifdef GENO_COUNT
		play_entry.stats.st_flags,
		play_entry.stats.st_genos);
#else
		play_entry.stats.st_flags);
#endif
	    break;
	}
    }
    close(fd);
    if (mode=='T') {
	printout("Total play time for this game has been at least %ld seconds\n",
	    totalTime);
    }
    if (mode=='o' || mode=='b' || mode=='p' || mode=='d' || mode=='a' || mode=='s' || mode=='D' || mode=='h' || mode=='t' || mode=='n' || mode=='G' || mode=='g') {
	int lastrank= -1;
	temp=output;
	i=0;
/* Calculate std., averages */
	if (mode=='n') {
	  avgoffense = sumoffense / (double) numplayers;
	  avgdefense = sumdefense / (double) numplayers;
	  avgplanets = sumplanets / (double) numplayers;
	  avgbombing = sumbombing / (double) numplayers;
	  stdoffense = sqrt(sum2offense / (double) numplayers - avgoffense*avgoffense);
	  stddefense = sqrt(sum2defense / (double) numplayers - avgdefense*avgdefense);
	  stdplanets = sqrt(sum2planets / (double) numplayers - avgplanets*avgplanets);
	  stdbombing = sqrt(sum2bombing / (double) numplayers - avgbombing*avgbombing);
	  printout("    Average                       %4.2f       %4.2f        %4.2f       %4.2f\n",avgplanets,avgbombing,avgoffense,avgdefense); 
	  printout("    Std. Dev.                     %4.2f       %4.2f        %4.2f       %4.2f\n",stdplanets,stdbombing,stdoffense,stddefense); 
	}
	while (temp!=NULL) {
	    if (((mode == 'a') || (mode == 'n') || mode=='G') && lastrank != temp->rank) {
		lastrank = temp->rank;
		printout("\n%s:\n", ranks[lastrank].name);
	    }
	    i++;
	    switch(mode) {
	    case 'o':
	    case 'b':
	    case 'p':
	    case 'd':
	    case 'a':
	    default:
		printout("%3d %-16.16s %6.2f %5d %5.2f %5d %5.2f %5d %5.2f %5d %5.2f\n",
		    i,
		    temp->name,
		    temp->ttime / 3600.0,
		    temp->nplan,
		    temp->planets,
		    temp->nbomb,
		    temp->bombing,
		    temp->nkill,
		    temp->offense,
		    temp->ndie,
		    temp->defense);
		break;
#ifdef GENO_COUNT
	      case 'g':
		printout("%3d %-16.16s %6.2f %7.2f %8.2f  %3d\n",
		    i,
		    temp->name,
		    temp->ttime / 3600.0,
		    temp->bombing+temp->offense+temp->planets,
		    (temp->bombing+temp->offense+temp->planets)*temp->ttime/3600.0,
		    temp->genos);
		break;
	      case 'G':
		printout("%3d %-16.16s %6.2f %5d %5.2f %5d %5.2f %5d %5.2f %5d %5.2f %3d\n",
		    i,
		    temp->name,
		    temp->ttime / 3600.0,
		    temp->nplan,
		    temp->planets,
		    temp->nbomb,
		    temp->bombing,
		    temp->nkill,
		    temp->offense,
		    temp->ndie,
		    temp->defense,
		    temp->genos);
		break;
#endif
	    case 'n':
		printout("%3d %-16.16s%6.2f %5.2f(%5.2f) %5.2f(%5.2f) %5.2f(%5.2f) %5.2f(%5.2f)\n",
		    i,
		    temp->name,
		    temp->ttime / 3600.0,
		    temp->planets,
		    (temp->planets-avgplanets)/stdplanets,
		    temp->bombing,
		    (temp->bombing-avgbombing)/stdbombing,
		    temp->offense,
		    (temp->offense-avgoffense)/stdoffense,
		    temp->defense,
		    (temp->defense-avgdefense)/stddefense);
		break;
	    case 'D':
	    case 'h':
		printout("%3d %-16.16s %6.2f %7.2f %8.2f  %s",
		    i,
		    temp->name,
		    temp->ttime / 3600.0,
		    temp->bombing+temp->offense+temp->planets,
		    (temp->bombing+temp->offense+temp->planets)*temp->ttime/3600.0,
		    ctime(&(temp->lastlogin)));
		break;
	    case 's':
		printout("%3d %-16.16s %6.2f %7d %8d %7.2f %9.2f %9.2f %6.2f\n", 
		    i,
		    temp->name,
		    temp->sbtime / 3600.0,
		    temp->sbkills,
		    temp->sblosses,
		    (temp->sblosses==0) ? (float) temp->sbkills : (float) temp->sbkills/temp->sblosses,
		    temp->sbmaxkills,
		    (float) temp->sbkills * 3600.0 / (float) temp->sbtime,
		    (temp->sblosses*10 < temp->sbkills) ?
		    ((float) temp->sbkills/10.0 - (float) temp->sblosses) *
	            ((float) temp->sbkills * 3600.0 / (float) temp->sbtime) / 90.0 :
		    ((float) temp->sbkills/10.0 - (float) temp->sblosses) /
	            ((float) temp->sbkills * 3600.0 / (float) temp->sbtime) * 90.0);
		break;
	    }
	    temp=temp->next;
	}
    }
#endif /* LTD_STATS */
    return 1;		/* satisfy lint */
}

void fixkeymap(char *s)
{
    int i;

    for (i=0; i<95; i++) {
	if ((s[i]==0) || (s[i] == 10)) { /* Pig fix (no ^J) 7/8/92 TC */
	    s[i]=i+32;
	}
    }
    s[95]=0;
}

void cumestats(struct rawdesc *new)
{
  numplayers++;
  sumoffense += (double) new->offense;
  sumdefense += (double) new->defense;
  sumplanets += (double) new->planets;
  sumbombing += (double) new->bombing;
  sum2offense += (double) new->offense*new->offense;
  sum2defense += (double) new->defense*new->defense;
  sum2planets += (double) new->planets*new->planets;
  sum2bombing += (double) new->bombing*new->bombing;
  return;
}

void usage(void)
{
    printout("usage: scores [mode]\n");
    printout("Modes:\n");
    printout("a - print best players in order\n");
    printout("o - list players with best offense\n");
    printout("d - list players with best defense\n");
    printout("p - list players with best planet rating\n");
    printout("b - list players with best bombing rating\n");
    printout("t - list players with best total ratings\n");
    printout("s - list best starbase players\n");
#ifdef GENO_COUNT
    printout("G - same as a, but show genocides too\n");
#endif
    printout("  Note: for modes [aodpbs], players with less than 1 hour\n");
    printout("    or who haven't played for four weeks are omitted\n");
    printout("    Add 'a' to the letter to include players older than 4\n");
    printout("    weeks (eg, scores sa to get all base players)\n");
    printout("n - print best players in order, showing std. deviations\n");
    printout("r - list all players, hours, and ratings (no order)\n");
    printout("A - list entire database in ascii form (for use with newscores)\n");
    printout("X - list entire database in ascii form without passwords\n");
    printout("    (for debug use with newscores)\n");
    printout("T - print rough number of seconds of play time\n");
    printout("O - print players who haven't logged in for 4 weeks\n");
    printout("D - list players by DI\n");
    printout("h - list players by hours\n");
#ifdef GENO_COUNT
    printout("g - list players by genos\n");
#endif
    printout("none: the default is scores a\n");
    exit(0);
}

void printout(char *format, ...)
{
   va_list	args;

   va_start(args, format);

   if(out == 1){
      vfprintf(stdout, format, args);
   }
   else {
      char      buf[512];
      vsprintf(buf, format, args);
      write(out, buf, strlen(buf));
   }
   va_end(args);
}
