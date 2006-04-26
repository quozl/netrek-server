/* $Id: ntscmds.c,v 1.8 2006/04/24 12:35:17 quozl Exp $
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
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "gencmds.h"
#include "proto.h"

#define C_PR_INPICKUP    C_PR_1

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

#if defined (TRIPLE_PLANET_MAYHEM)
void do_balance(void);
void do_triple_planet_mayhem(void);
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
	C_VC_TEAM | C_GLOG | C_PLAYER | C_PR_INPICKUP,
	"Eject a player               e.g. 'EJECT 0 IDLE'", 
	do_player_eject,				/* EJECT */
	2, PV_EJECT, 120, 600},
    { "BAN",
	C_VC_TEAM | C_GLOG | C_PLAYER | C_PR_INPICKUP,
	"Eject and ban a player       e.g. 'BAN 0'", 
	do_player_ban,					/* BAN */
	2, PV_BAN, 120, 600},
#if defined(TRIPLE_PLANET_MAYHEM)
    { "TRIPLE",
        C_VC_ALL | C_GLOG | C_PR_INPICKUP,
        "Start triple planet mayhem by vote",
        do_triple_planet_mayhem,
	2, PV_OTHER, 0},
    { "BALANCE",
        C_VC_ALL | C_GLOG | C_PR_INPICKUP,
        "Request team randomise & balance",
        do_balance,
        4, PV_OTHER+1, 0 },
#endif
#if defined(AUTO_INL)
  { "INL",
	C_VC_ALL | C_GLOG | C_PR_INPICKUP,
	"Start game under INL rules.",
	do_start_inl,
	1, PV_OTHER+2, 0 },
#endif
#if defined(AUTO_PRACTICE)
  { "PRACTICE",
	C_VC_ALL | C_PR_INPICKUP,
	"Start basepractice by majority vote.",
	do_start_basep,
	1, PV_OTHER+3, 0 },
#endif
#if defined(AUTO_HOCKEY)
  { "HOCKEY",
	C_VC_ALL | C_GLOG | C_PR_INPICKUP,
	"Start hockey by majority vote.",
	do_start_puck,
	1, PV_OTHER+4, 0 },
#endif
#if defined(AUTO_DOGFIGHT)
  { "DOGFIGHT",
	C_VC_ALL | C_GLOG | C_PR_INPICKUP,
	"Start dogfight tournament by majority vote.",
	do_start_mars,
	1, PV_OTHER+5, 0 },
#endif

    /* crosscheck, last voting array element used (PV_OTHER+n) must
       not exceed PV_TOTAL, see include/defs.h */

    { NULL }
};

int check_command(struct message *mess)
{
  return check_2_command(mess, nts_commands,
			 (status->gameup & GU_INROBOT) ? 0 : C_PR_INPICKUP);
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
    ban_player(j->p_no);
}

void ban_player(int who)
{
  int i;
  struct player *j = &players[who];
  for (i=0; i<MAXBANS; i++) {
    struct ban *b = &bans[i];
    if (b->b_expire == 0) {
      strcpy(b->b_ip, j->p_ip);
      b->b_expire = ban_vote_length;
      ERROR(2,( "ban of %s was voted\n", b->b_ip));
      return;
    }
  }
  pmessage(0, MALL, addr_mess(who,MALL), 
	   " temporary ban list is full, ban ineffective");
}

#if defined(AUTO_PRACTICE)
void do_start_basep(void)
{
  if (vfork() == 0) {
    (void) SIGNAL(SIGALRM,SIG_DFL);
    execl(Basep, "basep", 0);
    perror(Basep);
  }
}
#endif

#if defined(AUTO_INL)
void do_start_inl(void)
{
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
  if (vfork() == 0) {
    (void) SIGNAL(SIGALRM,SIG_DFL);
    execl(Puck, "puck", 0);
    perror(Puck);
  }
}
#endif

#if defined(AUTO_DOGFIGHT)
void do_start_mars(void)
{
  if (vfork() == 0) {
    (void) SIGNAL(SIGALRM,SIG_DFL);
    execl(Mars, "mars", 0);
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

#ifdef TRIPLE_PLANET_MAYHEM
/*
**  16-Jul-1994 James Cameron
**
**  Balances teams according to player statistics.
**  Intended for use with Triple Planet Mayhem
*/

/*
**  Tell all that player will be moved
*/
static void moveallmsg(int p_no, int ours, int theirs, int p_value)
{
    struct player *k = &players[p_no];
    pmessage(0, MALL, addr_mess(p_no, MALL),
       "Balance: %16s (slot %c, rating %.2f) is to join the %s",
       k->p_name, shipnos[p_no], 
       (float) ( p_value / 100.0 ),
       team_name(ours));

    /* annoying compiler warning */
    if (theirs) ;
}

/*
**  Move a player to the specified team, if they are not yet there.
**  Make them peaceful with the new team, and hostile/at war with the
**  other team.
*/
static void move(int p_no, int ours, int theirs)
{
  struct player *k = &players[p_no];
  int queue;
    
  if ( k->p_team != ours ) {
    pmessage(k->p_no, MINDIV, addr_mess(k->p_no,MINDIV),
	     "%s: please SWAP SIDES to the --> %s <--", k->p_name, team_name(ours));
  }
  else {
    pmessage(k->p_no, MINDIV, addr_mess(k->p_no,MINDIV),
	     "%s: please remain with the --> %s <--", k->p_name, team_name(ours));
  }
  
  printf("Balance: %16s (%s) is to join the %s\n", 
	 k->p_name, k->p_mapchars, team_name(ours));
  
  /* cope with a balance during INL pre-game, if we don't shift players who
     are on the QU_HOME or QU_AWAY queues then the queue masks will force
     them to join the team they were on anyway. */
  queue = ( ours == FED ) ? QU_HOME : QU_AWAY;
  if (k->w_queue != QU_PICKUP && k->w_queue != queue) {
    queues[k->w_queue].free_slots++;
    k->w_queue = queue;
    queues[k->w_queue].free_slots--;
  }  
  
  k->p_hostile |= theirs;
  k->p_swar    |= theirs;
  k->p_hostile &= ~ours;
  k->p_swar    &= ~ours;
  k->p_war      = (k->p_hostile | k->p_swar);
  k->p_team     =  ours;
  sprintf(k->p_mapchars, "%c%c", teamlet[k->p_team], shipnos[p_no]);
  sprintf(k->p_longname, "%s (%s)", k->p_name, k->p_mapchars);
  
  k->p_status = PEXPLODE;
  k->p_whydead = KPROVIDENCE; /* should be KTOURNSTART? */
  if (k->p_ship.s_type == STARBASE)
    k->p_explode = 2 * SBEXPVIEWS;
  else
    k->p_explode = 10;
  k->p_ntorp = 0;
  k->p_nplasmatorp = 0;
  k->p_hostile = (FED | ROM | ORI | KLI);
  k->p_war     = (k->p_hostile | k->p_swar);
}

/*
**  Return two team masks corresponding to the teams of the first two
**  teams found in the player list.
*/
static void sides (int *one, int *two)
{
    struct player *k;
    int i;
    int unseen;

    unseen = (FED | ROM | ORI | KLI);
    *one = 0;
    *two = 0;
    k = &players[0];
    for(i=0;i<MAXPLAYER;i++)
    {
        if ( (k->p_status != PFREE) && (!(k->p_flags & PFROBOT)))
        {
            if ( ( unseen & k->p_team ) != 0 )
            {
                if ( *one == 0 )
                {
                    *one = k->p_team;
                    unseen &= ~k->p_team;
                    k++;
                    continue;
                }
                *two = k->p_team;
                return;
            }
        }
        k++;
    }
}

/*
**  Calculate a player value
*/
static int value (struct player *k)
{
    return
    (int)
    (
        (float)
        (
#ifdef LTD_STATS
            ltd_bombing_rating(k) * BALANCE_BOMBING +
            ltd_planet_rating(k)  * BALANCE_PLANET  +
            ltd_defense_rating(k) * BALANCE_DEFENSE +
            ltd_offense_rating(k) * BALANCE_OFFENSE
#else
            bombingRating(k) * BALANCE_BOMBING +
            planetRating(k)  * BALANCE_PLANET  +
            defenseRating(k) * BALANCE_DEFENSE +
            offenseRating(k) * BALANCE_OFFENSE
#endif /* LTD_STATS */
        )
    );
}

/*
**  Balance the teams
**
**  Uses an exhaustive algorithm (I'm exhausted!) to find the best combination
**  of the current players that balances the teams in terms of statistics.
**  The algorithm will support only the number of players that fits into the
**  number of bits in an int.
**
**  If there are multiple "best" combinations, then the combination
**  involving the least number of team swaps will be chosen.
*/
void do_balance(void)
{
    int i, j;                   /* miscellaneous counters    */
    int records;                /* number of players in game */
    int one;                    /* team number one mask      */
    int two;                    /* team number two mask      */

    struct player *k;           /* pointer to current player */

    struct item
    {
        int p_no;               /* player number             */
        int p_value;            /* calculated player value   */
        int p_team;             /* team player on previously */
    } list[MAXPLAYER];          /* working array             */

    struct
    {
        int combination;        /* combination number        */
        int value;              /* team balance difference   */
        int one;                /* team one total value      */
        int two;                /* team two total value      */
        int swaps;              /* number of swaps involved  */
    } best;                     /* best team combination     */

    /* which teams are playing?  give up if only one found */
    sides ( &one, &two );
    if ( two == 0 )
    {
	/* addr_mess shouldn't be called with 0 as first arg
         * for MALL, but we don't have a player numer here.
         * Let addr_mess catch it. -da
         */

        pmessage ( 0, MALL, addr_mess(0 ,MALL),
          "Can't balance only one team!" );
        pmessage ( 0, MALL, addr_mess(0 ,MALL),
          "Please could somebody move to another team, then all vote again?" );
        return;
    }

    /* initialise best to worst case */
    best.combination = -1;
    best.value = 1<<30;
    best.one = 0;
    best.two = 0;
    best.swaps = 1<<30;

    /* reset working array */
    for(i=0;i<MAXPLAYER;i++)
    {
        list[i].p_no    = 0;
        list[i].p_value = 0;
    }

    /* insert players in working array */
    records = 0;
    k = &players[0];
    for(i=0;i<MAXPLAYER;i++)
    {
        if ( (k->p_status != PFREE) && (!(k->p_flags & PFROBOT)))
        {
            list[records].p_no    = k->p_no;
            list[records].p_value = value ( k );
            list[records].p_team  = k->p_team;
            records++;
        }
        k++;
    }

    /* randomise the working array; may cause different team mixes */
    for(i=0;i<records;i++)
    {
        int a, b;
        struct item swapper;

        a = random() % records;
        b = random() % records;

        swapper = list[a];
        list[a] = list[b];
        list[b] = swapper;
    }

    /* loop for every _possible_ combination to find the best */
    for(i=0;i<(1<<records);i++)
    {
        int difference;         /* difference in team total       */
        int value_a, value_b;   /* total stats per team           */
        int count_a, count_b;   /* total count of players on team */
        int swaps;              /* number of swaps involved       */

        /* if this a shadow combination already considered, ignore it */
        /* if ( ( i ^ ( ( 1<<records ) - 1 ) ) < i ) continue; */
        /* disabled - it will interfere with swap minimisation goal */
        
        /* is this combination an equal number of players each side? */
        count_a = 0;
        count_b = 0;

        for(j=0;j<records;j++)
            if((1<<j)&i)
                count_a++;
            else
                count_b++;

        /* skip this combination if teams are significantly unequal */
        if ( abs ( count_a - count_b ) > 1 ) continue;

        /* reset team total for attempt */
        value_a = 0;
        value_b = 0;

        /* calculate team total stats */
        for(j=0;j<records;j++)
            if((1<<j)&i)
                value_a += list[j].p_value;
            else
                value_b += list[j].p_value;

        /* calculate number of swaps this combination produces */
        swaps = 0;
        for(j=0;j<records;j++)
            if((1<<j)&i)
            {
                if ( list[j].p_team != one ) swaps++;
	    }
            else
	    {
                if ( list[j].p_team != two ) swaps++;
	    }

        /* calculate difference in team total stats */
        difference = abs ( value_a - value_b );

        /* if this combo is better than the previous one we had,
           or the combo is the same and the number of swaps is lower...  */
        if ( ( difference < best.value )
            || ( ( difference == best.value )
                && ( swaps < best.swaps ) ) )
        {
            /* remember it */
            best.value = difference;
            best.combination = i;
            best.one = value_a;
            best.two = value_b;
            best.swaps = swaps;
        }
    }

    /* don't do balance if not worth it */
    if ( best.swaps != 0 )
    {
        /* move players to their teams */
        for(j=0;j<records;j++)
            if ( (1<<j)&best.combination )
                move ( list[j].p_no, one, two );
            else
                move ( list[j].p_no, two, one );
        
        /* build messages to ALL about the results */
        for(j=0;j<records;j++)
            if ( (1<<j)&best.combination )
                moveallmsg ( list[j].p_no, one, two, list[j].p_value );
    
	/* addr_mess ignores first arg if second is MALL. -da
         */

        pmessage(0, MALL, addr_mess(0, MALL),
            "Balance:                           total rating %.2f",
            (float) ( best.one / 100.0 ) );
        
        for(j=0;j<records;j++)
            if ( !((1<<j)&best.combination) )
                moveallmsg ( list[j].p_no, two, one, list[j].p_value );
    
        pmessage(0, MALL, addr_mess(0, MALL),
            "Balance:                           total rating %.2f",
            (float) ( best.two / 100.0 ) );
    }
    else
    {
        pmessage ( 0, MALL, addr_mess(0, MALL),
            "No balance performed, this is the best: %-3s %.2f, %-3s %.2f",
            team_name(one), (float) ( best.one / 100.0 ),
            team_name(two), (float) ( best.two / 100.0 ) );
    }
    
}

void do_triple_planet_mayhem(void)
{
    int i;

    /* balance the teams */
    do_balance();

    /* move all planets off the galaxy */
    for (i=0; i<MAXPLANETS; i++)
    {
        planets[i].pl_flags = 0;
        planets[i].pl_owner = 0;
        planets[i].pl_x = -10000;
        planets[i].pl_y = -10000;
        planets[i].pl_info = 0;
        planets[i].pl_armies = 0;
        strcpy ( planets[i].pl_name, "" );
    }

    /* disable Klingon and Orion teams; stop people from joining them */
    planets[20].pl_couptime = 999999; /* no Klingons */
    planets[30].pl_couptime = 999999; /* no Orions */

    /* initialise earth */
    i = 0;
    planets[i].pl_flags |= FED | PLHOME | PLCORE | PLAGRI | PLFUEL | PLREPAIR;
    planets[i].pl_x = 40000;
    planets[i].pl_y = 65000;
    planets[i].pl_armies = 40;
    planets[i].pl_info = FED;
    planets[i].pl_owner = FED;
    strcpy ( planets[i].pl_name, "Earth" );

    /* initialise romulus */
    i = 10;
    planets[i].pl_flags |= ROM | PLHOME | PLCORE | PLAGRI | PLFUEL | PLREPAIR;
    planets[i].pl_x = 40000;
    planets[i].pl_y = 35000;
    planets[i].pl_armies = 40;
    planets[i].pl_info = ROM;
    planets[i].pl_owner = ROM;
    strcpy ( planets[i].pl_name, "Romulus" );

    /* initialise indi */
    i = 18;
    planets[i].pl_flags |= PLFUEL | PLREPAIR;
    planets[i].pl_flags &= ~PLAGRI;
    planets[i].pl_x = 15980;
    planets[i].pl_y = 50000;
    planets[i].pl_armies = 4;
    planets[i].pl_info &= ~ALLTEAM;
    strcpy ( planets[i].pl_name, "Indi" );

    /* fix all planet name lengths */
    for (i=0; i<MAXPLANETS; i++)
    {
        planets[i].pl_namelen = strlen(planets[i].pl_name);
    }

    /* advise players */
    {
        char *list[] =
        {
	    "Galaxy reset for triple planet mayhem!",
	    "Rule 1: they take Indi, they win,",
	    "Rule 2: they take your home planet, they win,",
	    "Rule 3: you can't bomb Indi,",
	    "Rule 4: you may bomb their home planet, and;",
	    "Rule 5: all planets are FUEL & REPAIR, home planets are AGRI.",
	    ""
	};

	for ( i=0; strlen(list[i])!=0; i++ )

            /* addr_mess ignores first arg for MALL. -da
             */

            pmessage ( 0, MALL, addr_mess(0,MALL), list[i] );
    }
}
#endif /* TRIPLE_PLANET_MAYHEM */

void do_password(char *comm, struct message *mess)
{
  int who = mess->m_from;
  struct player *p = &players[who];
  char *addr = addr_mess(who,MINDIV);
  char *one, *two;

  /* guests have no player file position */
  if (me->p_pos < 0) {
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
  struct player *victim;
  char *addr = addr_mess(whofrom,MINDIV);
  char *who, *what;
  char *usage = "transwarp usage: 'TRANSWARP ON|GREEN|YELLOW|SHIELD|OFF'";
  int slot;
  
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

#ifdef CONTINUUM_COMINDICO
  /* 2005-01-26 temporary password access to admin for me */
  if (!authorised) {
    if (whitelisted) {
      if (!strcasecmp(comm, "ADMIN password duafpouwaimahghedahaengoosoh")) {
	authorised = 1;
	pmessage(who, MINDIV, addr, "admin: authorised");
	return;
      }
    }
  }

  /* 2005-01-26 temporary player name specific access to admin for me */
  if (!authorised) {
    if (!strcmp(p->p_name, "Quozl")) {
      authorised = 1;
    }
  }
#endif

  if (!authorised) {
    if (p->w_queue != QU_GOD_OBS && p->w_queue != QU_GOD) {
      pmessage(who, MINDIV, addr, "Sorry, no.", p->w_queue);
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
    sprintf(command, "tools/admin/quit %s %c", p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s forced to quit.", two);
  } else if (!strcmp(one, "kill")) {
    if (them == NULL) return;
    sprintf(command, "tools/admin/kill %s %c", p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s killed.", two);
  } else if (!strcmp(one, "free")) {
    if (them == NULL) return;
    sprintf(command, "tools/admin/free %s %c", p->p_full_hostname, them->p_mapchars[1]);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s free-ed.", two);
  } else if (!strcmp(one, "ban")) {
    if (them == NULL) return;
    sprintf(command, "tools/admin/ban %s %s", p->p_full_hostname, them->p_full_hostname);
    system(command);
    pmessage(who, MINDIV, addr, "admin: player %s banned.", two);
  } else if (!strcmp(one, "reset")) {
    sprintf(command, "tools/admin/reset %s", p->p_full_hostname);
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
