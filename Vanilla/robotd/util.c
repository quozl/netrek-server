/*
 * util.c
 */
#include "copyright.h"

#include <stdio.h>
#include <stdarg.h>

#include <math.h>
#include <signal.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "robot.h"

#define ADDRLEN		10

struct distress *loaddistress(enum dist_type i);

mprintf(char *format, ...)
{
   va_list	ap;

   if(!read_stdin)
      return;
   
   va_start(ap, format);
   (void)vprintf(format, ap);
   fflush(stdout);
   va_end(ap);
}

/*
** Provide the angular distance between two angles.
*/
angdist(x, y)
unsigned char x, y;
{
    register unsigned char res;

    if (x>y) res=x-y;
    else res=y-x;
    if (res > 128)
	return(256 - (int) res);
    return((int) res);
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

warning(s, o)

   char	*s;
   int	o;
{
   if(rsock != -1 && !o) 
      return;

   mprintf("-> %s\n", s);
   fflush(stdout);
}


/* return rnd between -range & range */
rrnd(range)

   int  range;
{
   return RANDOM() % (2*range) - range;
}

edist_to_me(j)

   struct player        *j;
{
   if(_state.wrap_around)
      return wrap_dist(me->p_x, me->p_y, j->p_x, j->p_y);
   else{
      double       dx = j->p_x - me->p_x;
      double       dy = j->p_y - me->p_y;
      return (int)ihypot(dx,dy);
   }
}

pdist_to_p(j1, j2)

   struct player	*j1,*j2;
{
   if(_state.wrap_around)
      return wrap_dist(j2->p_x, j2->p_y, j1->p_x, j1->p_y);
   else{
      double	dx = j1->p_x - j2->p_x;
      double	dy = j1->p_y - j2->p_y;
      return (int) ihypot(dx,dy);
   }
}

unsigned char get_wrapcourse(x,y)

   int	x,y;
{
   if(!_state.wrap_around) return get_course(x,y);
   else
      return get_course(wrap_x(x, me->p_x), wrap_y(y, me->p_y));
}

wrap_x(x, mx)
{
   register	xd = x - mx;

   if(xd < 0){
      if(-xd > GWIDTH/2)
	 x = GWIDTH + x;
   }
   else {
      if(xd > GWIDTH/2)
	 x = -(GWIDTH-x);
   }
   return x;
}

wrap_y(y, my)
{
   register	yd = y - my;
   if(yd < 0){
      if(-yd > GWIDTH/2)
	 y = GWIDTH + y;
   }
   else {
      if(yd > GWIDTH/2)
	 y = -(GWIDTH-y);
   }
   return y;
}

wrap_dist(x1, y1, x,y)
{
   int	xr = wrap_x(x, x1),
	yr = wrap_y(y, y1);
   double dx = (double)(xr - x1),
	  dy = (double)(yr - y1);
   
   return (int)ihypot(dx,dy);
}

/* get course from me to x,y */
unsigned char get_course(x,y)
int     x,y;
{
   unsigned char	ret_crs;

   if(x == me->p_x && y == me->p_y)
      return 0;

   return (unsigned char) (atan2((double) (x- me->p_x),
      (double) (me->p_y - y)) / 3.141592 * 128.);
}

/* get course from (mx,my) to (x,y) */
unsigned char get_acourse(x,y, mx,my)

   int	x,y,mx,my;
{
   if(x == mx && y == my)
      return 0;

   return (unsigned char) (atan2((double) (x - mx),
      (double) (my - y)) / 3.14159 * 128.);
}

unsigned char get_awrapcourse(x,y, mx,my)

   int	x,y;
{
   if(!_state.wrap_around) return get_acourse(x,y,mx,my);
   else
      return get_course(wrap_x(x, mx), wrap_y(y,my));
}

/* return distance of closest enemy to planet in question */
edist_to_planet(pl)

   struct planet        *pl;
{
   return dist_to_planet(_state.closest_e, pl);
}

dist_to_planet(e,pl)

   Player		*e;
   struct planet	*pl;
{
   double               dx,dy;
   int			d;

   if(!e || !e->p || !isAlive(e->p) || e->invisible)
      return GWIDTH;
   
   if(_state.wrap_around)
      return wrap_dist(e->p->p_x, e->p->p_y, pl->pl_x, pl->pl_y);

   dx = pl->pl_x - e->p->p_x;
   dy = pl->pl_y - e->p->p_y;
   d = (int)ihypot(dx, dy);
   if(d == 0) d = 1;		/* avoid div by zero errors */

   return d;
}

mydist_to_planet(pl)

   struct planet        *pl;
{
   return dist_to_planet(me_p, pl);
}

/* returns angdst of enemy course & given course <= range given,
   i.e. is difference of enemy course to given course within range? */
running_away(e, crs, r)

   Player                *e;
   unsigned char        crs;
{
   return angdist(e->p->p_dir, crs) <= r;
}

/* returns true if enemy 'e' is "attacking" the specified
   approaching course */

attacking(e, crs, r)

   Player                *e;
   unsigned char        crs;
{
   unsigned char        ecrs = e->p->p_dir;
   unsigned int		rd = crs - 128;

   rd = NORMALIZE(rd);
   return angdist((unsigned char)rd, ecrs) <= r;
}

avoiddir(dir, mcrs, r)

   unsigned char	dir, 	/* course to be avoided */
			r;	/* range of direction change */
   unsigned char        *mcrs;	/* my course -- used and returned */
{
   register unsigned char	mydir = *mcrs;
   register int			nd = 0;
   unsigned char		td;

   nd = angdist(dir, mydir);

   if(nd < r){
      
      td = mydir + nd;
      if(td == dir){
	 /* turn ccw */
	 *mcrs = dir - r;
      }
      else{
	 /* turn cw */
	 *mcrs = dir + r;
      }
      return r - nd;
   }
   return 0;
   /* otherwise unchanged */
}

/* defend */

/* how close do we get? */
defend_dist(p)

   Player	*p;
{
   struct planet	*pl, *team_planet();

   if(p && p->p && p->p->p_ship.s_type == STARBASE)
      return 4000;

   if(_state.warteam && (pl = team_planet(_state.warteam))){
      if(pl->pl_mydist > dist_to_planet(p,pl)){
	 /* my distance to enemy planet is greater then his distance */
	 return 0;
      }
      else
	 return 10000;
   }
   return 6000;
}

/* what's too close */
tdefend_dist()
{
   if(starbase(_state.protect_player))
      return 4000;
   else
      return 3500;
}

/* how close to enemy before attack */
edefend_dist()
{
   return 9000;
}

unsigned char choose_course(scrs, ecrs)

   unsigned char scrs;
   unsigned char ecrs;
{
   unsigned char td;
   int           nd = angdist(ecrs, scrs);

   if(nd > 80) return scrs;
   td = scrs + nd;
   if(td == ecrs)
      return (unsigned char) (ecrs - 80);
   else
      return (unsigned char) (ecrs + 80);
}

on_screen(p)

   Player	*p;
{
   register	x,y;
   if(!p || !p->p || !isAlive(p->p))
      return 0;
   
   x = p->p->p_x; y = p->p->p_y;
   
   return !(x < me->p_x - SCALE*WINSIDE/2 ||
	   x > me->p_x + SCALE*WINSIDE/2 ||
	   y > me->p_y + SCALE*WINSIDE/2 ||
	   y < me->p_y - SCALE*WINSIDE/2);
}

#define TIME(x)		((_udcounter - (x))/10)

do_alert()
{
   static int	lastalert;
   int		critical = 0;
   int		intr;

   if(_state.total_friends == 0) return;

   if(me->p_ship.s_type != STARBASE) return;

   if(MYDAMAGE() > 50)
      critical ++;
   if(MYWTEMP() > 90)
      critical ++;
   if(me->p_flags & PFWEP)
      critical ++;
   
   if((me->p_flags & PFGREEN) && TIME(lastalert) > 60){
      lastalert = _udcounter;
      emergency();
      return;
   }
   if(critical && TIME(lastalert) > 10){
      lastalert = _udcounter;
      emergency();
      return;
   }
}

emergency()
{
    struct distress *dist;

    dist = loaddistress(GENERIC);
    send_RCD(dist);
}

/*
 * gives approximate distance from (x1,y1) to (x2,y2)
 * with only overestimations, and then never by more
 * than (9/8) + one bit uncertainty.
 */

int ihypot(xd1, yd1)
   double xd1, yd1;
{
   register 	x1 = (int)xd1,
		y1 = (int)yd1,
		x2 = 0, 
		y2 = 0;

   if ((x2 -= x1) < 0) x2 = -x2;
   if ((y2 -= y1) < 0) y2 = -y2;
   return (x2 + y2 - (((x2>y2) ? y2 : x2) >> 1) );
}

#if ! HAVE_NINT
nint(x)
   
   double x;
{
   double	rint();
   return (int)rint(x);
}
#endif

mfprintf(FILE *fo, char *format, ...)
{
   va_list	ap;

   if(!read_stdin)
      return;
   
   va_start(ap, format);
   (void)vfprintf(fo, format, ap);
   fflush(fo);
   va_end(ap);
}

