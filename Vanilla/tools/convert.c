#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

struct old_status {
    int         active;
    u_char      tourn;          /* Tournament mode? */
    /* These stats only updated during tournament mode */
    u_int       armsbomb, planets, kills, losses, time;
    /* Use long for this, so it never wraps */
    U_LONG	timeprod;
    int         gameup;
};

int main(void)
{
    int glfd;
    struct status status;
    struct old_status old_stats;
    char command[256];
    struct stat gl_stat;

    sprintf(Global,"%s/%s",LOCALSTATEDIR, N_GLOBAL);
    printf("Checking %s file for conversion\n",Global);
    glfd = open(Global, O_RDWR, 0744);
    if (glfd < 0) {
        fprintf(stderr, "No global file.  Resetting all stats\n");
        memset((char *) &old_stats, 0, sizeof(struct old_status));
		glfd = open(Global, O_RDWR|O_CREAT, 0744);
		if (glfd < 0) {
			fprintf(stderr, "Unable to create new global file.");
			exit(1);
		}
    } else {
	fstat(glfd, &gl_stat);
	if (gl_stat.st_size == sizeof(struct status)) {
	    printf("Looks like a double global file. Not doing anything.\n");
	    exit(0);
	}
        if ((read(glfd, (char *) &old_stats, sizeof(struct old_status))) !=
        	sizeof(struct old_status)) {
            fprintf(stderr, "Global file wrong size.  Creating new one.\n");
        }
    }

    sprintf(command,"cp %s %s/.GLOBAL.BAK",Global,LOCALSTATEDIR);
    printf("Copying current .global to %s/.GLOBAL.BAK\n",LOCALSTATEDIR);
    system(command);

    status.active = old_stats.active;
    status.tourn = old_stats.tourn;
    status.armsbomb = old_stats.armsbomb;
    status.planets = old_stats.planets;
    status.kills = old_stats.kills;
    status.losses = old_stats.losses;
    status.time = old_stats.time;
    status.timeprod = old_stats.timeprod;
    status.gameup = old_stats.gameup;

    printf("New Global Stats:\n");
    printf("\tHours: %-8.1f\n",status.timeprod/36000.0);
    printf("\tPlanets: %-8d\n", status.planets);
    printf("\tBombing: %-8d\n", status.armsbomb);
    printf("\tOffense: %-8d\n", status.kills);
    printf("\tDefense: %-8d\n", status.losses);

    printf("Writing new global\n");
    if (glfd >= 0) {
        (void) lseek(glfd, (off_t) 0, SEEK_SET);
        if (write(glfd, (char *) &status, sizeof(struct status)) < 0) {
			fprintf(stderr, "Write failed.\n");
			exit(1);
		}
    }
    close(glfd);
    return 0;
}

