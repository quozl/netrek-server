/*
 * redraw.c
 */
#include "copyright.h"

#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

static int clearzone[4][(MAXTORP + 1) * MAXPLAYER + 
			(MAXPLASMA + 1) * MAXPLAYER + MAXPLANETS +1];
static int clearcount;
static int clearline[4][MAXPLAYER + 2*MAXPLAYER];
static int clearlcount;
static int mclearzone[6][MAXPLAYER+1];	/* For map window (+1 for xsg) */

/* TSH 2/10/93 */
static int mclearlcount;
static int mclearline[4][MAXPLAYER + 2*MAXPLAYER];

static int mclearpcount;
static int mclearpoint[2][(MAXTORP + 1)*MAXPLAYER];

/*
 * This is PLREDRAW for every planet.  The server's copy isn't useful,
 * because it doesn't actually display anything, and therefore doesn't worry
 * about certain things.  So, we need to keep our own copy.
 */
int plredraw[MAXPLANETS];	/* PLREDRAW for every planet */

static short nplayers;

#define xsg_width 20
#define xsg_height 20

redraw()
{
    /* erase warning line if necessary */
    if ((warntimer <= udcounter) && (warncount > 0)) {
	W_ClearArea(warnw, 5, 5, W_Textwidth * warncount, W_Textheight, backColor);
	warncount = 0;
    }

    if (W_FastClear) {
	W_ClearWindow(w);
	clearcount=0;
	clearlcount=0;
    } else {
	/* TSH 2/10/93 */
	while (clearcount) {
	    clearcount--;
	    W_CacheClearArea(w, clearzone[0][clearcount], 
		clearzone[1][clearcount],
		clearzone[2][clearcount], clearzone[3][clearcount]);
	}
	while (clearlcount) {
	    clearlcount--;
	    W_CacheLine(w, clearline[0][clearlcount], clearline[1][clearlcount],
		clearline[2][clearlcount], clearline[3][clearlcount],
		backColor);
	}
	W_FlushClearAreaCache(w);
	W_FlushLineCaches(w);

	while (mclearlcount){
	    mclearlcount--;
	    W_CacheLine(mapw, mclearline[0][mclearlcount], 
		mclearline[1][mclearlcount], mclearline[2][mclearlcount], 
		mclearline[3][mclearlcount],
		backColor);
	}
	while (mclearpcount){
	    mclearpcount--;
	    W_CachePoint(mapw, mclearpoint[0][mclearpcount], 
		mclearpoint[1][mclearpcount], backColor);
	}
	W_FlushLineCaches(mapw);
	W_FlushPointCaches(mapw);
    }

    local();	/* redraw local window */

    if (mapmode) map();

    if (W_IsMapped(statwin)){
	updateStats();
    }

    /* need a status line but we'll make do with the bottom of the local
       display for now */
    if(runclock)
       run_clock(1);	/* isae */
    if(record)
      show_record(1);
    if(playback)
      show_playback(1);

    if(playback && extracting)
       show_extracting(extracting); /* 7/27/93 BM */

    /* W_Flush(); */
}

W_Icon planetBitmap(p)
register struct planet *p;
{
    int i;

    if (showlocal==2) {
	return(bplanets[0]);
    } else {
	if (showlocal==1) {
	    i=0;
	    if (p->pl_armies > 4) i+=4;
	    if (p->pl_flags & PLREPAIR) i+=2;
	    if (p->pl_flags & PLFUEL) i+=1;
	    return(bplanets2[i]);
	} else {
	    return(bplanets[remap[p->pl_owner]]);
	}
    }
}


W_Icon planetmBitmap(p)
register struct planet *p;
{
    int i;

    if (showgalactic==2) {
	return(mbplanets[0]);
    } else {
	if (showgalactic==1) {
	    i=0;
	    if (p->pl_armies > 4) i+=4;
	    if (p->pl_flags & PLREPAIR) i+=2;
	    if (p->pl_flags & PLFUEL) i+=1;
	    return(mbplanets2[i]);
	} else {
	    return(mbplanets[remap[p->pl_owner]]);
	}
    }
}

local()
{
    register int h, i;
    register struct player *j;
    register struct torp *k;
    register struct planet *l;
    register struct phaser *php;
    register struct torp *pt;

    int dx, dy;
    int view, fin;
    char idbuf[2];
    W_Icon (*ship_bits)[VIEWS];

    /* Kludge to try to fix missing ID chars on tactical (short range) display. */
    idbuf[0] = '0';
    idbuf[1] = '\0';
    /* Draw Planets */
    view = SCALE * WINSIDE / 2;
    for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
	dx = l->pl_x - me->p_x;
	dy = l->pl_y - me->p_y;
	if (dx > view || dx < -view || dy > view || dy < -view)
	    continue;
	dx = dx / SCALE + WINSIDE / 2;
	dy = dy / SCALE + WINSIDE / 2;
	W_WriteBitmap(dx - (planet_width/2), dy - (planet_height/2),
	    planetBitmap(l), planetColor(l));
	if (namemode) {
	    W_MaskText(w, dx - (planet_width/2), dy + (planet_height/2),
		planetColor(l), l->pl_name, l->pl_namelen,
		planetFont(l));
	    clearzone[0][clearcount] = dx - (planet_width/2);
	    clearzone[1][clearcount] = dy + (planet_height/2);
	    clearzone[2][clearcount] = W_Textwidth * l->pl_namelen;
	    clearzone[3][clearcount] = W_Textheight;
	    clearcount++;
	}
	clearzone[0][clearcount] = dx - (planet_width/2);
	clearzone[1][clearcount] = dy - (planet_height/2);
	clearzone[2][clearcount] = planet_width;
	clearzone[3][clearcount] = planet_height;
	clearcount++;
    }

    /* Show xsg position */
    if (show_xsg_posn) {
	dx = dy = WINSIDE / 2;
	W_WriteBitmap(dx - (xsg_width/2),
	    dy - (xsg_height/2), xsgbits, myColor);
	clearzone[0][clearcount] = dx - (xsg_width/2);
	clearzone[1][clearcount] = dy - (xsg_height/2);
	clearzone[2][clearcount] = xsg_width;
	clearzone[3][clearcount] = xsg_height;
	clearcount++;
    }

    /* Draw ships */
    nplayers = 0;
    view = SCALE * WINSIDE / 2;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        int tx, ty;
	if ((j->p_status != PALIVE) && (j->p_status != PEXPLODE) &&
	    (j->p_status != PDEAD))
	    continue;
	nplayers++;

	dx = j->p_x - me->p_x;
	dy = j->p_y - me->p_y;
	if (dx > view || dx < -view || dy > view || dy < -view) 
	    continue;
	dx = dx / SCALE + WINSIDE / 2;
	dy = dy / SCALE + WINSIDE / 2;
	if (j->p_flags & PFCLOAK && (j->p_cloakphase == (CLOAK_PHASES-1))) {
	    if (myPlayer(j)) {
		W_WriteBitmap(dx - (cloak_width/2), dy - (cloak_height/2),
		    cloakicon, myColor);
		clearzone[0][clearcount] = dx - (shield_width/2);
		clearzone[1][clearcount] = dy - (shield_height/2);
		clearzone[2][clearcount] = shield_width;
		clearzone[3][clearcount] = shield_height;
		clearcount++;
	    }
/*	    continue;*/
	}
	if (j->p_status == PALIVE) {
	    switch (j->p_team) {
		case NOBODY:		/* independent/Iggy */
		    ship_bits = ind_bitmaps;
		    break;
		case FED:
		    ship_bits = fed_bitmaps;
		    break;
		case ROM:
		    ship_bits = rom_bitmaps;
		    break;
		case KLI:
		    ship_bits = kli_bitmaps;
		    break;
		default:
		    ship_bits = ori_bitmaps;
		    break;
	    }

	    clearzone[0][clearcount] = dx - (shield_width/2);
	    clearzone[1][clearcount] = dy - (shield_height/2);
	    clearzone[2][clearcount] = shield_width;
	    clearzone[3][clearcount] = shield_height;
	    clearcount++;

	    W_WriteBitmap(dx - (j->p_ship.s_width/2),
		dy - (j->p_ship.s_height/2),
		ship_bits[j->p_ship.s_type][rosette(j->p_dir)],
		playerColor(j));

            if (j->p_cloakphase > 0) {
		W_WriteBitmap(dx - (cloak_width/2),
		    dy - (cloak_height/2), cloakicon, W_Grey);
		/*		TSH 
		continue;
		*/
	    }

	    if (showShields && (j->p_flags & PFSHIELD)) {
		W_WriteBitmap(dx - (shield_width/2),
		    dy - (shield_height/2), shield, playerColor(j));
	    }

	    idbuf[0] = *(shipnos+j->p_no);

	    W_MaskText(w, dx + (j->p_ship.s_width/2), 
		dy - (j->p_ship.s_height/2), 
		(j->p_cloakphase > 0)?W_Grey:playerColor(j),
		idbuf, 1, shipFont(j));
	       
	    clearzone[0][clearcount] = dx + (j->p_ship.s_width/2);
	    clearzone[1][clearcount] = dy - (j->p_ship.s_height/2);
	    clearzone[2][clearcount] = W_Textwidth;
	    clearzone[3][clearcount] = W_Textheight;
	    clearcount++;
	}
	else if (j->p_status == PEXPLODE) {

	    fin=get_expview(j);
	    if (fin<EX_FRAMES || 
		(fin<SBEXPVIEWS && j->p_ship.s_type==STARBASE)) {

		if (j->p_ship.s_type == STARBASE) {
		    W_WriteBitmap(dx - (sbexp_width/2), 
			dy - (sbexp_height/2), sbexpview[fin], 
			playerColor(j));
		    clearzone[0][clearcount] = dx - (sbexp_width/2);
		    clearzone[1][clearcount] = dy - (sbexp_height/2);
		    clearzone[2][clearcount] = sbexp_width;
		    clearzone[3][clearcount] = sbexp_height;
		} else {
		    W_WriteBitmap(dx - (ex_width/2), dy - (ex_height/2),
			expview[fin], playerColor(j));
		    clearzone[0][clearcount] = dx - (ex_width/2);
		    clearzone[1][clearcount] = dy - (ex_height/2);
		    clearzone[2][clearcount] = ex_width;
		    clearzone[3][clearcount] = ex_height;
		}
	        clearcount++;
	    }
	}
	/* Now draw his phaser (if it exists) */
	php = &phasers[j->p_no];
	if (php->ph_status != PHFREE) {
	    if (php->ph_status == PHMISS) {
		/* Here I will have to compute end coordinate */
		tx = j->p_x + PHASEDIST * j->p_ship.s_phaserdamage / 100 * Cos[php->ph_dir];
		ty = j->p_y + PHASEDIST * j->p_ship.s_phaserdamage / 100 * Sin[php->ph_dir];
		tx = (tx - me->p_x) / SCALE + WINSIDE / 2;
		ty = (ty - me->p_y) / SCALE + WINSIDE / 2;
	    } else if (php->ph_status == PHHIT2) {
		tx = (php->ph_x - me->p_x) / SCALE + WINSIDE / 2;
		ty = (php->ph_y - me->p_y) / SCALE + WINSIDE / 2;
	    } else { /* Start point is dx, dy */
		tx = (players[php->ph_target].p_x - me->p_x) /
		    SCALE + WINSIDE / 2;
		ty = (players[php->ph_target].p_y - me->p_y) /
		    SCALE + WINSIDE / 2;
	    }

	    /* Scott: 9/30/90: 5 lines */
	    if ((php->ph_fuse % 2) == 1)
		W_CacheLine(w, dx, dy, tx, ty, foreColor);
	    else
		W_CacheLine(w, dx, dy, tx, ty, shipCol[remap[j->p_team]]);

	    /* OLD: W_MakeLine(w,dx, dy, tx, ty,phaserColor(php)); */

	    clearline[0][clearlcount] = dx;
	    clearline[1][clearlcount] = dy;
	    clearline[2][clearlcount] = tx;
	    clearline[3][clearlcount] = ty;
	    clearlcount++;
	}
	if(j->p_status == PDEAD)
	    continue;

	/* ATM - show tractor/pressor beams */
	/* (works for xsg since we have p_tractor) */
	if (j->p_flags & PFTRACT || j->p_flags & PFPRESS) {
	    double theta;
	    unsigned char dir;
	    int lx[2], ly[2];

	    tx = (players[j->p_tractor].p_x - me->p_x) / SCALE + WINSIDE / 2;
	    ty = (players[j->p_tractor].p_y - me->p_y) / SCALE + WINSIDE / 2;

	    if (tx == dx && ty == dy)
		continue;		/* this had better be last in for(..) */

#define XPI	3.1415926
	    theta = atan2((double) (tx - dx), (double) (dy - ty)) + XPI / 2.0;
	    dir = (unsigned char) (theta / XPI * 128.0);

	    lx[0] = tx + (Cos[dir] * (shield_width/2));
	    ly[0] = ty + (Sin[dir] * (shield_width/2));
	    lx[1] = tx - (Cos[dir] * (shield_width/2));
	    ly[1] = ty - (Sin[dir] * (shield_width/2));
#undef XPI
	    if(j->p_flags & PFPRESS) {	/* TSH 3/10/93 */
	       W_MakeTractLine(w, dx, dy, lx[0], ly[0], W_Yellow);
	       W_MakeTractLine(w, dx, dy, lx[1], ly[1], W_Yellow);
	    }
	    else {
	       W_MakeTractLine(w, dx, dy, lx[0], ly[0], W_Green);
	       W_MakeTractLine(w, dx, dy, lx[1], ly[1], W_Green);
	    }

	    clearline[0][clearlcount] = dx;
	    clearline[1][clearlcount] = dy;
	    clearline[2][clearlcount] = lx[0];
	    clearline[3][clearlcount] = ly[0];
	    clearlcount++;
	    clearline[0][clearlcount] = dx;
	    clearline[1][clearlcount] = dy;
	    clearline[2][clearlcount] = lx[1];
	    clearline[3][clearlcount] = ly[1];
	    clearlcount++;
	}
    }
    /* Draw torps */
    view = SCALE * WINSIDE / 2;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	if (!j->p_ntorp)
	    continue;
	for (h = 0, k = &torps[MAXTORP * i + h]; h < MAXTORP; h++, k++) {
	    if (!k->t_status)
		continue;
	    dx = k->t_x - me->p_x;
	    dy = k->t_y - me->p_y;
            if (dx > view || dx < -view || dy > view || dy < -view)
   		continue;

	    dx = dx / SCALE + WINSIDE / 2;
	    dy = dy / SCALE + WINSIDE / 2;
	    if (k->t_status == TEXPLODE){
		fin = get_texp(k);
		if(fin < NUMDETFRAMES){
		   W_WriteBitmap(dx - (cloud_width/2), dy - (cloud_height/2),
		       cloud[fin], torpColor(k));
		   clearzone[0][clearcount] = dx - (cloud_width/2);
		   clearzone[1][clearcount] = dy - (cloud_height/2);
		   clearzone[2][clearcount] = cloud_width;
		   clearzone[3][clearcount] = cloud_height;
		   clearcount++;
		}
	    }
	    else
	    {
	        /* actually drawing a torp as two lines (much faster the 
  		   bitmap) -- TSH */
                W_CacheLine(w, dx-(mtorp_width/2), dy, dx+(mtorp_width/2), dy,
                  torpColor(k));
                W_CacheLine(w, dx, dy-(mtorp_width/2), dx, dy+(mtorp_width/2),
                  torpColor(k));

		/*
		W_WriteBitmap(dx - (mtorp_width/2), dy - (mtorp_height/2),
		    mtorp, torpColor(k));
		*/
		clearzone[0][clearcount] = dx - (mtorp_width/2);
		clearzone[1][clearcount] = dy - (mtorp_height/2);
		clearzone[2][clearcount] = mtorp_width;
		clearzone[3][clearcount] = mtorp_height;
		clearcount++;
	    }
	}
    }
    /* Draw plasma torps */
    view = SCALE * WINSIDE / 2;
    for (j=firstPlayer; j<=lastPlayer; j++) {
      if (j->p_nplasmatorp == 0)
	continue;
      for (pt=firstPlasmaOf(j); pt<=lastPlasmaOf(j); pt++) {
	if (pt->t_status == TFREE)
	  continue;
	dx = pt->t_x - me->p_x;
	dy = pt->t_y - me->p_y;
	if (dx > view || dx < -view || dy > view || dy < -view)
	  continue;
	dx = dx / SCALE + WINSIDE / 2;
	dy = dy / SCALE + WINSIDE / 2;
	if (pt->t_status == TEXPLODE) {
	  fin = get_texp(pt);

	  if (fin < NUMDETFRAMES) {
	    W_WriteBitmap(dx - (plasmacloud_width/2),
			  dy - (plasmacloud_height/2), 
			  plasmacloud[fin], plasmatorpColor(pt));
	    clearzone[0][clearcount] = dx - (plasmacloud_width/2);
	    clearzone[1][clearcount] = dy - (plasmacloud_height/2);
	    clearzone[2][clearcount] = plasmacloud_width;
	    clearzone[3][clearcount] = plasmacloud_height;
	    clearcount++;
	  }
	}
	else if ((pt->t_owner != me->p_no) && 
		 ((pt->t_war & me->p_team) ||
		  (players[pt->t_owner].p_team & me->p_war)))
	  {
	    W_WriteBitmap(dx - (eplasmatorp_width/2),
			  dy - (eplasmatorp_height/2), 
			  eplasmatorp, plasmatorpColor(pt));
	    clearzone[0][clearcount] = dx - (eplasmatorp_width/2);
	    clearzone[1][clearcount] = dy - (eplasmatorp_height/2);
	    clearzone[2][clearcount] = eplasmatorp_width;
	    clearzone[3][clearcount] = eplasmatorp_height;
	    clearcount++;
	  }
	else {
	  W_WriteBitmap(dx - (mplasmatorp_width/2),
			dy - (mplasmatorp_height/2), 
			mplasmatorp, plasmatorpColor(pt));
	  clearzone[0][clearcount] = dx - (mplasmatorp_width/2);
	  clearzone[1][clearcount] = dy - (mplasmatorp_height/2);
	  clearzone[2][clearcount] = mplasmatorp_width;
	  clearzone[3][clearcount] = mplasmatorp_height;
	  clearcount++;
	}
      }
    }

    /* Draw Edges */
    if (me->p_x < (WINSIDE / 2) * SCALE) {
	int	sy, ey;

	dx = (WINSIDE / 2) - (me->p_x) / SCALE;
	sy = (WINSIDE / 2) + (0 - me->p_y) / SCALE;
	ey = (WINSIDE / 2) + (GWIDTH - me->p_y) / SCALE;
	if (sy < 0) sy = 0;
	if (ey > WINSIDE - 1) ey = WINSIDE - 1;
	W_CacheLine(w, dx, sy, dx, ey, warningColor);
	clearline[0][clearlcount] = dx;
	clearline[1][clearlcount] = sy;
	clearline[2][clearlcount] = dx;
	clearline[3][clearlcount] = ey;
	clearlcount++;
    }
    if ((GWIDTH - me->p_x) < (WINSIDE / 2) * SCALE) {
	int	sy, ey;

	dx = (WINSIDE / 2) + (GWIDTH - me->p_x) / SCALE;
	sy = (WINSIDE / 2) + (0 - me->p_y) / SCALE;
	ey = (WINSIDE / 2) + (GWIDTH - me->p_y) / SCALE;
	if (sy < 0) sy = 0;
	if (ey > WINSIDE - 1) ey = WINSIDE - 1;
	W_CacheLine(w, dx, sy, dx, ey, warningColor);
	clearline[0][clearlcount] = dx;
	clearline[1][clearlcount] = sy;
	clearline[2][clearlcount] = dx;
	clearline[3][clearlcount] = ey;
	clearlcount++;
    }
    if (me->p_y < (WINSIDE / 2) * SCALE) {
	int	sx, ex;

	dy = (WINSIDE / 2) - (me->p_y) / SCALE;
	sx = (WINSIDE / 2) + (0 - me->p_x) / SCALE;
	ex = (WINSIDE / 2) + (GWIDTH - me->p_x) / SCALE;
	if (sx < 0) sx = 0;
	if (ex > WINSIDE - 1) ex = WINSIDE - 1;
	W_CacheLine(w, sx, dy, ex, dy, warningColor);
	clearline[0][clearlcount] = sx;
	clearline[1][clearlcount] = dy;
	clearline[2][clearlcount] = ex;
	clearline[3][clearlcount] = dy;
	clearlcount++;
    }
    if ((GWIDTH - me->p_y) < (WINSIDE / 2) * SCALE) {
	int	sx, ex;

	dy = (WINSIDE / 2) + (GWIDTH - me->p_y) / SCALE;
	sx = (WINSIDE / 2) + (0 - me->p_x) / SCALE;
	ex = (WINSIDE / 2) + (GWIDTH - me->p_x) / SCALE;
	if (sx < 0) sx = 0;
	if (ex > WINSIDE - 1) ex = WINSIDE - 1;
	W_CacheLine(w, sx, dy, ex, dy, warningColor);
	clearline[0][clearlcount] = sx;
	clearline[1][clearlcount] = dy;
	clearline[2][clearlcount] = ex;
	clearline[3][clearlcount] = dy;
	clearlcount++;
    }

    /* Change border color to signify alert status */

    if (oldalert != (me->p_flags & (PFGREEN|PFYELLOW|PFRED))) {
        oldalert = (me->p_flags & (PFGREEN|PFYELLOW|PFRED));
	switch (oldalert) {
	    case PFGREEN:
		W_ChangeBorder(baseWin, gColor);
		W_ChangeBorder(iconWin, gColor);
		break;
	    case PFYELLOW:
		W_ChangeBorder(baseWin, yColor);
		W_ChangeBorder(iconWin, yColor);
		break;
	    case PFRED:
		W_ChangeBorder(baseWin, rColor);
		W_ChangeBorder(iconWin, rColor);
		break;
	}
    }
    W_FlushLineCaches(w);
}

map()
{
    static int plRedrawCyc = 0;
    register int i, h;
    register struct player *j;
    register struct planet *l;
    register struct phaser *ph;
    register struct torp *k;
    int dx, dy, tx, ty;
    extern char *itoa();		/* playerlist.c */
    char	*s;

    /*
     * Slight problem introduced in xsg: normally PLREDRAW comes from the
     * server or checkRedraw, but now it comes from daemon.  Since it will
     * be set when we orbit a planet, the ship will NOT be redrawn if
     * mapmode == 1, but the planet will be.  This is annoying.  One solution
     * is to do a reverse mapping from redrawn planets to nearby ships and
     * to set redrawPlayer accordingly.  Another solution is to redraw planets
     * only every 5 cycles no matter what.
     *
     * The easiest solution is to just forget it and select "redraw galactic
     * map frequently"...
     */
    if (redrawall) W_ClearWindow(mapw);
    /* Erase ships */
    else for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	lastUpdate[i]++;
	/* Erase the guy if:
	 * redrawPlayer[i] is set and the mapmode setting allows it.
	 */
	if ( !redrawPlayer[i] || (mapmode==1 && lastUpdate[i]<5))
	    continue;
	lastUpdate[i]=0;
	/* Clear his old image... */
	if (mclearzone[2][i]) {
	    W_CacheClearArea(mapw, mclearzone[0][i], mclearzone[1][i],
		mclearzone[2][i], mclearzone[3][i]);
	    /* Redraw the hole just left next update */
	    checkRedraw(mclearzone[4][i], mclearzone[5][i]);
	    mclearzone[2][i]=0;
	}
    }

    /* clear xsg marker if we're drawing planets this phase */
    /* (yes, this a kludge) */
    i = MAXPLAYER;
    if (redrawall || (mapmode != 1) || (plRedrawCyc >= 4)) {
	if (mclearzone[2][i]) {
	    W_CacheClearArea(mapw, mclearzone[0][i], mclearzone[1][i],
		mclearzone[2][i], mclearzone[3][i]);
	    /* Redraw the hole just left next update */
	    checkRedraw(mclearzone[4][i], mclearzone[5][i]);
	    mclearzone[2][i]=0;
	}
    }
    W_FlushClearAreaCache(mapw);

    /* Draw Planets */
    if (redrawall || (mapmode != 1) || (++plRedrawCyc >= 5)) {
	for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
	    char buf[80];
	    if (!(l->pl_flags & PLREDRAW) && (!redrawall) && (!plredraw[i])
	       && !updatePlanet[i])
		continue;
	    updatePlanet[i] = 0;

/*	    l->pl_flags &= ~PLREDRAW;*/ 	/* server does this */
	    plredraw[i] = 0;
	    dx = l->pl_x * WINSIDE / GWIDTH;
	    dy = l->pl_y * WINSIDE / GWIDTH;
	    W_ClearArea(mapw, dx - (mplanet_width/2), dy - (mplanet_height/2), 
		mplanet_width, mplanet_height, backColor);
	    W_WriteBitmap(dx - (mplanet_width/2), dy - (mplanet_height/2),
		planetmBitmap(l), planetColor(l));
	    W_WriteText(mapw, dx - (mplanet_width/2), dy + (mplanet_height/2),
		planetColor(l), l->pl_name, 3, planetFont(l));
	    s = itoa(buf, l->pl_armies, 3, 0);	/* playerlist.c */
	    if((int)(s - &buf[0]) == 1)
	       buf[1] = ' ';
	    
	    if (l->pl_flags & PLAGRI)
	    W_WriteText(mapw, dx + (mplanet_width/2)+2,
			dy - (mplanet_height/4),
			textColor, buf, 2, planetFont(l));
	    else
		W_WriteText(mapw, dx + (mplanet_width/2)+2,
			    dy - (mplanet_height/4),
			    planetColor(l), buf, 2,
					planetFont(l));	 
	}
	plRedrawCyc = 0;
    }
    /* Draw ships */
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	/* We draw the guy if redrawall, or we just erased him.
	 * Also, we redraw if we haven't drawn for 30 frames.
	 * (in case he was erased by other ships).
	 */
	if (lastUpdate[i]!=0 && (!redrawall) && lastUpdate[i]<30) 
	    continue;
	if (j->p_status != PALIVE)
	    continue;
	lastUpdate[i]=0;
	dx = j->p_x * WINSIDE / GWIDTH;
	dy = j->p_y * WINSIDE / GWIDTH;
	if (j->p_flags & PFCLOAK) {
	    char buf[3];			/* TSH 2/10/93 */
	    buf[0] = '?';
	    buf[1] = j->p_mapchars[1];
	    buf[2] = '\0';
	    W_WriteText(mapw, dx - W_Textwidth,
		dy - W_Textheight/2, playerColor(j), buf, 2, W_RegularFont);
	} 
#ifdef nodef
	else if (j->p_status == PEXPLODE){	/* TSH 2/10/93 */
	    W_WriteText(mapw, dx - W_Textwidth/2, 
		dy - W_Textheight/2, playerColor(j), "*", 1,
		shipFont(j));
	} 
#endif
	else{
	    W_WriteText(mapw, dx - W_Textwidth, 
		dy - W_Textheight/2, playerColor(j), j->p_mapchars, 2,
		shipFont(j));
	}

	mclearzone[0][i] = dx - W_Textwidth;
	mclearzone[1][i] = dy - W_Textheight/2;
	mclearzone[2][i] = W_Textwidth * 2;
	mclearzone[3][i] = W_Textheight;
	/* Set these so we can checkRedraw() next time */
	mclearzone[4][i] = j->p_x;
	mclearzone[5][i] = j->p_y;
	redrawPlayer[i]=0;
    }

    /* xsg stuff (draw as often as we draw planets) */
    if (show_xsg_posn && !plRedrawCyc) {
	i = MAXPLAYER;
	dx = me->p_x * WINSIDE / GWIDTH;
	dy = me->p_y * WINSIDE / GWIDTH;
	W_WriteText(mapw, dx - W_Textwidth,
	    dy - W_Textheight/2, unColor, "{}", 2, W_RegularFont);
	mclearzone[0][i] = dx - W_Textwidth;
	mclearzone[1][i] = dy - W_Textheight/2;
	mclearzone[2][i] = W_Textwidth * 2;
	mclearzone[3][i] = W_Textheight;
	/* Set these so we can checkRedraw() next time */
	mclearzone[4][i] = me->p_x;
	mclearzone[5][i] = me->p_y;
    }

    redrawall = 0;


    /* TSH 2/93 */

    if(!mapfire) return;

    /* draw phasers and torps on map display */


    for(i=0,j=players; i< MAXPLAYER; i++,j++){
       if (j->p_status == PFREE)
	    continue;
       dx = j->p_x * WINSIDE / GWIDTH;
       dy = j->p_y * WINSIDE / GWIDTH;
       ph = &phasers[j->p_no];
       if(ph->ph_status != PHFREE && (j->p_status == PALIVE || 
				      j->p_status == PEXPLODE ||
				      j->p_status == PDEAD)){
           switch(ph->ph_status){
    	      case PHMISS:
                    /* Here I will have to compute end coordinate */
                    tx = (j->p_x + PHASEDIST * j->p_ship.s_phaserdamage / 100 
	    		* Cos[ph->ph_dir]) * WINSIDE / GWIDTH;
                    ty = (j->p_y + PHASEDIST * j->p_ship.s_phaserdamage / 100 
	    		* Sin[ph->ph_dir]) * WINSIDE / GWIDTH;
		    break;
	      case PHHIT2:
                    tx = ph->ph_x * WINSIDE / GWIDTH;
                    ty = ph->ph_y * WINSIDE / GWIDTH;
		    break;
	      default:
                    tx = players[ph->ph_target].p_x * WINSIDE / GWIDTH;
                    ty = players[ph->ph_target].p_y * WINSIDE / GWIDTH;
		    break;
	    }
            W_CacheLine(mapw, dx, dy, tx, ty, shipCol[remap[j->p_team]]);

            mclearline[0][mclearlcount] = dx;
            mclearline[1][mclearlcount] = dy;
            mclearline[2][mclearlcount] = tx;
            mclearline[3][mclearlcount] = ty;
            mclearlcount++;
	}

	if (!j->p_ntorp)
	    continue;

	for(h=0, k= &torps[MAXTORP*i + h]; h < MAXTORP; h++,k++){
	     if (!k->t_status)
	         continue;
	     dx = k->t_x * WINSIDE / GWIDTH;
	     dy = k->t_y * WINSIDE / GWIDTH;
	     if(k->t_x < 0 || k->t_y < 0){
	       /* seems to happen once in a great while */
	       k->t_x = 0;
	       k->t_y = 0;
	     }
	     W_CachePoint(mapw, dx,dy, shipCol[remap[j->p_team]]);

	     mclearpoint[0][mclearpcount] = dx;
	     mclearpoint[1][mclearpcount] = dy;
	     mclearpcount ++;
	}
    }
   W_FlushLineCaches(mapw);
   W_FlushPointCaches(mapw);
}

newcourse(x, y)
int x, y;
{
    return((unsigned char) (atan2((double) (x - me->p_x),
	(double) (me->p_y - y)) / 3.14159 * 128.));
}

/* 
 * p_explode decrements from some number -- the opposite of what we
 * want. 
 */

get_expview(j)
   
   struct player	*j;
{
   int			td = uspeeds[updateSpeed] + 1;

   if(td < 200000) td = 200000;

   if(j->p_ship.s_type == STARBASE){
      if(j->p_explode > 2*SBEXPVIEWS)
	 return j->p_explode;
      else
	 return (2*SBEXPVIEWS - j->p_explode)/((2*td)/200000);
   }
   else if(j->p_explode > 10)
      return j->p_explode;
   else
      return (10 - j->p_explode)/((2*td)/200000);
}

get_texp(t)

   struct torp	*t;
{
   int			td = uspeeds[updateSpeed] + 1;

   if(td < 200000) td = 200000;

   if(t->t_fuse > 10)
      return t->t_fuse;
   else
      return (10 - t->t_fuse)/((2*td)/200000);
}


/* -- isae, from MOO client */
run_clock(f)
   
   int	f;
{
  char timebuf[32], *s = timebuf, *itoa();
  long curtime;
  struct tm *tm;
  int	l;

  /* If we're playing back the game, we're much
   * more interested in the frame count.
   * BM 7/30/93 
   */
  if(playback){
     s = itoa(s, currentFrame, 6, 2);
     W_WriteText(w, W_Textwidth, WINSIDE-W_Textheight, W_Green, 
	timebuf, s-timebuf, W_BoldFont);
  } 
  else if(f){
     time(&curtime); 
     tm = localtime(&curtime);
     s = itoa(s, tm->tm_hour, 2, 2);
     *s++ = ':';
     s = itoa(s, tm->tm_min, 2, 2);
     *s++ = ':';
     s = itoa(s, tm->tm_sec, 2, 2);
     W_WriteText(w, W_Textwidth, WINSIDE-W_Textheight, W_Green, 
	timebuf, 8, W_BoldFont);
  }
  else{
     W_WriteText(w, W_Textwidth, WINSIDE-W_Textheight, W_Green, 
	"        ", 8, W_BoldFont);
  }
}

show_record(f)

   int	f;
{
   if(f)
      W_WriteText(w, WINSIDE - 7*W_Textwidth, WINSIDE - W_Textheight, W_Red,
	 "RECORD", 6, W_BoldFont);
   else
      W_WriteText(w, WINSIDE - 7*W_Textwidth, WINSIDE - W_Textheight, W_Red,
	 "      ", 6, W_BoldFont);
}

/* Hyped up show_playback to tell you how you're
 * viewing the game.
 * 7/30/93 BM
 */
show_playback(f)
   int	f;
{
   static char buf[80];
   static int oldPlayMode = -1;
   static int cnt;
   
   if (f)
   {
	if (oldPlayMode != playMode)
	{
		/* Clear the old status */
		memset(buf, ' ', cnt);
		W_WriteText(w, WINSIDE - (cnt+2)*W_Textwidth, WINSIDE - W_Textheight, W_Green,
			buf, cnt, W_BoldFont);

		buf[0] = (char) NULL;

		if (playMode & SINGLE_STEP)
			strcat(buf, "SINGLE-STEP ");
		if (playMode & T_SKIP)
			strcat(buf, "T-SKIP ");
		if (playMode & T_FF)
			strcat(buf, "FAST-");
		if (playMode & FORWARD)
			strcat(buf, "FORWARD");
		if (playMode & BACKWARD)
			strcat(buf, "REWIND");

		oldPlayMode = playMode;
		cnt = strlen(buf);

		W_WriteText(w, WINSIDE - (cnt+2)*W_Textwidth, WINSIDE - W_Textheight, W_Green,
			buf, cnt, W_BoldFont);
	}
   }
   else
   {
	if (buf[0] != ' ')
		memset(buf, ' ', cnt);

	W_WriteText(w, WINSIDE - 9*W_Textwidth, WINSIDE - W_Textheight, W_Red,
			buf, cnt, W_BoldFont);

	playMode = -1;
   }
}

show_extracting(f)
int f;
{
   if(f)
	W_WriteText(w, WINSIDE - 40*W_Textwidth, WINSIDE - W_Textheight, W_Cyan,
		"EXTRACTING", 10, W_BoldFont);
   else
	W_WriteText(w, WINSIDE - 40*W_Textwidth, WINSIDE - W_Textheight, W_Cyan,
		"          ", 10, W_BoldFont);
}
