#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <math.h>
#include <sys/file.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

int theteam[9];
int armies[9];
char *teamchars[] = {"IND", "FED", "ROM", "X", "KLI", "X", "X", "X", "ORI"};

extern int openmem(int);

int main()
{
    int i;

    openmem(0);
    for (i=0; i<40; i++) {
	printf("%2d: (%s) %-16s %4d %-7.7s %-4.4s %-4.4s %-4.4s %c%c%c%c %5d %5d\n",
	    i, teamchars[planets[i].pl_owner],
	    planets[i].pl_name,
	    planets[i].pl_armies,
	    planets[i].pl_flags & PLREPAIR ? "REPAIR" : "",
	    planets[i].pl_flags & PLFUEL ? "FUEL" : "",
	    planets[i].pl_flags & PLAGRI ? "AGRI" : "",
	    planets[i].pl_flags & PLCORE ? "CORE" : "",	/* 3/17/92 TC */
	    planets[i].pl_info & FED ? 'F' : ' ',
	    planets[i].pl_info & ROM ? 'R' : ' ',
	    planets[i].pl_info & KLI ? 'K' : ' ',
	    planets[i].pl_info & ORI ? 'O' : ' ',
	    planets[i].pl_x, planets[i].pl_y);
	theteam[planets[i].pl_owner]++;
	if (planets[i].pl_armies > 4) { 
	    armies[planets[i].pl_owner] += planets[i].pl_armies-4;
	}
    }
    printf("Planets:\n");
    printf("IND: %d  FED: %d  ROM: %d  KLI: %d  ORI: %d\n",
	theteam[0], theteam[1], theteam[2], theteam[4], theteam[8]);
    printf("Spare armies:\n");
    printf("IND: %d  FED: %d  ROM: %d  KLI: %d  ORI: %d\n",
	armies[0], armies[1], armies[2], armies[4], armies[8]);
    printf("Surrender clock:\n");
    printf("IND: %d  FED: %d  ROM: %d  KLI: %d  ORI: %d\n",
	teams[0].te_surrender, teams[1].te_surrender, teams[2].te_surrender, teams[4].te_surrender, teams[8].te_surrender);
    return 1;		/* satisfy lint */
}
