/*
 * modify.c
 * (adapted, more or less, from option.c)
 */
#include "copyright.h"

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

static int notdone;		/* not done flag */
static int mode;		/* 0=player, 1=planet */
static int nummodify;		/* either NUMPMODIFY or NUMPLMODIFY */
static struct player *mpp;	/* pointer to player being modified */
static struct planet *mplp;	/* pointer to planet being modified */
static struct player pl;	/* local copy of player */
static struct planet pln;	/* local copy of planet */

static modifyrefresh();

static char *ship_classes[] = {
	"Ship class: Scout (SC)",
	"Ship class: Destroyer (DD)",
	"Ship class: Cruiser (CA)",
	"Ship class: Battleship (BB)",
	"Ship class: Assault (AS)",
	"Ship class: Starbase (SB)",
#ifdef SGALAXY
	"Ship class: Galaxy (GA)",
#endif
#ifdef ATT
	"Ship class: AT&T",
#endif
	""};

static char *team_names[] = {
	"Team: Independent",
	"Team: Federation",
	"Team: Romulan",
	"Team: Klingon",
	"Team: Orion",
	""};
static short p_team, pl_team;

/* I don't know if this is the best way to implement this, but it's easy */
/* (assumes PLREPAIR=0x10, PLFUEL=0x20, PLAGRI=0x40)                     */
static char *planet_features[] = {
	"Features: None",		/* 000 */
	"Features: REPAIR",		/* 001 */
	"Features: FUEL",		/* 010 */
	"Features: REPAIR FUEL",	/* 011 */
	"Features: AGRI",		/* 100 */
	"Features: REPAIR AGRI",	/* 101 */
	"Features: FUEL AGRI",		/* 110 */
	"Features: REPAIR FUEL AGRI",	/* 111 */
	""};
static unsigned short pl_flags;

typedef enum MOD_TYPE_T { M_MULTI, M_ACTION, M_STRING } MOD_TYPE;
struct modify {
    int num;
    MOD_TYPE type;
    char *text;
    void *status;
};

/* only the M_ACTION ones really need constants */
#define PMOD_NAME	0
#define PMOD_TEAM	1
#define PMOD_CLASS	2
#define PMOD_HEAL	3
#define PMOD_HELP	4
#define PMOD_HARM	5
#define PMOD_HOSE	6
#define PMOD_COOL	7		/* TSH 2/93 */
#define PMOD_PLKILLS	8
#define PMOD_MIKILLS	9
#define PMOD_TOGSHLD	10
#define PMOD_TOGCLOAK	11		/* TSH 4/93 */
#define PMOD_NUKE	12
#define PMOD_EJECT	13
#define PMOD_NOORBIT	14
#define PMOD_PACIFY	15
#define PMOD_GHOSTBUST	16		/* TSH 2/93 */
#define PMOD_FREE	17		/* TSH 2/93 */
#define PMOD_DONE	18

#ifndef GHOSTTIME
#define GHOSTTIME       (30 * 1000000 / UPDATE) /* 30 secs, TSH 2/93 */
#endif

static struct modify pmodify[] = {
    { PMOD_NAME,	M_STRING,	"Player name", (void *) pl.p_name },
    { PMOD_TEAM,	M_MULTI,	(char *) team_names, (void *) &p_team },
    { PMOD_CLASS,	M_MULTI,	(char *) ship_classes, (void *) &(pl.p_ship.s_type) },
    { PMOD_HEAL,	M_ACTION,	"Heal", NULL },
    { PMOD_HELP,	M_ACTION,	"Help", NULL },
    { PMOD_HARM,	M_ACTION,	"Harm", NULL },
    { PMOD_HOSE,	M_ACTION,	"Hose", NULL },
    { PMOD_COOL,	M_ACTION,	"Cool weapons", NULL },
    { PMOD_PLKILLS,	M_ACTION,	"Kills +1", NULL },
    { PMOD_MIKILLS,	M_ACTION,	"Kills -1", NULL },
    { PMOD_TOGSHLD,	M_ACTION,	"Toggle shields", NULL },
    { PMOD_TOGCLOAK,	M_ACTION,	"Toggle cloak", NULL },
    { PMOD_NUKE,	M_ACTION,	"Nuke", NULL },
    { PMOD_EJECT,	M_ACTION,	"EJECT PLAYER", NULL },
    { PMOD_NOORBIT,	M_ACTION,	"Knock out of orbit", NULL },
    { PMOD_PACIFY,	M_ACTION,	"Make peaceful", NULL },
    { PMOD_GHOSTBUST,	M_ACTION,	"GHOSTBUST", NULL },
    { PMOD_FREE,	M_ACTION,	"FREE SLOT", NULL },
    { PMOD_DONE,	M_ACTION,	"Done", NULL },
    { -1, -1, NULL, NULL }
};
#define NUMPMODIFY ((sizeof(pmodify)/sizeof(pmodify[0]))-1)

#define PLMOD_NAME	0
#define PLMOD_TEAM	1
#define PLMOD_FEATURES	2
#define PLMOD_PLARM	3
#define PLMOD_MIARM	4
#define PLMOD_DESTROY	5
#define PLMOD_DONE	6

static struct modify plmodify[] = {
    { PLMOD_NAME,	M_STRING,	"Planet name", (void *) pln.pl_name },
    { PLMOD_TEAM,	M_MULTI,	(char *) team_names, (void *) &pl_team },
    { PLMOD_FEATURES,	M_MULTI,	(char *) planet_features, (void *) &pl_flags },
    { PLMOD_PLARM,	M_ACTION,	"Armies +5", NULL },
    { PLMOD_MIARM,	M_ACTION,	"Armies -5", NULL },
    { PLMOD_DESTROY,	M_ACTION,	"Destroy", NULL },
    { PLMOD_DONE,	M_ACTION,	"Done", NULL },
    { -1, -1, NULL, NULL }
};
#define NUMPLMODIFY ((sizeof(plmodify)/sizeof(plmodify[0]))-1)

#define MAXSTRING	15	/* only strings are player & planet names */

#define MODIFYBORDER	2
#define MODIFYLEN	35

/* Set up the modify window */
modifywindow(target)
struct obtype *target;
{
    register int i;
    char buf[BUFSIZ];

    /* Init not done flag */
    notdone = 1;

    /* locate the player's struct */
    if (target->o_type == PLAYERTYPE) {
	mode = 0;
	nummodify = NUMPMODIFY;
	mpp = &players[target->o_num];
	sprintf(buf, "Modifying player %s (%s)", mpp->p_name, mpp->p_mapchars);
	warning(buf);
	memcpy(&pl, mpp, sizeof(struct player));

	p_team = 0;
	switch (mpp->p_team) {
	case ORI:
	    p_team++;
	case KLI:
	    p_team++;
	case ROM:
	    p_team++;
	case FED:
	    p_team++;
	case NOBODY:
	    break;
	default:
	    fprintf(stderr,"Internal error: mpp->p_team is from mars (%x)\n",
		mpp->p_team);
	}
    } else {
	mode = 1;
	nummodify = NUMPLMODIFY;
	mplp = &planets[target->o_num];
	sprintf(buf, "Modifying planet %s", mplp->pl_name);
	warning(buf);
	memcpy(&pln, mplp, sizeof(struct planet));

	pl_team = 0;
	switch (mplp->pl_owner) {
	case ORI:
	    pl_team++;
	case KLI:
	    pl_team++;
	case ROM:
	    pl_team++;
	case FED:
	    pl_team++;
	case NOBODY:
	    break;
	default:
	    fprintf(stderr,"Internal error: mplp->pl_owner is from mars (%x)\n",
		mplp->pl_owner);
	}
	pl_flags = (mplp->pl_flags & (PLREPAIR|PLFUEL|PLAGRI)) >> 4;
    }

    /* Create window big enough to hold modify windows */
    if (modifyWin==NULL) {
	modifyWin = W_MakeMenu("modify", WINSIDE+10, -BORDER+10, MODIFYLEN, 
	  MAX(NUMPMODIFY,NUMPLMODIFY), baseWin, MODIFYBORDER);

    }
    if(mode == 0)		/* player modify */
       W_ResizeMenu(modifyWin, MODIFYLEN, NUMPMODIFY);
    else			/* planet modify */
       W_ResizeMenu(modifyWin, MODIFYLEN, NUMPLMODIFY);
	 
    for (i=0; i< (mode ? NUMPLMODIFY : NUMPMODIFY); i++) {
	if (i >= nummodify)
	    W_WriteText(modifyWin, 0, i, textColor, "", 0, 0);	/* blank */
	else
	    modifyrefresh(mode ? &(plmodify[i]) : &(pmodify[i]));
    }

    /* Map window */
    W_MapWindow(modifyWin);
}

/* Refresh the modify window given by the modify struct */
static
modifyrefresh(op)
register struct modify *op;
{
    char buf[BUFSIZ];

    switch (op->type) {
    case M_MULTI:
	/* note I expect to index on a short, not an int */
	sprintf(buf, "%s", ((char **) op->text)[*((short *) op->status)]);
	break;
    case M_ACTION:
	sprintf(buf, "%s", op->text);
	break;
    case M_STRING:
	sprintf(buf, "%s: %s_", op->text, (char *) op->status);
	break;
    default:
	fprintf(stderr, "Internal error: bad op->type\n");
    }

    W_WriteText(modifyWin, 0, op->num, textColor, buf, strlen(buf), 0);
}

modifyaction(data)
W_Event *data;
{
    register struct modify *op;
    char buf[BUFSIZ];
    int i;
    register char *cp;

    if (data->y >= nummodify) return(0);
    op= (mode ? &(plmodify[data->y]) : &(pmodify[data->y]));
    switch (op->type) {
    case M_STRING:
	if (data->type == W_EV_BUTTON) return(0);
	switch (data->key) {
	case '\b':
	case '\177':
	    cp = (char *) op->status;
	    i = strlen(cp);
	    if (i > 0) {
		cp += i - 1;
		*cp = '\0';
	    }
	    break;

	case '\027':	/* word erase */
	    cp = (char *) op->status;
	    i = strlen(cp);
	    /* back up over blanks */
	    while (--i >= 0 && isspace(cp[i]))
		;
	    i++;
	    /* back up over non-blanks */
	    while (--i >= 0 && !isspace(cp[i]))
		;
	    i++;
	    cp[i] = '\0';
	    break;

	case '\025':
	case '\030':
	    ((char *) op->status)[0] = '\0';
	    break;

	default:
	    if (data->key < 32 || data->key > 127) break;
	    cp = (char *) op->status;
	    i = strlen(cp);
	    if (i < MAXSTRING && !iscntrl(data->key)) {
		cp += i;
		cp[1] = '\0';
		cp[0] = data->key;
	    } else
		W_Beep();
		break;
	}
	break;

    case M_MULTI:
	if (data->type == W_EV_KEY) return(0);
#define IDX	*((short *) op->status)
	( IDX )++;
	if (( *((char **) op->text)[IDX] ) == '\0')
	    ( IDX ) = 0;
#undef IDX
	break;

    case M_ACTION:
	if (data->type == W_EV_KEY) return(0);

	if (!mode) {
	    switch (op->num) {
	    case PMOD_HEAL:
		pl.p_shield = mpp->p_shield = mpp->p_ship.s_maxshield;
		pl.p_damage = mpp->p_damage = 0;
		pl.p_fuel   = mpp->p_fuel   = mpp->p_ship.s_maxfuel;
		mpp->p_wtemp = mpp->p_etemp = 0;
		mpp->p_wtime = mpp->p_etime = 0;
		break;
	    case PMOD_HELP:
		pl.p_shield += pl.p_ship.s_maxshield / 10;
		if (pl.p_shield > pl.p_ship.s_maxshield)
		    pl.p_shield = pl.p_ship.s_maxshield;
		mpp->p_shield = pl.p_shield;
		pl.p_damage -= pl.p_ship.s_maxshield / 10;
		if (pl.p_damage < 0)
		    pl.p_damage = 0;
		mpp->p_damage = pl.p_damage;
		pl.p_fuel += pl.p_ship.s_maxfuel / 10;
		if (pl.p_fuel > pl.p_ship.s_maxfuel)
		    pl.p_fuel = pl.p_ship.s_maxfuel;
		mpp->p_fuel = pl.p_fuel;
		break;
	    case PMOD_COOL:			/* TSH 2/93 */
		mpp->p_wtemp = 0;
		break;

	    case PMOD_HARM:
		pl.p_shield -= pl.p_ship.s_maxshield / 10;
		if (pl.p_shield < 0)
		    pl.p_shield = 0;
		mpp->p_shield = pl.p_shield;
		pl.p_damage += pl.p_ship.s_maxshield / 10;
		if (pl.p_damage > pl.p_ship.s_maxshield)
		    pl.p_damage = pl.p_ship.s_maxshield;
		mpp->p_damage = pl.p_damage;
		pl.p_fuel -= pl.p_ship.s_maxfuel / 10;
		if (pl.p_fuel < 0)
		    pl.p_fuel = 0;
		mpp->p_fuel = pl.p_fuel;
		break;
	    case PMOD_HOSE:
		pl.p_shield = mpp->p_shield = 0;
		pl.p_damage = mpp->p_damage = mpp->p_ship.s_maxdamage;
		pl.p_fuel = mpp->p_fuel = 0;
		break;
	    case PMOD_NUKE:
		/* from xtkill.c */
		mpp->p_ship.s_type = STARBASE;
		sprintf(buf, "%s (%2s) was utterly obliterated.",
		    mpp->p_name, mpp->p_mapchars);
		pmessage(buf, 0, MALL);
		mpp->p_explode=10;
		mpp->p_status=PEXPLODE;
		mpp->p_whydead=KPROVIDENCE;
		mpp->p_whodead=0;
		notdone = 0;	/* he's dead, stop editing */
		break;
	    case PMOD_EJECT:
		mpp->p_ship.s_type = STARBASE;
		sprintf(buf, "%s (%2s) was ejected from the game.",
		    mpp->p_name, mpp->p_mapchars);
		pmessage(buf, 0, MALL);
		mpp->p_explode=10;
		mpp->p_status=PEXPLODE;
		mpp->p_whydead=KQUIT;	/* make him self destruct */
		mpp->p_whodead=0;
		notdone = 0;	/* he's history, stop editing */
		break;
	    case PMOD_PLKILLS:
		mpp->p_kills++;
		break;
	    case PMOD_MIKILLS:
		mpp->p_kills--;
		if (mpp->p_kills < 0) mpp->p_kills = 0;
		break;
	    case PMOD_TOGSHLD:
		if (mpp->p_flags & PFSHIELD) {
		    mpp->p_flags &= ~PFSHIELD;
		    pl.p_flags &= ~PFSHIELD;
		} else {
		    mpp->p_flags |= PFSHIELD;
		    pl.p_flags |= PFSHIELD;
		}
		break;
	    case PMOD_TOGCLOAK:
		if (mpp->p_flags & PFCLOAK) {
		    mpp->p_flags &= ~PFCLOAK;
		    pl.p_flags &= ~PFCLOAK;
		} else {
		    mpp->p_flags |= PFCLOAK;
		    pl.p_flags |= PFCLOAK;
		}
		break;
	    case PMOD_NOORBIT:
		mpp->p_flags &= ~PFORBIT;
		pl.p_flags &= ~PFORBIT;
		break;
	    case PMOD_PACIFY:
		mpp->p_war = mpp->p_hostile = mpp->p_swar = 0;
		pl.p_war = pl.p_hostile = pl.p_swar = 0;
		break;
	    case PMOD_GHOSTBUST:
		if(mpp->p_status != PFREE)
		    mpp->p_ghostbuster = GHOSTTIME;
		break;

	    case PMOD_FREE:
		if (mpp->p_process != 0) {
		  ERROR(1,("xtkilling player %i, process %i\n",mpp->p_no,mpp->p_process));
                  if (kill(mpp->p_process, SIGTERM) < 0) freeslot(mpp);
		} else {
		  freeslot(mpp);
		}
		break;

	    case PMOD_DONE:
		notdone = 0;
		break;
	    default:
		fprintf(stderr, "Internal error: unknown p action item\n");
	    }
	} else {
	    switch (op->num) {
	    case PLMOD_PLARM:
		mplp->pl_armies += 5;
		break;
	    case PLMOD_MIARM:
		mplp->pl_armies -= 5;
		if (mplp->pl_armies < 1) mplp->pl_armies = 1;
		break;
	    case PLMOD_DESTROY:
		mplp->pl_armies = 0;
		mplp->pl_owner = NOBODY;
		pl_team = 0;
		modifyrefresh(&(plmodify[PLMOD_TEAM]));
		/* could this cause genocide? */
		break;
	    case PLMOD_DONE:
		notdone = 0;
		break;
	    default:
		fprintf(stderr, "Internal error: unknown pl action item\n");
	    }
	}
	break;

    default:
	fprintf(stderr, "Internal error: bad type %d\n", op->type);
    }

    if (!notdone)
	modifydone();
    else
	modifyrefresh(op);
}

modifydone()
{
    /* Unmap window */
    W_UnmapWindow(modifyWin);

    if (mode) {
	/* update non-action items (planet name, team, flags) */
	strcpy(mplp->pl_name, pln.pl_name);
	mplp->pl_namelen = strlen(mplp->pl_name);
	if (!pl_team)
	    mplp->pl_owner = NOBODY;
	else
	    mplp->pl_owner = 1 << (pl_team-1);
	mplp->pl_flags &= ~(PLREPAIR|PLFUEL|PLAGRI);
	mplp->pl_flags |= (pl_flags << 4);

    } else {
	/*
	 * Now, update any non-action items (player name, ship type, etc).
	 *
	 * Note that the shield/damage/fuel is scaled proportionately at the
	 * time that "done" is clicked.  The "action items" must therefore
	 * affect pl * and mpp at the time they are pressed; otherwise you
	 * could heal somebody * and then have the effect reversed when you
	 * pressed "done".
	 */
	strcpy(mpp->p_name, pl.p_name);
/*	mpp->p_flags = pl.p_flags;	/* transfer flags */

	if (pl.p_ship.s_type != mpp->p_ship.s_type) {
	    /*this can be bad if the guy dies and comes back as something else*/
	    getship(&(mpp->p_ship), pl.p_ship.s_type);
	    if ((pl.p_ship.s_type != STARBASE) && (pl.p_ship.s_type != ATT) &&
					    mpp->p_kills < (float) plkills) {
		mpp->p_ship.s_plasmacost = -1;
	    }

	    /* give proportionate shield/damage */
	    mpp->p_shield = (int) ( (double) mpp->p_ship.s_maxshield *
		    (double) mpp->p_shield / (double) pl.p_ship.s_maxshield );
	    mpp->p_damage = (int) ( (double) mpp->p_ship.s_maxdamage *
		    (double) mpp->p_damage / (double) pl.p_ship.s_maxdamage );
	    mpp->p_fuel = (int) ( (double) mpp->p_ship.s_maxfuel *
		    (double) mpp->p_fuel / (double) pl.p_ship.s_maxfuel );

	    /* reduce speed, if necessary */
	    if (mpp->p_speed > mpp->p_ship.s_maxspeed)
		mpp->p_speed = mpp->p_ship.s_maxspeed;

	    /* preserve certain optional robot features */
	    if (mpp->p_flags & PFROBOT) {
		if (!pl.p_ship.s_phasercost)
		    mpp->p_ship.s_phasercost = 0;
		if (!pl.p_ship.s_torpcost)
		    mpp->p_ship.s_torpcost = 0;
		if (!pl.p_ship.s_cloakcost)
		    mpp->p_ship.s_cloakcost = 0;
		mpp->p_ship.s_torpturns = pl.p_ship.s_torpturns;
	    }
	}

	if (!p_team)
	    mpp->p_team = NOBODY;
	else
	    mpp->p_team = 1 << (p_team-1);
    }
}

