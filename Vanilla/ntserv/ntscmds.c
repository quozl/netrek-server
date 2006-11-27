/* $Id: ntscmds.c,v 1.11 2006/05/06 13:12:56 quozl Exp $
 */

/*
   Command interface routines general to ntserv processes.  This
   is a replacement for the overloaded compile method used previously in
   commands.c .  This file contains the defines, and code for performing
   general commands issued to the server.
   It doesn't contain the PUCK/DOG/INL and general command checking
   routines that used to be grouped together in commands.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "gencmds.h"
#include "proto.h"

void do_player_eject(int who, int player, int mflags, int sendto);
void do_player_ban(int who, int player, int mflags, int sendto);

#if defined (AUTO_INL)
void do_start_inl(void);
#endif

#if defined (AUTO_PRACTICE)
void do_start_basep(void);
#endif

#if defined(AUTO_HOCKEY)
void do_start_puck(void);
#endif

#if defined(AUTO_DOGFIGHT)
void do_start_mars(void);
#endif

void do_password(char *comm, struct message *mess);
void do_nodock(char *comm, struct message *mess);
void do_transwarp(char *comm, struct message *mess);
void do_admin(char *comm, struct message *mess);
#ifdef REGISTERED_USERS
void do_register(char *comm, struct message *mess);
#endif

const char myname[] = {"GOD"};

void do_time_msg(char *comm, struct message *mess);
void do_sbtime_msg(char *comm, struct message *mess);
void do_queue_msg(char *comm, struct message *mess);
void do_nowobble(char *comm, struct message *mess);
#ifdef EXPERIMENTAL_BECOME
void do_become(char *comm, struct message *mess);
#endif

#ifdef GENO_COUNT
void do_genos_query(char *comm, struct message *mess, int who);
#endif
void eject_player(int who);
void ban_player(int who);
void do_client_query(char *comm, struct message *mess, int who);
void do_ping_query(char *comm, struct message *mess, int who);
void do_stats_query(char *comm, struct message *mess, int who);
void do_sbstats_query(char *comm, struct message *mess, int who);
void do_whois_query(char *comm, struct message *mess, int who);

/********* COMMANDS LIST ********
	Note: - The commands *must* be in upper case. All commands are
	        converted to upper case before they are checked.
	The first field is the command,
	        second is the command flags,
		the third is a description string (up to 50 chars.),
		fourth is the function to call.
	There may be more options depending on the command flags.
*/

static struct command_handler_2 nts_commands[] =
{
    { "Possible commands are:", C_DESC },
    { "HELP",
	        0,
		"Show this help information",
		(void (*)()) do_help },     			/* HELP */
    { "CLIENT",
		C_PLAYER,
		"Show player's client         e.g. 'CLIENT 0'",
		do_client_query },		/* CLIENT */
    { "PING",
		C_PLAYER,
		"Show player's ping (lag)     e.g. 'PING 0'",
		do_ping_query },		/* PING */
    { "STATS",
		C_PLAYER,
		"Show player's t-mode stats   e.g. 'STATS 0'",
		do_stats_query },		/* STATS */
    { "WHOIS",
		C_PLAYER,
		"Show player's login info     e.g. 'WHOIS 0'",
		do_whois_query },		/* WHOIS */
    { "PASSWORD",
		C_PR_INPICKUP,
		"Change your password         e.g. 'password neato neato'",
		do_password },			/* PASSWORD */
    { "ADMIN",
		C_PR_INPICKUP,
		"Administration commands for privileged users",
		do_admin },			/* ADMIN */
#ifdef REGISTERED_USERS
    { "REGISTER",
		C_PR_INPICKUP,
		"Register your character, e.g. 'register you@example.com'",
		do_register },			/* ADMIN */
#endif
#ifdef NODOCK
    { "DOCK",
      C_PR_INPICKUP,
      "Toggle individual player docking permission. eg. 'DOCK 0 ON|OFF'",
      do_nodock },
#endif

#ifdef NOTRANSWARP
    { "TRANSWARP",
      C_PR_INPICKUP,
      "Set transwarp permission for all players. eg. 'TRANSWARP ON|OFF'",
      do_transwarp },
#endif

#ifdef ALLOW_PAUSE
    { "PAUSE",
                0,
                "Pause the game",
                game_pause },
    { "UNPAUSE",
                0,
                "Resume the game",
                game_resume },
#endif
    { "SBSTATS",
                C_PLAYER,
                "Show player's base stats     e.g. 'SBSTATS 0'",
                do_sbstats_query },             /* SBSTATS */
						/***** Vanilla commands */
    { "QUEUE",
		0,
		"Show how many people are on the queue.",
		do_queue_msg },			/* QUEUE */
    { "(type QUEUE HOSTS to see the addresses of those on queue)", C_DESC },
    { "NOWOBBLE",
		0,
		"Test new wobble on planet lock fix.",
		do_nowobble },			/* NOWOBBLE */
#ifdef EXPERIMENTAL_BECOME
    { "BECOME",
		0,
		"Become a different slot number",
		do_become },			/* BECOME */
#endif
    { "TIME",
		C_PR_INPICKUP,
		"Show time left on surrender timer.",
		do_time_msg },			/* TIME */
	{ "SBTIME",
		C_PLAYER,
		"Show time until the Starbase is available.",
		do_sbtime_msg },		/* SBTIME */
#ifdef GENO_COUNT
    { "GENOS",
		C_PLAYER | C_PR_INPICKUP,
		"Show player's genocides      e.g. 'GENOS 0'",
		do_genos_query },                       /* GENOS */
#endif
    { "The following votes can be used:  (M=Majority, T=Team vote)",
      C_DESC  | C_PR_INPICKUP},
    { "EJECT",
	C_VC_TEAM | C_GLOG | C_PLAYER | C_PR_INPICKUP | C_PR_VOTE,
	"Eject a player               e.g. 'EJECT 0 IDLE'", 
	do_player_eject,				/* EJECT */
	2, PV_EJECT, 120, 300},
    { "BAN",
	C_VC_TEAM | C_GLOG | C_PLAYER | C_PR_INPICKUP | C_PR_VOTE,
	"Eject and ban a player       e.g. 'BAN 0'", 
	do_player_ban,					/* BAN */
	4, PV_BAN, 120, 120},
#if defined(TRIPLE_PLANET_MAYHEM)
    { "TRIPLE",
        C_VC_ALL | C_GLOG | C_PR_INPICKUP | C_PR_VOTE,
        "Start triple planet mayhem by vote",
        do_triple_planet_mayhem,
	2, PV_OTHER, 0},
    { "BALANCE",
        C_VC_ALL | C_GLOG | C_PR_INPICKUP | C_PR_VOTE,
        "Request team randomise & balance",
        do_balance,
        4, PV_OTHER+1, 0 },
#endif
#if defined(AUTO_INL)
  { "INL",
	C_VC_ALL | C_GLOG | C_PR_INPICKUP | C_PR_VOTE,
	"Start game under INL rules.",
	do_start_inl,
	1, PV_OTHER+2, 0 },
#endif
#if defined(AUTO_PRACTICE)
  { "PRACTICE",
	C_VC_ALL | C_PR_INPICKUP | C_PR_VOTE,
	"Start basepractice by majority vote.",
	do_start_basep,
	1, PV_OTHER+3, 0 },
#endif
#if defined(AUTO_HOCKEY)
  { "HOCKEY",
	C_VC_ALL | C_GLOG | C_PR_INPICKUP | C_PR_VOTE,
	"Start hockey by majority vote.",
	do_start_puck,
	1, PV_OTHER+4, 0 },
#endif
#if defined(AUTO_DOGFIGHT)
  { "DOGFIGHT",
	C_VC_ALL | C_GLOG | C_PR_INPICKUP | C_PR_VOTE,
	"Start dogfight tournament by majority vote.",
	do_start_mars,
	1, PV_OTHER+5, 0 },
#endif

    /* crosscheck, last voting array element used (PV_OTHER+n) must
       not exceed PV_TOTAL, see include/defs.h */

    { NULL }
};

int do_check_command(struct message *mess)
{
  return check_2_command(mess, nts_commands,
			 (inl_mode) ? 0 : C_PR_INPICKUP);
}

void do_player_eject(int who, int player, int mflags, int sendto)
{
    register struct player *j;
    char *reason = NULL;

    j = &players[player];

    if (!eject_vote_enable) {
      reason = "Eject voting disabled in server configuration.";
    } else if (j->p_status == PFREE) {
      reason = "You may not eject a free slot.";
    } else if (j->p_flags & PFROBOT) {
      reason = "You may not eject a robot.";
    } else if (j->p_team != players[who].p_team) {
      reason = "You may not eject players of the other team.";
    } else if (eject_vote_only_if_queue && 
	       (queues[QU_PICKUP].q_flags & QU_OPEN) &&
	       (queues[QU_PICKUP].count == 0)){
      reason = "You may not eject if there is no queue.";
    }

    if (reason != NULL) {
      pmessage(players[who].p_team, MTEAM, 
	       addr_mess(players[who].p_team,MTEAM), 
	       reason);
      return;
    }

    pmessage(0, MALL, addr_mess(who,MALL), 
	"%2s has been ejected by their team", j->p_mapchars);

    eject_player(j->p_no);
}

void eject_player(int who)
{
  struct player *j;

  j = &players[who];
  j->p_ship.s_type = STARBASE;
  j->p_whydead=KQUIT;
  j->p_explode=10;
  /* note vicious eject prevents animation of ship explosion */
  j->p_status=PEXPLODE;
  j->p_whodead=me->p_no;
  bay_release(j);

  if (eject_vote_vicious) {
                                      /* Eject AND free the slot. I am sick
                                         of the idiots who login and just
                                         make the game less playable. And
                                         when ejected by the team they log
                                         right back in. Freeing the slot
                                         should help a little. */
    if (j->p_process != 0) {
      if (kill(j->p_process, SIGTERM) < 0) freeslot(j);
    } else {
      freeslot(j);
    }
  }
}

void do_player_ban(int who, int player, int mflags, int sendto)
{
    struct player *j;
    char *reason = NULL;

    j = &players[player];

    if (!ban_vote_enable) {
      reason = "Ban voting disabled in server configuration.";
    } else if (j->p_status == PFREE) {
      reason = "You may not ban a free slot.";
    } else if (j->p_flags & PFROBOT) {
      reason = "You may not ban a robot.";
    } else if (j->p_team != players[who].p_team) {
      reason = "You may not ban players of the other team.";
    }

    if (reason != NULL) {
      pmessage(players[who].p_team, MTEAM, 
	       addr_mess(players[who].p_team,MTEAM), 
	       reason);
      return;
    }

    pmessage(0, MALL, addr_mess(who,MALL), 
	"%2s has been temporarily banned by their team", j->p_mapchars);

    eject_player(j->p_no);
    if (!bans_add_temporary_by_player(j->p_no, " by the players")) {
      pmessage(0, MALL, addr_mess(who,MALL), 
	       " temporary ban list is full, ban ineffective");
    }
}

#if defined(AUTO_PRACTICE)
void do_start_basep(void)
{
  if (practice_mode) {
    pmessage(0, MALL, "GOD->ALL",
        "Basepractice mode already running, vote ignored.");
    return;
  }
  if (vfork() == 0) {
    (void) SIGNAL(SIGALRM,SIG_DFL);
    execl(Basep, "basep", (char *) NULL);
    perror(Basep);
  }
}
#endif

#if defined(AUTO_INL)
void do_start_inl(void)
{
  if (inl_mode) {
    pmessage(0, MALL, "GOD->ALL",
        "INL mode already running, vote ignored.");
    return;
  }
  if (vfork() == 0) {
    (void) SIGNAL(SIGALRM,SIG_DFL);
    execl(Inl, "inl", 0);
    perror(Inl);
  }
}
#endif

#if defined(AUTO_HOCKEY)
void do_start_puck(void)
{
  if (hockey_mode) {
    pmessage(0, MALL, "GOD->ALL",
        "Hockey mode already running, vote ignored.");
    return;
  }
  if (vfork() == 0) {
    (void) SIGNAL(SIGALRM,SIG_DFL);
    execl(Puck, "puck", (char *) NULL);
    perror(Puck);
  }
}
#endif

#if defined(AUTO_DOGFIGHT)
void do_start_mars(void)
{
  if (dogfight_mode) {
    pmessage(0, MALL, "GOD->ALL",
        "Dogfight mode already running, vote ignored.");
    return;
  }
  if (vfork() == 0) {
    (void) SIGNAL(SIGALRM,SIG_DFL);
    execl(Mars, "mars", (char *) NULL);
    perror(Mars);
  }
}
#endif

/*** QUERIES ***/
void send_query(char which, int who, int from)
{
	pmessage2(who, MINDIV, " ", from, "%c", which);
}

/* ARGSUSED */
void do_client_query(char *comm, struct message *mess, int who)
{
#ifdef RSA
	send_query('#', who, mess->m_from); 
#endif
}

/* ARGSUSED */
void do_ping_query(char *comm, struct message *mess, int who)
{
      send_query('!', who, mess->m_from);
}

/* ARGSUSED */
void do_stats_query(char *comm, struct message *mess, int who)
{
      send_query('?', who, mess->m_from); 
}

/* ARGSUSED */
void do_whois_query(char *comm, struct message *mess, int who)
{
      send_query('@', who, mess->m_from); 
}

#ifdef RSA
int bounceRSAClientType(int from)
{
        bounce(from,"Client: %s", RSA_client_type);

        return 1;
}
#endif

int bounceSessionStats(int from)
{
    float                          sessionBombing, sessionPlanets,
                                   sessionOffense, sessionDefense;
    int                            deltaArmies, deltaPlanets,
                                   deltaKills, deltaLosses,
                                   deltaTicks;

#ifdef LTD_STATS

    deltaPlanets = ltd_planets_taken(me, LTD_TOTAL) - startTplanets;
    deltaArmies  = ltd_armies_bombed(me, LTD_TOTAL) - startTarms;
    deltaKills   = ltd_kills(me, LTD_TOTAL)         - startTkills;
    deltaLosses  = ltd_deaths(me, LTD_TOTAL)        - startTlosses;
    deltaTicks   = ltd_ticks(me, LTD_TOTAL)         - startTticks;

#else

    deltaPlanets = me->p_stats.st_tplanets - startTplanets;
    deltaArmies = me->p_stats.st_tarmsbomb - startTarms;
    deltaKills = me->p_stats.st_tkills - startTkills;
    deltaLosses = me->p_stats.st_tlosses - startTlosses;
    deltaTicks = me->p_stats.st_tticks - startTticks;

#endif


    if (deltaTicks == 0) {
        bounce(from,
                "Session stats are only available during t-mode.");
        return 1; /* non t-mode */
    }

    sessionPlanets = (double ) deltaPlanets * status->timeprod /
        ((double) deltaTicks * status->planets);

    sessionBombing = (double) deltaArmies * status->timeprod /
        ((double) deltaTicks * status->armsbomb);

    sessionOffense = (double) deltaKills * status->timeprod /
        ((double) deltaTicks * status->kills);

    sessionDefense = (double) deltaTicks * status->losses /
        (deltaLosses!=0 ? (deltaLosses * status->timeprod) : (status->timeprod));

    bounce(from,
        "%2s stats: %d planets and %d armies. %d wins/%d losses. %5.2f hours.",
        me->p_mapchars,
        deltaPlanets,
        deltaArmies,
        deltaKills,
        deltaLosses,
        (float) deltaTicks/36000.0);
    bounce(from,
        "Ratings: Pla: %5.2f  Bom: %5.2f  Off: %5.2f  Def: %5.2f  Ratio: %4.2f",
        sessionPlanets,
        sessionBombing,
        sessionOffense,
        sessionDefense,
        (float) deltaKills /
        (float) ((deltaLosses == 0) ? 1 : deltaLosses));
    return 1;
}

int bounceSBStats(int from)
{
    float                       sessionRatio, sessionKPH, sessionDPH,
                                overallKPH, overallDPH, overallRatio;
    int                         deltaKills, deltaLosses,
                                deltaTicks;

#ifdef LTD_STATS

    deltaKills   = ltd_kills(me, LTD_SB)  - startSBkills;
    deltaLosses  = ltd_deaths(me, LTD_SB) - startSBlosses;
    deltaTicks   = ltd_ticks(me, LTD_SB)  - startSBticks;
    overallRatio = (float) (ltd_deaths(me, LTD_SB) == 0) ?
      (float) ltd_kills(me, LTD_SB):
      (float) ltd_kills(me, LTD_SB) / (float) ltd_deaths(me, LTD_SB);

#else

    deltaKills = me->p_stats.st_sbkills - startSBkills;
    deltaLosses = me->p_stats.st_sblosses - startSBlosses;
    deltaTicks = me->p_stats.st_sbticks - startSBticks;
    overallRatio = (float) (me->p_stats.st_sblosses==0) ?
      (float) me->p_stats.st_sbkills :
      (float) me->p_stats.st_sbkills / (float) me->p_stats.st_sblosses;

#endif /* LTD_STATS */

/*  if (deltaTicks == 0) {
        bounce(from,
        "No SB time yet this session.");
        return 0;
    } */

    if (deltaTicks != 0) {
        sessionKPH = (float) deltaKills * 36000.0 / (float) deltaTicks;
        sessionDPH = (float) deltaLosses * 36000.0 / (float) deltaTicks;
    } else {
        sessionKPH = sessionDPH = 0.0;
    }

    sessionRatio = (float) (deltaLosses==0) ? (float) deltaKills :
        (float) deltaKills / (float) deltaLosses;

#ifdef LTD_STATS

    if (ltd_ticks(me, LTD_SB) != 0) {
        overallKPH = (float) ltd_kills(me, LTD_SB) * 36000.0 /
                (float) ltd_ticks(me, LTD_SB);
        overallDPH = (float) ltd_deaths(me, LTD_SB) * 36000.0 /
                (float) ltd_ticks(me, LTD_SB);
#else

    if (me->p_stats.st_sbticks != 0) {
        overallKPH = (float) me->p_stats.st_sbkills * 36000.0 /
                (float) me->p_stats.st_sbticks;
        overallDPH = (float) me->p_stats.st_sblosses * 36000.0 /
                (float) me->p_stats.st_sbticks;

#endif /* LTD_STATS */
    }
    else {
        overallKPH = overallDPH = 0.0;
    }

    bounce(from,
      "%2s overall SB stats: %d wins/%d losses. %5.2f hours. Ratio: %5.2f",
      me->p_mapchars,

#ifdef LTD_STATS
      ltd_kills(me, LTD_SB),
      ltd_deaths(me, LTD_SB),
      (float) ltd_ticks(me, LTD_SB) / 36000.0,

#else
      me->p_stats.st_sbkills,
      me->p_stats.st_sblosses,
      (float) me->p_stats.st_sbticks/36000.0,
#endif /* LTD_STATS */
      overallRatio);

    if (deltaTicks)
        bounce(from,
          "%2s session SB stats: %d wins/%d losses. %5.2f hours. Ratio: %5.2f",
          me->p_mapchars,
          deltaKills,
          deltaLosses,
          (float) deltaTicks/36000.0,
          sessionRatio);
        bounce(from,
          "Kills/Hour: %5.2f (%5.2f total), Deaths/Hour: %4.2f (%4.2f total)",
          sessionKPH,
          overallKPH,
          sessionDPH,
          overallDPH);
    return 1;
}


#ifdef PING
int bouncePingStats(int from)
{
    if(me->p_avrt == -1){
        /* client doesn't support it or server not pinging */
        bounce(from,"No PING stats available for %c%c",
           me->p_mapchars[0], me->p_mapchars[1]);
    }
    else{
        bounce(from,
            "%c%c PING stats: Average: %d ms, Stdv: %d ms, Loss: %d%%/%d%% s->c/c->s",
            me->p_mapchars[0], me->p_mapchars[1],
            me->p_avrt,
            me->p_stdv,
            me->p_pkls_s_c,
            me->p_pkls_c_s);
    }
    return 1;
}
#endif

int bounceWhois(int from)
{
  if (whitelisted) {
    bounce(from, "%s is %s (%s)", me->p_mapchars, me->p_name, me->p_login);
  } else {
    bounce(from, "%s is %s (%s@%s)", me->p_mapchars, me->p_name, me->p_login, me->p_full_hostname);
    bounce(from, "%s at %s (ip)", me->p_mapchars, me->p_ip);
  }
  return 1;
}


/* ARGSUSED */
void do_sbstats_query(char *comm, struct message *mess, int who)
{
      send_query('^', who, mess->m_from);	
}

#ifdef GENO_COUNT
void do_genos_query(char *comm, struct message *mess, int who)
{
  char *addr;

  addr = addr_mess(mess->m_from,MINDIV);
  pmessage(mess->m_from, MINDIV, addr, "%s has won the game %d times.",
	players[who].p_name,players[who].p_stats.st_genos);
}
#endif


/* ARGSUSED */
void do_time_msg(char *comm, struct message *mess)
{
  int who;
  int t;
  char *addr;

  who = mess->m_from;
  addr = addr_mess(who,MINDIV);

  for (t=0;((t<=MAXTEAM)&&(teams[t].s_surrender==0));t++);

  if (t>MAXTEAM) {
    pmessage(who, MINDIV, addr, "No one is considering surrender now.  Go take some planets.");
  } else {
    pmessage(who, MINDIV, addr, "The %s have %d minutes left before they surrender.", team_name(t), teams[t].s_surrender);
  }
}

/* ARGSUSED */
void do_sbtime_msg(char *comm, struct message *mess)
{
  if (teams[players[mess->m_from].p_team].s_turns > 0)
	pmessage(mess->m_from, MINDIV, addr_mess(mess->m_from, MINDIV), "Starbase construction will be complete in %d minutes.", teams[players[mess->m_from].p_team].s_turns);
  else
	pmessage(mess->m_from, MINDIV, addr_mess(mess->m_from, MINDIV), "Your Starbase is available.");
}

/*
 * Give information about the waitqueue status
 */

/* ARGSUSED */
void do_queue_msg(char *comm, struct message *mess)
{
    int who;
    char *addr;
    int i;
    int full;

    who = mess->m_from;
    addr = addr_mess(who,MINDIV);

    full = 0;
    if (strstr(comm, "hosts")) full++;
    if (strstr(comm, "HOSTS")) full++;

    for (i=0; i < MAXQUEUE; i++) {
        int count;

        /* Only report open queues that have the report flag set. */
        if (!(queues[i].q_flags & QU_OPEN))   continue;
	if (!(queues[i].q_flags & QU_REPORT)) continue;
      
	count = queues[i].count;
	if (count > 0) {
  	    pmessage(who, MINDIV, addr, "Approximate %s queue size: %d", 
		     queues[i].q_name, count);

	    /* if player typed "queue hosts", dump out the host names */
	    if (full) {
	        int k = queues[i].first;
		int j;
		
		for (j=0; j < count; j++) {
#ifdef NO_HOSTNAMES
		  char *them = "hidden";
#else
		  char *them = waiting[k].host;
#endif
                    int m = find_slot_by_host(waiting[k].host, 0);
		    if (m != -1) {
		        struct player *p = &players[m];
		        pmessage(who, MINDIV, addr, "Q%d: (aka %s %s) %s",
				 j, p->p_mapchars, p->p_name, them);
		    } else {
		        pmessage(who, MINDIV, addr, "Q%d: %s",
				 j, them);
		    }
		    k = waiting[k].next;
		}
	    }
	} else {
	    pmessage(who, MINDIV, addr, "There is no one on the %s queue.",
		     queues[i].q_name);
	}
    }
}

/* test wobble lock code */
int nowobble;
void do_nowobble(char *comm, struct message *mess)
{
    int who;
    char *addr;

    who = mess->m_from;
    addr = addr_mess(who,MINDIV);

    nowobble = atoi(comm+strlen("nowobble "));

    pmessage(who, MINDIV, addr, "No wobble fix is now %s [%d] {%s}", 
	     nowobble ? "on (new test mode)" : "off (classic mode)", nowobble, comm );
}

#ifdef EXPERIMENTAL_BECOME
/* become another slot ... lacking client support, in progress */
void do_become(char *comm, struct message *mess)
{
    int who, pno;
    char *addr;

    who = mess->m_from;
    addr = addr_mess(who,MINDIV);

    pno = atoi(comm+strlen("become "));

    if (pno < 0) return;
    if (pno > (MAXPLAYER-1)) return;

    if (me->p_no == pno) {
      pmessage(who, MINDIV, addr, "you are this slot already");
      return;
    }

    struct player *be = &players[pno];
    if (be->p_status != PFREE) {
      pmessage(who, MINDIV, addr, "slot is not free");
      return;
    }

    memcpy(be, me, sizeof(struct player));
    me->p_status = PFREE;
    me = be;
    me->p_no = pno;
    sprintf(me->p_mapchars,"%c%c",teamlet[me->p_team], shipnos[me->p_no]);
    sprintf(me->p_longname, "%s (%s)", me->p_name, me->p_mapchars);
    updateSelf(1);
}
#endif

void do_password(char *comm, struct message *mess)
{
  int who = mess->m_from;
  struct player *p = &players[who];
  char *addr = addr_mess(who,MINDIV);
  char *one, *two;

  /* guests have no player file position */
  if (p->p_pos < 0) {
    pmessage(who, MINDIV, addr, 
	     "You can't change your password, sorry!");
    return;
  }

  /* consume the command */
  one = strtok(comm, " ");
  if (one == NULL) return;

  /* look at the first word after command */
  one = strtok(NULL, " ");
  if (one == NULL) {
    pmessage(who, MINDIV, addr, 
	     "Enter your new password twice after the word PASSWORD");
    return;
  }

  /* look at the second word after command */
  two = strtok(NULL, " ");
  if (two == NULL) {
    pmessage(who, MINDIV, addr, 
	     "Need new password twice, with a space between");
    return;
  }

  /* compare and reject if different */
  if (strcmp(one, two)) {
    pmessage(who, MINDIV, addr, 
	     "No way, the two passwords are different!");
    return;
  }

  /* change the password */
  changepassword(one);

  /* tell her we changed it */
  pmessage(who, MINDIV, addr, 
	   "Password changed to %s", one);
}


void do_nodock(char *comm, struct message *mess)
{
  int whofrom = mess->m_from;
  struct player *p = &players[whofrom];
  struct player *victim;
  char *addr = addr_mess(whofrom,MINDIV);
  char *who, *what;
  int slot;
  
  if (p->p_ship.s_type != STARBASE)
    {
      pmessage(whofrom, MINDIV, addr, "dock: must be a starbase to use nodock");
      return;
    }
  
#ifdef OBSERVERS
  if (p->p_status == POBSERV)
    {
      pmessage(whofrom, MINDIV, addr, "dock: Observers cannot change dock permissions");
      return;
    }
#endif

  who = strtok(comm, " ");
  if ( who == NULL)
    return;

  who = strtok(NULL, " ");
  if(who == NULL)
    {
      pmessage(whofrom, MINDIV, addr, "dock usage: 'DOCK 0 ON|OFF'");
      return;
    }

  what = strtok(NULL, " ");
  if(what == NULL)
    {
      pmessage(whofrom, MINDIV, addr, "dock usage: 'DOCK 0 ON|OFF'");
      return;
    }

  *who = toupper(*who);
  if( (*who >= '0') && (*who <= '9') )
    slot = *who - '0';
  else if ( (*who >= 'A') && (*who <= ('A' + MAXPLAYER - 10) ) )
    slot = 10 + *who - 'A';
  else 
    {
      pmessage(whofrom, MINDIV, addr, "dock: unrecognized slot");
      return;
    }
  victim = &players[slot];

  if (victim->p_status == PFREE) 
    {
      pmessage(whofrom, MINDIV, addr, "dock: ignored, slot is free");
      return;
    }

  if( !strcmp("off", what) )
    {
      victim->p_candock = 0;
      pmessage(whofrom, MINDIV, addr, "Slot %c no longer allowed to dock to SB", *who);
    }
  else if ( !strcmp("on", what) )
         {
           victim->p_candock = 1;
           pmessage(whofrom, MINDIV, addr, "Slot %c is allowed to dock to SB", *who);
         }
       else pmessage(whofrom, MINDIV, addr, "dock usage: dock 0 on|off");
}

void do_transwarp(char *comm, struct message *mess)
{
  int whofrom = mess->m_from;
  struct player *p = &players[whofrom];
  char *addr = addr_mess(whofrom,MINDIV);
  char *what;
  char *usage = "transwarp usage: 'TRANSWARP ON|GREEN|YELLOW|SHIELD|OFF'";
  
  if (p->p_ship.s_type != STARBASE) {
    pmessage(whofrom, MINDIV, addr, "transwarp: must be a starbase to use this");
    return;
  }
  
#ifdef OBSERVERS
  if (p->p_status == POBSERV) {
    pmessage(whofrom, MINDIV, addr, "transwarp: observers may not do this");
    return;
  }
#endif

  what = strtok(comm, " ");
  if (what == NULL) return;

  what = strtok(NULL, " ");
  if (what == NULL) {
    pmessage(whofrom, MINDIV, addr, usage);
    return;
  }

  if (!strcmp("off", what) ) {
    me->p_transwarp = 0;
    pmessage(me->p_team, MTEAM, 
	     addr_mess(me->p_team,MTEAM), 
	     "Starbase %s refusing transwarp", me->p_mapchars);
  } else if (!strcmp("green", what)) {
    me->p_transwarp = PFGREEN;
    pmessage(me->p_team, MTEAM, 
	     addr_mess(me->p_team,MTEAM), 
	     "Starbase %s refusing transwarp in red or yellow alert", me->p_mapchars);
  } else if (!strcmp("yellow", what)) {
    me->p_transwarp = PFYELLOW|PFGREEN;
    pmessage(me->p_team, MTEAM, 
	     addr_mess(me->p_team,MTEAM), 
	     "Starbase %s refusing transwarp in red alert", me->p_mapchars);
  } else if (!strcmp("shield", what)) {
    me->p_transwarp = PFSHIELD;
    pmessage(me->p_team, MTEAM, 
	     addr_mess(me->p_team,MTEAM), 
	     "Starbase %s refusing transwarp while shields up", me->p_mapchars);
  } else if (!strcmp("on", what)) {
    me->p_transwarp = PFGREEN|PFYELLOW|PFRED;
    pmessage(me->p_team, MTEAM, 
	     addr_mess(me->p_team,MTEAM), 
	     "Starbase %s transwarp restored", me->p_mapchars);
  }
  else pmessage(whofrom, MINDIV, addr, usage);
}

static int authorised = 0;

void do_admin(char *comm, struct message *mess)
{
  int who = mess->m_from;
  struct player *p = &players[who];
  char *addr = addr_mess(who,MINDIV);
  char *one, *two;
  char command[256];
  int slot;
  struct player *them = NULL;

  if (!authorised) {
    if (strstr(comm, "ADMIN password")) {
      char *cchar = &comm[15];
      if (!strcmp(cchar, admin_password)) {
        if (!strcmp(cchar, "password")) {
          pmessage(who, MINDIV, addr, "admin: denied, password is not yet set");
          ERROR(2,("%s admin: authorisation ignored, ip=%s\n",
                   p->p_mapchars, p->p_ip));
          return;
        }
        authorised = 1;
        pmessage(who, MINDIV, addr, "admin: authorised");
        ERROR(2,("%s admin: authorised, ip=%s\n", p->p_mapchars, p->p_ip));
        return;
      } else {
        ERROR(2,("%s admin: authorisation failure, ip=%s\n",
                 p->p_mapchars, p->p_ip));
        pmessage(who, MINDIV, addr, "admin: denied");
        sleep(2);
      }
    }
  }

  if (!authorised) {
    if (p->w_queue != QU_GOD_OBS && p->w_queue != QU_GOD) {
      pmessage(who, MINDIV, addr, "Sorry, no.", p->w_queue);
      ERROR(2,("%s admin: not yet authorised, ip=%s\n", p->p_mapchars, p->p_ip));
      return;
    }
  }

  /* admin quit n - simulate quit */
  /* admin kill n - blow them up */
  /* admin ban n - ban the host */
  /* admin reset - reset galactic */
  
  /* admin */
  one = strtok(comm, " ");
  if (one == NULL) return;

  /* command */
  one = strtok(NULL, " ");
  if (one == NULL) {
    pmessage(who, MINDIV, addr, "admin: expected command, kill/quit/ban/reset");
    return;
  }

  /* argument */
  two = strtok(NULL, " ");
  if (two != NULL) {
    *two = toupper(*two);
    if ((*two >= '0') && (*two <='9'))
      slot = *two - '0';
    else if ((*two >= 'A') && (*two <= ('A' + MAXPLAYER - 10)))
      slot = *two - 'A' + 10;
    else {
      pmessage(who, MINDIV, addr, "admin: ignored, slot not recognised");
      return;
    }
    them = &players[slot];
    if (them->p_status == PFREE) {
      pmessage(who, MINDIV, addr, "admin: ignored, slot is free");
      return;
    }
  }

  if (!strcmp(one, "mute")) {
    pmessage(who, MINDIV, addr, "admin: erm, send 'em a \"mute on\"");
  } else if (!strcmp(one, "quit")) {
    if (them == NULL) return;
    sprintf(command, "../tools/admin/quit %s %c", p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s forced to quit.", two);
  } else if (!strcmp(one, "kill")) {
    if (them == NULL) return;
    sprintf(command, "../tools/admin/kill %s %c", p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s killed.", two);
  } else if (!strcmp(one, "free")) {
    if (them == NULL) return;
    sprintf(command, "../tools/admin/free %s %c", p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s free-ed.", two);
  } else if (!strcmp(one, "ban")) {
    if (them == NULL) return;
    sprintf(command, "../tools/admin/ban %s %s", p->p_full_hostname, them->p_full_hostname);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s banned.", two);
  } else if (!strcmp(one, "reset")) {
    sprintf(command, "../tools/admin/reset %s", p->p_full_hostname);
    system(command);
    pmessage(who, MINDIV, addr, "admin: galactic has been reset.");
  } else {
    pmessage(who, MINDIV, addr, "admin: what? kill/quit/ban/free/reset, lowercase");
  }
}

#ifdef REGISTERED_USERS
void do_register(char *comm, struct message *mess)
{
  int who = mess->m_from;
  struct player *p = &players[who];
  char *addr = addr_mess(who,MINDIV);
  extern char *registered_users_name, *registered_users_pass;
  char *one, *two, filename[64], command[128];
  FILE *file;
  int status;

  if (strchr(registered_users_name, '\t') != NULL) {
    pmessage(who, MINDIV, addr, "Registration refused for character names containing tabs.");
    return;
  }

  if (strchr(registered_users_pass, '\t') != NULL) {
    pmessage(who, MINDIV, addr, "Registration refused for passwords containing tabs.");
    return;
  }

  if (strchr(host, '\t') != NULL) {
    pmessage(who, MINDIV, addr, "Registration refused for hosts containing tabs.");
    return;
  }

  /* register */
  one = strtok(comm, " ");
  if (one == NULL) return;

  /* address */
  two = strtok(NULL, " ");
  if (two == NULL) {
    pmessage(who, MINDIV, addr, "Try again, type your e-mail address after the word REGISTER");
    return;
  }

  if (strchr(two, '@') == NULL) {
    pmessage(who, MINDIV, addr, "That doesn't look like an e-mail address!");
    return;
  }

  if (strchr(two, '\t') != NULL) {
    pmessage(who, MINDIV, addr, "Registration refused for e-mail addresses containing a tab.");
    return;
  }

  sprintf(filename, "user-requests/%d", p->p_no);
  file = fopen(filename, "a");
  if (file == NULL) {
    pmessage(who, MINDIV, addr, "Registration refused.");
    return;
  }

  fprintf(file, "%s\t%s\t%s\t%s\t%s\n",
	  (!strcmp(p->p_name, "guest")) ? "add" : "change", 
	  host, registered_users_name, registered_users_pass, two);
  if (fclose(file) != 0) {
    pmessage(who, MINDIV, addr, "Registration could not be recorded.");
    return;
  }

  sprintf(command, "tools/register %s", filename);
  status = system(command);
  if (status != 0) {
    pmessage(who, MINDIV, addr, "Registration could not be processed.");
    return;
  }

  pmessage(who, MINDIV, addr, "Registration recorded, expect an e-mail.");
}
#endif
