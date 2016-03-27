/*
 * input.c
 *
 * Modified to work as client in socket based protocol
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include "Wlib.h"
#include "defs.h"
#include <time.h>
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"
#include "util.h"

struct obtype *gettarget();
static char buf[BUFSIZ];
int mapStats = 0;

#define set_speed(speed) {me->p_speed=speed; me->p_flags&=~(PFPLOCK|PFPLLOCK);}
#define set_course(course) {me->p_dir=course; me->p_flags&=~(PFPLOCK|PFPLLOCK);}

static int intrupt_flag = 1;

void
intrupt_setflag()
{
   signal(SIGALRM,intrupt_setflag);
   intrupt_flag = 1;
}

input()
{
    W_Event 	data;
    int 	rs, intrupt();
    fd_set	fd;

    while (1) {

	while (!W_EventsPending()) {

again:
	    /* check remains for W_Socket == -1 just in case this is compiled
	       with some ancient X version (< R3?) */
	    if (W_Socket() != -1) {
		FD_ZERO(&fd);
		FD_SET(W_Socket(), &fd);
                rs = select(32, &fd, 0, 0, 0);
		if(intrupt_flag){
		     intrupt();
		     intrupt_flag = 0;
		}
		if(rs < 0) goto again;
	    }
	    else if(intrupt_flag){
		intrupt();
		intrupt_flag = 0;
	    }
	}
	W_NextEvent(&data);
	switch ((int) data.type) {
	    case W_EV_KEY:
		if (data.Window==optionWin)
		    optionaction(&data);
		else if (data.Window == modifyWin)
		    modifyaction(&data);
		else if (data.Window == messagew) 
		    smessage(data.key);
		else 
		    keyaction(&data);
		break;
	    case W_EV_BUTTON:
		if (data.Window == war) 
		    waraction(&data);
		if (data.Window == optionWin) 
		    optionaction(&data);
		if (data.Window == robotwin) 
		    robotoptionaction(&data);
		else if (data.Window == modifyWin)
		    modifyaction(&data);
		else
		    buttonaction(&data);
		break;
	    case W_EV_EXPOSE:
		if (data.Window == statwin)
		    redrawStats();
		if (data.Window == mapw)
		    redrawall = 1;
		else if (data.Window == iconWin)
		    drawIcon();
		else if (data.Window == helpWin)
		    fillhelp();
		else if (data.Window == plstatw) 
		    plstat();
		else if (data.Window == warnw) 
                    W_ClearWindow(warnw);
                else if (data.Window == messagew)
                    W_ClearWindow(messagew);
		break;
	    default:
		break;
	}
    }
}

keyaction(data)
W_Event *data;
{
    unsigned char course;
    char key=data->key;

    if (data->Window!=mapw && data->Window!=w && data->Window!=infow && data->Window!=plstatw) return;
    switch(key) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    set_speed(key - '0');
	    localflags &= ~(PFREFIT);
	    break;
	case ')':
	    set_speed(10);
	    localflags &= ~(PFREFIT);
	    break;
	case '!':
	    set_speed(11);
	    localflags &= ~(PFREFIT);
	    break;
	case '@':
	    set_speed(12);
	    localflags &= ~(PFREFIT);
	    break;
	case '#':
	    set_speed(me->p_ship.s_maxspeed/2);
	    localflags &= ~(PFREFIT);
	    break;
	case '%':
	    set_speed(40);	/* Max speed... */
	    localflags &= ~(PFREFIT);
	    break;
	case 'k': /* k = set course */
	    course = getcourse(data->Window, data->x, data->y);
	    set_course(course);
	    me->p_flags &= ~(PFPLOCK | PFPLLOCK);
	    localflags &= ~(PFREFIT);
	    break;
	case 'O': /* O = options Window */
	    if (optionWin!=NULL && W_IsMapped(optionWin))
		optiondone();
	    else
		optionwindow();
	    break;
	case 'Q':
	    exit(0);		/* just flat out bail */
	    break;
	case 'V':
	    showlocal++;
	    if (showlocal==3) showlocal=0;
	    break;
	case 'B':
	    showgalactic++;
	    if (showgalactic==3) showgalactic=0;
	    redrawall=1;
	    break;
	case 'l': /* l = lock onto */
	    lock(data->Window, data->x, data->y);
	    break;
	case 'r': /* r = move (was 'm') */
	    move_object(data->Window, data->x, data->y);
	    break;
	case ' ': /* ' ' = clear special windows */
	    if (infomapped)
		destroyInfo();
	    W_UnmapWindow(helpWin);
	    if (optionWin)
		optiondone();
            playMode ^= SINGLE_STEP;
            playMode |= DO_STEP;
	    break;
	case 'L': /* L = Player list (change to or toggle in) */
	    if (plmode > 1) plmode -= 2;
	    else plmode = 1-plmode;
	    plstat();
	    break;
	case 'P': /* P = Planet list */
	    if (plmode < 2) plmode += 2;
	    plstat();
	    break;
#ifdef nodef			/* TSH 3/10/93 */
	case 'M': /* M = Toggle Map mode */
	    mapmode = !mapmode;
	    if (mapmode) mapmode++;	/* switch back to "frequently" */
	    if (optionWin)
		optionredrawoption(&mapmode);
	    break;
#endif
	case 'N': /* N = Toggle Name mode */
	    namemode = !namemode;
	    if (optionWin)
		optionredrawoption(&namemode);
	    break;
	case 'i': /* i = get information */
	case 'I': /* I = get extended information */
	    if (!infomapped)
		inform(data->Window, data->x, data->y, key);
	    else
		destroyInfo();
	    break;
	case 'h': /* h = Map help window */
	    if (W_IsMapped(helpWin)) {
		W_UnmapWindow(helpWin);
	    } else {
		W_MapWindow(helpWin);
	    }
	    if (optionWin)
		optionredrawtarget(helpWin);
	    break;
	case 'm': /* m = warp to message window */
	    W_WarpPointer(messagew);
	    break;
	case 'R':
	    if (robotwin!=NULL && W_IsMapped(robotwin))
		robotoptiondone();
	    else
		robotoptionwindow();
	    break;
	case 'S': /* S = toggle stat mode */
	    mapStats = !mapStats;
	    if (!mapStats && W_IsMapped(statwin))
		W_UnmapWindow(statwin);
	    break;
	case 'w':
	    if (W_IsMapped(war))
		W_UnmapWindow(war);
	    else
		if (me->p_flags & PFPLOCK)
		    warwindow();
	    break;
/* special playback keys */
        case 'f':
	    if(!playback) break;
            playMode &= ~BACKWARD;
            playMode |= (FORWARD|DO_STEP);
            break;
        case 'b':
	    if(!playback) break;
            playMode &= ~FORWARD;
            playMode |= (BACKWARD|DO_STEP);
            break;
        case 't':
	    if(!playback) break;
            warning("Skipping forward to t-mode");
            playMode &= ~(BACKWARD|SINGLE_STEP);
            playMode |= (T_SKIP|DO_STEP);
	    show_playback(1);
            break;
        case '+':
	    if(!playback) break;
            if (--updateSpeed < 0)
              updateSpeed = 0;
            if (optionWin != NULL)
              optionredrawoption(&updateSpeed);
            updatetimer();
            break;
        case '-':
	    if(!playback) break;
#ifdef REALLYFAST
            if (++updateSpeed > 4)
              updateSpeed = 4;
#else
            if (++updateSpeed > 3)
              updateSpeed = 3;
#endif
            if (optionWin != NULL)
              optionredrawoption(&updateSpeed);
            updatetimer();
            break;
       case 'F':		/* fast forward */
	    if(!playback) break;
	    playMode ^= T_FF;
	    show_playback(1);

            break;
	case 'X':		/* extract! */
	    if(!playback) break;
	    extracting = !extracting;
	    show_extracting(extracting);

	    break;
	case '^':		/* restart cameron@stl.dec.com 30-Apr-1997 */
	    if(!playback) break;
	    restarting++;

	    break;

	default:
	    W_Beep();
	    break;
    }
}

buttonaction(data)
W_Event *data;
{
    struct obtype *target = NULL;
    unsigned char course;

    if(data->Window != w && data->Window != mapw && data->Window != plstatw)
	return;

    if (data->key==W_RBUTTON) {
	if(data->Window != plstatw){
	    /* set course */
	    course = getcourse(data->Window, data->x, data->y);
	    set_course(course);
	}
	return;
    }

    if (data->key==W_LBUTTON) {
	/* lock on */
	lock(data->Window, data->x, data->y);
	return;
    }

    if (data->key==W_MBUTTON) {
	/* modify */
	if (/*modifyWin!=NULL &&*/ W_IsMapped(modifyWin))
	    modifydone();
	else{
	    target = gettarget(data->Window, data->x, data->y, 
	       TARG_PLAYER|TARG_PLANET);
	}
	if(target)
	    modifywindow(target);
	return;
    }
}

getcourse(ww, x, y)
W_Window ww;
int x, y;
{
    if (ww == mapw) {
	int	me_x, me_y;

	me_x = me->p_x * WINSIDE / GWIDTH;
	me_y = me->p_y * WINSIDE / GWIDTH;
	return((unsigned char) (atan2((double) (x - me_x),
	    (double) (me_y - y)) / 3.14159 * 128.));
    }
    else
	return((unsigned char) (atan2((double) (x - WINSIDE/2),
	    (double) (WINSIDE/2 - y))
		/ 3.14159 * 128.));
}

lock(ww, x, y)
W_Window ww;
int x, y;
{
    struct obtype *target;

    set_speed(0);	/* also clears existing lock flags */
    target = gettarget(ww, x, y, TARG_PLAYER|TARG_PLANET);
    if(!target) return;

    if (target->o_type == PLAYERTYPE) {
	me->p_flags |= PFPLOCK;
	me->p_playerl = target->o_num;
	sprintf(buf, "Locked onto %s (%s)",
	    players[target->o_num].p_name,
	    players[target->o_num].p_mapchars);
	warning(buf);
    }
    else { 	/* It's a planet */
	me->p_flags |= PFPLLOCK;
	me->p_planet = target->o_num;
	sprintf(buf, "Locked onto %s", planets[target->o_num].pl_name);
	warning(buf);
    }
}

move_object(ww, x, y)
W_Window ww;
int x, y;
{
    static struct obtype *move_target = NULL;
    int g_x, g_y;

    if (move_target == NULL) {
	move_target = gettarget(ww, x, y, TARG_PLAYER|TARG_PLANET);
	if(!move_target) return;
	if (move_target->o_type == PLAYERTYPE) {
	    sprintf(buf, "Select new location for player %s (%s) and hit 'r'",
		players[move_target->o_num].p_name,
		players[move_target->o_num].p_mapchars);
	    warning(buf);
	} else {
	    sprintf(buf, "Select new location for planet %s and hit 'r'",
		planets[move_target->o_num].pl_name);
	    warning(buf);
	}

    } else {
	if (ww == mapw) {
	    g_x = x * GWIDTH / WINSIDE;
	    g_y = y * GWIDTH / WINSIDE;
	} else {
	    g_x = me->p_x + ((x - WINSIDE/2) * SCALE);
	    g_y = me->p_y + ((y - WINSIDE/2) * SCALE);
	}

	if (move_target->o_type == PLAYERTYPE) {
	    if (players[move_target->o_num].p_flags & PFORBIT)
		players[move_target->o_num].p_flags ^= PFORBIT;
	    p_x_y_set(&players[move_target->o_num], g_x, g_y);
	} else {
	    planets[move_target->o_num].pl_x = g_x;
	    planets[move_target->o_num].pl_y = g_y;
	    redrawall = 1;	/* clean up display */
	}
	warning("Moved");
	move_target = NULL;
    }
}

