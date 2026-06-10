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
#include <time.h>

void mprintf(char *format, ...);
void sendTeamReq(int team, int ship);
void socketPause(void);
int isServerDead(void);
int closeUdpConn(void);
void connectToServer(int port);
int numShips(int owner);
#ifdef ATM
int readFromServer(int pollmode);
#else
int readFromServer(void);
#endif

int teamRequest(int team, int ship)
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

void showteams(void)
{
   mprintf("FED: %d, ROM: %d, KLI: %d, ORI: %d\n",
      numShips(FED), numShips(ROM), numShips(KLI), numShips(ORI));
}

int numShips(int owner)
{
	int		i, num = 0;
	struct player	*p;

	for (i = 0, p = players; i < MAXPLAYER; i++, p++)
		if (p->p_status == PALIVE && p->p_team == owner)
			num++;
	return (num);
}

int realNumShips(int owner)
{
	int		i, num = 0;
	struct player	*p;

	for (i = 0, p = players; i < MAXPLAYER; i++, p++)
		if (p->p_status != PFREE && 
		    p->p_team == owner)
			num++;
	return (num);
}

int deadTeam(int owner)
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

void do_refit(int type)
{	
    sendRefitReq(type);
    localflags &= ~PFREFIT;
}
