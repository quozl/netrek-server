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
#else /*hpux*/
#include <sys/time.h>
#endif /*hpux*/
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"
#include "bitmaps.h"
#include "oldbitmaps.h"
/*#include "packets.h"*/
static int line=0;
static int maxline=0;

#define SIZEOF(a)	(sizeof (a) / sizeof (*(a)))

#define BOXSIDE		(WINSIDE / 5)
#define TILESIDE	16
#define MESSAGESIZE	20
#define STATSIZE	(MESSAGESIZE * 2 + BORDER)
#define YOFF		0
#define PLSTAT		((MAXPLAYER+3) * W_Textheight)

#define stipple_width 16
#define stipple_height 16
static unsigned char stipple_bits[] = {
   0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08,
   0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x80, 0x80,
   0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08,
   0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x80, 0x80};
#define xsg_width 20
#define xsg_height 20
static unsigned char xsg_bits[] = {
   0x01, 0x00, 0x08, 0x02, 0x00, 0x04, 0x04, 0x00, 0x02, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x02, 0x00, 0x04, 0x01, 0x00, 0x08};

newwin(hostmon, progname)
char *hostmon, *progname;
{
    W_Initialize(hostmon);

    baseWin = W_MakeWindow("XShowGalaxy",0,YOFF,WINSIDE*2+1*BORDER,
	WINSIDE+2*BORDER+PLSTAT,NULL,BORDER,gColor);
    iconWin = W_MakeWindow("xsg_icon",0, 0, icon_width, icon_height, NULL,
	BORDER, gColor);
    W_SetIconWindow(baseWin, iconWin);
    w = W_MakeWindow("local",-BORDER, -BORDER, WINSIDE, WINSIDE, baseWin, 
	BORDER, foreColor);
    mapw = W_MakeWindow("map", WINSIDE, -BORDER, WINSIDE, WINSIDE, baseWin, 
	BORDER, foreColor);
    warnw = W_MakeWindow("warn", 0, WINSIDE, WINSIDE-BORDER, MESSAGESIZE, 
	baseWin, BORDER, foreColor);
    statwin = W_MakeWindow("stats", WINSIDE-BORDER-160, WINSIDE, 160, 95,
			   baseWin, 5, foreColor);
    messagew = W_MakeWindow("message", 0, WINSIDE+BORDER+MESSAGESIZE,
	WINSIDE-BORDER, MESSAGESIZE, baseWin, BORDER, foreColor);
    plstatw = W_MakeTextWindow("plstat",WINSIDE+BORDER,
	YOFF+WINSIDE+BORDER-1, 83, MAXPLAYER+3, baseWin, 2);
    helpWin = W_MakeTextWindow("help",0,YOFF+WINSIDE+2*BORDER+2*MESSAGESIZE,
	160, 14, baseWin, BORDER);
    reviewWin = W_MakeScrollingWindow("review", 0,
	YOFF + WINSIDE +2*BORDER+2*MESSAGESIZE,81,18, baseWin,BORDER);
    W_DefineCursor(baseWin, 16, 16, cross_bits, crossmask_bits, 7, 7);
    W_DefineCursor(reviewWin, 16, 16, cross_bits, crossmask_bits, 7, 7);
    W_DefineCursor(helpWin, 16, 16, cross_bits, crossmask_bits, 7, 7);
    W_DefineCursor(plstatw, 16, 16, cross_bits, crossmask_bits, 7, 7);
    W_DefineCursor(statwin, 16, 16, cross_bits, crossmask_bits, 7, 7);

#define WARHEIGHT 2
#define WARWIDTH 20
#define WARBORDER 2

    war = W_MakeMenu("war", WINSIDE+10, -BORDER+10, WARWIDTH, 6, baseWin, 
	WARBORDER);

    getResources(progname);
    savebitmaps();
}

mapAll()
{
    W_MapWindow(plstatw);
    W_MapWindow(reviewWin);
    W_MapWindow(warnw);
    W_MapWindow(messagew);
    W_MapWindow(mapw);
    W_MapWindow(w);
    W_MapWindow(baseWin);
}

savebitmaps()
{
    register int i;
    for (i = 0; i < VIEWS; i++) { 
	fed_bitmaps[SCOUT][i] = 
	  W_StoreBitmap(fed_scout_width, fed_scout_height,
			fed_scout_bits[i], w);
	fed_bitmaps[DESTROYER][i] = 
	  W_StoreBitmap(fed_destroyer_width, fed_destroyer_height,
			fed_destroyer_bits[i], w);
	fed_bitmaps[CRUISER][i] = 
	  W_StoreBitmap(fed_cruiser_width, fed_cruiser_height,
			fed_cruiser_bits[i], w);
	fed_bitmaps[BATTLESHIP][i] = 
	  W_StoreBitmap(fed_battleship_width, fed_battleship_height,
			fed_battleship_bits[i], w);
	fed_bitmaps[ASSAULT][i] = 
	  W_StoreBitmap(fed_assault_width, fed_assault_height,
			fed_assault_bits[i], w);
	fed_bitmaps[STARBASE][i] = 
	  W_StoreBitmap(fed_starbase_width, fed_starbase_height,
			fed_starbase_bits[i], w);

#ifdef SGALAXY
	fed_bitmaps[SGALAXY][i] = 		/* ATM - Galaxy */
	  W_StoreBitmap(fed_galaxy_width, fed_galaxy_height,
			fed_galaxy_bits[i], w);
#endif

 	kli_bitmaps[SCOUT][i] = 
	  W_StoreBitmap(kli_scout_width, kli_scout_height,
			kli_scout_bits[i], w);
 	kli_bitmaps[DESTROYER][i] = 
	  W_StoreBitmap(kli_destroyer_width, kli_destroyer_height,
			kli_destroyer_bits[i], w);
	kli_bitmaps[CRUISER][i] = 
	  W_StoreBitmap(kli_cruiser_width, kli_cruiser_height,
			kli_cruiser_bits[i], w);
	kli_bitmaps[BATTLESHIP][i] = 
	  W_StoreBitmap(kli_battleship_width, kli_battleship_height,
			kli_battleship_bits[i], w);
 	kli_bitmaps[ASSAULT][i] = 
	  W_StoreBitmap(kli_assault_width, kli_assault_height,
			kli_assault_bits[i], w);
 	kli_bitmaps[STARBASE][i] = 
	  W_StoreBitmap(kli_starbase_width, kli_starbase_height,
			kli_starbase_bits[i], w);

#ifdef SGALAXY
 	kli_bitmaps[SGALAXY][i] = 		/* ATM - Galaxy */
	  W_StoreBitmap(kli_galaxy_width, kli_galaxy_height,
			kli_galaxy_bits[i], w);
#endif

	rom_bitmaps[SCOUT][i] = 
	  W_StoreBitmap(rom_scout_width, rom_scout_height,
			rom_scout_bits[i], w);
	rom_bitmaps[DESTROYER][i] = 
	  W_StoreBitmap(rom_destroyer_width, rom_destroyer_height,
			rom_destroyer_bits[i], w);
	rom_bitmaps[CRUISER][i] = 
	  W_StoreBitmap(rom_cruiser_width, rom_cruiser_height,
			rom_cruiser_bits[i], w);
	rom_bitmaps[BATTLESHIP][i] = 
	  W_StoreBitmap(rom_battleship_width, rom_battleship_height,
			rom_battleship_bits[i], w);
	rom_bitmaps[ASSAULT][i] = 
	  W_StoreBitmap(rom_assault_width, rom_assault_height,
			rom_assault_bits[i], w);
	rom_bitmaps[STARBASE][i] = 
	  W_StoreBitmap(rom_starbase_width, rom_starbase_height,
			rom_starbase_bits[i], w);

#ifdef SGALAXY
	rom_bitmaps[SGALAXY][i] = 		/* ATM - Galaxy */
	  W_StoreBitmap(rom_galaxy_width, rom_galaxy_height,
			rom_galaxy_bits[i], w);
#endif

	ori_bitmaps[SCOUT][i] = 
	  W_StoreBitmap(ori_scout_width, ori_scout_height,
			ori_scout_bits[i], w);
	ori_bitmaps[DESTROYER][i] = 
	  W_StoreBitmap(ori_destroyer_width, ori_destroyer_height,
			ori_destroyer_bits[i], w);
	ori_bitmaps[CRUISER][i] = 
	  W_StoreBitmap(ori_cruiser_width, ori_cruiser_height,
			ori_cruiser_bits[i], w); 
	ori_bitmaps[BATTLESHIP][i] = 
	  W_StoreBitmap(ori_battleship_width, ori_battleship_height,
			ori_battleship_bits[i], w);
	ori_bitmaps[ASSAULT][i] = 
	  W_StoreBitmap(ori_assault_width, ori_assault_height,
			ori_assault_bits[i], w);
	ori_bitmaps[STARBASE][i] = 
	  W_StoreBitmap(ori_starbase_width, ori_starbase_height,
			ori_starbase_bits[i], w);

#ifdef SGALAXY
	ori_bitmaps[SGALAXY][i] = 		/* ATM - Galaxy */
	  W_StoreBitmap(ori_galaxy_width, ori_galaxy_height,
			ori_galaxy_bits[i], w);
#endif

	ind_bitmaps[SCOUT][i] = 
	  W_StoreBitmap(ind_scout_width, ind_scout_height,
			ind_scout_bits[i], w);
	ind_bitmaps[DESTROYER][i] = 
	  W_StoreBitmap(ind_destroyer_width, ind_destroyer_height,
			ind_destroyer_bits[i], w);
	ind_bitmaps[CRUISER][i] = 
	  W_StoreBitmap(ind_cruiser_width, ind_cruiser_height,
			ind_cruiser_bits[i], w); 
	ind_bitmaps[BATTLESHIP][i] = 
	  W_StoreBitmap(ind_battleship_width, ind_battleship_height,
			ind_battleship_bits[i], w);
	ind_bitmaps[ASSAULT][i] = 
	  W_StoreBitmap(ind_assault_width, ind_assault_height,
			ind_assault_bits[i], w);
	ind_bitmaps[STARBASE][i] = 
	  W_StoreBitmap(ind_starbase_width, ind_starbase_height,
			ind_starbase_bits[i], w);
#ifdef SGALAXY
	ind_bitmaps[SGALAXY][i] =
	  W_StoreBitmap(ind_galaxy_width, ind_galaxy_height,
			ind_galaxy_bits[i], w);
#endif

#ifdef ATT
	fed_bitmaps[ATT][i] = kli_bitmaps[ATT][i] = rom_bitmaps[ATT][i] =
	    ori_bitmaps[ATT][i] = ind_bitmaps[ATT][i] = 
	  W_StoreBitmap(att_width, att_height, att_bits[i], w);
#endif
    }
    for (i=0; i<5; i++) {
	cloud[i] = W_StoreBitmap(cloud_width, cloud_height, cloud_bits[4-i],w);
	plasmacloud[i] = W_StoreBitmap(plasmacloud_width, 
	    plasmacloud_height, plasmacloud_bits[4-i],w);
    }
    etorp = W_StoreBitmap(etorp_width, etorp_height, etorp_bits,w);
    mtorp = W_StoreBitmap(mtorp_width, mtorp_height, mtorp_bits,w);
    eplasmatorp = 
      W_StoreBitmap(eplasmatorp_width, eplasmatorp_height, eplasmatorp_bits,w);
    mplasmatorp = 
      W_StoreBitmap(mplasmatorp_width, mplasmatorp_height, mplasmatorp_bits,w);
    bplanets[0] = W_StoreBitmap(planet_width, planet_height, indplanet_bits,w);
    bplanets[1] = W_StoreBitmap(planet_width, planet_height, fedplanet_bits,w);
    bplanets[2] = W_StoreBitmap(planet_width, planet_height, romplanet_bits,w);
    bplanets[3] = W_StoreBitmap(planet_width, planet_height, kliplanet_bits,w);
    bplanets[4] = W_StoreBitmap(planet_width, planet_height, oriplanet_bits,w);
    bplanets[5] = W_StoreBitmap(planet_width, planet_height, planet_bits,w);
    mbplanets[0] = W_StoreBitmap(mplanet_width, mplanet_height, indmplanet_bits,mapw);
    mbplanets[1] = W_StoreBitmap(mplanet_width, mplanet_height, fedmplanet_bits,mapw);
    mbplanets[2] = W_StoreBitmap(mplanet_width, mplanet_height, rommplanet_bits,mapw);
    mbplanets[3] = W_StoreBitmap(mplanet_width, mplanet_height, klimplanet_bits,mapw);
    mbplanets[4] = W_StoreBitmap(mplanet_width, mplanet_height, orimplanet_bits,mapw);
    mbplanets[5] = W_StoreBitmap(mplanet_width, mplanet_height, mplanet_bits,mapw);
    bplanets2[0] = bplanets[0];
    mbplanets2[0] = mbplanets[0];
    bplanets2[1] = W_StoreBitmap(planet_width, planet_height, planet001_bits,w);
    bplanets2[2] = W_StoreBitmap(planet_width, planet_height, planet010_bits,w);
    bplanets2[3] = W_StoreBitmap(planet_width, planet_height, planet011_bits,w);
    bplanets2[4] = W_StoreBitmap(planet_width, planet_height, planet100_bits,w);
    bplanets2[5] = W_StoreBitmap(planet_width, planet_height, planet101_bits,w);
    bplanets2[6] = W_StoreBitmap(planet_width, planet_height, planet110_bits,w);
    bplanets2[7] = W_StoreBitmap(planet_width, planet_height, planet111_bits,w);
    mbplanets2[1]=W_StoreBitmap(mplanet_width,mplanet_height,mplanet001_bits,mapw);
    mbplanets2[2]=W_StoreBitmap(mplanet_width,mplanet_height,mplanet010_bits,mapw);
    mbplanets2[3]=W_StoreBitmap(mplanet_width,mplanet_height,mplanet011_bits,mapw);
    mbplanets2[4]=W_StoreBitmap(mplanet_width,mplanet_height,mplanet100_bits,mapw);
    mbplanets2[5]=W_StoreBitmap(mplanet_width,mplanet_height,mplanet101_bits,mapw);
    mbplanets2[6]=W_StoreBitmap(mplanet_width,mplanet_height,mplanet110_bits,mapw);
    mbplanets2[7]=W_StoreBitmap(mplanet_width,mplanet_height,mplanet111_bits,mapw);
    for (i = 0; i < EX_FRAMES; i++) {
	expview[i]=W_StoreBitmap(ex_width, ex_height, ex_bits[i],w);
    }
    for (i = 0; i < SBEXPVIEWS; i++) {
	sbexpview[i]=W_StoreBitmap(sbexp_width, sbexp_height, sbexp_bits[i],w);
    }
    shield = W_StoreBitmap(shield_width, shield_height, shield_bits,w);
    cloakicon = W_StoreBitmap(cloak_width, cloak_height, cloak_bits,w);
    icon = W_StoreBitmap(icon_width, icon_height, icon_bits, iconWin);
    xsgbits = W_StoreBitmap(xsg_width, xsg_height, xsg_bits, w);
}

numShips(owner)
{
	int		i, num = 0;
	struct player	*p;

	for (i = 0, p = players; i < MAXPLAYER; i++, p++)
		if (p->p_status == PALIVE && p->p_team == owner)
			num++;
	return (num);
}

deadTeam(owner)
int owner;
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

#ifdef FUBAR
static char	*AUTHOR[] = {
    "",
    "---  XtrekII Release Version 6.1 ---",
    "",
    "By Chris Guthrie, Ed James,",
    "Scott Silvey (scott@scam), and Kevin Smith (ksmith@miro)"
};
#endif

struct list {
    char bold;
    struct list *next;
    char *data;
};

getResources(prog)
char	*prog;
{
    getColorDefs();
    getTiles();
}

getTiles()
{
    stipple = W_StoreBitmap(stipple_width, stipple_height, stipple_bits, w);
}

char *help_message[] = {
    /*
    "12345678901234567890123456789012345678901234567890"
    */
    "0-9      Set speed",
    ")        Speed = 10",
    "!        Speed = 11",
    "@        Speed = 12",
    "#        Speed = 1/2 max (20)",
    "%        Speed = max (40)",
    "k        Set course",
    "O        Options window",
    "Q        Quit",
    "V        Rotate local planet display",
    "B        Rotate galactic planet display",
    "l        Lock on to player/planet",
    "S        Show status window of selected player",
    "w        Show war status of selected player",
    "r        Relocate player/planet (again to place)",
    "L        List players (two displays)",
    "P        List planets",
    "N        Planet names toggle",
    "i        Info on player/planet",
    "I        Extended info on player",
    "h        Help window toggle",
    "m        Warp mouse to message window",
    "R        Robot option window",
    "Space    Remove info and options windows",
    "Left     Lock on player or planet",
    "Middle   Modify player or planet",
    "Right    Set course (drop lock)",
    "f        play game forward       [PLAYBACK ONLY]",
    "b        play game backward      [PLAYBACK ONLY]",
    "Space    toggle single-step mode [PLAYBACK ONLY]",
    "+        speed up playback       [PLAYBACK ONLY]",
    "-        slow down playback      [PLAYBACK ONLY]",
    "t        skip until t-mode       [PLAYBACK ONLY]",
    "F        fast forward            [PLAYBACK ONLY]",
    "^        restart from beginning  [PLAYBACK ONLY]",
    0,
};

#define MAXHELP 50

fillhelp()
{
    register int i = 0, row, column;

    W_ClearWindow(helpWin);
    for (column = 0; column < 3; column++) {
	for (row = 1; row < 13; row++) {
	    if (help_message[i] == 0)
		break;
	    else {
		W_WriteText(helpWin, MAXHELP * column, row, textColor,
		    help_message[i], strlen(help_message[i]), W_RegularFont);
		i++;
	    }
	}
	if (help_message[i] == 0)
	    break;
    }
}

drawIcon()
{
    W_WriteBitmap(0, 0, icon, W_White);
}

