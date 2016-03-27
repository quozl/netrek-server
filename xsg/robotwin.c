/*
 * option.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"
#include <time.h>
#include <signal.h>

static int notdone;		/* not done flag */
static int dummy;

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

static
struct option option[] = {
    { 0, "Federation M5",	NULL, NULL, NULL, 0, NULL},
    { 1, "Romulan Colossuss",	NULL, NULL, NULL, 0, NULL},
    { 2, "Klingon Guardian",	NULL, NULL, NULL, 0, NULL},
    { 3, "Orion HAL",		NULL, NULL, NULL, 0, NULL},
    { 4, "HunterKiller",	NULL, NULL, NULL, 0, NULL},
    { 5, "done",		&notdone, NULL, NULL, 0, NULL},
    { 0, NULL,			NULL, NULL, NULL, 0, NULL}
};

#define ROBOTNUMOPTION ((sizeof(option)/sizeof(option[0]))-1)

#define ROBOTOPTIONBORDER	2
#define ROBOTOPTIONLEN	35

void	reaper();

/* Set up the option window */
robotoptionwindow()
{
    register int i;

    /* Init not done flag */
    notdone = 1;

    /* Create window big enough to hold option windows */
    if (robotwin==NULL) {
	signal(SIGCHLD, reaper);

	robotwin = W_MakeMenu("robot", WINSIDE+10, -BORDER+10, 
			      ROBOTOPTIONLEN, ROBOTNUMOPTION, 
			      baseWin, ROBOTOPTIONBORDER);

	for (i=0; i<ROBOTNUMOPTION; i++) {
	    robotoptionrefresh(&(option[i]));
	}
    }

    /* Map window */
    W_MapWindow(robotwin);
}

/* Redraw the specified target window */
robotoptionredrawtarget(win)
W_Window win;
{
    register struct option *op;

    for (op = option; op->op_text; op++) {
	if (op->op_targetwin && win == *op->op_targetwin) {
	    robotoptionrefresh(op);
	    break;
	}
    }
}

/* Redraw the specified option option */
robotoptionredrawoption(ip)
int *ip;
{
    register struct option *op;

    for (op = option; op->op_text; op++) {
	if (ip == op->op_option) {
	    robotoptionrefresh(op);
	    break;
	}
    }
}

/* Refresh the option window given by the option struct */
robotoptionrefresh(op)
register struct option *op;
{
    register int on;
    char buf[BUFSIZ];
    unsigned char color;

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

    if (op->op_num < 4)
	color = shipCol[op->op_num+1];
    else
	color = textColor;

    W_WriteText(robotwin, 0, op->op_num, color, buf, strlen(buf), 0);
}

robotoptionaction(data)
W_Event *data;
{
    register struct option *op;
    int i;
    register char *cp;
    char *arg1 = NULL;
    int pid;

    op= &(option[data->y]);
    /* Update string; don't claim keystrokes for non-string options */
    if (op->op_string == 0) {
	if (data->type == W_EV_KEY)
	    return(0);
    } else {
	if (data->type == W_EV_BUTTON) return(0);
	if (data->key > 32 && data->key < 127) {
	    cp = op->op_string;
	    i = strlen(cp);
	    if (i < (op->op_size - 1) && !iscntrl(data->key)) {
		cp += i;
		cp[1] = '\0';
		cp[0] = data->key;
	    } else
		W_Beep();
	}
    }

    /* Toggle int, if it exists */
    if (op->op_array) {
	(*op->op_option)++;
	if (*(op->op_array)[*op->op_option] == '\0') {
	    *op->op_option=0;
	}
    } else if (op->op_option) {
	*op->op_option = ! *op->op_option;
    }

    if (op->op_num < ROBOTNUMOPTION-1) {
	if ((pid = fork()) == 0) {
	    /*
	    (void) close(0);
	    (void) close(1);
	    (void) close(2);
	    */
    
	    signal(SIGALRM, SIG_DFL);

	    switch (op->op_num) {
	    case 0:
		arg1 = "-Tf";
		break;
	    case 1:
		arg1 = "-Tr";
		break;
	    case 2:
		arg1 = "-Tk";
		break;
	    case 3:
		arg1 = "-To";
		break;
	    case 4:
		arg1 = "-Ti";
		break;
	    }
#ifdef NBR
#ifdef N_ROBOT
            if(op->op_num !=4 )
               execl(Robot, "robot", arg1, 0);
            else
               execl(Robot, "robot", arg1, "-P", 0);

            perror(Robot);
#endif
#else
#ifdef ROBOT
	    if(op->op_num !=4 )
	       execl(ROBOT, "robot", arg1, 0);
	    else
	       execl(ROBOT, "robot", arg1, "-P", 0);

	    perror(ROBOT);
#endif
#endif
	    _exit(1);
	}
    }

    if (!notdone)	/* if done, that is */
	robotoptiondone();
    else
	robotoptionrefresh(op);
    return(1);
}

robotoptiondone()
{
    struct itimerval    udt;

    /* Unmap window */
    W_UnmapWindow(robotwin);

}

void
reaper()
{
#if defined (SYSV) && !defined(hpux)
  while (waitid(P_ALL, 0, (siginfo_t *)NULL, WNOHANG) > 0)
#else
  while (wait3((int *) 0, WNOHANG, (struct rusage *) 0) > 0)
#endif
    ;
}
