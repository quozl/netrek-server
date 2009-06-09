/* sortdb.c
 * sort player db by last time player played (for faster searchs for commonly
 *   used players)
 *
 *   Nick Trown (12/20/95)
 */
#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "config.h"


#define ENT_QUANTUM	512

struct statentry *se = NULL;

int num_players = 0;
char pl_filename[] = {LOCALSTATEDIR"/players"};
int tilde = 0;
time_t 	currenttime;

static void read_files(void)
{
    int pl_fd;
    int num_ent;

    num_players = 0;
    num_ent = ENT_QUANTUM;	/* default size is ENT_QUANTUM entries */

    if ((pl_fd = open(pl_filename, O_RDONLY)) < 0) {
	perror(pl_filename);
	exit(-1);
    }

    se = (struct statentry*) malloc(sizeof(struct statentry) * num_ent);
    for (;;) {
	if (num_players == num_ent) {
	    /* about to run out of space; realloc buffer */
	    num_ent += ENT_QUANTUM;
	    se = (struct statentry *) realloc(se, 
			sizeof(struct statentry) * num_ent);
	}
	if (read(pl_fd, &(se[num_players]), sizeof(struct statentry))
						== sizeof(struct statentry)) {
	    /* fix for Pig borg corrupted scores files */
	    if (strlen(se[num_players].name)) {
#ifdef LTD_STATS
#ifndef LTD_PER_RACE
		if (!se[num_players].stats.ltd[0][LTD_SB].ticks.total) 
		    se[num_players].stats.ltd[0][LTD_SB].ticks.total = 1;
#endif
#else
                if (!se[num_players].stats.st_tticks)
                    se[num_players].stats.st_ticks = 1;
#endif
		if (se[num_players].stats.st_lastlogin > currenttime) {
		    printf("%s's time is greater than the current time. Setting it\n", se[num_players].name);
		    se[num_players].stats.st_lastlogin = currenttime;
		}
	    }
	    num_players++;
	} else
	    break;
    }

    if (!num_players) {
	fprintf(stderr, "no entries in player database");
	exit(0);
    }
    close(pl_fd);
}


static void write_files(void)
{
    char filename2[256];
    int i, fd;

    strcpy(filename2, pl_filename);
    strcat(filename2, "~");
    if (!tilde) {
	/* if this is the first time, back up the old file */
	unlink(filename2);	/* remove existing ~ file */
	if (link(pl_filename, filename2) < 0) goto failed;
	printf("Saved old player db to: %s\n", filename2);
    }
    if (unlink(pl_filename) < 0) goto failed;
    if ((fd = open(pl_filename, O_WRONLY|O_CREAT, 0644)) < 0) goto failed;
    for (i = 0; i < num_players; i++) {
	if (write(fd, &(se[i]), sizeof(struct statentry))
		!= sizeof(struct statentry)) {
	    goto failed;
	}
    }
    close(fd);

    tilde = 1;

    return;

failed:
    fprintf(stderr,"Error! (%d)\n", errno);
}

int sorter(struct statentry *p1, struct statentry *p2)
{
    if (p1->stats.st_lastlogin < p2->stats.st_lastlogin)
	return 1;
    if (p1->stats.st_lastlogin > p2->stats.st_lastlogin)
	return (-1);
    return 0; 
}

int main(void)
{
    printf("This will sort the player db.\n\n");
    printf("Make SURE no is playing when this is runs. You have 5 seconds to stop it\n");
    sleep(5);
    printf("Ok, here we go!\n");

    currenttime = time(NULL);
    read_files();

    printf("%d players read in. Starting sort\n", num_players);
    qsort(se, num_players, sizeof(struct statentry),
	  (int (*)(const void *, const void *)) sorter);

    printf("Sorted.. saving out\n");

    write_files();
    return 0;
}
