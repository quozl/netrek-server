/*
 * playerlist.c
 */
#include "copyright.h"

#include <stdio.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

static char *classes[NUM_TYPES] = {
	"SC", "DD", "CA", "BB", "AS", "SB",
#ifdef SGALAXY
	"GA",
#endif
	"??"
};

char *itof42();
char *itoa();
char *statchars();

/* generic player/planet update */
plstat()
{
    int i;
    char buf[100];

    switch (plmode) {
    case 0:
	W_ClearWindow(plstatw);
	(void) strcpy(buf, "# Type Login     Name               Kills Shields Damage Armies  Fuel Speed  WT");
	W_WriteText(plstatw, 0, 1, textColor, buf, strlen(buf), W_RegularFont);
	for (i=0; i<MAXPLAYER; i++) {
	    updatePlayer[i]=1;
	}
	break;
    case 1:
	W_ClearWindow(plstatw);
	(void) strcpy(buf, "# Type Rank      Name            Kills   Win  Loss  Ratio Offense Defense     DI");
	W_WriteText(plstatw, 0, 1, textColor, buf, strlen(buf), W_RegularFont);
	for (i=0; i<MAXPLAYER; i++) {
	    updatePlayer[i]=1;
	}
	break;
    case 2:
    case 3:
	/* planets */
	W_ClearWindow(plstatw);
	strcpy(buf, "Planet Name      own armies flags info   Planet Name      own armies flags info");
	W_WriteText(plstatw, 2, 1, textColor, buf, strlen(buf), W_RegularFont);
	for (i=0; i<MAXPLANETS; i++) {
	    updatePlanet[i]=1;
	}
	break;
    default:
	fprintf(stderr, "Internal error: bad plmode in plstat2\n");
    }

    plstat2();
}

plstat2()
{
    switch (plmode) {
    case 0:
	playerlist0();
	break;
    case 1:
	playerlist1();
	break;
    case 2:
    case 3:
	planetlist();
	break;
    default:
	fprintf(stderr, "Internal error: bad plmode in plstat2\n");
	break;
    }
}

playerlist0()
{
    register int i;
    char buf[100];
    register struct player *j;
    int		len;

    if (!W_IsMapped(plstatw)) return;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	if (!updatePlayer[i]) continue;
	updatePlayer[i]=0;
/*	if (j->p_status != PALIVE) {*/
	if (j->p_status == PFREE) {
	    W_ClearArea(plstatw, 0, i+2, 83, 1, backColor);
	    continue;
	}

      /* Since a) this is the most common player list shown, and b) sprintf is
	 very computationally expensive (i.e. 20% of total xsg time is 
	 spent in _doprnt) we do this like the client handles the stat bar --
	 brute force. */
	
	len = build_playerstats_buf(buf, j);
#ifdef nodef
	(void) sprintf(buf, "%c%c %s  %-9.9s %-16.16s %7.2f   %5d  %5d    %3d %5d    %2d %3d%c",
	    j->p_team == ALLTEAM?'A':teamlet[j->p_team],
	    shipnos[j->p_no],
	    (j->p_status == PALIVE) ? classes[j->p_ship.s_type] : 
	       statchars(j->p_status),
	    j->p_login,
	    j->p_name,
	    j->p_kills,
	    j->p_shield,
	    j->p_damage,
	    j->p_armies,
	    j->p_fuel,
	    j->p_speed,
	    j->p_wtemp/10,	/* TSH 3/10/93 (client shows div by 10)*/
	    (j->p_flags & PFWEP)?'W':' ');
	len = strlen(buf);
#endif
	W_WriteText(plstatw, 0, i+2, 
		/* TSH 2/10/93 */
		(me->p_flags & PFPLOCK && j->p_no == me->p_playerl)?
		  myColor: playerColor(j), buf, len,
	    shipFont(j));
    }
}

playerlist1()
{
    register int i;
    char buf[100];
    register struct player *j;
    int kills, losses;
    double ratio;
    float pRating, oRating, dRating, bRating;

    if (!W_IsMapped(plstatw)) return;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	if (!updatePlayer[i]) continue;
	updatePlayer[i]=0;
/*	if (j->p_status != PALIVE) {*/
	if (j->p_status == PFREE) {
	    W_ClearArea(plstatw, 0, i+2, 83, 1, backColor);
	    continue;
	}
#ifndef LTD_STATS
	if (j->p_ship.s_type == STARBASE) {
	    kills=j->p_stats.st_sbkills;
	    losses=j->p_stats.st_sblosses;
	} else {
	    kills=j->p_stats.st_kills + j->p_stats.st_tkills;
	    losses=j->p_stats.st_losses + j->p_stats.st_tlosses;
	}
	if (losses==0) {
	    ratio=kills;
	} else {
	    ratio=(double) kills/losses;
	}
	oRating = offenseRating(j);
	dRating = defenseRating(j);
	pRating = planetRating(j);
	bRating = bombingRating(j);
#else /* LTD_STATS */
	kills = 0;
	losses = 0;
	ratio = 0;
	oRating = 0;
	dRating = 0;
	pRating = 0;
	bRating = 0;
#endif /* LTD_STATS */
	(void) sprintf(buf, "%c%c %s  %-9.9s %-16.16s%5.2f %5d %5d %6.2lf   %5.2f   %5.2f %8.2f",
	    teamlet[j->p_team],
	    shipnos[j->p_no],
	    (j->p_status == PALIVE) ? classes[j->p_ship.s_type] : 
	       statchars(j->p_status),
	    ranks[j->p_stats.st_rank].name,
	    j->p_name,
	    j->p_kills,
	    kills,
	    losses,
	    ratio,
	    oRating,
	    dRating,
#ifndef LTD_STATS
	    (oRating+pRating+bRating)*(j->p_stats.st_tticks/36000.0));
#else /* LTD_STATS */
	    0.0);
#endif /* LTD_STATS */
	W_WriteText(plstatw, 0, i+2, 
		/* TSH 2/10/93 */
		(me->p_flags & PFPLOCK && j->p_no == me->p_playerl)?
		myColor: playerColor(j), buf, strlen(buf),
	    shipFont(j));
    }
}

build_playerstats_buf(buf, j)

   char			*buf;
   struct player	*j;
{
   register char	*s = buf, *r;
   register		i;

   /* team (width 2) */
   *s++ = (j->p_team == ALLTEAM)?'A':teamlet[j->p_team];
   *s++ = shipnos[j->p_no];
   *s++ = ' ';
   /* status (width 2) */
   strncpy(s, (j->p_status == PALIVE)?classes[j->p_ship.s_type] : 
	       statchars(j->p_status), 2);
   s += 2;
   *s++ = ' ';
   *s++ = ' ';
   /* login  (width 9, lj) */
   for(i=0, r=j->p_login; i< 9; i++){
      if(*r)
	 *s++ = *r++;
      else
	 *s++ = ' ';
   }
   *s++ = ' ';
   /* name (width 16, lj) */
   for(i=0, r=j->p_name; i< 16; i++){
      if(*r)
	 *s++ = *r++;
      else
	 *s++ = ' ';
   }
   *s++ = ' ';
   /* kills (width 4.2, rj) */
   s = itof42(s, j->p_kills);
   *s++ = ' ';
   *s++ = ' ';
   *s++ = ' ';
   /* shields (width 5) */
   s = itoa(s, j->p_shield, 5, 1);
   *s++ = ' ';
   *s++ = ' ';
   /* shields (width 5) */
   s = itoa(s, j->p_damage, 5, 1);
   *s++ = ' ';
   *s++ = ' ';
   *s++ = ' ';
   *s++ = ' ';
   /* shields (width 3) */
   s = itoa(s, j->p_armies, 3, 1);
   *s++ = ' ';
   /* fuel (width 5) */
   s = itoa(s, j->p_fuel, 5, 1);
   *s++ = ' ';
   *s++ = ' ';
   *s++ = ' ';
   *s++ = ' ';
   /* speed (width 2) */
   s = itoa(s, j->p_speed, 2, 1);
   *s++ = ' ';
   /* wtemp (width 3) */
   s = itoa(s, j->p_wtemp/10, 3, 1);

   *s++ = (j->p_flags & PFWEP)?'W':' ';
   return (int)(s - buf);
}


/*
 * buf -- string space
 * n   -- number
 * w   -- field with
 *
 * sp = 0, no space (lj)
 * sp = 1, space    (rj)
 * sp = 2, 0 instead of space.
 */

char *
itoa(buf, n, w, sp)

   char *buf;
   int  n;
   int	w, sp;
{
   register char        *s = buf;

   if(w > 6){
      *s = '0' + n/10000000;
      if(*s != '0' || sp == 2) 
	 s++;
      else if(sp == 1)
	 *s++ = ' ';
   }

   if(w > 5){
      *s = '0' + (n % 1000000)/100000;
      if(*s != '0' || n >= 1000000 || sp == 2) 
	 s++;
      else if(*s == '0' && sp)
	 *s++ = ' ';
   }

   if(w > 4){
      *s = '0' + (n % 100000)/10000;
      if(*s != '0' || n >= 100000 || sp == 2) 
	 s++;
      else if(*s == '0' && sp)
	 *s++ = ' ';
   }

   if(w > 3){
      *s = '0' + (n % 10000)/1000;
      if(*s != '0' || n >= 10000 || sp == 2) 
	 s++;
      else if(*s == '0' && sp)
	 *s++ = ' ';
   }

   if(w > 2){
      *s = '0' + (n %1000)/100;
      if(*s != '0' || n >= 1000 || sp == 2) 
	 s++;
      else if(*s == '0' && sp)
	 *s++ = ' ';
   }

   if(w > 1){
      *s = '0' + (n % 100)/10;
      if(*s != '0' || n >= 100 || sp == 2) 
	 s++;
      else if(*s == '0' && sp)
	 *s++ = ' ';
   }

   *s = '0' + (n % 10);

   return ++s;
}

char *
itof42(s, f)

   char		*s;
   float	f;
{
   *s = '0' + (int) f/1000;
   if(*s == '0')
      *s = ' ';
   *++s = '0' + ((int) f % 1000)/100;
   if(*s == '0' && (int)f < 1000)
      *s = ' ';
   *++s = '0' + ((int) f % 100)/10;
   if(*s == '0' && (int)f < 100)
      *s = ' ';
   *++s = '0' + ((int) f % 10);
   *++s = '.';
   *++s = '0' + ((int) (f * 10)) % 10;
   *++s = '0' + ((int) (f * 100)) % 10;

   return ++s;
}

char *
statchars(status)

   int	status;
{
   switch(status){
      case PEXPLODE:
	 return "++";
      case PDEAD:
	 return "..";
      case POUTFIT:
	 return "--";
#ifdef POBSERV
      case POBSERV:
	 return "ob";
#endif
      default:
	 return "??";
   }
}
