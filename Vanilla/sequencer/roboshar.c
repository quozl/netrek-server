#include <stdio.h>
#include <varargs.h>
#include "defs.h"
#include "struct.h"
#include "data.h"


/* Each robot must declare the following variables as globals */
extern int debug; 

/*
 *  The following routines are shared code
 */

/* ARGSUSED */
void message_flag(cur,address)
struct message *cur;
char *address;
{
    /* This is to prevent false sending with SP_S_WARNING */
    cur->args[0] = DINVALID;
}

/*#include "warnings.h"*/
/* only the fct, robots cannot set send_short */

/* ARGSUSED */
void swarning( whichmessage, argument,argument2)
unsigned char whichmessage,argument,argument2;
{
}

/* ARGSUSED */
void spwarning(text,index)
char *text;
int index;
{
}

/* ARGSUSED */
void s_warning(text,index)
char *text;
int index;
{
} /* Only stubs to silence the linker */


void new_warning(index,mess)
int index;
char *mess;
{
    if (debug)
        ERROR(1,("warning: (%d)%s\n", index,mess));
}


void warning(mess)
int mess;
{
    if (debug)
	ERROR(1,("warning: %s\n", mess));
}


/*
 * This routine sets up the robot@nowhere name
 */

void robonameset(myself)
struct player *myself;
{

    (void) strncpy(myself->p_login, "Robot", sizeof (myself->p_login));
    myself->p_login[sizeof(myself->p_login) - 1] = '\0';
    (void) strncpy(myself->p_monitor, "Nowhere", sizeof(myself->p_monitor));
    myself->p_monitor[sizeof(myself->p_monitor) - 1] = '\0';

#ifdef FULL_HOSTNAMES

    /* repeat "Nowhere" for completeness 4/13/92 TC */
    (void) strncpy(myself->p_full_hostname, "Nowhere",
		   sizeof(myself->p_full_hostname));
    myself->p_full_hostname[sizeof(myself->p_full_hostname) - 1] = '\0';

#endif

}


/*
 * messAll sends a message from a robot to everyone in the game.
 * 
 * The first argument, mynum, specifies who the message is from.
 * This allows the robot to:
 * a)  Have messages come from god by setting mynum to 255.  This
 *     way, the player will see the true robot name, such as puck->All,
 *     and cannot ignore them.
 * b)  Have the messages come from the slot number, by setting mynum
 *     to me->p_no.  This way, the player can ignore the messages, but
 *     the player will see Ig->All instead of the robot name.  This 
 *     allows a robot to take two slots, and send messages of different
 *     priorities from the different slots.  Puck uses this.
 * c)  Forge a message by setting mynum to whatever it wants.
 */

void messAll(mynum,name,va_alist)
int mynum;
char *name;
va_dcl
{
    va_list args;
    char addrbuf[15];

    va_start(args);

/* +++ 2.6pl0 cameron@sna.dec.com */
#if defined(__alpha)
    sprintf(addrbuf, "%s->ALL", &name ); 
#else
    sprintf(addrbuf, "%s->ALL", name );
#endif
/* --- */
    do_message(0, MALL, addrbuf, mynum, args);
    va_end(args);
}

/*
 * messOne sends a message to s specific player in the game.
 *
 * The same comments as messAll apply here, too.
 */

void messOne(mynum,name,who, va_alist)
int mynum;
char *name;
int who;
va_dcl
{
    va_list args;
    char addrbuf[15];

    /* On the chance that I am sending myself a message */
    /* And it is not from god.  This is a bad thing, I think. */
    if (who == mynum){
      ERROR(2,("roboshar:messOne(): Messaging self (slot %i)!\n",who));
      return; /* hack: don't message self :) */
    }
    sprintf(addrbuf, "%s->%2s", name, players[who].p_mapchars);
    va_start(args);
    do_message(who, MINDIV, addrbuf, mynum, args);
    va_end(args);
}


int game_pause(comm,mess)
char *comm;
struct message *mess;
{
  status->gameup|=GU_PAUSED;
}

int game_resume(comm,mess)
char *comm;
struct message *mess;
{
  status->gameup&= ~GU_PAUSED;
}

#ifndef M_PI
#include <math.h>
#endif

/*
 * This returns the direction needed to travel to get from
 * (x1,y1) to (x2,y2). (x1,y1) are commonly me->p_x and me->p_y.
 */
u_char getcourse2(int x1,int y1,int x2,int y2)
{
    return((u_char) nint((atan2((double) (x2 - x1),
        (double) (y1 - y2)) / M_PI * 128.)));
}
