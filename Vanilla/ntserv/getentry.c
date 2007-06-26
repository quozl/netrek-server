/*
 * getentry.c
 *
 * Kevin P. Smith  1/31/89
 *
 * Get an outfit and team entry request from user.
 *
 * Modified
 * mjk - June 1995 - Supports improved queue structure
 */
#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "util.h"

/* file scope prototypes */
static int deadTeam(int owner);
static int tournamentMask(int team, int w_queue);


void getEntry(int *team, int *stype)
{
    int i;
    int switching = -1;		/* confirm switches 7/27/91 TC */

    FD_SET (CP_OUTFIT, &inputMask);
    for (;;) {
	/* updateShips so he knows how many players on each team */
	updateShips();
	sendMaskPacket(tournamentMask(me->p_team,me->w_queue));
	flushSockBuf();		
	/* Have we been busted? */
	if (me->p_status == PFREE) {
            ERROR(1,("%s: getentry() unfreeing a PFREE (pid %i)\n",
		     me->p_mapchars, (int) getpid()));
	    me->p_status=PDEAD;
	    me->p_explode=600;
	}
	socketPause(1);
	readFromClient();
	if (isClientDead()) {
	    int i;

	    if (noressurect) exitGame();
      	    ERROR(2,("%s: getentry() commences resurrection\n",
        		me->p_mapchars));

	    /* UDP fail-safe */
	    commMode = COMM_TCP;
	    if (udpSock >= 0)
		closeUdpConn();

	    /* For next two minutes, we try to restore connection */
	    shutdown(sock, 2);
	    sock= -1;
	    for (i=0;; i++) {
		switch(me->p_status) {
		case PFREE:
                    ERROR(1,("%s: getentry() unfreeing a PFREE", 
			     me->p_mapchars));
		    me->p_status=PDEAD;
		    me->p_explode=600;
		    break;
		case PALIVE:
		    me->p_ghostbuster=0;
		    break;
                case POBSERV:
                  me->p_status = PDEAD;
		case PDEAD:
		    me->p_explode=600;
		    break;
		default:
		    me->p_explode=600;
		    me->p_ghostbuster=0;
		    break;
		}
		sleep(5);
		if (connectToClient(host, nextSocket)) break;
		if (i>=11) {
		    ERROR(2,("%s: getentry() giving up\n", me->p_mapchars)); 
		    switch(me->p_status) {
		    case PFREE:
			break;
		    case PALIVE:
			me->p_ghostbuster=1000000; 
			break;
                    case POBSERV:
                      me->p_status = PDEAD;
		    case PDEAD:
			me->p_explode=0;
			break;
		    default:
			me->p_explode=0;
			me->p_ghostbuster=1000000;
			break;
		    }
		    exitGame();
		}
	    }
	    ERROR(2,("%s: getentry() resurrected\n", me->p_mapchars));

            if(me->p_process != getpid()){
                /* whoops, co-pilot.  We're outa here */
	        ERROR(1,("%s: getentry() exiting a co-pilot\n",
			 me->p_mapchars));
                (void) forceShutdown(-10);
            }

	    testtime = -1;		/* do verification after GB  - NBT */
	    teamPick = -1;
	    updateSelf(0);
	    updateShips();
	    flushSockBuf();
	}
	if (teamPick != -1) {
	    if (teamPick < 0 || teamPick > 3) {
		new_warning(UNDEF,"Get real!");
		sendPickokPacket(0);
		teamPick= -1;
		continue;
	    } 
	    if (!(tournamentMask(me->p_team,me->w_queue) & (1<<teamPick))) {
                new_warning(9,"I cannot allow that.  Pick another team");
		sendPickokPacket(0);
		teamPick= -1;
		continue;
	    }

	    /* switching teams 7/27/91 TC */
	    if (((1<<teamPick) != me->p_team) && 
		(me->p_team != ALLTEAM) && 
		(switching != teamPick) && 
		(me->p_whydead != KGENOCIDE) && 
		(me->p_whydead != KWINNER) && 
		(me->p_whydead != KPROVIDENCE) ) {
		    switching = teamPick;
                    new_warning(10,"Please confirm change of teams.  Select the new team again.");
		    sendPickokPacket(0);
		    teamPick= -1;
		    continue;
		}

	    /* His team choice is ok, but ship is not. */
	    if (shipPick<0 || shipPick>ATT) {
                new_warning(11,"That is an illegal ship type.  Try again.");
		sendPickokPacket(0);
		teamPick= -1;
		continue;
	    }
            /* force choice if there is only one type allowed */
            if (is_only_one_ship_type_allowed(&shipPick)) {
                new_warning(UNDEF,"Only one ship type is allowed, you get that one.");
            }
            /* refuse choice if selected ship type not available */
	    if (shipsallowed[shipPick]==0) {
	        if(send_short){
		    swarning(TEXTE,12,0);
	        }
	        else
                  new_warning(UNDEF,"That ship hasn't been designed yet.  Try again.");
		sendPickokPacket(0);
		teamPick= -1;
		continue;
	    }
            if (shipPick==DESTROYER) {
                if (mystats->st_rank < ddrank) {
                    new_warning(UNDEF,"You need a rank of %s or higher to command a destroyer!", ranks[ddrank].name);
                    sendPickokPacket(0);
                    teamPick= -1;
                    continue;
                }
            }
            if (shipPick==SGALAXY) {
                if (mystats->st_rank < garank) {
                    new_warning(UNDEF,"You need a rank of %s or higher to command a galaxy class ship!", ranks[garank].name);
                    sendPickokPacket(0);
                    teamPick= -1;
                    continue;
                }
            }
	    if (shipPick==STARBASE) {
		if (teams[1<<teamPick].s_turns > 0 && !chaos && !topgun) {

		    new_warning(UNDEF,"Not constructed yet. %d minutes required for completion",teams[1<<teamPick].s_turns);
		    sendPickokPacket(0);
		    teamPick= -1;
		    continue;
		}
		if (mystats->st_rank < sbrank) {
                    if(send_short){
                        swarning(SBRANK_TEXT,sbrank,0);
                    }
                    else {
                      new_warning(UNDEF,"You need a rank of %s or higher to command a starbase!", ranks[sbrank].name);
                    }

		    sendPickokPacket(0);
		    teamPick= -1;
		    continue;
		}
		if (realNumShips(1<<teamPick) < 3 && !chaos && !topgun) {
		if(send_short){
		  swarning(TEXTE,14,0);
		}
		else
		    new_warning(UNDEF,"Your team is not capable of defending such an expensive ship!");
		    sendPickokPacket(0);
		    teamPick= -1;
		    continue;
		}
		if (numPlanets(1<<teamPick) < sbplanets && !topgun) {
        	if(send_short){
                  swarning(TEXTE,15,0);
		}
		else
		    new_warning(UNDEF,"Your team's stuggling economy cannot support such an expenditure!");
		    sendPickokPacket(0);
		    teamPick= -1;
		    continue;
		}
		if (chaos) {
		  int num_bases = 0;
		  for (i=0; i<MAXPLAYER; i++) {
		    if (i != me->p_no &&
			players[i].p_status == PALIVE &&
			players[i].p_team == (1<<teamPick) &&
			players[i].p_ship.s_type == STARBASE) {
			num_bases++;
		    }
        	    if (num_bases >= max_chaos_bases) {
			new_warning(UNDEF,"Your side already has %d starbase%s",max_chaos_bases,(max_chaos_bases>1) ? "s!":"!");
			sendPickokPacket(0);
			teamPick= -1;
			break;
		    }
		  }
		}
		if (!chaos) {
		  for (i=0; i<MAXPLAYER; i++) {
		    if (i != me->p_no &&
			players[i].p_status == PALIVE &&
			players[i].p_team == (1<<teamPick) &&
			players[i].p_ship.s_type == STARBASE && !chaos) {
		      if(send_short){
		        swarning(TEXTE,16,0);
		      }
		    else
			new_warning(UNDEF,"Your side already has a starbase!");
			sendPickokPacket(0);
			teamPick= -1;
			break;
		    }
		  }
		}
	    }
	    if (teamPick==-1) continue;
	    break;
	}
    }
    *team=teamPick;
    *stype=shipPick;
    sendPickokPacket(1);
    flushSockBuf();
}

/*
 * New tournamentMask function, reimplemented from scratch. The old
 * code was an unreadable mess.
 * This removes some old junk, and adds new features including diagonal
 * restrictions on teams with 2 players, and not allowing new players
 * to join a team with 4 players on it before T mode starts. These
 * changes are to get people on the right teams faster to get T mode.
 * Behavior during T mode has not changed from the previous code.
 *
 * May 25 2007 -karthik
 */

static int tournamentMask(int team, int queue)
{
    int mask = queues[queue].tournmask;
    int large[2] = {0, 0};
    int count[NUMTEAM];
    int i;
    
    /* Handle special cases that disallow all teams */
    /* INL guests cannot play, but they can read the MOTD and can observe */
    if ((inl_mode && !Observer && is_guest(me->p_name)) ||
    /* Restricted server hours */
        !time_access() ||
    /* Broken daemon */
        !(status->gameup & GU_GAMEOK) ||
    /* Must exit is set */
        (mustexit))
        return 0;
    
    /* Handle special cases that do other things */
    /* Chaos or topgun mode */
    if (chaos || topgun)
        return mask;
    /* Queue with no restrictions */
    if (!(queues[queue].q_flags & QU_RESTRICT))
        return queues[queue].tournmask;
    
    /* Find the two largest teams, include bots in the count if in
       pre-T mode */
    memset(count, 0, NUMTEAM * sizeof(int));
    for (i = 0; i < NUMTEAM; i++) {
#ifdef PRETSERVER
        count[i] = (pre_t_mode ? realNumShipsBots(1 << i) :
                                 realNumShips(1 << i));
#else
        count[i] = realNumShips(1 << i);
#endif

        /* Mask out full teams, unless we are on one */
        if (((count[i] >= 8) || (count[i] >= 4 && pre_t_mode)) &&
            (team != (1 << i)) && !Observer)
            mask &= ~(1 << i);
        
        /* large[0] == largest team, large[1] == second largest team */
        if (count[i] > count[large[0]]) {
            large[1] = large[0];
            large[0] = i;
        } else if ((count[i] > count[large[1]]) || (large[0] == large[1]))
            large[1] = i;
    }
    
    /* Handle rejoining after a team has been timercided, disallow the
       old team and disallow the two largest teams (may overlap)
       Return early so we can't diagonal-mask out all 4 teams */
    if (deadTeam(team)) {
        mask &= ~team;
        mask &= ~(1 << large[0]);
        mask &= ~(1 << large[1]);
        return mask;
    }

    /* Disallow diagonals from the 2 largest teams with >= 2 players
       The sysdef CLASSICTOURN option disables this logic */
    if (nodiag && (!classictourn || status->tourn))
        for (i = 0; i < 2; i++)
            if (count[large[i]] >= 2)
                switch (1 << large[i]) {
                    case FED:
                        mask &= ~KLI;
                        break;
                    case ROM:
                        mask &= ~ORI;
                        break;
                    case KLI:
                        mask &= ~FED;
                        break;
                    case ORI:
                        mask &= ~ROM;
                        break;
                }
    
    /* Allow observers to pick any valid team on initial entry */
    if ((team == ALLTEAM) && Observer)
        return mask;

    /* Prevent new players from joining a team with 4+ players if
       there is no T mode.  Existing players get to keep their slot on
       death.  The sysdef CLASSICTOURN option disables this logic */
    if (!classictourn && (!status->tourn) && (team == ALLTEAM)) {
        if (count[large[0]] >= 4)
            mask &= ~(1 << large[0]);
        if (count[large[1]] >= 4)
            mask &= ~(1 << large[1]);
        return mask;
    }

    /* Let existing players switch to a team that's down by 2 or more
       slots if they are already on one of the two largest teams,
       otherwise keep them on the same team */
    if ((team != ALLTEAM) && ((team == (1 << large[0])) ||
                              (team == (1 << large[1]))))
    {
        if ((team == (1 << large[0])) &&
            ((count[large[1]] + 2) > (count[large[0]])))
            mask &= ~(1 << large[1]);
        else if ((team == (1 << large[1])) &&
                 ((count[large[0]] + 2) > (count[large[1]])))
            mask &= ~(1 << large[0]);
        return mask;
    }

    /* Let new players or dead 3rd race players join a team that's up
       by 1 slot or less */
    if (count[large[0]] > (count[large[1]] + 1))
        mask &= ~(1 << large[0]);
    else if (count[large[1]] > (count[large[0]] + 1))
        mask &= ~(1 << large[1]);
    return mask;
}

static int deadTeam(int owner)
{
    int i,num=0;
    struct planet *p;

    if (!time_access()) return 1; /* 8/2/91 TC */
    for (i=0, p=planets; i<MAXPLANETS; i++,p++) {
        if (p->pl_owner & owner) {
            num++;
        }
    }
    if (num!=0) return(0);
    return(1);
}
