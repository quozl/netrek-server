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
#ifdef OBSERVERS
                case POBSERV:
                  me->p_status = PDEAD;
#endif
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
#ifdef OBSERVERS
                    case POBSERV:
                      me->p_status = PDEAD;
#endif
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
 * We never force you to switch unless your team is dead or not in the T group.
 * But we always offer you the option to even things up when your team is
 * two players ahead.  This code does not ensure non-diagonal games,
 * some people like them, especially with two-genocides to conquer
 * set in SYSDEF.
 */

/*
 * For the initial entry, team is set to ALLTEAM
 */

static int tournamentMask(int team, int w_queue)
{
    int i, np[NUMTEAM];
    int largest = 0, nextlargest = 0, il = 0, inl = 0;
    int allteams = queues[w_queue].tournmask;
    int tinydiff;

    /* First, handle any special cases */
    /* Is the server closed, or did the daemond die. */
    if ((!time_access()) || !(status->gameup & GU_GAMEOK)) return 0;
    /* You must leave */
    if (mustexit) return 0;
    /* Special modes */
    if (chaos || topgun) return (allteams);
    /* Only continue if the queue has mask restrictions */
    if (!(queues[w_queue].q_flags&QU_RESTRICT)) return (allteams);

    /*
     * Observers can watch any team in their queue, but may not
     * change teams once they have made their selection
     */

    for (i = 0; i < NUMTEAM; i++) {
        if ((np[i] = realNumShips(1 << i)) > largest) {
            nextlargest = largest;
            inl = il;
            largest = np[i];
            il = 1 << i;
        }
        else if (np[i] > nextlargest) {
            nextlargest = np[i];
            inl = 1 << i;
        }
        if (deadTeam(1 << i) || (np[i] >= 8) || (bot_in_game && np[i] >=  4))
            allteams &= ~ (1 <<i);
    }
    if (nodiag) {
      u_int rem = deadTeam(il) ? inl : il;
      if (((rem <<= 2) & 0xf) == 0)
               rem >>= 4;
      allteams &= ~rem; 
      }
    if((pre_t_mode && inl == 0) || (!pre_t_mode && !status->tourn))
      return allteams;
    /*
     * We think we are in t-mode and go for re-entry.  
     * We know the t-mode teams, we think.
     */

    /*
     * First, make sure observers pick a t-mode team
     */
    if (queues[w_queue].q_flags&QU_OBSERVER){
     if (team == ALLTEAM || ((team & (il | inl)) == 0))  /* initial entry case */
        return (il | inl); 
     else if (deadTeam(team))                            /* must have been on one of those two? */
        return (allteams & ~il & ~inl);                 /* switch, but not to one of those two? */
    }   

    /*
     * Next, make sure players pick a t-mode team that needs players
     */

    tinydiff = (largest - nextlargest) < 2;
    if (team == ALLTEAM || ((team & (il | inl)) == 0))  /* initial entry case */
        return (tinydiff ? allteams & (il | inl) : (inl & allteams ? inl : allteams & ~il));
    else if (deadTeam(team))                            /* must have been on one of those two? */
        return (allteams & ~il & ~inl);                 /* switch, but not to opponent */
    else if (tinydiff || (team == inl))
        return team;                  /* you are on smaller or small enough team, stay there */
    else
        return (team | (allteams & inl ? inl : allteams));       /* you may switch to opponent */
}


static int deadTeam(int owner)
{
    int i,num=0;
    struct planet *p;

    if (!time_access()) return 1; /* 8/2/91 TC */
    if (planets[remap[owner]*10-10].pl_couptime == 0) return(0);
    for (i=0, p=planets; i<MAXPLANETS; i++,p++) {
        if (p->pl_owner & owner) {
            num++;
        }
    }
    if (num!=0) return(0);
    return(1);
}
