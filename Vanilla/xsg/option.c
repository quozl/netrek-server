/*
 * option.c
 */
#include "copyright.h"

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

static int 	notdone;		/* not done flag */
static int 	newrecord;
static int 	newclock = 1;
static char	_godsname[32];
	   
int truespeed = 2;		/* updateSpeed, updated when optiondone */

char *localmes[] = { "Show owner on local planets",
		     "Show resources on local planets",
		     "Show nothing on local planets",
		     ""};

char *galacticmes[] = { "Show owner on galactic map",
		        "Show resources on galactic map",
		        "Show nothing on galactic map",
		        ""};

char *updatemess[] = { 
#ifdef REALLYFAST
		       "10 updates per second",
#endif
		       "5 updates per second",
		       "4 updates per second",
		       "2 updates per second",
		       "1 update per second",
		       ""};

char *recordmess[] = { 
#ifdef REALLYFAST
                       "record 10 frames per second",
#endif
                       "record 5 frames per second",
		       "record 4 frames per second",
		       "record 2 frames per second",
		       "record 1 frames per second",
		       ""};

char *mapupdates[] = { "Don't update galactic map",
		       "Update galactic map rarely",
		       "Update galactic map frequently",
		       ""};

int uspeeds[] = {
#ifdef REALLYFAST
		 100000, 
#endif
		 200000, 250000, 500000, 999999};

/*
 * Only one of op_option, op_targetwin, and op_string should be
 * defined. If op_string is defined, op_size should be too and
 * op_text is used without a "Don't" prefix.
 */
struct option {
    int op_num;
    char *op_text;		/* text to display when on */
    int *op_option;		/* variable to test/modify (optional) */
    W_Window *op_targetwin;	/* target window to map/unmap (optional) */
    char *op_string;		/* string to modify (optional) */
    int op_size;		/* size of *op_string (optional) */
    char **op_array; 		/* array of strings to switch between */
};

static struct option option[] = {
    { 0, "show tactical planet names", &namemode, NULL, NULL, 0, NULL},
    { 1, "show shields", &showShields, NULL, NULL, 0, NULL},
    { 2, "", &mapmode, NULL, NULL, 0, mapupdates},
    { 3, "show help window", NULL, &helpWin, NULL, 0, NULL},
    { 4, "", &showlocal, NULL, NULL, 0, localmes},
    { 5, "", &showgalactic, NULL, NULL, 0, galacticmes},
    { 6, "", &updateSpeed, NULL, NULL, 0, updatemess},
    { 7, "report kill messages", &reportKills, NULL, NULL, 0, NULL},
    { 8, "beep on message to GOD", &msgBeep, NULL, NULL, 0, NULL},
    { 9, "God's name", NULL, NULL, _godsname, 31, NULL},
    { 10, "show xsg position", &show_xsg_posn, NULL, NULL, 0, NULL},
    { 11, "show clock", &newclock, NULL, NULL, 0, NULL},
    { 12, "show torps & phasers on map", &mapfire, NULL, NULL, 0, NULL},
    { 13, "record game", &newrecord, NULL, NULL, 0, NULL},
    { 14, "", &recordSpeed, NULL, NULL, 0, recordmess},
    { 15, "done", &notdone, NULL, NULL, 0, NULL},
    { 0,  NULL, NULL, NULL, NULL, 0, NULL}
};

#define NUMOPTION ((sizeof(option)/sizeof(option[0]))-1)

#define OPTIONBORDER	2
#define OPTIONLEN	35

/* Set up the option window */
optionwindow()
{
    register int i;

    /* Init not done flag */
    notdone = 1;

    /* Create window big enough to hold option windows */
    if (optionWin==NULL) {
	strcpy(_godsname, godsname);
	optionWin = W_MakeMenu("option", WINSIDE+10, -BORDER+10, OPTIONLEN, 
	    NUMOPTION, baseWin, OPTIONBORDER);

	for (i=0; i<NUMOPTION; i++) {
	    optionrefresh(&(option[i]));
	}
    }

    /* Map window */
    W_MapWindow(optionWin);
}

/* Redraw the specified target window */
optionredrawtarget(win)
W_Window win;
{
    register struct option *op;

    for (op = option; op->op_text; op++) {
	if (op->op_targetwin && win == *op->op_targetwin) {
	    optionrefresh(op);
	    break;
	}
    }
}

/* Redraw the specified option option */
optionredrawoption(ip)
int *ip;
{
    register struct option *op;

    for (op = option; op->op_text; op++) {
	if (ip == op->op_option) {
	    optionrefresh(op);
	    break;
	}
    }
}

/* Refresh the option window given by the option struct */
optionrefresh(op)
register struct option *op;
{
    register int on;
    char buf[BUFSIZ];

    if (op->op_string) {
	(void) sprintf(buf, "%s: %s_", op->op_text, op->op_string);
    } else if (op->op_array) {	/* Array of strings */
	strcpy(buf, op->op_array[*op->op_option]);
    } else {
	/* Either a boolean or a window */
	if (op->op_option)
	    on = *op->op_option;		/* use int for status */
	else if (op->op_targetwin)
	    on = W_IsMapped(*op->op_targetwin);	/* use window for status */
	else
	    on = 1;				/* shouldn't happen */

	if (!on)
	    strcpy(buf, "Don't ");
	else
	    buf[0] = '\0';
	strcat(buf, op->op_text);
    }
    if (islower(buf[0]))
	buf[0] = toupper(buf[0]);

    W_WriteText(optionWin, 0, op->op_num, textColor, buf, strlen(buf), 0);
}

optionaction(data)
W_Event *data;
{
    register struct option *op;
    int i;
    register char *cp;

    op= &(option[data->y]);
    /* Update string; don't claim keystrokes for non-string options */
    if (op->op_string == 0) {
	if (data->type == W_EV_KEY)
	    return(0);
    } else {
	if (data->type == W_EV_BUTTON) return(0);
	switch (data->key) {

	case '\b':
	case '\177':
	    cp = op->op_string;
	    i = strlen(cp);
	    if (i > 0) {
		cp += i - 1;
		*cp = '\0';
	    }
	    break;

	case '\027':	/* word erase */
	    cp = op->op_string;
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
	    op->op_string[0] = '\0';
	    break;

	default:
	    if (data->key < 32 || data->key > 127) break;
	    cp = op->op_string;
	    i = strlen(cp);
	    if (i < (op->op_size - 1) && !iscntrl(data->key)) {
		cp += i;
		cp[1] = '\0';
		cp[0] = data->key;
	    } else
		W_Beep();
		break;
	}
    }

    /* Toggle int, if it exists */
    if (op->op_array) {
	/* kludge: don't change update or record speed while recording.*/
	if(record && (op->op_option == &updateSpeed || 
		     op->op_option == &recordSpeed))
	    return 0;

	(*op->op_option)++;
	if (*(op->op_array)[*op->op_option] == '\0') {
	    *op->op_option=0;
	}
    } else if (op->op_option) {
	*op->op_option = ! *op->op_option;
    }

    /* Map/unmap window, if it exists */
    if (op->op_targetwin) {
	if (W_IsMapped(*op->op_targetwin))
	    W_UnmapWindow(*op->op_targetwin);
	else
	    W_MapWindow(*op->op_targetwin);
    }
    if (!notdone)	/* if done, that is */
	optiondone();
    else
	optionrefresh(op);
    return(1);
}

optiondone()
{
    /* Unmap window */
    W_UnmapWindow(optionWin);

    if(newrecord && !record){
      /* nah */
      if(playback){
	 newrecord = 0;
	 optionredrawoption(&newrecord);
      }
      else if((UPS(updateSpeed) % UPS(recordSpeed)) != 0){
	 warning("Update speed must be divisible by record speed.");
	 newrecord = 0;
	 optionredrawoption(&newrecord);
      }
    }
    else if(!newrecord && record)
      show_record(0);
    record = newrecord;

    if(strcmp(_godsname, godsname) != 0){
      strncpy(godsname, _godsname, 32);
      godsname[31] = '\0';
    }

    if(runclock && !newclock)
      run_clock(0);
    runclock = newclock;


    updatetimer();
}

updatetimer()
{
    struct itimerval    udt;

    udt.it_interval.tv_sec = 0;
    udt.it_interval.tv_usec = uspeeds[updateSpeed];
    udt.it_value.tv_sec = 0;
    udt.it_value.tv_usec = uspeeds[updateSpeed];
    setitimer(ITIMER_REAL, &udt, 0);
    truespeed = updateSpeed;
}
