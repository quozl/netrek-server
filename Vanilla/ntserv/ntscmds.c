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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <dirent.h>

#include "defs.h"
#include "struct.h"
#include "data.h"

#include "proto.h"
#include "gencmds.h"
#include "util.h"
#include "ip.h"
#include "slotmaint.h"
#include "packets.h"
#include "genspkt.h"

/* Enable to allow BE support to move players between slots; disabled by
   default as the code is experimental and may cause problems with certain
   clients */
#undef EXPERIMENTAL_BE

void do_player_eject(int who, int player, int mflags, int sendto);
void do_player_ban(int who, int player, int mflags, int sendto);
void do_player_nopick(int who, int player, int mflags, int sendto);

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
void do_tips(char *comm, struct message *mess);
#ifdef EXPERIMENTAL_BE
void do_be(char *comm, struct message *mess);
void do_sub(char *comm, struct message *mess);
#endif

#ifdef GENO_COUNT
void do_genos_query(char *comm, struct message *mess, int who);
#endif
void do_display_ignoring(char *comm, struct message *mess, int who);
void do_display_ignoredby(char *comm, struct message *mess, int who);
void do_display_ignores(char *comm, struct message *mess, int who, int type);
void eject_player(int who, int why);
void ban_player(int who);
void do_client_query(char *comm, struct message *mess, int who);
void do_ping_query(char *comm, struct message *mess, int who);
void do_stats_query(char *comm, struct message *mess, int who);
#ifdef LTD_STATS
void do_ltd_query(char *comm, struct message *mess, int who);
#endif
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
#ifdef LTD_STATS
    { "LTD",
		C_PLAYER,
		"Show your LTD stats via a web page",
		do_ltd_query },			/* LTD */
#endif
    { "WHOIS",
		C_PLAYER,
		"Show player's login info     e.g. 'WHOIS 0'",
		do_whois_query },		/* WHOIS */
    { "PASSWORD",
		C_PR_INPICKUP,
		"Change your password         e.g. 'password neato neato'",
		do_password },			/* PASSWORD */
    { "ADMIN",
		0,
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
    { "TIPS",
		0,
		"Test new tips feature, may require client support.",
		do_tips },			/* TIPS */
#ifdef EXPERIMENTAL_BE
    { "BE",
		0,
		"Switch between observer, player, or a different slot.",
		do_be },			/* BE */
    { "SUB",
		0,
		"Sub in or out, between observer and player.",
		do_sub },			/* SUB */
#endif
    { "TIME",
		C_PR_INPICKUP,
		"Show time left on surrender timer.",
		do_time_msg },			/* TIME */
	{ "SBTIME",
		C_PLAYER,
		"Show time until the Starbase is available.",
		do_sbtime_msg },		/* SBTIME */
	{ "IGNORING",
		C_PLAYER,
		"Display list of ip's you are ignoring.",
		do_display_ignoring },		/* IGNORING */
	{ "IGNOREDBY",
		C_PLAYER,
		"Display list of ip's that are ignoring you.",
		do_display_ignoredby },		/* IGNOREDBY */
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
    { "NOPICK",
	C_VC_TEAM | C_GLOG | C_PLAYER | C_PR_INPICKUP | C_PR_VOTE,
	"Prevent player from picking, e.g. 'NOPICK 0'", 
	do_player_nopick,				/* NOPICK */
	4, PV_NOPICK, 120, 120},
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

    eject_player(j->p_no, BADVERSION_NOSLOT);
}

void eject_player(int who, int why)
{
  struct player *j;

  j = &players[who];
  j->p_ship.s_type = STARBASE;
  j->p_whydead=KQUIT;
  j->p_explode=10;
  j->p_status=PEXPLODE;
  j->p_whodead=me->p_no;
  bay_release(j);

  if (eject_vote_vicious) {
    if (j->p_process != 0) {
      j->p_disconnect = why;
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

    eject_player(j->p_no, BADVERSION_BANNED);
    if (!bans_add_temporary_by_player(j->p_no, " by the players")) {
      pmessage(0, MALL, addr_mess(who,MALL), 
	       " temporary ban list is full, ban ineffective");
    }
}

void do_player_nopick(int who, int player, int mflags, int sendto)
{
    register struct player *j;
    char *reason = NULL;

    j = &players[player];

    if (!nopick_vote_enable) {
      reason = "Nopick voting disabled in server configuration.";
    } else if (j->p_status == PFREE) {
      reason = "You may not vote for a free slot.";
    } else if (j->p_flags & PFROBOT) {
      reason = "You may not prevent a robot from picking.";
    } else if (j->p_team != players[who].p_team) {
      reason = "You may not vote for players of the other team.";
    }

    if (reason != NULL) {
      pmessage(players[who].p_team, MTEAM,
               addr_mess(players[who].p_team, MTEAM),
               reason);
      return;
    }

    j->p_no_pick = 1;

    pmessage(me->p_team, MTEAM, addr_mess(me->p_team, MTEAM),
             "%2s is no longer able to pick up armies", j->p_mapchars);
    return;
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
    (void) signal(SIGALRM,SIG_DFL);
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
    (void) signal(SIGALRM,SIG_DFL);
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
    (void) signal(SIGALRM,SIG_DFL);
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
    (void) signal(SIGALRM,SIG_DFL);
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
  char *addr;

  addr = addr_mess(mess->m_from,MINDIV);
  pmessage(mess->m_from, MINDIV, addr, "%s using %s",
           players[who].p_mapchars, players[who].p_ident);
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
        godf(from, "Client: %s", RSA_client_type);

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
    char                           bufPlanets[8], bufBombing[8],
                                   bufOffense[8], bufDefense[8];

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
        god(from, "Session stats are only available during t-mode.");
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

    godf(from,
         "%2s stats: %d planets and %d armies. %d wins/%d losses. %5.2f hours.",
         me->p_mapchars,
         deltaPlanets,
         deltaArmies,
         deltaKills,
         deltaLosses,
         (float) deltaTicks/36000.0);
    ftoa(sessionPlanets, bufPlanets, 0, 2, 2);
    ftoa(sessionBombing, bufBombing, 0, 2, 2);
    ftoa(sessionOffense, bufOffense, 0, 2, 2);
    ftoa(sessionDefense, bufDefense, 0, 2, 2);
    godf(from,
         "Ratings: Pla: %s  Bom: %s  Off: %s  Def: %s  Ratio: %4.2f",
         bufPlanets,
         bufBombing,
         bufOffense,
         bufDefense,
         (float) deltaKills /
         (float) ((deltaLosses == 0) ? 1 : deltaLosses));
#ifndef CARRY_STATUS_SHOW_ALL
    if (players[from].p_team == me->p_team)
#endif
        if (me->p_no_pick)
            god(from, "Carry status: Prohibited");
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
        god(from, "No SB time yet this session.");
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

    godf(from,
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
        godf(from,
             "%2s session SB stats: %d wins/%d losses. %5.2f hours. Ratio: %5.2f",
             me->p_mapchars,
             deltaKills,
             deltaLosses,
             (float) deltaTicks/36000.0,
             sessionRatio);
        godf(from,
             "Kills/Hour: %5.2f (%5.2f total), Deaths/Hour: %4.2f (%4.2f total)",
             sessionKPH,
             overallKPH,
             sessionDPH,
             overallDPH);
    return 1;
}


int bouncePingStats(int from)
{
    if(me->p_avrt == -1){
        /* client doesn't support it or server not pinging */
        godf(from, "No PING stats available for %c%c",
             me->p_mapchars[0], me->p_mapchars[1]);
    }
    else{
        godf(from,
             "%c%c PING stats: Avg: %d ms, Stdv: %d ms, Loss: %0.1f%%/%0.1f%% s->c/c->s",
             me->p_mapchars[0], me->p_mapchars[1],
             me->p_avrt,
             me->p_stdv,
             me->p_pkls_s_c,
             me->p_pkls_c_s);
    }
    return 1;
}

int bounceUDPStats(int from)
{
    godf(from, "%c%c last UDP update size: %d bytes",
         me->p_mapchars[0], me->p_mapchars[1], last_udp_size);
    return 1;
}

int bounceWhois(int from)
{
    char msgbuf[255];

    if (hidden) {
        godf(from, "%s is %s (%s)", me->p_mapchars, me->p_name, me->p_login);
    } else {
        snprintf(msgbuf, 255, "%s is %s (%s@%s)", me->p_mapchars, me->p_name,
                 me->p_login, me->p_full_hostname);
        if (strlen(msgbuf) > MSGTEXT_LEN) {
            godf(from, "%s is %s:", me->p_mapchars, me->p_name);
            /* There is a slight possibility of this still being cut
               off with a really long username and hostname, but let's
               not send TOO many lines. The missing end chars will be
               in the playerlist anyways. */
            godf(from, "(%s@%s)", me->p_login, me->p_full_hostname);
        } else
            god(from, msgbuf);
        if (ip_check_dns && strcmp(me->p_full_hostname, me->p_dns_hostname)) {
            snprintf(msgbuf, 255, "[DNS Mismatch] %s resolves to %s", 
                     me->p_mapchars, me->p_dns_hostname);
            if (strlen(msgbuf) > MSGTEXT_LEN) {
                godf(from, "[DNS Mismatch] %s resolves to:", me->p_mapchars);
                god(from, me->p_dns_hostname);
            }
            else
                god(from, msgbuf);
        }
        else
            godf(from, "%s at %s (IP)", me->p_mapchars, me->p_ip);
#ifdef DNSBL_SHOW
        if ((me->p_sorbsproxy && (me->p_sorbsproxy != 8)) ||
            me->p_njablproxy) {
            godf(from,
                "[ProxyCheck] NOTE: %s (%s) may be using an open proxy.",
                me->p_mapchars, me->p_ip);
            if (me->p_xblproxy)
                godf(from,
                    "[ProxyCheck] %s is on the Spamhaus XBL (POSSIBLE open proxy)",
                     me->p_mapchars);
            if (me->p_sorbsproxy)
                godf(from, "[ProxyCheck] %s is on SORBS (PROBABLE open %s%s%s%s%s proxy)", me->p_mapchars,
                    (me->p_sorbsproxy & 1) == 1 ? "HTTP" : "", (me->p_sorbsproxy & 3) == 3 ? "|" : "",
                    (me->p_sorbsproxy & 2) == 2 ? "SOCKS" : "", (me->p_sorbsproxy & 6) == 6 ? "|" : "",
                    (me->p_sorbsproxy & 4) == 4 ? "MISC" : "");
            if (me->p_njablproxy)
                godf(from, "[ProxyCheck] %s is on the NJABL proxy list (PROBABLE open proxy)", me->p_mapchars);
        }
#endif
    }
    return 1;
}

/* ARGSUSED */
void do_display_ignoring(char *comm, struct message *mess, int who)
{
    do_display_ignores(comm, mess, who, IGNORING);
}

/* ARGSUSED */
void do_display_ignoredby(char *comm, struct message *mess, int who)
{
    do_display_ignores(comm, mess, who, IGNOREDBY);
}

void do_display_ignores(char *comm, struct message *mess, int who, int igntype)
{
    DIR *dir;
    FILE *ignorefile;
    struct dirent *dirent;
    struct player *p;
    struct player *other;
    int whofrom = mess->m_from;
    int ignmask = 0;
    int slot = -1;
    int hits = 0;
    char dirname[FNAMESIZE];
    char msg[MSG_LEN];
    char filename[FNAMESIZE];
    char *addr = addr_mess(whofrom,MINDIV);
    char *dname, *srcip, *destip;

    p = &players[whofrom];

    snprintf(dirname, FNAMESIZE, "%s/ip/ignore/by-ip", LOCALSTATEDIR);

    if (!(dir = opendir(dirname))) {
        pmessage(whofrom, MINDIV, addr,
                  "Sorry, not able to get list of ignores");
        ERROR(1,("Not able to opendir(%s) and get ignore list\n", dirname));
        return;
    }

    pmessage(whofrom, MINDIV, addr,
             "You are currently %s the following players:",
             (igntype == IGNORING) ? "ignoring" : "being ignored by");

    while ((dirent = readdir(dir))) {
        ignmask = 0;
        msg[0] = '\0';
        dname = strdup(dirent->d_name);
        srcip = strtok(dname, "-");
        destip = strtok(NULL, "-");

        if ((srcip == NULL) || (destip == NULL)) {
            free(dname);
            continue;
        }

        if (!strcmp((igntype == IGNORING) ? srcip : destip, p->p_ip)) {
            slot = find_slot_by_ip((igntype == IGNORING) ? destip : srcip, 0);
            if (slot == -1) {
                strcat(msg, "  ");
            } else {
                other = &players[slot];
                sprintf(msg, "%s", other->p_mapchars);
            }

            sprintf(msg, "%s %-15s ", msg,
                    (igntype == IGNORING) ? destip : srcip);
            sprintf(filename, "%s/%s", dirname, dirent->d_name);
            ignorefile = fopen(filename, "r");
            if (ignorefile == NULL) {
                pmessage(whofrom, MINDIV, addr,
                         "Unable to get ignore settings");
                ERROR(1,("Not able to fopen(%s) and get ignore value\n",
                         filename));
                free(dname);
                continue;
            }
            fscanf(ignorefile, "%d\n", &ignmask);
            fclose(ignorefile);
            if (ignmask == 0) {
                free(dname);
                continue;
            }
            if (ignmask & MINDIV) {
                strcat(msg, "Indiv ");
            }
            if (ignmask & MTEAM) {
                strcat(msg, "Team ");
            }
            if (ignmask & MALL) {
                strcat(msg, "All ");
            }
            pmessage(whofrom, MINDIV, addr, "%s", msg);
            hits++;
        }
        free(dname);
    }
    closedir(dir);
    if (hits > 0) {
        pmessage(whofrom, MINDIV, addr, "%d player%s %s.", hits,
                 (hits == 1) ? " is" : "s are",
                 (igntype == IGNORING) ? "being ignored by you" : "ignoring you");
    } else {
        pmessage(whofrom, MINDIV, addr, "No entries found.");
    }
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
  int who, t, remaining;
  char *addr;

  who = mess->m_from;
  addr = addr_mess(who,MINDIV);

  t = context->quorum[0];
  if (teams[t].te_surrender == 0) {
    t = context->quorum[1];
  }
  if (teams[t].te_surrender == 0) {
    pmessage(who, MINDIV, addr, "No one is considering surrender now.  Go take some planets.");
    return;
  }

  switch (teams[t].te_surrender_pause) {
    case TE_SURRENDER_PAUSE_OFF:
      remaining = 60 - ((context->frame - teams[t].te_surrender_frame) / fps);
      if (remaining < 50 && remaining > 0) {
        pmessage(who, MINDIV, addr, "The %s have %d minute%s %d second%s left before they surrender.", team_name(t), teams[t].te_surrender-1, (teams[t].te_surrender-1 == 1) ? "" : "s", remaining, (remaining == 1) ? "" : "s");
      } else {
        pmessage(who, MINDIV, addr, "The %s have %d minute%s left before they surrender.", team_name(t), teams[t].te_surrender, (teams[t].te_surrender == 1) ? "" : "s");
      }
      break;
    case TE_SURRENDER_PAUSE_ON_TOURN:
      pmessage(who, MINDIV, addr, "The %s will have %d minute%s left when T mode resumes", team_name(t), teams[t].te_surrender, (teams[t].te_surrender == 1) ? "" : "s");
      break;
    case TE_SURRENDER_PAUSE_ON_PLANETS:
      pmessage(who, MINDIV, addr, "The %s will have %d minute%s left if they are down to %d planets", team_name(t), teams[t].te_surrender, (teams[t].te_surrender == 1) ? "" : "s", surrenderStart);
      break;
  }
}

/* ARGSUSED */
void do_sbtime_msg(char *comm, struct message *mess)
{
  if (teams[players[mess->m_from].p_team].te_turns > 0)
	pmessage(mess->m_from, MINDIV, addr_mess(mess->m_from, MINDIV), "Starbase construction will be complete in %d minute%s.", teams[players[mess->m_from].p_team].te_turns, (teams[players[mess->m_from].p_team].te_turns == 1) ? "" : "s");
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

void do_tips(char *comm, struct message *mess)
{
    int who;
    char *addr;
    extern int tips_enabled;

    who = mess->m_from;
    addr = addr_mess(who,MINDIV);

    if (!F_tips) {
      pmessage(who, MINDIV, addr,
               "Your client does not support tips properly, please upgrade.");
    }

    tips_enabled = !tips_enabled;

    pmessage(who, MINDIV, addr, "Tips mode is now %s",
             tips_enabled ? "on (send play tips on death)" : "off");
}

#ifdef EXPERIMENTAL_BE
/* become another slot, or switch between observer and player mode */
void do_be(char *comm, struct message *mess)
{
    int who, pno;
    char *more, *addr;

    who = mess->m_from;
    addr = addr_mess(who,MINDIV);

    if (!strcmp(comm, "be")) {
      pmessage(who, MINDIV, addr, "be observer - switch to observe on death");
      pmessage(who, MINDIV, addr, "be player   - switch to player");
      pmessage(who, MINDIV, addr, "be n        - switch slot number");
      return;
    }

    more = comm + strlen("be ");

    if (!strcmp("observer", more) || !strcmp("observe", more)) {
      if (!Observer) {
        switch_to_player = 0;
        switch_to_observer = 1;
        pmessage(who, MINDIV, addr, "You will be made an observer on death.");
      } else {
        pmessage(who, MINDIV, addr, "You are already an observer!");
      }
      return;
    }

    if (!strcmp("player", more) || !strcmp("play", more)) {
      // FIXME: preconditions test, what if eight players already?
      if (Observer) {
        switch_to_observer = 0;
        switch_to_player = 1;
        me->p_whydead = KPROVIDENCE;
        me->p_explode = 10;
        me->p_status = PEXPLODE;
        me->p_whodead = 0;
      } else {
        pmessage(who, MINDIV, addr, "You are already a player!");
      }
      return;
    }

    pno = slot_num(*more);

    if (pno < 0) return;
    if (pno > (MAXPLAYER-1)) return;

    if (me->p_no == pno) {
      pmessage(who, MINDIV, addr, "You are this slot already!");
      return;
    }

    struct player *be = &players[pno];
    if (be->p_status != PFREE) {
      pmessage(who, MINDIV, addr, "Slot is not free!");
      return;
    }

    /*! @bug: race condition, with slot allocation */

    memcpy(be, me, sizeof(struct player));
    me->p_status = PFREE;
    me = be;
    me->p_no = pno;
    sprintf(me->p_mapchars,"%c%c",teamlet[me->p_team], shipnos[me->p_no]);
    sprintf(me->p_longname, "%s (%s)", me->p_name, me->p_mapchars);
    updateSelf(1);
}

/*! @brief cooperative substition between observer and player
    @details observer must send 'sub in' to mark themselves as willing
    to play, player must send 'sub out' at which time the observer
    will be forced to lock onto the player, then when the player dies
    and rejoins they will switch to being an observer, and the
    observer will die and rejoin as a player.
  */
void do_sub(char *comm, struct message *mess)
{
  int i, who;
  char *more, *addr;
  struct player *p, *q;
  
  who = mess->m_from;
  addr = addr_mess(who,MINDIV);
  
  if (!strncmp(comm, "sub", 3)) {
    pmessage(who, MINDIV, addr, "HOWTO: (1) an obs 'sub in', (2) player 'sub out', (3) player dies.");
    for (i=0, p=players; i<MAXPLAYER; i++, p++) {
      if (p->p_team != me->p_team) continue;
      if (p->p_sub_out) {
	q = &players[p->p_sub_out_for];
	pmessage(who, MINDIV, addr, "%s is ready to sub out, with %s sub in",
		 p->p_mapchars, q->p_mapchars);
	break;
      }
    }
    for (i=0, p=players; i<MAXPLAYER; i++, p++) {
      if (p->p_team != me->p_team) continue;
      if (p->p_sub_in) {
	pmessage(who, MINDIV, addr, "%s is ready to sub in", p->p_mapchars);
      }
    }
    return;
  }

  more = comm + strlen("sub ");

  if (strcmp("in", more) && strcmp("out", more)) return;

  if (Observer) {
    /* we are an observer, toggle willingness */
    me->p_sub_in = ~me->p_sub_in;
    me->p_sub_in_for = -1;
    pmessage(who, MINDIV, addr, me->p_sub_in ? 
	     "Sub in ready, waiting for a player to sub out." : 
	     "Sub in cancelled!");
    return;
  }

  /* we are a player */

  /* are we sub out pending? */
  if (me->p_sub_out) {
    p = &players[me->p_sub_out_for];
    p->p_sub_in_for = -1;
    me->p_sub_out = 0;
    me->p_sub_out_for = -1;
    pmessage(who, MINDIV, addr, "Sub out cancelled!");
    return;
  }

  /* we are not sub out pending */

  /* find the first waiting substitute, lowest slot */
  for (i=0, p=players; i<MAXPLAYER; i++, p++) {
    if (p->p_status != POBSERV) continue;
    if (p->p_team != me->p_team) continue;
    if (p->p_sub_in) {
      p->p_sub_in_for = me->p_no;
      me->p_sub_out_for = p->p_no;
      // FIXME: tell team that other->p_mapchars subs for me->p_mapchars 
      pmessage(who, MINDIV, addr,
	       "Sub out ready ... on your death %s will enter", p->p_mapchars);
      /* the sub in now watches the sub out, enforced */
      p->p_playerl = me->p_no;
      p->p_flags &= ~(PFPLLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
      p->p_flags |= PFPLOCK;
      /* the sub out will switch to observer on next death */
      me->p_sub_out = 1;
      // on enter of *this* slot, kill the other
      return;
    }
  }
  pmessage(who, MINDIV, addr, "Sub out failed, no sub in waiting!");
  return;
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

  if (strlen(one) > 8) {
    pmessage(who, MINDIV, addr,
             "WARNING PASSWORD OVERLONG: "
             "only the first eight letters are used!");
  }
}


void do_nodock(char *comm, struct message *mess)
{
  int whofrom = mess->m_from;
  struct player *p = &players[whofrom];
  struct player *victim;
  char *addr = addr_mess(whofrom,MINDIV);
  char *who, *what;
  char *usage = "dock usage: 'DOCK 0 ON|OFF'";
  int slot;
  
  if (p->p_ship.s_type != STARBASE)
    {
      pmessage(whofrom, MINDIV, addr, "dock: must be a starbase to use nodock");
      return;
    }
  
  if (p->p_status == POBSERV)
    {
      pmessage(whofrom, MINDIV, addr, "dock: Observers cannot change dock permissions");
      return;
    }

  who = strtok(comm, " ");
  if ( who == NULL)
    return;

  who = strtok(NULL, " ");
  if(who == NULL)
    {
      pmessage(whofrom, MINDIV, addr, usage);
      return;
    }

  what = strtok(NULL, " ");
  if(what == NULL)
    {
      pmessage(whofrom, MINDIV, addr, usage);
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
  else pmessage(whofrom, MINDIV, addr, usage);
}

void do_transwarp(char *comm, struct message *mess)
{
  int whofrom = mess->m_from;
  struct player *p = &players[whofrom];
  struct player *victim;
  char *addr = addr_mess(whofrom,MINDIV);
  char *who, *what;
  char *usage = "transwarp usage: 'TRANSWARP ON|GREEN|YELLOW|SHIELD|OFF'";
  char *usage2 = "additional usage: 'TRANSWARP ON|OFF player#'";
  int slot;

  if (p->p_ship.s_type != STARBASE) {
    pmessage(whofrom, MINDIV, addr, "transwarp: must be a starbase to use this");
    return;
  }
  
  if (p->p_status == POBSERV) {
    pmessage(whofrom, MINDIV, addr, "transwarp: observers may not do this");
    return;
  }

  what = strtok(comm, " ");
  if (what == NULL) return;

  what = strtok(NULL, " ");
  if (what == NULL) {
    pmessage(whofrom, MINDIV, addr, usage);
    pmessage(whofrom, MINDIV, addr, usage2);
    return;
  }

  if (!strcmp("off", what) || !strcmp("on", what)) {
    who = strtok(NULL, " ");
    if (who == NULL) {
      if ( !strcmp("off", what) ) {
        me->p_transwarp = 0;
        pmessage(me->p_team, MTEAM, 
	         addr_mess(me->p_team,MTEAM), 
	         "Starbase %s refusing transwarp", me->p_mapchars);
      } else {
        me->p_transwarp = PFGREEN|PFYELLOW|PFRED;
        pmessage(me->p_team, MTEAM,
	         addr_mess(me->p_team,MTEAM),
	         "Starbase %s transwarp restored", me->p_mapchars);
      }
    } else {
      /* Individual transwarp toggle */
      *who = toupper(*who);
      if( (*who >= '0') && (*who <= '9') )
        slot = *who - '0';
      else if ( (*who >= 'A') && (*who <= ('A' + MAXPLAYER - 10) ) )
        slot = 10 + *who - 'A';
      else {
        pmessage(whofrom, MINDIV, addr, "transwarp: unrecognized slot");
        return;
      }
      victim = &players[slot];

      if (victim->p_status == PFREE) {
        pmessage(whofrom, MINDIV, addr, "transwarp: ignored, slot is free");
        return;
      }

      if( !strcmp("off", what) ) {
        victim->p_cantranswarp = 0;
        pmessage(whofrom, MINDIV, addr, "Slot %c no longer allowed to transwarp to SB", *who);
      } else {
        victim->p_cantranswarp = 1;
        pmessage(whofrom, MINDIV, addr, "Slot %c is allowed to transwarp to SB", *who);
      }
    }
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
  } else {
    pmessage(whofrom, MINDIV, addr, usage);
    pmessage(whofrom, MINDIV, addr, usage2);
  }
}

void do_admin(char *comm, struct message *mess)
{
  int who = mess->m_from;
  struct player *p = &players[who];
  char *addr = addr_mess(who,MINDIV);
  char *one, *two = NULL;
  char command[256];
  int slot;
  struct player *them = NULL;

  if (!p->p_authorised) {
    if (strstr(comm, "ADMIN password")) {
      char *cchar = &comm[15];
      if (!strcmp(cchar, admin_password)) {
        if (!strcmp(cchar, "password")) {
          pmessage(who, MINDIV, addr, "admin: denied, password is not yet set");
          ERROR(2,("%s admin: authorisation ignored, ip=%s\n",
                   p->p_mapchars, p->p_ip));
          return;
        }
        p->p_authorised = 1;
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

  if (!p->p_authorised) {
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
  /* admin exec - execute shell command */
  
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
  if (strcmp(one, "exec"))
  {
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
  }

  if (!strcmp(one, "duplicates")) {
    if (them == NULL) return;
    if (!strcmp(them->p_ip, "127.0.0.1")) {
      pmessage(who, MINDIV, addr,
               "admin: cannot tweak duplicates for hidden ip or local slots");
      /* instead, manually touch ${SYSCONFDIR}/ip/duplicates/127.0.0.1 */
      return;
    }
    if (them->p_ip_duplicates) {
      ip_duplicates_clear(them->p_ip);
      them->p_ip_duplicates = 0;
      pmessage(who, MINDIV, addr, "admin: duplicates flag cleared for %s",
               them->p_ip);
    } else {
      ip_duplicates_set(them->p_ip);
      them->p_ip_duplicates = 1;
      pmessage(who, MINDIV, addr, "admin: duplicates flag set for %s",
               them->p_ip);
    }
  } else if (!strcmp(one, "mute")) {
    pmessage(who, MINDIV, addr, "admin: erm, send 'em a \"mute on\"");
  } else if (!strcmp(one, "quit")) {
    if (them == NULL) return;
    sprintf(command, "%s/tools/admin/quit %s %c", LIBDIR, p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s forced to quit.", two);
  } else if (!strcmp(one, "kill")) {
    if (them == NULL) return;
    sprintf(command, "%s/tools/admin/kill %s %c", LIBDIR, p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s killed.", two);
  } else if (!strcmp(one, "free")) {
    if (them == NULL) return;
    sprintf(command, "%s/tools/admin/free %s %c", LIBDIR, p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s free-ed.", two);
  } else if (!strcmp(one, "ban")) {
    if (them == NULL) return;
    sprintf(command, "%s/tools/admin/ban %s %s %s", LIBDIR, p->p_full_hostname, them->p_ip, them->p_full_hostname);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s banned.", two);
  } else if (!strcmp(one, "reset")) {
    sprintf(command, "%s/tools/admin/reset %s t", LIBDIR, p->p_full_hostname);
    system(command);
    pmessage(who, MINDIV, addr, "admin: galactic has been reset.");
  } else if (!strcmp(one, "dfreset")) {
    sprintf(command, "%s/tools/admin/reset %s d", LIBDIR, p->p_full_hostname);
    system(command);
    pmessage(who, MINDIV, addr, "admin: galactic has been reset for df'ing.");
  } else if (!strcmp(one, "exec")) {
    if (!adminexec)
      pmessage(who, MINDIV, addr, "admin: exec is not enabled.");
    else
    {
      system(one + 5);
      pmessage(who, MINDIV, addr, "admin: executed \"%s\"", one + 5);
    }
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
	  is_guest(p->p_name) ? "add" : "change", 
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

#ifdef LTD_STATS
static char *ltd_stats_url_prefix() {
#define MAXPATH 256
  char name[MAXPATH];
  snprintf(name, MAXPATH, "%s/%s", SYSCONFDIR, "ltd-stats-url-prefix");
  FILE *file = fopen(name, "r");
  if (file == NULL) return NULL;
  static char text[80];
  char *res = fgets(text, 80, file);
  fclose(file);
  if (res == NULL) return NULL;
  res[strlen(res)-1] = '\0';
  return res;
}

void do_ltd_query(char *comm, struct message *mess, int who)
{
  char *url;
  char name[256];
  int ticks;

  sendLtdPacket();

  /* fail if the player has no player file position reference */
  if (me->p_pos < 0) {
    pmessage(who, MINDIV, addr_mess(who,MINDIV),
             "Statistics are not kept for guests");
    return;
  }

  /* fail if the server owner has not set up this up yet */
  url = ltd_stats_url_prefix();
  if (url == NULL) {
    pmessage(who, MINDIV, addr_mess(who,MINDIV),
             "Server owner has not configured etc/ltd-stats-url-prefix");
    return;
  }

  /* invent a file name for the stats dump request output */
  ticks = ltd_ticks(me, LTD_TOTAL);
  snprintf(name, 256-1, "%s/www/stats/%08x.%08x.html", LOCALSTATEDIR,
           me->p_pos, ticks);

  /* in a new process create the file and exit */
  if (fork() == 0) {
    (void) signal(SIGALRM,SIG_DFL);
    FILE *fp = fopen(name, "w");
    if (fp == NULL) return;
    int fd = fileno(fp);
    dup2(fd, 1);
    execl("tools/ltd_dump_html", "ltd_dump_html", me->p_name, (char *) NULL);
    perror("ltd_dump_html");
  }

  /* tell the user where to go to see it */
  snprintf(name, 256-1, "%s/stats/%08x.%08x.html", url, me->p_pos, ticks);
  pmessage(who, MINDIV, addr_mess(who,MINDIV), name);

  /* @bug: race condition, client might, if fast enough, access the
  link before the ltd_dump_html process is finished, but this is
  unlikely since the process should take a very very small time, it
  uses indexed access to player file. */
}
#endif
