/*
 * planetlist.c
 */
#include "copyright.h"
#include <stdio.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

static char *teamname[9] = {
    "IND",
    "FED",
    "ROM",
    "",
    "KLI",
    "",
    "",
    "",
    "ORI"
};

#define COL2	(MAXPLANETS/2)

planetlist()
{
    register int i;
    register int k = 0;
    char buf[BUFSIZ];
    register struct planet *j;

    k = 2;
    for (i = 0; i < MAXPLANETS/2; i++) {
	if (updatePlanet[i]) {
	    j = &planets[i];
	    (void) sprintf(buf, "%-16s %3s %3d    %c%c%c   %c%c%c%c",
		j->pl_name,
		teamname[j->pl_owner],
		j->pl_armies,
		(j->pl_flags & PLREPAIR ? 'R' : ' '),
		(j->pl_flags & PLFUEL ? 'F' : ' '),
		(j->pl_flags & PLAGRI ? 'A' : ' '),
		(j->pl_info & FED ? 'F' : ' '),
		(j->pl_info & ROM ? 'R' : ' '),
		(j->pl_info & KLI ? 'K' : ' '),
		(j->pl_info & ORI ? 'O' : ' '));
	    W_WriteText(plstatw, 2, k+i, planetColor(j), buf, strlen(buf),
		planetFont(j));
	    updatePlanet[i] = 0;
	}
	if (updatePlanet[i+COL2]) {
	    j = &planets[i+COL2];
	    (void) sprintf(buf, "%-16s %3s %3d    %c%c%c   %c%c%c%c",
		j->pl_name,
		teamname[j->pl_owner],
		j->pl_armies,
		(j->pl_flags & PLREPAIR ? 'R' : ' '),
		(j->pl_flags & PLFUEL ? 'F' : ' '),
		(j->pl_flags & PLAGRI ? 'A' : ' '),
		(j->pl_info & FED ? 'F' : ' '),
		(j->pl_info & ROM ? 'R' : ' '),
		(j->pl_info & KLI ? 'K' : ' '),
		(j->pl_info & ORI ? 'O' : ' '));
	    W_WriteText(plstatw, 43, k+i, planetColor(j), buf, strlen(buf),
	   	planetFont(j));
	    updatePlanet[i+COL2] = 0;
	}
    }
}
