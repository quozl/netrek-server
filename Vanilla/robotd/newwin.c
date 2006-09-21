/*
 * newwin.c
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#ifdef hpux
#include <time.h>
#else
#include <sys/time.h>
#endif
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"

/* Attempt to pick specified team & ship */
teamRequest(team, ship) 
int team,ship;
{
    int lastTime;

    if(team == -1) {
       mprintf("no team in teamRequest\n");
       return 0;
    }

    pickOk= -1;
    sendTeamReq(team,ship);
    lastTime=time(NULL);
    while (pickOk == -1) {
	if (lastTime+3 < time(NULL)) {
	    mprintf("resending team req\n");
	    sendTeamReq(team,ship);
	    lastTime=time(NULL);
	}
	socketPause();
#ifdef ATM
	readFromServer(0);
#else
	readFromServer();
#endif
	if (isServerDead()) {
	    printf("Whoops!  We've been ghostbusted!\n");
	    printf("Pray for a miracle!\n");
#ifdef ATM
            /* UDP fail-safe */
            commMode = commModeReq = COMM_TCP;
            commSwitchTimeout = 0;
            if (udpSock >= 0)
                closeUdpConn();
#endif
	    connectToServer(nextSocket);
	    printf("Yea!  We've been resurrected!\n");
	    pickOk=0;
	    break;
	}
    }
    return(pickOk);
}

showteams()
{
   mprintf("FED: %d, ROM: %d, KLI: %d, ORI: %d\n",
      numShips(FED), numShips(ROM), numShips(KLI), numShips(ORI));
}

numShips(owner)
{
	int		i, num = 0;
	struct player	*p;

	for (i = 0, p = players; i < MAXPLAYER; i++, p++)
		if (p->p_status == PALIVE && p->p_team == owner)
			num++;
	return (num);
}

realNumShips(owner)
{
	int		i, num = 0;
	struct player	*p;

	for (i = 0, p = players; i < MAXPLAYER; i++, p++)
		if (p->p_status != PFREE && 
		    p->p_team == owner)
			num++;
	return (num);
}

deadTeam(owner)
int owner;
/* The team is dead if it has no planets and cannot coup it's home planet */
{
    int i,num=0;
    struct planet *p;

    if (planets[remap[owner]*10-10].pl_couptime == 0) return(0);
    for (i=0, p=planets; i<MAXPLANETS; i++,p++) {
	if (p->pl_owner & owner) {
	    num++;
	}
    }
    if (num!=0) return(0);
    return(1);
}

do_refit(type)
    int type;
{	
    sendRefitReq(type);
    localflags &= ~PFREFIT;
}
