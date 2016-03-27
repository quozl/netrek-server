/*
 * war.c
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

static int newhostile;

/* Set up the war window and map it */
static char *feds = "FED - ";
static char *roms = "ROM - ";
static char *klis = "KLI - ";
static char *oris = "ORI - ";
static char *gos = "  Re-program";
static char *exs = "  Exit - no change";
static char *peaces = "Peace";
static char *hostiles = "Hostile";
static char *wars = "War";

warwindow()
{
    struct player *pp;

    pp = players+me->p_playerl;

    W_MapWindow(war);
    newhostile = pp->p_hostile;
    warrefresh();
}

warrefresh()
{
    struct player *pp;

    pp = players+me->p_playerl;

    fillwin(0, feds, newhostile, pp->p_swar, FED);
    fillwin(1, roms, newhostile, pp->p_swar, ROM);
    fillwin(2, klis, newhostile, pp->p_swar, KLI);
    fillwin(3, oris, newhostile, pp->p_swar, ORI);
    W_WriteText(war, 0, 4, textColor, gos, strlen(gos), 0);
    W_WriteText(war, 0, 5, textColor, exs, strlen(exs), 0);
}

fillwin(menunum, string, hostile, warbits, team)
int menunum;
char *string;
int hostile, warbits;
int team;
{
    char buf[80];

    warbits &= newhostile;

    if (team & warbits) {
	(void) sprintf(buf, "  %s%s", string, wars);
	W_WriteText(war, 0, menunum, rColor, buf, strlen(buf), 0);
    }
    else if (team & hostile) {
	(void) sprintf(buf, "  %s%s", string, hostiles);
	W_WriteText(war, 0, menunum, yColor, buf, strlen(buf), 0);
    }
    else {
	(void) sprintf(buf, "  %s%s", string, peaces);
	W_WriteText(war, 0, menunum, gColor, buf, strlen(buf), 0);
    }
}

waraction(data)
W_Event *data;
{
    int enemyteam = 0;
    int i;
    struct player *pp;

    pp = players+me->p_playerl;

    if (data->y == 4) {
	W_UnmapWindow(war);
	for (i=0; i<4; i++) {
	    enemyteam = 1<<i;
	    if ((pp->p_swar & enemyteam) && !(newhostile & enemyteam))
		pp->p_swar ^= enemyteam;
	}
	pp->p_hostile = newhostile;
	pp->p_war = (pp->p_swar | pp->p_hostile);
	return;
    }

    if (data->y == 5) {
	W_UnmapWindow(war);
	return;
    }
    if (data->y == 0) enemyteam=FED;
    if (data->y == 1) enemyteam=ROM;
    if (data->y == 2) enemyteam=KLI;
    if (data->y == 3) enemyteam=ORI;

    newhostile ^= enemyteam;

    warrefresh();
}

