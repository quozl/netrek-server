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
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "genspkt.h"
#include "util.h"
#include "ltd_stats.h"

/* file scope prototypes */
static int deadTeam(int owner);
static int tournamentMask(int team, int w_queue);

int tips_enabled = 1;

static char *tip_select() {
  char *tip = NULL;

  /* a default message of encouragement */
  if (F_tips) {
    tip = "";
  } else {
    tip = "Well done, now come back and try some more Netrek!";
  }

  // FIXME: translate keys using user keymap

  // FIXME: if explosion did no damage, suggest that the explosion be
  // positioned more carefully.

  if (has_repaired == 0) {
    tip = "Learn how to repair your ship.\n \n"
      "You died without ever trying out the repair services of the\n"
      "engineering deck.  The scotsman is unimpressed.\n \n"
      "When you ask for repair, your shields will be down and you won't\n"
      "be able to fire.  Raise your shields to cancel the repair work.\n \n"
      "Keys: R (repair)";
  }

  if (has_beamed_down == 0 && me->p_armies > 0) {
    tip = "Learn how to keep armies alive.\n \n"
      "You died with armies but without ever beaming them down.\n"
      "The armies are dead, and you lost your kills.\n \n"
      "Let your team know that you are carrying, and they may be able to\n"
      "protect you.  This depends on how good you seem to them.  Battle\n"
      "other ships with caution when you have armies on board!\n \n"
      "Keys: F (announce carrying armies) Control-T (taking to a planet)";
  }

  if (has_beamed_up == 0 && me->p_kills > 2) {
    // FIXME: only if kept kills for a reasonable time
    tip = "Learn how to carry.\n \n"
      "You died with kills but without ever beaming up armies.\n"
      "Your kills are lost, but you can always get more kills.\n \n"
      "Once you feel you can get to an enemy planet either unchallenged or\n"
      "with an escort, and especially once you have the confidence of your\n"
      "team, it is time to learn how to make use of the kills to take planets.\n \n"
      "Get a kill, fly to one of your planets with spare armies, beam them up,\n"
      "take them to an enemy planet, and beam them down.\n \n"
      "Keys: z (beam up) and x (beam down)";
  }

  if (has_bombed == 0) {
    // FIXME: only if the enemy has planets that need bombing
    tip = "Learn how to bomb.\n \n"
      "Bombing enemy planets helps your team, by preparing the planets for a take,\n"
      "and by denying spare armies to the enemy, preventing them from taking\n"
      "your team's planets.\n \n"
      "Head for the nearest enemy planet with spare armies,\n"
      "orbit it, then bomb until the spare armies are gone.\n"
      "Then fly to another planet and repeat.\n \n"
      "Keys: b (bomb)";
      ;
  }

  if (has_shield_down == 0) {
    tip = "Fuel efficiency.\n \n"
      "You have not yet learned how to lower your shields.\n \n"
      "Your shields are the ring around your ship, and they protect you\n"
      "from attack until they are damaged.\n \n"
      "Flying with your shields up costs more fuel than having them down,\n"
      "but remember to put them up when you are under attack.\n \n"
      "Keys: s u\n";
  }

  /* detect death with shields down and capacity remaining */
  if (!(me->p_flags & PFSHIELD) &&
      me->p_shield > 0 &&
      (me->p_whydead == KTORP ||
       me->p_whydead == KPHASER ||
       me->p_whydead == KPLASMA ||
       me->p_whydead == KSHIP ||
       me->p_whydead == KTORP2 ||
       me->p_whydead == KSHIP2)) {
    if (has_shield_up == 0) {
      tip = "You forgot to raise your shields after you put them down.\n \n"
        "Keys: s u\n";
    } else {
      tip = "You died with your shields down,\n"
        "but the shields still had some power left in them.\n \n"
        "Try to remember to raise your shields.";
    }
  }

  /* detect non-moving death */
  if (me->p_speed == 0 && me->p_whydead == KTORP) {
	// FIXME: Don't issue this message if the torps were detted
    tip = "You died to an enemy torpedo while not moving,\n"
      "so you had no way to dodge.\n \n"
      "Next time, always keep moving when an enemy is nearby,\n \n"
      "Use the number keys to set your speed.\n"
      "Use the right-hand mouse button to turn.\n \n"
      "Keys: 0 1 2 3 4 5 6 7 8 9 ! %\n"
      "Keys: k\n";
  }

  if (has_set_course == 0 && has_set_speed > 0) {
    tip = "Good, you've learned how to move, but you have to learn to turn.\n \n"
      "Use the right-hand mouse button to turn.\n"
      "Especially turn when you are under fire, to dodge torpedos.\n";
  }

  if (has_set_speed == 0) {
    tip = "You haven't moved yet.  You were a sitting duck.\n \n"
      "Use the number keys to set your speed.\n"
      "Use the right-hand mouse button to turn.\n";
  }

  return tip;
}

static void tips() {
  static int count = 0;
  char *tip = NULL;

  /* first time in, */
  count++;
  if (count == 1) {
    /* show no tip this once, let them read the message of the day */
    return;
  } else if (count == 2) {
    /* do not show message based tips unless they ask after first death */
    if (!F_tips) tips_enabled = 0;
  }

  /* do not proceed if user has toggled tips off */
  if (!tips_enabled) return;

  /* select the most pertinent tip */
  tip = tip_select();
  if (strlen(tip) == 0) return;

  /* issue the tip to the client */
  {
    char *line;
    char *tip_c;

    if (F_tips) {
#define MOTDCLEARLINE  "\033\030CLEAR_MOTD\000"
      sendMotdLine(MOTDCLEARLINE);
    } else {
      if (!F_tips) pmessage(me->p_no, MINDIV, "", "-- -- -- -- -- --");
    }

    tip_c = strdup(tip);
    line = strtok(tip_c, "\n");
    while (line != NULL) {
      int len = strlen(line);
      if (len == 0) continue;
      len--;
      if (line[len] == '\n') line[len] = '\0';
      if (F_tips) {
        sendMotdLine(line);
      } else {
        if (strlen(line) > 1)
          pmessage(me->p_no, MINDIV, "", line);
      }
      line = strtok(NULL, "\n");
    }
    free(tip_c);
    if (!F_tips) pmessage(me->p_no, MINDIV, "", "-- -- -- -- -- --");
  }
}

void getEntry(int *team, int *stype)
{
    int i;
    int switching = -1;		/* confirm switches 7/27/91 TC */
    float playerOffense;

#ifdef LTD_STATS
    playerOffense = ltd_offense_rating(me);
    sendLtdPacket();
#else
    playerOffense = offenseRating(me);
#endif
    tips();

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
	if (me->p_disconnect) exitGame(me->p_disconnect);
	readFromClient();
	if (isClientDead())
	    exitGame(0);
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
            if (shipPick==ASSAULT) {
                if ((!inl_mode) && (!practice_mode)) {
                    if (playerOffense < as_minimal_offense) {
                        new_warning(UNDEF,"You need an offense of %2.1f or higher to command an assault ship!", as_minimal_offense);
                        sendPickokPacket(0);
                        teamPick= -1;
                        continue;
                    }
                }
            }
            if (shipPick==BATTLESHIP) {
                if ((!inl_mode) && (!practice_mode)) {
                    if (playerOffense < bb_minimal_offense) {
                        new_warning(UNDEF,"You need an offense of %2.1f or higher to command a battleship!", bb_minimal_offense);
                        sendPickokPacket(0);
                        teamPick= -1;
                        continue;
                    }
                }
            }
            if (shipPick==DESTROYER) {
                if (mystats->st_rank < ddrank) {
                    new_warning(UNDEF,"You need a rank of %s or higher to command a destroyer!", ranks[ddrank].name);
                    sendPickokPacket(0);
                    teamPick= -1;
                    continue;
                }
                if ((!inl_mode) && (!practice_mode)) {
                    if (playerOffense < dd_minimal_offense) {
                        new_warning(UNDEF,"You need an offense of %2.1f or higher to command a destroyer!", dd_minimal_offense);
                        sendPickokPacket(0);
                        teamPick= -1;
                        continue;
                    }
                }
            }
            if (shipPick==SCOUT) {
                if ((!inl_mode) && (!practice_mode)) {
                    if (playerOffense < sc_minimal_offense) {
                        new_warning(UNDEF,"You need an offense of %2.1f or higher to command a scout!", sc_minimal_offense);
                        sendPickokPacket(0);
                        teamPick= -1;
                        continue;
                    }
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
	        if (teams[1<<teamPick].te_turns > 0 && !chaos && !topgun) {
	            new_warning(UNDEF,"Not constructed yet. %d minute%s required for completion",teams[1<<teamPick].te_turns, (teams[1<<teamPick].te_turns == 1) ? "" : "s");
	            sendPickokPacket(0);
	            teamPick= -1;
	            continue;
	        }
	        if (mystats->st_rank < sbrank) {
                    if(send_short){
                        swarning(SBRANK_TEXT,sbrank,0);
                    } else {
                        new_warning(UNDEF,"You need a rank of %s or higher to command a starbase!", ranks[sbrank].name);
                    }

		    sendPickokPacket(0);
		    teamPick= -1;
		    continue;
	        }
                if ((!inl_mode) && (!practice_mode) && (!pre_t_mode)) {
                    if (playerOffense < sb_minimal_offense) {
                        new_warning(UNDEF,"You need an offense of %2.1f or higher to command a starbase!", sb_minimal_offense);
                        sendPickokPacket(0);
                        teamPick= -1;
                        continue;
                    }
		}
		if (realNumShips(1<<teamPick) < 3 && !chaos && !topgun) {
                    if(send_short){
                        swarning(TEXTE,14,0);
                    } else
		        new_warning(UNDEF,"Your team is not capable of defending such an expensive ship!");
		    sendPickokPacket(0);
		    teamPick= -1;
		    continue;
		}
		if (numPlanets(1<<teamPick) < sbplanets && !topgun) {
        	    if(send_short){
                        swarning(TEXTE,15,0);
		    } else
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
                            } else
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
    int origmask = queues[queue].tournmask;
    int mask = origmask;
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
    /* admin observer */
    if (me->p_authorised && (me->p_flags & PFOBSERV))
        return ALLTEAM;
    /* df_galaxy set - permit all teams */
    if (recreational_dogfight_mode)
        return ALLTEAM;
    /* Chaos or topgun mode */
    if (chaos || topgun)
        return origmask;
    /* Queue with no restrictions */
    if (!(queues[queue].q_flags & QU_RESTRICT))
        return origmask;

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
        mask = origmask & ~team;
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
