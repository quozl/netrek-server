/*
 * mergescores.c  7/27/92 Terence Chang
 *
 * See README.mergescores for instructions.  Read before using.
 *
 * General plan:
 *   1.  Read in an ASCII dump of a foreign .global.
 *   2.  Read in the local global stats.
 *   3.  Read in the local players' names, passwords, and stats.
 *   4.  Read in an ASCII dump of a foreign .players and for each player:
 *   4.a.  Normalize the planet, bombing, offense, and defense of the foreign
 *         players, so that each rating remains unchanged.  This normalization
 *         will be done by moving t-mode counts over to non-tmode (but keeping
 *         all counts nonnegative).
 *   4.b.1.  Write out duplicate foreign players whose passwords do not match
 *           to stdout.
 *   4.b.2.  Fold the stats of duplicate foreign players whose passwords match.
 *           Sum up kills, etc.  Determine new max kills and SB max kills.
 *           Determine new rank.
 *   4.c   Append (actually prepend) unique foreign players to local .players.
 *   4.d.  Write all local players back to .players, including those with
 *         folded stats.
 *
 * derived from scores.c, newscores.c by Kevin Smith
 */

#include "config.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

#define DIFF_NAME -1
#define DUP_NAME_DIFF_PASSWORD -2
#define MAXBUFFER 512

struct statentry play_entry;
struct status *lstatus;
struct status *fstatus;
struct rawdesc *output=NULL;
struct statentry *lplayers[10000]; /* up to 10000 local players */
int lplayercount = 0;

void usage(void);
void read_lplayers(void);
int dumbsearch(struct statentry *);

void trimblanks2(char *str)
{
    *str=0;
    str--;
    while (*str==' ') {
	*str=0;
	str--;
    }
    if (*str=='_') *str=0;
}

void trimblanks(char *str)
{
    *str=0;
    str--;
    while (*str==' ') {
	*str=0;
	str--;
    }
}

/* ARGSUSED */
int main(int argc, char **argv)
{

#ifdef LTD_STATS

    printf("mergescores: this program cannot be used with LTD_STATS\n");
    exit(1);

#else
    int fd;
    int i;
    char buf[MAXBUFFER];
    float fplanets, farmsbomb, fkills, flosses;
    float temp;
    int delta;
    int fcount = 0;
    int dupcount = 0;
    int foldcount = 0;
    char namebuf[NAME_LEN+1];

    if (argc>1) usage();
    getpath();
    fprintf(stderr, "Warning:  If you do not know how to use this program, break it now!\n");
    lstatus=(struct status *) malloc(sizeof(struct status));
    fstatus=(struct status *) malloc(sizeof(struct status));
    scanf("%10ld %10d %10d %10d %10d %10lf\n", 
	(long int *) &fstatus->time, 
	&fstatus->planets, 
	&fstatus->armsbomb, 
	&fstatus->kills, 
	&fstatus->losses,
	&fstatus->timeprod);

    fd = open(Global, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "Cannot open the global file!\n");
	exit(0);
    }
    read(fd, (char *) lstatus, sizeof(struct status));
    close(fd);

    fprintf(stderr, "          Ticks    Planets   Armsbomb      Kills     Losses   Timeprod\n");
    fprintf(stderr, "Loc: %10ld %10d %10d %10d %10d %10lf\n", 
	(long int) lstatus->time, 
	lstatus->planets, 
	lstatus->armsbomb, 
	lstatus->kills, 
	lstatus->losses,
	lstatus->timeprod);
    fprintf(stderr, "For: %10ld %10d %10d %10d %10d %10lf\n", 
	(long int) fstatus->time, 
	fstatus->planets, 
	fstatus->armsbomb, 
	fstatus->kills, 
	fstatus->losses,
	fstatus->timeprod);

/* want factors such that (For. rating)*factor == (Local rating)
 * (to scale t-mode kills).
 *                                       foreign->timeprod * local->kills
 * therefore, factor is local/foreign == -----------------------------------
 *                                       foreign->kills    * local->timeprod 
 * (for defense, use reciprocal)
 */

    fplanets = (((float) fstatus->timeprod)*((float) lstatus->planets))/
                (((float) fstatus->planets)*((float) lstatus->timeprod));
    farmsbomb = (((float) fstatus->timeprod)*((float) lstatus->armsbomb))/
                (((float) fstatus->armsbomb)*((float) lstatus->timeprod));
    fkills = (((float) fstatus->timeprod)*((float) lstatus->kills))/
                (((float) fstatus->kills)*((float) lstatus->timeprod));
    flosses = (((float) fstatus->losses)*((float) lstatus->timeprod))/
              (((float) fstatus->timeprod)*((float) lstatus->losses));
    fprintf(stderr, "Convert Factors:%10.3f %10.3f %10.3f %10.3f\n",
	    fplanets,
	    farmsbomb,
	    fkills,
	    flosses);
    fprintf(stderr, "(foreign rating * factor) = local rating\n\n");
    
    read_lplayers();

    fd = open(PlayerFile, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
	perror(PlayerFile);
	exit(1);
    }

    fprintf(stderr, "Processing foreign players...\n");
    while (fgets(buf, MAXBUFFER, stdin)) {
	if (strlen(buf) > 0) buf[strlen(buf)-1] = '\0';
	trimblanks2(buf+16);
	trimblanks(buf+33);
	trimblanks(buf+129);
	strcpy(play_entry.name, buf);
	fcount++;
	strcpy(play_entry.password, buf+17);
	strcpy(play_entry.stats.st_keymap, buf+34);
	sscanf(buf+130, " %d %lf %d %d %d %d %d %d %d %d %d %d %d %d %d %lf %ld %d\n",
	    &play_entry.stats.st_rank,
	    &play_entry.stats.st_maxkills,
	    &play_entry.stats.st_kills,
	    &play_entry.stats.st_losses,
	    &play_entry.stats.st_armsbomb,
	    &play_entry.stats.st_planets,
	    &play_entry.stats.st_ticks,
	    &play_entry.stats.st_tkills,
	    &play_entry.stats.st_tlosses,
	    &play_entry.stats.st_tarmsbomb,
	    &play_entry.stats.st_tplanets,
	    &play_entry.stats.st_tticks,
	    &play_entry.stats.st_sbkills,
	    &play_entry.stats.st_sblosses,
	    &play_entry.stats.st_sbticks,
	    &play_entry.stats.st_sbmaxkills,
	    &play_entry.stats.st_lastlogin,
	    &play_entry.stats.st_flags);
	temp = play_entry.stats.st_tplanets * fplanets;
	delta = temp - play_entry.stats.st_tplanets; /* increase/decrease */
	play_entry.stats.st_tplanets = temp;
	play_entry.stats.st_planets -= delta;
	if (play_entry.stats.st_planets < 0) play_entry.stats.st_planets = 0;

	temp = play_entry.stats.st_tarmsbomb * farmsbomb;
	delta = temp - play_entry.stats.st_tarmsbomb; /* increase/decrease */
	play_entry.stats.st_tarmsbomb = temp;
	play_entry.stats.st_armsbomb -= delta;
	if (play_entry.stats.st_armsbomb < 0) play_entry.stats.st_armsbomb = 0;

	temp = play_entry.stats.st_tkills * fkills;
	delta = temp - play_entry.stats.st_tkills; /* increase/decrease */
	play_entry.stats.st_tkills = temp;
	play_entry.stats.st_kills -= delta;
	if (play_entry.stats.st_kills < 0) play_entry.stats.st_kills = 0;

	temp = play_entry.stats.st_tlosses * flosses;
	delta = temp - play_entry.stats.st_tlosses; /* increase/decrease */
	play_entry.stats.st_tlosses = temp;
	play_entry.stats.st_losses -= delta;
	if (play_entry.stats.st_losses < 0) play_entry.stats.st_losses = 0;

	play_entry.stats.st_lastlogin = time(NULL);

	i = dumbsearch(&play_entry);
	switch (i) {
	  case DUP_NAME_DIFF_PASSWORD:
	    dupcount++;
	    strcpy(namebuf, play_entry.name);
	    strcat(namebuf, "_");
	    printf("%-16.16s %-16.16s %-96.96s %1d %9.2lf %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %9.2lf %9ld %7d\n",
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
		   play_entry.stats.st_flags);
	    break;
	  case DIFF_NAME:	/* append (actually, prepend) */
	    write(fd, (char *) &play_entry, sizeof(struct statentry));
	    break;
	  default: /* fold stats together */
	    foldcount++;

	    lplayers[i]->stats.st_maxkills = MAX(lplayers[i]->stats.st_maxkills, play_entry.stats.st_maxkills);

	    lplayers[i]->stats.st_kills +=play_entry.stats.st_kills;
	    lplayers[i]->stats.st_losses += play_entry.stats.st_losses;
	    lplayers[i]->stats.st_armsbomb += play_entry.stats.st_armsbomb;
	    lplayers[i]->stats.st_planets += play_entry.stats.st_planets;
	    lplayers[i]->stats.st_ticks += play_entry.stats.st_ticks;
	    lplayers[i]->stats.st_tkills += play_entry.stats.st_tkills;
	    lplayers[i]->stats.st_tlosses += play_entry.stats.st_tlosses;
	    lplayers[i]->stats.st_tarmsbomb += play_entry.stats.st_tarmsbomb;
	    lplayers[i]->stats.st_tplanets += play_entry.stats.st_tplanets;
	    lplayers[i]->stats.st_tticks += play_entry.stats.st_tticks;
	    lplayers[i]->stats.st_sbkills += play_entry.stats.st_sbkills;
	    lplayers[i]->stats.st_sblosses += play_entry.stats.st_sblosses;
	    lplayers[i]->stats.st_sbticks += play_entry.stats.st_sbticks;

	    lplayers[i]->stats.st_lastlogin = play_entry.stats.st_lastlogin;

	    lplayers[i]->stats.st_sbmaxkills = MAX(lplayers[i]->stats.st_sbmaxkills, play_entry.stats.st_sbmaxkills);

	    lplayers[i]->stats.st_rank = MAX(lplayers[i]->stats.st_rank, play_entry.stats.st_rank);
	    
	    break;
	}			/* end switch */
    }				/* end while */

    fprintf(stderr, "Rewriting local/folded players...\n");
    for (i = 0; i < lplayercount; i++) {
	write(fd, (char *) lplayers[i], sizeof(struct statentry));
    }

    fprintf(stderr, "Done!  %d entries appended/folded (%d folded).\n",
	    fcount - dupcount , foldcount);
    fprintf(stderr, "%d entries not appended, written to stdout.\n", dupcount);
    close(fd);

#endif /* LTD_STATS */

    return 1;		/* satify lint */
}

void read_lplayers(void)
{
    int lfd;

    fprintf(stderr, "Reading in local players.\n");
    lfd = open(PlayerFile, O_RDONLY);
    if (lfd < 0) {
	perror(PlayerFile);
	exit(1);
    }

    lplayercount = 0;
    for (;;) {
	lplayers[lplayercount] = (struct statentry *) malloc(sizeof(struct statentry));
	if (read(lfd,
		 (char *) lplayers[lplayercount],
		 sizeof(struct statentry)) != sizeof(struct statentry))
	    break;
	lplayercount++;
    }
    fprintf(stderr, "%4d local players read...\n", lplayercount);
}

int dumbsearch(struct statentry *p)
{
    register int i;
    for (i = 0; i < lplayercount; i++) {
	if (strcmp(p->name, lplayers[i]->name) == 0) {
	    if (strcmp(p->password, lplayers[i]->password) == 0)
		return i;
	    else
		return DUP_NAME_DIFF_PASSWORD;
	}
    }
    return DIFF_NAME;
}

void usage(void)
{
    printf("Usage: mergescores < scoredb\n");
    printf("This program takes input of the form generated by the 'scores A'\n");
    printf(" command.  This allows you to create an ascii form of the database\n");
    printf(" with 'scores A > scoredb', then you can edit it as needed, then you\n");
    printf(" can issue a 'newscores < scoredb' command to restore the database.\n");
    printf("Note:  The form of the output from the 'scores A' command is very\n");
    printf(" specific.  Be sure that the input stream matches it very accurately\n");
    exit(0);
}

/* 19 Exxon Valdez      543.26   2471  1.76 77784  2.09 15082  1.10 16350  0.93 */

