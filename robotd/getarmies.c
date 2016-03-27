#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

struct distress *loaddistress(enum dist_type i);
void send_RCD(struct distress *dist);
struct planet *find_army_planet();

/* get armies */

getarmies()
{
   register			i;
   register struct planet	*pl;
   struct planet		*aspl = _state.assault_planet;
   struct planet		*apl = _state.army_planet;
   int				tarmy;

   apl = _state.army_planet = find_army_planet(apl);
   if(!apl){
      if(DEBUG & DEBUG_ASSAULT){
	 printf("no armies.\n");
      }
      if(me->p_armies == 0)
	 unassault_c("no armies.");
      else{	/* even if we don't have enough */
	 _state.p_desspeed = me->p_speed;
	 _state.lock = 0;
	 _state.assault = 1;

	 notify_take(aspl, 0);
      }
	 
      return;
   }

   tarmy = 5;
   
   if(apl && apl->pl_armies > 10){
      if(inl)
	 tarmy = 5;
      else
	 tarmy = 8;
   }
   
   if(tarmy > troop_capacity(me)) 
      tarmy = troop_capacity(me);

   if(me->p_armies >= tarmy){ 
      if(me->p_flags & PFBEAMUP){
	 req_shields_up();
      }
      _state.p_desspeed = me->p_speed;
      _state.lock = 0;
      _state.assault = 1;

      notify_take(aspl, 0);
      return;
   }

   if(DEBUG & DEBUG_ASSAULT){
      printf("%s chosen for armies.\n", apl->pl_name);
   }

   if(!orbiting(apl)){
      goto_planet(apl);
      return;
   }
   beam_up_armies(apl);
}

/* UNUSED */
goto_armypl(pl)

   struct	planet	*pl;
{
   Player		*e = _state.closest_e;
   struct player	*j = e?e->p:NULL;
   unsigned char	pcrs,ecrs,crs_r;
   int			pdist,edist,dectime, lvorbit;
   double		dx,dy;
   int			speed,speed_r,hittime,orbit=0, cloak;
   int			sb = (me->p_ship.s_type == STARBASE);

   dx = pl->pl_x - me->p_x;
   dy = pl->pl_y - me->p_y;
   pdist = (int)ihypot(dx,dy);
   pcrs = get_wrapcourse(pl->pl_x,pl->pl_y);

   dectime = (pdist - (ORBDIST/2) < (14000 * me->p_speed * me->p_speed)/
      SH_DECINT(me));
   
   if(pdist > 20000){
      if((pl->pl_flags & PLFUEL) || pdist < 30000)
	 speed = myemg_speed();
      else
	 speed = mymax_safe_speed();
   }
   else
      speed = mymax_safe_speed();

   if(j){
      ecrs = _state.closest_e->crs;
      edist = e->dist;
      if((edist < 9000 || _state.recharge_danger) && pdist > 8000){
	 if(angdist(pcrs,ecrs) <= 32){
	    speed = sb?mymax_safe_speed():mymax_safe_speed()-2;
	    avoiddir(ecrs,&pcrs,32);
	 }
      }
   }
   if(pdist < 8000 && !dectime){
      /* look for more enemies here */
      _state.lock = 0;
      if(j){
         if(e->dist < 9000  && e->dist > 5000 && PL_FUEL(e) > 40){
            speed = mylowfuel_speed();
         }
         else if(e->dist < 5000 && PL_FUEL(e) > 30)
            speed = myfight_speed();
      }
   }
   else if(dectime){
      if(!j || (e->dist > 5000 || PL_FUEL(e) < 30)){
	 
	 if(!_state.lock && _state.do_lock){
	    if(DEBUG & DEBUG_ASSAULT){
	       printf("lock request: %s\n", pl->pl_name);
	    }
	    req_planetlock(pl->pl_no);
	    _state.lock = 1;
	 }
      }
      else{
         speed = myfight_speed();
	 _state.lock = 0;
      }
   }

   if(angdist(me->p_dir, pcrs) > 32 && speed > 5)
      speed = 5;

   if(!_state.lock){
      compute_course(pcrs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
   }
}

beam_up_armies(pl)
   
   struct planet	*pl;
{
   int		armies = pl->pl_armies;

   if(armies > 4){
      /* wait til cloaked before beaming up */
      if(!(me->p_flags & PFCLOAK)){
	 req_cloak_on();
	 return;
      }
      req_beamup();
   }
   else{
      _state.lock = 0;
      req_shields_up();
      req_cloak_off("no armies to beam");
      _state.p_desspeed = me->p_speed;
      return;
   }
}

troop_capacity(j)
   
   struct player	*j;
{
   if (j->p_ship.s_type == ASSAULT)
      return (((j->p_kills * 3) > j->p_ship.s_maxarmies) ?
	 j->p_ship.s_maxarmies : (int) (j->p_kills * 3));
   else if (j->p_ship.s_type != STARBASE)
      return (((j->p_kills * 2) > j->p_ship.s_maxarmies) ?
	 j->p_ship.s_maxarmies : (int) (j->p_kills * 2));
   else
      return j->p_ship.s_maxarmies;
}


others_taking_armies(pl)

   struct planet	*pl;
{
   register			i;
   register Player		*p;
   register struct player	*j;

   for(i=0,p=_state.players; i < MAXPLAYER; i++,p++){
      if(!p->enemy && p->p && isAlive(p->p) && p->p != me){
	 j = p->p;
	 if((j->p_flags & PFORBIT) && j->p_planet == pl->pl_no){
	    if(j->p_kills > .99){
	       if(DEBUG & DEBUG_ASSAULT){
		  printf("found %s possibly taking armies.\n",
		     j->p_name);
	       }
	       return 1;
	    }
	 }
      }
   }
   return 0;
}

struct planet *find_army_planet(pl)

   struct planet	*pl;
{
   register			i;
   register struct planet	*k, *mp = NULL;
   int				mdist = INT_MAX, pdist, hdist;
   Player			*p, *get_nearest_to_pl_dist();
   double			dx,dy;
   unsigned char		pcrs;

   for(i=0,k=planets; i< MAXPLANETS; i++,k++){
      if(k->pl_owner != me->p_team)
	 continue;
#ifdef nodef
      if(k == pl)
	 continue;
#endif
      if(k->pl_armies < 5)
	 continue;

#ifdef nodef
      p = get_nearest_to_pl_dist(k, &hdist);
      if(p && (hdist < 6000 || (hdist < 15000 && edang(p, 64))))
	 continue;
#endif

#ifdef nodef
      dx = k->pl_x - me->p_x;
      dy = k->pl_y - me->p_y;
      pdist = (int)ihypot(dx,dy);
#endif
      pdist = k->pl_mydist;

      if(pdist < mdist){
         pcrs = get_wrapcourse(k->pl_x, k->pl_y);
	 if(me->p_armies > 0 && not_safe(k, pcrs, pdist) 
	    && _server != SERVER_GRIT)
	    continue;
	 mp = k;
	 mdist = pdist;
      }
   }
   if(DEBUG & DEBUG_ASSAULT)
      printf("find_army_planet returns %s\n", mp?mp->pl_name:"(null)");
   return mp;
}

orbiting(pl)

   struct planet	*pl;
{
   if(!pl){
      mprintf("bad value in orbiting.\n");
      return 0;
   }
   return (me->p_flags & PFORBIT) && me->p_planet == pl->pl_no;
}

need_armies()
{
   int	tarmy;

   if(unknownpl(_state.assault_planet) || _state.assault_planet->pl_armies > 4)
      tarmy = 5;
   else
      tarmy = _state.assault_planet->pl_armies + 1;
   
   if(tarmy > troop_capacity(me))
      tarmy = troop_capacity(me);
   
   return me->p_armies < tarmy;
}

notify_take(aspl, phase)
   
   struct planet	*aspl;
   int			phase;
{
   static int 	tms;
   char 	*short_name_print();
   struct distress *dist;
   
   if(!inl && !_state.take_notify) return;
   if(!aspl) return;

   if(phase == 0){
      tms = _udcounter;
      dist = loaddistress(CARRYING);
      send_RCD(dist);
   }
   else if(tms > -1){
      if(_udcounter - tms > 10){
	 dist = loaddistress(TAKE);
	 dist->tclose_pl = aspl->pl_no;
	 send_RCD(dist);
	 tms = -1;
      }
   }
}
