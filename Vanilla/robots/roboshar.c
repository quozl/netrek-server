#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "roboshar.h"

#ifndef M_PI
#include <math.h>
#endif

extern void do_message(int recip, int group, char *address, u_char from,
		       const char *fmt, va_list args);

/* Each robot must declare the following variables as globals */
extern int debug; 

/*
 *  The following routines are shared code
 */

/*#include "warnings.h"*/
/* only the fct, robots cannot set send_short */

void swarning(unsigned char whichmessage, unsigned char argument,
              unsigned char argument2)
{
}

void spwarning(char *text, int index)
{
}

/* ARGSUSED */
void s_warning(char *text, int index)
{
} /* Only stubs to silence the linker */


void new_warning(int index, const char *mess, ...)
{
    if (debug)
        ERROR(1,("warning: (%d)%s\n", index,mess));
}


void warning(char *mess)
{
    if (debug)
	ERROR(1,("warning: %s\n", mess));
}


/*
 * This routine sets up the robot@nowhere name
 */

void robonameset(struct player *myself)
{

#ifdef CHRISTMAS
    (void) strncpy(myself->p_login, "Claus", sizeof (myself->p_login));
#else
    (void) strncpy(myself->p_login, "Robot", sizeof (myself->p_login));
#endif
    myself->p_login[sizeof(myself->p_login) - 1] = '\0';
#ifdef CHRISTMAS
    (void) strncpy(myself->p_monitor, "workshop1.npole", sizeof(myself->p_monitor));
#else
    (void) strncpy(myself->p_monitor, "Nowhere", sizeof(myself->p_monitor));
#endif
    myself->p_monitor[sizeof(myself->p_monitor) - 1] = '\0';

    /* repeat "Nowhere" for completeness 4/13/92 TC */
#ifdef CHRISTMAS
    (void) strncpy(myself->p_full_hostname, "workshop1.npole",
#else
    (void) strncpy(myself->p_full_hostname, "Nowhere",
#endif
		   sizeof(myself->p_full_hostname));
    myself->p_full_hostname[sizeof(myself->p_full_hostname) - 1] = '\0';
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

void messAll(int mynum, char *name, const char *fmt, ...)
{
    va_list args;
    char addrbuf[15];

    va_start(args, fmt);

    sprintf(addrbuf, "%s->ALL", name );
    do_message(0, MALL, addrbuf, mynum, fmt, args);
    va_end(args);
}

/*
 * messOne sends a message to s specific player in the game.
 *
 * The same comments as messAll apply here, too.
 */

void messOne(int mynum, char *name, int who, const char *fmt, ...)
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
    va_start(args, fmt);
    do_message(who, MINDIV, addrbuf, mynum, fmt, args);
    va_end(args);
}


void game_pause(char *comm, struct message *mess)
{
  status->gameup|=GU_PAUSED;
}

int game_resume(char *comm, struct message *mess)
{
  status->gameup&= ~GU_PAUSED;
  return 1;
}

/*
 * This returns the direction needed to travel to get from
 * (x1,y1) to (x2,y2). (x1,y1) are commonly me->p_x and me->p_y.
 */
u_char getcourse2(int x1, int y1, int x2, int y2)
{
    return((u_char) rint((atan2((double) (x2 - x1),
        (double) (y1 - y2)) / M_PI * 128.)));
}

/*
 * Null client packet sending function for ntserv specific code in
 * enter.c and interface.c so that we can avoid compiling it in the
 * robots directory, and use libnetrek instead.
 */
void sendClientPacket(void *ignored)
{
  return;
}

/* Handle help messages */
void robohelp(struct player *me, int oldmctl, char *roboname)
{
    if (messages[oldmctl].m_flags & MINDIV) {
        if (messages[oldmctl].m_recpt == me->p_no)
            check_command(&messages[oldmctl]);
    } else if ((messages[oldmctl].m_flags & MALL) &&
            !(messages[oldmctl].m_from & MGOD)) {
        if (strstr(messages[oldmctl].m_data, "help") != NULL)
            messOne(255,roboname,messages[oldmctl].m_from,
               "If you want help, send YOURSELF the message 'help'.");
    }
}
