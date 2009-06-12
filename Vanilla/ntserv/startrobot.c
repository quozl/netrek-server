/*
 * startrobot.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

int practice_robo(void)
{
    char *arg1;
    register int i;
    register struct player *j;

    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	if (j->p_status != PALIVE)
	    continue;
	if (j->p_flags & PFPRACTR)
	    continue;
	if (j == me)
	    continue;
	new_warning(84,"Can't send in practice robot with other players in the game.");
	return 0;
    }

    if ((int)fork() == 0) {
	(void) signal(SIGALRM, SIG_DFL);
	(void) close(0);
	(void) close(1);
	(void) close(2);
	switch (me->p_team) {
	    case FED:
		arg1 = "-Tf";
		break;
	    case ROM:
		arg1 = "-Tr";
		break;
	    case KLI:
		arg1 = "-Tk";
		break;
	    case ORI:
		arg1 = "-To";
		break;
	    default:
		arg1 = "-Tf";
	}
	execl(Robot, "robot", arg1, "-p", "-f", "-h", (char *) NULL);
	_exit(1);
    }
    return 1;
}

