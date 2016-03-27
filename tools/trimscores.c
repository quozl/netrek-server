/*
 *  Kevin P. Smith      12/05/88
 *
 *  Takes the output of scores A and generates the player file.
 *  (Also creates the .GLOBAL file, depending on #define's)
 *  This program reads stdin for its data.
 */

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

#include "proto.h"

#define MAXBUFFER 512

struct statentry play_entry;
struct rawdesc *output=NULL;
char mode;

void usage(void);

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

int main(int argc, char **argv)
{

#ifdef LTD_STATS

    fprintf(stderr, "trimscores: This program does not work with LTD_STATS\n");
    exit(1);

#else

    int fd;
    struct player j;
    int count, kept = 0;
    char buf[MAXBUFFER];
    int harsh=10;	/* How strict we will be with player trimming */
    const LONG currenttime = time (NULL);

    if (argc==2) {
	harsh=atoi(argv[1]);
	if (harsh<=0) usage();
    }
    if (argc>2) usage();
    getpath();
    fprintf(stderr,"Warning: if you do not know how to use this program,\n");
    fprintf(stderr,"         you're about to lose the player database\n");
    status=(struct status *) malloc(sizeof(struct status));
    scanf("%10d %10d %10d %10d %10d %10lf\n", 
	&status->time, 
	&status->planets, 
	&status->armsbomb, 
	&status->kills, 
	&status->losses,
	&status->timeprod);

#ifdef DOGLOBAL
    fd = open(Global, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
        fprintf(stderr, "Cannot open the global file!\n");
        exit(0);
    }
    write(fd, (char *) status, sizeof(struct status));
    close(fd);
#endif

    fd = open(PlayerFile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr,"Cannot open player file\n");
    } else {
        fprintf(stderr,"Player database truncated, now reading input\n");
    }

    count=0;
    while (fgets(buf, MAXBUFFER, stdin)) {
	if (strlen(buf) > 0) buf[strlen(buf)-1] = '\0';
	trimblanks2(buf+16);
	trimblanks(buf+33);
	trimblanks(buf+129);
	strcpy(play_entry.name, buf);
	strcpy(play_entry.password, buf+17);
	strcpy(play_entry.stats.st_keymap, buf+34);
#ifdef GENO_COUNT
	sscanf(buf+130, " %d %lf %d %d %d %d %d %d %d %d %d %d %d %d %d %lf %ld %d %d\n",
#else
	sscanf(buf+130, " %d %lf %d %d %d %d %d %d %d %d %d %d %d %d %d %lf %ld %d\n",
#endif
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
#ifdef GENO_COUNT
	    &play_entry.stats.st_flags,
	    &play_entry.stats.st_genos);
#else
	    &play_entry.stats.st_flags);
#endif
	/* Player 0 is always saved. */
	/* This formula reads:
	 * If (deadtime - (10 + rank^2 + playtime/2.4)*n days > 0, nuke him.
	 */
	if (count!=0 && harsh<100 && ((currenttime - play_entry.stats.st_lastlogin - 864000*harsh) - 
	    play_entry.stats.st_rank * play_entry.stats.st_rank * harsh * 86400 -
	    (play_entry.stats.st_tticks + play_entry.stats.st_ticks + 
	     play_entry.stats.st_sbticks) * harsh > 0)) {
	    j.p_stats = play_entry.stats;
	    printf("%-16.16s %7.2f   %4d %5.2f  %4d %5.2f  %4d %5.2f  %4d %5.2f\n",
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
	    continue;
	}
	else if (count !=0 && harsh==100 && play_entry.stats.st_tticks == 1
		&& play_entry.stats.st_tkills == 0
		&& play_entry.stats.st_tlosses == 0
		&& play_entry.stats.st_tarmsbomb == 0
		&& play_entry.stats.st_planets == 0
		&& play_entry.stats.st_rank == 0) {
            j.p_stats = play_entry.stats;
            printf("%-16.16s %7.2f   %4d %5.2f  %4d %5.2f  %4d %5.2f  %4d %5.2f\n",
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
            continue;
	}
	if (fd>=0) {
	    write(fd, (char *) &play_entry, sizeof(struct statentry));
	    kept++;
	}
	count++;
    }
    if (fd>=0) {
	close(fd);
    }
    fprintf(stderr,"Read %d records from input\n", count);
    fprintf(stderr,"Wrote %d records to new player database\n", kept);

#endif /* LTD_STATS */

    return 1;		/* satisfy lint */
}

void usage(void)
{
    printf("Usage: trimscores [n] < scores > dropped\n\
\n\
Takes input of the form generated by the 'scores A' command.  But do\n\
not run it at the same time as 'scores A', because is writes to the\n\
input files by 'scores'.\n\
\n\
It then recreates the database (.players), throwing away characters\n\
which haven't been used recently.  The n tells it how harsh to be when\n\
determining who to throw away.  The default n is 10.\n\
\n\
Alternatively, use a n value of 100 to instead trim players who have a\n\
0 value for rank (i.e. ensign), tkills, tlosses, tarmsbomb, and\n\
tplanets, and a 1 value for tticks.  Useful for trimming unplayed chars.\n\
\n\
For characters dropped, a summary line is written to standard output.\n");
    exit(0);
}
