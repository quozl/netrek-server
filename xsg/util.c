/*
 * util.c
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

/*
** Find the object nearest mouse.  Returns a pointer to an
** obtype structure.  This is used for info and locking on.
**
** Because we are never interested in it, this function will
** never return your own ship as the target.
**
** Finally, this only works on the two main windows
*/

static struct obtype _target;

struct obtype *
gettarget(ww, x, y, targtype)
W_Window ww;
int x, y;
int targtype;
{
    register int i;
    register struct player *j;
    register struct planet *k;
    int	g_x = 0, g_y = 0;
    double dist, closedist;

    targtype |= TARG_CLOAK;		/* get cloakers too */

    if (ww == mapw) {
	g_x = x * GWIDTH / WINSIDE;
	g_y = y * GWIDTH / WINSIDE;
    }
    else if(ww == w){
	g_x = me->p_x + ((x - WINSIDE/2) * SCALE);
	g_y = me->p_y + ((y - WINSIDE/2) * SCALE);
    }
    else if(ww == plstatw){	/* note: ignores targtype */

	switch(plmode){
	 case 0:
	 case 1:
	    /* player list */
	   _target.o_type = PLAYERTYPE;
	   _target.o_num = y-2;
	   if(_target.o_num < 0 || _target.o_num >= MAXPLAYER ||
	      players[_target.o_num].p_status == PFREE){
	      return NULL;
	   }
	   return &_target;
	 
	 case 2:
	 case 3:
	    /* planet list */
	   _target.o_type = PLANETTYPE;
	   _target.o_num = y-2;

	   if(_target.o_num < 0 || _target.o_num >= MAXPLANETS){
	       return NULL;
	   }

	   if(x > (W_WindowWidth(ww)*W_Textwidth)/2)
	        _target.o_num += MAXPLANETS/2;

	   if(_target.o_num < 0 || _target.o_num >= MAXPLANETS){
	       return NULL;
	   }

	   return &_target;
       }

   }
   else {
      fprintf(stderr, "Uknown window in gettarget.\n");
      return NULL;
   }

    closedist = GWIDTH;

    if (targtype & TARG_PLANET) {
	for (i = 0, k = &planets[i]; i < MAXPLANETS; i++, k++) {
	    dist = hypot((double) (g_x - k->pl_x), (double) (g_y - k->pl_y));
	    if (dist < closedist) {
		_target.o_type = PLANETTYPE;
		_target.o_num = i;
		closedist = dist;
	    }

	}
    }

    if (targtype & TARG_PLAYER) {
	for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	    if (j->p_status != PALIVE)
		continue;
	    if ((j->p_flags & PFCLOAK) && (!(targtype & TARG_CLOAK)))
		continue;
	    if (j == me && !(targtype & TARG_SELF))
		continue;
	    dist = hypot((double) (g_x - j->p_x), (double) (g_y - j->p_y));
	    if (dist < closedist) {
		_target.o_type = PLAYERTYPE;
		_target.o_num = i;
		closedist = dist;
	    }
	}
    }

    if (closedist == GWIDTH) {		/* Didn't get one.  bad news */
	return NULL;
    }
    else {
	return(&_target);
    }
}

#ifdef hpux

srandom(foo)
int foo;
{
    rand(foo);
}

random()
{
    return(rand());
}

#include <time.h>
#include <sys/resource.h>

getrusage(foo, buf)
int foo;
struct rusage *buf;
{
    buf->ru_utime.tv_sec = 0;
    buf->ru_stime.tv_sec = 0;
}

#ifdef nodef
#include <sys/signal.h>

int (*
signal(sig, funct))()
int sig;
int (*funct)();
{
    struct sigvec vec, oldvec;

    sigvector(sig, 0, &vec);
    vec.sv_handler = funct;
    sigvector(sig, &vec, (struct sigvec *) 0);
}
#endif
#endif /*hpux*/
