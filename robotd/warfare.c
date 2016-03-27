#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#undef hypot		/* exact hypot used here */

#include "packets.h"
#include "robot.h"

/* debug */
int		dntorps;
unsigned char	dtcrs;
int		dtint;
int		dgo;


warfare(type)
   
   int	type;
{
   Player		*e = _state.current_target;
   struct player	*j = e?e->p:NULL;
   int			edist, mtorps;

   /* debug */

#ifdef nodef
   if(dgo){
      register	i;
      _state.torp_i = dtint;
      mtorps = 8 - me->p_ntorp;

      for(i=0; i < dntorps && i< mtorps; i++)
	 qtorp(dtcrs);
      dgo = 0;
      return;
   }
#endif

   if(!isAlive(me))
      return;
   
   if(_state.defend && (!_state.protect_player->p || 
      !isAlive(_state.protect_player->p))){
      _state.defend = 0;
      _state.protect_player = NULL;
      if(DEBUG & DEBUG_PROTECT){
	 printf("defend off\n");
      }
   }

   if(!e || !j || (j->p_status != PALIVE)){
      emptytorpq();
      return;
   }

   edist = edist_to_me(j);

   if(e->robot && rship_ok(e->p->p_name, e->p->p_ship.s_type)){
      if(type == WPHASER){
	 if(_cycletime < 500)
	    phaser_attack_robot(e, j, edist);
      }
      else if(type == WTORP)
	 torp_attack_robot(e, j, edist);
   }
   else{
      if(!_state.ogg && _state.closest_e != e){
	 if(_state.closest_e && _state.closest_e->p && 
	    isAlive(_state.closest_e->p) &&
	    _state.closest_e->dist < 5000)
	    pwarfare(type, _state.closest_e, _state.closest_e->p, 
	       _state.closest_e->dist);
      }
      pwarfare(type, e, j, edist);
   }
}

rship_ok(n, s)

   char		*n;
   int		s;
{
   if(_server == SERVER_GRIT) return 0;
   if(strcmp(n, "Hoser") == 0)
      return 0;

   switch(s){
      case DESTROYER:
      case CRUISER:
      case BATTLESHIP:
      case ASSAULT:
      case SCOUT:
      case GALAXY:
	 return 1;
      default:
	 return 0;
   }
}

qtorps(size, urnd, tcrs)

   int	size, urnd;
   unsigned char	tcrs;
{
   register	i;
   int	v;

   for(i=0; i < size; i++){

      if(urnd){
	 v = urnd - RANDOM() % (2*urnd+1);
	 v = NORMALIZE(tcrs + v);
	 qtorp(v);
      }
      else
	 qtorp(tcrs);
   }
}

qtorp(crs)

   unsigned char        crs;
{
   TorpQ	*tq = _state.torpq;

   if(tq->ntorps == 2*MAXTORP)
      return;

   tq->torp_dir[tq->ipos] = crs;

   tq->ipos++;
   tq->ntorps ++;

   if(tq->ipos == 2*MAXTORP)
      tq->ipos = 0;
}

/* called from intrupt() */
do_torps()
{
   int  		r;
   unsigned char	tdir;
   TorpQ		*tq = _state.torpq;

   if(tq->ntorps == 0)
      return;
   
   tdir = tq->torp_dir[tq->opos];
   r = req_torp(tdir);
   if(r < 0){
      /* out of fuel or too many torps -- clear the queue */
      emptytorpq();
   }
   else if (r == 1){	/* successfully fired */
      tq->opos++;
      tq->ntorps --;
      if(tq->opos == 2*MAXTORP)
	 tq->opos = 0;
   
   }
   /* otherwise just mistimed */
}

emptytorpq()
{
   TorpQ	*tq = _state.torpq;
   tq->ntorps = 0;
   tq->ipos = tq->opos = 0;
}

get_torp_course(j, crs, cx,cy)

   struct player        *j;
   unsigned char        *crs;
   int			cx,cy;
{
   double       vxa, vya, vxs, vys, vxt, vyt;
   double       dp, l, dx,dy;
   register int	x,y, mx,my, speed;
   unsigned char	dir;

   static int	badguess, pspeed;
   static unsigned char	pdir;
   static struct player	*pj;

   speed = j->p_speed;
   dir = j->p_dir;

   if((_state.human && !(j->p_flags & PFROBOT)) || randtorp){

      if(badguess && pj == j){
	 speed = pspeed;
	 dir = pdir;
	 badguess ++;
	 if(badguess == 3+_state.human)
	    badguess = 0;
      }
      else if(RANDOM()%10 < 5){

	 /* make this a function of distance */
	 badguess = 1;
	 if(RANDOM()%2)
	    pspeed = j->p_speed+(1+(RANDOM()%2)+_state.human*2);
	 else
	    pspeed = j->p_speed-(1+(RANDOM()%2)+_state.human*2);

	 if(RANDOM()%2)
	    pdir = j->p_dir + (1 + (RANDOM()%5+_state.human*4));
	 else
	    pdir = j->p_dir - (1 + (RANDOM()%5+_state.human*4));

	 pj = j;

      }
   }
      
   if(j->p_flags & PFORBIT){
      get_orbit_torp_course(j, crs, cx, cy);
      return 1;
   }

   x = j->p_x + _avsdelay * (double)speed * Cos[dir] * WARP1;
   y = j->p_y + _avsdelay * (double)speed * Sin[dir] * WARP1;
   mx = cx;
   my = cy;
      
   vxa = (x - mx);
   vya = (y - my);

   l = (double)hypot(vxa, vya);
   if(l != 0) {
      vxa /= l;
      vya /= l;
   }
   vxs = (Cos[dir] * speed) * WARP1;
   vys = (Sin[dir] * speed) * WARP1;
   dp = vxs * vxa + vys * vya;  /* dot product of (va vs) */
   dx = vxs - dp * vxa;
   dy = vys - dp * vya;
   l = (double)hypot(dx, dy);
   dp = (double) (me->p_ship.s_torpspeed * WARP1);
   l = (dp*dp - l*l);
   if(l > 0){
      l = sqrt(l);
      vxt = l * vxa + dx;
      vyt = l * vya + dy;
      *crs = get_acourse((int)vxt+mx, (int)vyt+my, cx, cy);
      return 1;
   }
   /* out of range? */
   return 0;
}

get_orbit_torp_course(j, crs, cx, cy)
   
   struct player	*j;
   unsigned char	*crs;
   int			cx,cy;
{
   register		x,y, px,py;
   int			edist,cycles;
   unsigned char	dir;
   double		dx,dy;

   px = planets[j->p_planet].pl_x;
   py = planets[j->p_planet].pl_y;

   x = px;
   y = py;

   /* cycles to reach center of planet */
   edist = (int)hypot((double)(x-cx), (double)(y-cy));
   cycles = edist/(me->p_ship.s_torpspeed * WARP1);

   dir = j->p_dir;

   /* location of player after cycles */
   x = px+ORBDIST * Cos[(unsigned char)(dir+2*cycles - (unsigned char) 64)];
   y = py+ORBDIST * Sin[(unsigned char)(dir+2*cycles - (unsigned char) 64)];

   *crs = get_acourse(x,y, cx,cy);
}

/* called from update_players */
planet_from_ppos(j)

   struct player	*j;
{
   int				x,y;
   register struct planet	*pl;
   register int			i;

#ifdef NO_PFORBIT
   x = j->p_x - ORBDIST * Cos[(unsigned char)(j->p_dir - (unsigned char) 64)];
   y = j->p_y - ORBDIST * Sin[(unsigned char)(j->p_dir - (unsigned char) 64)];

   /*
   printf("in planet_from_ppos for %s\n", j->p_name);
   */

   for(i=0,pl=planets; i< MAXPLANETS; i++,pl++){
      if(ABS(pl->pl_x - x) < ORBDIST && ABS(pl->pl_y - y) < ORBDIST){
	 /*
	 printf("%s orbiting %s\n", j->p_name, pl->pl_name);
	 */
	 return pl->pl_no;
      }
   }
   /*
   printf("%s not orbiting anything.\n", j->p_name);
   */
   return -1;
#endif
   
   for(i=0, pl=planets; i < MAXPLANETS; i++, pl++){
      if(ABS(pl->pl_x - j->p_x) < 5000 && ABS(pl->pl_y - j->p_y) < 5000)
	 return pl->pl_no;
   }
   return -1;
}
      
phaser_condition(e, edist)

   Player	*e;
   int		edist;
{
   struct player	*j = e->p;
   unsigned char	crs;

   return 1;		/* for now */

#ifdef nodef
   if(_serverdelay > 2.0 || _state.human){

      if(j->p_speed <= 6)
	 return 1;
      
      crs = e->crs;
      if(attacking(e, crs, 32) && edist < 6000)
	 return 1;
      if(running_away(e, crs, 32) && edist < 6000)
	 return 1;
      
      return 0;
   }
   return 1;
#endif
}
