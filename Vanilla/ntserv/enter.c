/*
 * enter.c
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include <time.h>		/* 7/16/91 TC */
#include <netinet/in.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "sturgeon.h"
#include "util.h"

/* file scope prototypes */

static void auto_peace(void);
static void placeIndependent(void);

/* Enter the game */

#if defined (ONCHECK) && !defined(ROBOT)
static int first_time = 1;
#endif

static struct planet *pick_starting_planet(int p_team)
{
    int i, tno = team_no(p_team);
    for (;;) {
        i = tno * 10 + random() % 10;
        if (startplanets[i]) return &planets[i];
    }
}

static u_char face_enemy(void)
{
    int team_other, x, y;
    struct planet *planet_other;
    u_char dir;

    /* determine the team that is the enemy, if none return default */
    team_other = team_opposing(me->p_team);
    if (team_other == NOBODY) {
        ERROR(5,("face_enemy: no opposing team to face.\n"));
        return 0;
    }

    /* identify one of the enemy home worlds */
    planet_other = pick_starting_planet(team_other);

    /* face in that direction, modulus 32 (45 degrees) */
    x = planet_other->pl_x;
    y = planet_other->pl_y;

    dir = ((u_char) nint(atan2((double) (x - me->p_x),
                               (double) (me->p_y - y))
                         / 3.14159 * 128.0 / 32.0) * 32);

    ERROR(5,("face_enemy: team %s enemy %s planet %s set direction %d.\n",
            team_name(me->p_team), team_name(team_other),
            planet_other->pl_name, dir));
    return dir;
}

/* change 5/10/91 TC -- if tno == 4, independent */
/*                         tno == 5, Terminator  */
/* change 3/25/93 NBT -- added promote string    */
/* change 9/29/94 MJK -- enter no longer uses global variables */
/*                       this allows one robot to enter several ships */

/*ARGSUSED*/
void enter(int tno, int disp, int pno, int s_type, char *name)
     /* tno  0=fed, 1=rom, 2=kli, 3=ori, indep, termie */
{
    static int lastteam= -1;
    static int lastrank= -1;
    char addrbuf[10];
    struct planet *starting_planet;
    int i;

    /* Use local variables, not the globals */
    struct player *me = &players[pno], *j;
    struct ship *myship = &me->p_ship;

#if defined (ONCHECK) && !defined(ROBOT)
/* produces a file for xbiff to watch daemon start */
    if (first_time) {
	char buf[80];
	sprintf(buf,"echo on >> %s",On_File);
	system(buf);
	first_time = 0;
    }
#endif

    STRNCPY(me->p_name, name, NAME_LEN);
    me->p_name[NAME_LEN - 1] = '\0';
    getship(myship, s_type);

#ifdef STURGEON
    if (sturgeon) sturgeon_hook_enter(me, s_type, tno);
#endif

    /* Alert client about new ship stats */
    sndShipCap();

#ifndef ROBOT
    p_ups_set(me, defups);
#endif /* of #ifndef ROBOT */

    /* Check if can use plasma torpedoes */
    if (s_type != STARBASE && s_type != ATT && (
#ifdef STURGEON  /* force plasmas -> upgrade feature */
        !sturgeon &&
#endif
        plkills > 0))
	me->p_ship.s_plasmacost = -1;
    me->p_updates = 0;
    me->p_flags = PFSHIELD|PFGREEN;
    if (s_type == STARBASE) {
	me->p_flags |= PFDOCKOK; /* allow docking by default */
	for(i = 0; i < MAXPLAYER; i++)
	    if (players[i].p_team == me->p_team) {
	        players[i].p_candock = 1; /* give team permission to dock */
	        players[i].p_cantranswarp = 1; /* and transwarp */
	    }
    }
    me->p_transwarp = PFGREEN|PFYELLOW|PFRED;
    me->p_speed = 0;
    me->p_desspeed = 0;
    me->p_subspeed = 0;
    if ((tno == 4) || (tno == 5) || (me->w_queue == QU_GOD_OBS) ) { 
        /* change 5/10/91 TC new case, indep */
        /* change 1/05/00 CYV new case, god obs */
        me->p_team = 0;
        placeIndependent();     /* place away from others 1/23/92 TC */
        me->p_dir = me->p_desdir = 0;
    } else {
        me->p_team = (1 << tno);
        starting_planet = pick_starting_planet(me->p_team);
        p_x_y_go(me,
                 starting_planet->pl_x + (random() % 10000) - 5000,
                 starting_planet->pl_y + (random() % 10000) - 5000);
        me->p_dir = me->p_desdir = face_enemy();
    }
    p_x_y_unbox(me);
    me->p_ntorp = 0;
    me->p_cloakphase = 0;
    me->p_nplasmatorp = 0;
    me->p_damage = 0;
    me->p_subdamage = 0;
    me->p_etemp = 0;
    me->p_etime = 0;
    me->p_wtemp = 0;
    me->p_wtime = 0;
    me->p_shield = me->p_ship.s_maxshield;
    me->p_subshield = 0;
    me->p_swar = 0;
    me->p_lastseenby = VACANT;
    me->p_kills = 0.0;
    me->p_armies = 0;
    bay_init(me);
    me->p_dock_with = 0;
    me->p_dock_bay = 0;
/*  if (!keeppeace) me->p_hostile = (FED|ROM|KLI|ORI); */
    if( !(me->p_flags & PFROBOT) && (me->p_team == NOBODY) ) {
      me->p_hostile = NOBODY;
      me->p_war = NOBODY;
      me->p_swar = NOBODY;
    } else {
      if (!keeppeace) auto_peace();
      me->p_hostile &= ~me->p_team;
      me->p_war = me->p_hostile;
    }

    /* reset eject voting to avoid inheriting this slot's last occupant's */
    /* escaped fate just in case the last vote comes through after the    */
    /* old guy quit and the new guy joined  -Villalpando req. by Cameron  */
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) 
      j->voting[me->p_no] = -1;

    me->p_inlcaptain = 0;

    /* join message stuff */
    sprintf(me->p_mapchars,"%c%c",teamlet[me->p_team], shipnos[me->p_no]);
    sprintf(me->p_longname, "%s (%s)", me->p_name, me->p_mapchars);	    

    /* add initialisation of struct player entries above */

    if (lastteam != tno || lastrank != mystats->st_rank) {

	/* Are joining a new team?  take care of a few things if so */

	if ((lastteam < NUMTEAM) && (lastteam != tno)) {
#ifdef LTD_STATS
	  setEnemy(tno, me);
#endif /*LTD_STATS*/
        /* If switching, become hostile to former team 6/29/92 TC */
	  if (lastteam >= 0)
	    declare_war(1<<lastteam, 0);
	}
	if (lastrank==-1) lastrank=mystats->st_rank;	/* NBT patch */
	lastteam=tno;
	sprintf(addrbuf, " %s->ALL", me->p_mapchars);
	if ((tno == 4) && (strcmp(me->p_monitor, "Nowhere") == 0)) {
	    /* change 5/10/91, 8/28/91 TC */
	    time_t curtime;
	    struct tm *tmstruct;
	    int hour;

	    time(&curtime);
	    tmstruct = localtime(&curtime);
	    if (!(hour = tmstruct->tm_hour%12)) hour = 12;
	    if ( queues[QU_PICKUP].count > 0){
	        pmessage2(0, MALL, addrbuf, me->p_no,
		    "It's %d:%02d%s, time to die.  Approximate queue size: %d.",
		    hour, 
		    tmstruct->tm_min,
                    tmstruct->tm_hour >= 12 ? "pm" : "am", 
		    queues[QU_PICKUP].count);
	    }
	    else
		pmessage2(0, MALL, addrbuf, me->p_no,
		    "It's %d:%02d%s, time to die.", 
		    hour,
		    tmstruct->tm_min,
                    tmstruct->tm_hour >= 12 ? "pm" : "am");
	}
	else if (tno == 5) {		/* change 7/27/91 TC */
	    if (players[disp].p_status != PFREE) {
		pmessage2(0, MALL, addrbuf, me->p_no,
		    "%c%c has been targeted for termination.",
                    teamlet[players[disp].p_team],
                    shipnos[players[disp].p_no]);
	    }
	}

	/* NBT added...nicer output I think */

	if (lastrank != mystats->st_rank) {
	  pmessage2(0, MALL | MJOIN, addrbuf, me->p_no,
	        "%.16s (%2.2s) promoted to %s (%.16s@%.32s)",
		me->p_name,
                me->p_mapchars,
                ranks[me->p_stats.st_rank].name,
                me->p_login,
		me->p_full_hostname);
	} else {
	/* new-style join message 4/13/92 TC */
	  pmessage2(0, MALL | MJOIN, addrbuf, me->p_no,
	        "%s %.16s is now %2.2s (%.16s@%s%.32s)", 
		ranks[me->p_stats.st_rank].name, 
		me->p_name, 
		me->p_mapchars, 
		me->p_login,
#ifdef IP_CHECK_DNS
        (!whitelisted && !is_robot(me) && strcmp(me->p_full_hostname, me->p_dns_hostname)) ? "=" : "",
#else
        "",
#endif
		me->p_full_hostname);
#ifdef IP_CHECK_DNS_VERBOSE
	  if (!whitelisted && !is_robot(me) && strcmp(me->p_full_hostname, me->p_dns_hostname))
    	    pmessage(0, MALL, "GOD->ALL",
		"[DNS Mismatch] %s is %s", me->p_mapchars, me->p_dns_hostname);
#endif
	}

	lastrank = mystats->st_rank;
    }

    me->p_fuel = myship->s_maxfuel;
    delay = 0;
}

/* my idea of avoiding using the war window 8/16/91 TC */
static void auto_peace(void)
{
    int i, num[MAXTEAM+1];	/* corrected 6/22/92 TC */
    struct player	*p;

    num[0] = num[FED] = num[ROM] = num[KLI] = num[ORI] = 0;
    for (i = 0, p = players; i < MAXPLAYER; i++, p++)
	if (p->p_status != PFREE)
	    num[p->p_team]++;
    if (status->tourn)
	me->p_hostile = 
	    ((FED * (num[FED] >= tournplayers)) |
	     (ROM * (num[ROM] >= tournplayers)) |
	     (KLI * (num[KLI] >= tournplayers)) |
	     (ORI * (num[ORI] >= tournplayers)));
    else
	me->p_hostile = FED|ROM|KLI|ORI;
}

/* code to avoid placing bots on top of others 1/23/92 TC */

#define INDEP (GWIDTH/3)	/* width of region they can enter */

static void placeIndependent(void)
{
    int i;
    struct player	*p;
    int good, failures;

    failures = 0;
    while (failures < 10) {	/* don't try too hard to get them in */
	p_x_y_go(me,
		 GWIDTH/2 + (random() % INDEP) - INDEP/2, /* middle 9th of */
		 GWIDTH/2 + (random() % INDEP) - INDEP/2); /* galaxy */
	good = 1;
	for (i = 0, p = players; i < MAXPLAYER; i++, p++)
	    if ((p->p_status != PFREE) && (p != me)) {

		/* was 5 tries, was < TRACTDIST 7/20/92 TC */
		if ((ABS(p->p_x - me->p_x) < 2*TRACTDIST) && /* arbitrary def */
		    (ABS(p->p_y - me->p_y) < 2*TRACTDIST)) { /* of too close */
		    failures++;
		    good = 0;
		    break;
		}
	    }
	if (good) return;
    }
    ERROR(2,("Couldn't place the bot successfully.\n"));
}
