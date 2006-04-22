#include <stdio.h>
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

unsigned char choose_ecourse();

/* called from s_target in robot.c */

engage(e)

   Player		*e;
{
   /* right place? */
   req_cloak_off("engaging");

   if(_state.protect)
      plengage();
   else if(_state.defend)
      dengage(e);
   else
      pengage(e);
}

plengage()
{
   Player		*e;
   unsigned char	crs = _state.p_desdir, crs_r;
   int			speed = _state.p_desspeed, speed_r, hittime;
   int			lvorbit;
   int			e_dist;
   struct player	*j;

   if(!_state.arrived_at_planet){
      if(DEBUG & DEBUG_PROTECT){
	 printf("not arrived at planet.\n");
      }
      goto_protect(_state.protect_planet, NULL, 0);
      return;
   }
   else if(_state.protect){
      e = _state.current_target;
      if(!e) {
	 if(DEBUG & DEBUG_PROTECT){
	    printf("no enemy nearest to planet.\n");
	 }
	 if(MYDAMAGE() > 0 || MYFUEL() < 75 || MYSHIELD() < 90){
	    disengage_c(EPROTECT, _state.protect_planet, "protect");
	    _timers.disengage_time = mtime(0);
	 }
	 else
	    speed = 0;
	 if(angdist(me->p_dir, crs) > 40)
	    speed = 2;
	 compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
	 req_set_course(crs_r);
	 req_set_speed(speed_r, __LINE__, __FILE__);

	 return;
      }
      j = e->p;
   }

   crs = e->crs;
   if(!j)
      e_dist = 100000;
   else
      e_dist = edist_to_me(j);

   if(e_dist >= 10000){
      if(MYDAMAGE() > 0 || MYFUEL() < 75 || MYSHIELD() < 90){
	 if(DEBUG & DEBUG_PROTECT){
	    printf("enemy far away -- disengaging.\n");
	 }
	 disengage_c(EPROTECT, _state.protect_planet, "protect");
	 _timers.disengage_time = mtime(0);
      }
      else
	 speed = 0;
      if(angdist(me->p_dir, crs) > 40)
	 speed = 2;
      compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
      return;
   }

   if(mydist_to_planet(_state.protect_planet) > 4000){
      /* return to planet */
      if(DEBUG & DEBUG_PROTECT){
	 printf("to far from planet -- setting not arrived-at-planet.\n");
      }
      _state.arrived_at_planet = 0;
      if(angdist(me->p_dir, crs) > 40)
	 speed = 2;
      compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
      return;
   }

   /* otherwise: me and enemy close to planet -- eengage */
   if(!ignoring_e(e))
      pengage(e);
}

dengage(e)

   Player	*e;
{
   unsigned char	crs = _state.p_desdir, crs_r;
   int			speed = _state.p_desspeed, speed_r, hittime, lvorbit;
   int			e_dist;
   struct player	*j;
   Player		*p = _state.protect_player;
   struct player	*dp = p->p;

   /* e is closest player to _state.protect_player */

   if(inl && p->p && p->p->p_flags & PFORBIT){
      rdefend_c("orbiting .. either taking or refueling\n");
      return;
   }

   if(!_state.arrived_at_player){
      if(DEBUG & DEBUG_PROTECT){
	 printf("not arrived at player.\n");
      }
      goto_protect(NULL, _state.protect_player, 0);
      return;
   }
   else {
      /* arrived at player */
      if(!e || !e->p || !isAlive(e->p)){
	 if(DEBUG & DEBUG_PROTECT){
	    printf("no enemy nearest to player.\n");
	 }
	 if(p->dist > defend_dist(p)){
	    if(mymax_safe_speed() > dp->p_speed)
	       speed = mymax_safe_speed();
	    else
	       speed = dp->p_speed+1;
	    crs = choose_ecourse(p, dp, p->dist);
	 }
	 else{
	    speed = dp->p_speed;
	    crs = choose_ecourse(p, dp, p->dist);
	 }
	 if(angdist(me->p_dir, crs) > 40)
	    speed = 2;
	 compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
	 req_set_course(crs_r);
	 req_set_speed(speed_r, __LINE__, __FILE__);
	 return;
      }
      j = e->p;
   }
   /* arrived at player & enemy near */

   e_dist = pdist_to_p(j, dp);

   if(e_dist < e->dist){
      speed = mymax_safe_speed();
      crs = e->icrs;

      if(DEBUG & DEBUG_PROTECT){
	 printf("intercepting enemy %s far away\n", j->p_name);
      }

      if(angdist(me->p_dir, crs) > 40)
	 speed = 2;
      compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
      return;
   }
   else if(p->dist <= defend_dist(p)){

      speed = myfight_speed();
      crs = e->icrs;

      if(DEBUG & DEBUG_PROTECT){
	 printf("engaging enemy %s\n", j->p_mapchars);
      }

      if(angdist(me->p_dir, crs) > 40)
	 speed = 2;
      compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
      return;
   }

   if(p->dist > defend_dist(p)){
      /* return to player */
      if(DEBUG & DEBUG_PROTECT){
	 printf("to far from player -- setting not arrived_at_player.\n");
      }
      _state.arrived_at_player = 0;
      /* course doesn't matter -- will be computed on next cycle */
      if(angdist(me->p_dir, crs) > 40)
	 speed = 2;
      compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
      return;
   }

   /* otherwise: me and enemy close to player -- eengage */
   if(!ignoring_e(e) && e->alive > 30)
      pengage(e);
}

pengage(e)

   Player	*e;
{
   int			edist, e_dist;
   unsigned char	crs = _state.p_desdir, crs_r;
   int			speed = _state.p_desspeed, speed_r, hittime;
   struct player	*j;
   int			lvorbit;
			
   if(!e || (!e->p) || (e->p->p_status != PALIVE)){
      if(MYDAMAGE() > 0 || MYFUEL() < 75 || MYSHIELD() < 90){
	 if(DEBUG & DEBUG_ENGAGE){
	    printf("no enemies -- disengaging.\n");
	 }
	 disengage_c(ENONE, NULL, "no enemies");
	 _timers.disengage_time = mtime(0);
      }
      if(angdist(me->p_dir, crs) > 40)
	 speed = 2;
      compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
      return;
   }
   else
      j = e->p;

   e_dist = edist_to_me(j);
   crs = choose_ecourse(e, j, e_dist);
   edist = mod_fight_dist(e);

   if(e_dist < edist){

      if(DEBUG & DEBUG_ENGAGE){
	 printf("enemy at %d -- beginning fight stage.\n", edist);
      }

      speed = myfight_speed();

      if(!running_away(e, crs, 100))
	 e->run_t = 0;

      /* avoid getting too close if we're not ogging */
      alter_course_distance(e, e_dist, &crs);
   }
   else{ /* not previous engagement distance more than 15000 */
      /* won't be reached for protect planet */
      speed = mymax_safe_speed();
   }
   if(angdist(me->p_dir, crs) > 40)
      speed = 2;

   compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
   req_set_course(crs_r);
   req_set_speed(speed_r, __LINE__, __FILE__);
}

goto_protect(pl, p, dock)
   
   struct planet	*pl;
   Player		*p;
   int			dock;
{
   struct player	*j = _state.closest_e?_state.closest_e->p : NULL;
   unsigned char	pcrs, crs_r, ecrs;
   double		dx,dy;
   int			pdist, speed, speed_r, hittime, edist, lvorbit;
   int			pl_protect = (pl != NULL);
   int			x,y, defd;
   int			sb = (me->p_ship.s_type == STARBASE);
   int			lkreq = 0, dntlock;

   speed = me->p_speed;

   if(!pl && !p){
      mprintf("goto_protect: NULL value.\n");
      return;
   }

   if(pl){
      x = pl->pl_x;
      y = pl->pl_y;
   }
   else{
      x = p->p->p_x;
      y = p->p->p_y;
   }

   dx = x - me->p_x;
   dy = y - me->p_y;
   pdist = (int) ihypot(dx,dy);

   if(pl)
      pcrs = get_wrapcourse(x,y);
   else
      pcrs = p->icrs;
   
   if(pl)
      defd = 4000;
   else
      defd = defend_dist(p);

   if(pdist >= defd){
      
      if(DEBUG & DEBUG_PROTECT){
	 printf("protect farther than %d -- emg speed.\n", defd);
      }
      if(pdist > 10000)
	 speed = sb?myemg_speed():myemg_speed()-2;
      else if(pdist > defd+5000)
	 speed = myfight_speed();
      else
	 speed = myemg_speed();

      if(j){
	 ecrs = _state.closest_e->crs;
	 edist = edist_to_me(j);
	 if(edist < 9000 && pdist > 8000){
	    if(angdist(pcrs, ecrs) <= 32){
	       speed = sb?myemg_speed():myemg_speed()/2;
	       avoiddir(ecrs, &pcrs, 32);

	       if(DEBUG & DEBUG_PROTECT){
		  printf("avoiding enemy.\n");
	       }
	    }
	 }
      }
   }
   else if(pdist < defd){
      if(dock && p) {
	 dntlock = 0;
	 /* attempt to dock on player */
	 if(speed < 3 && (_state.torp_danger || _state.recharge_danger)){
	    speed = 3;
	    dntlock = 1;
	 }
	 dntlock = (dntlock || !_state.do_lock);
	 if(pdist < ORBDIST/2 < (12000* (me->p_speed) * (me->p_speed))/
	    SH_DECINT(me)){
	    if(!_state.lock && !dntlock){
	       if(DEBUG & DEBUG_DISENGAGE){
		  printf("closing in on starbase %s\n", p->p->p_name);
		  printf("lock request.\n");
	       }
	       req_playerlock(p->p->p_no);
	       _state.lock = 1;
	       lkreq = 1;
	    }
	 }
	 else{
	    _state.lock = 0;
	    speed = 4;
	 }
      }
      else  if(!p){
	 if(MYFUEL() < 100 || MYDAMAGE() > 0 || MYSHIELD() < 100){
	    if(DEBUG & DEBUG_PROTECT){
	       printf("protect closer than 5000 -- setting disengage.\n", 
		  defd);
	    }
	    disengage_c(EPROTECT, NULL, "ship not perfect.");
	    _timers.disengage_time = mtime(0);
	    if(pl){
	       _state.planet = _state.protect_planet;
	    }
	    _state.diswhy = EPROTECT;
	 }
      }
      /*
      speed = 0;
      */
      if(pl)
	 _state.arrived_at_planet = 1;
      else
	 _state.arrived_at_player = 1;

      if(DEBUG & DEBUG_PROTECT){
	 printf("arrived at protect.\n");
      }
   }
   
   if(angdist(me->p_dir, pcrs) > 32 && speed > 5)
      speed = 2;
   
   if(!_state.lock && !lkreq){
      compute_course(pcrs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
   }
}


/* enemy */
Player	*get_nearest_to_pl(pl)

   struct planet	*pl;
{
   Player *get_nearest_to_pl_dist();
   return get_nearest_to_pl_dist(pl, NULL);
}

Player *get_nearest_to_pl_dist(pl, rdist)
   struct planet	*pl;
   int			*rdist;
{
   register int			i, dist;
   int				mdist = INT_MAX;
   register struct player	*j;
   register Player		*e, *ce = NULL;
   double			dx,dy;

   for(i=0, e=_state.players; i< MAXPLAYER; i++,e++){
      if(!e || !e->p || !isAlive(e->p) || !e->enemy || e->invisible)
	 continue;

      dist = dist_to_planet(e, pl);

      if(dist < mdist){
	 mdist = dist;
	 ce = e;
      }
   }
   if(rdist) *rdist = mdist;
   return ce;
}

Player *get_nearest_to_p(p, d)

   Player	*p;
   int		*d;
{
   register int			i,dist;
   int				mdist = INT_MAX;
   register struct player	*j;
   register Player		*e, *ce = NULL;
   double			dx,dy;

   for(i=0, e=_state.players; i < MAXPLAYER; i++,e++){
      if(!e || !e->p || !isAlive(e->p) || !e->enemy || e->invisible)
	 continue;
	 
      j = e->p;
      dist = pdist_to_p(p->p, j);

      if(dist < mdist){
	 mdist = dist;
	 ce = e;
      }
   }
   *d = mdist;
   return ce;
}

/* make sure  */
hostilepl(pl)

   struct planet	*pl;
{
   return (me->p_team != pl->pl_owner) &&
      ((me->p_swar | me->p_hostile) & pl->pl_owner);
}

unknownpl(pl)

   struct planet	*pl;
{
   if(!pl) return 0;
   return !(pl->pl_info & me->p_team);
}

/* likely team for unknown planet */

team_areapl(pl)

   struct planet	*pl;
{
   register	x = pl->pl_x, y = pl->pl_y;

   if(!unknownpl(pl))
      return pl->pl_owner;
   
   if(x < GWIDTH/2 && y > GWIDTH/2) return FED;
   if(x < GWIDTH/2 && y < GWIDTH/2) return ROM;
   if(x > GWIDTH/2 && y > GWIDTH/2) return ORI;
   if(x > GWIDTH/2 && y < GWIDTH/2) return KLI;

   return FED;
}

/* unknown planet -- guess what team it is */
guess_team(pl)

   struct planet	*pl;
{
   int	plteam = team_areapl(pl);
   int	team;

   if(plteam == me->p_team){
      /* unknown planet in my area */
      team = war_team();
   }
   else
      team = plteam;

   return team;
}

war_team()
{
   switch(me->p_team){
      case FED:
	 if(me->p_swar & ROM)
	    return ROM;
	 return ORI;
      
      case ROM:
	 if(me->p_swar & FED)
	    return FED;
	 return KLI;
      
      case ORI:
	 if(me->p_swar & FED)
	    return FED;
	 return KLI;
      
      case KLI:
	 if(me->p_swar & ROM)
	    return ROM;
	 return ORI;
   }
   return FED;
}


alter_course_distance(e, e_dist, crs)

   Player		*e;
   int			e_dist;
   unsigned char	*crs;
{
   register int	edist, myship = me->p_ship.s_type, eship = 
      e->p->p_ship.s_type;
   static int		cchange;
   static int		dir = 1, tm;
   int                  hispr = e->hispr - (RANDOM()%1000), dam;
   int			fuel;

   if(myship == SCOUT) 		hispr += 750;
   if(myship == BATTLESHIP) 	hispr -= 750;
   if(myship == GALAXY)		hispr -= 750;

   dam = MYDAMAGE() - PL_DAMAGE(e);
   if(dam > 0)
      hispr += dam*50;
   else
      hispr += dam*50;		/* negative */
   
   fuel = MYFUEL() - PL_FUEL(e);
   if(fuel > 0)
      hispr -= fuel*50;
   else
      hispr -= fuel*50;		/* negative */

   if(PL_FUEL(e) <= 10)
      hispr -= 3000;
   if(PL_DAMAGE(e) >= 90)
      hispr -= 3000;

#ifdef nodef
   switch(myship){
      
      case BATTLESHIP:
	 hispr -= 1000;
	 break;

      case CRUISER:
	 break;
      
      case DESTROYER:
	 hispr += 500;
	 break;

      case ASSAULT:
	 hispr += 500;
	 break;

      case SCOUT:
	 hispr += 1000;
	 break;
      
      case STARBASE:
	 hispr -= 2000;
	 break;
   }
   switch(eship){
      case BATTLESHIP:
	 hispr += 1000;
	 break;
      case ASSAULT:
	 hispr -= 500;
	 break;
      case CRUISER:
	 break;
      case DESTROYER:
	 hispr -= 500;
	 break;
      case SCOUT:
	 hispr -= 1000;
	 break;
      case STARBASE:
	 break;
   }
#endif
   
   if(e_dist < hispr){
      if(DEBUG & DEBUG_ENGAGE){
	 printf("enemy closer than %d -- changing course.\n", hispr);
      }
      if(_udcounter - cchange > tm){
	 if(RANDOM()%2 == 0)
	    dir = 1;
	 else
	    dir = -1;
	 cchange = _udcounter;
	 tm = RANDOM()%50 + 10;
      }
	 
      *crs += (dir*51);
   }
}

mod_fight_dist(e)

   Player	*e;
{
   int	sh	= me->p_ship.s_type;
   int	a 	= (attacking(e, me->p_dir, 32));
   int	s 	= (e->p->p_speed > 6);
   int	lf	= PL_FUEL(e) < 20;
   int	dm	= PL_DAMAGE(e) > 65;
   int	d;
      
   d = 5000;
   switch(sh){
      case SCOUT:
	 d = 9000;
	 if(a) 		d += 2000;
	 if(s) 		d += 2000;
	 if(lf) 	d -= 1000;
	 if(dm)		d -= 2000;
	 break;

      case DESTROYER:
	 d = 10000;
	 if(a) 		d += 2500;
	 if(s) 		d += 2500;
	 if(lf) 	d -= 1000;
	 if(dm)		d -= 2000;
	 break;

      case CRUISER:
	 d = 11000;
	 if(a) 		d += 2750;
	 if(s) 		d += 2750;
	 if(lf) 	d -= 1000;
	 if(dm)		d -= 2000;
	 break;

      case BATTLESHIP:
      case GALAXY:
	 d = 12000;
	 if(a) 		d += 2750;
	 if(s) 		d += 2750;
	 if(lf) 	d -= 1000;
	 if(dm)		d -= 2000;
	 break;

      case ASSAULT:
	 d = 9000;
	 if(a) 		d += 2000;
	 if(s) 		d += 2000;
	 if(lf) 	d -= 1000;
	 if(dm)		d -= 2000;
	 break;
      
      case STARBASE:
	 d = 3000;
	 if(a) 		d += 2000;
	 if(s) 		d += 2000;
	 if(lf) 	d -= 1000;
	 if(dm)		d -= 2000;
	 break;
   }
   if(PL_FUEL(e) <= 10) d = 2000;	/* try */
   if(PL_DAMAGE(e) >= 90) d = 2000;	/* try */

   if(dng_speed(e) && edang(e, 80))
      d += 3000;

   return d;
}

unsigned char choose_ecourse(p, j, edist)

   Player		*p;
   struct player	*j;
   int			edist;
{
   unsigned char	crs;
   static int		tm, lch;
   static int 		rcrs;

   if(edist > 9000){
      crs = p->crs;
      if(!_state.wrap_around)
	 edge_course(j, edist, &crs);
      return crs;
   }
   else {
   
      crs = p->icrs;

      if(_udcounter - lch > tm){
	 rcrs = rrnd((24*5000)/(edist+1));
	 if(rcrs > 32)
	    rcrs = 32;
	 else if(rcrs < -32)
	    rcrs = -32;

	 tm = (RANDOM()%32) + 10;
	 lch = _udcounter;
      }
      crs = crs + (unsigned char)rcrs;
      if(!_state.wrap_around)
	 edge_course(j, edist, &crs);
      return crs;
   }
}

#define EDGE_DIST	20000

/* Try to force player into the edge */
edge_course(j, edist, crs)

   struct player	*j;
   int			edist;
   unsigned char	*crs;
{
#ifdef nodef
   if(j->p_x > GWIDTH - EDGE_DIST)
      /* right edge */
      *crs = get_course(
      /* HERE */
#endif
}

/* based on get_torp_course */
get_intercept_course(p, j, edist, crs)

   Player		*p;
   struct player	*j;
   int			edist;
   unsigned char	*crs;
{
   double		vxa, vya, vxs, vys, vxt, vyt;
   double		dp,l,dx,dy;
   register int		x,y,mx,my;
   int			xr,yr, hspeed = j->p_speed;

   x = j->p_x; y = j->p_y;
   mx = me->p_x; my = me->p_y;

   if(_state.wrap_around){
      x = wrap_x(x, mx);
      y = wrap_y(y, my);
   }

   vxa = (x - mx);
   vya = (y - my);

   l = (double)ihypot(vxa,vya);
   if(l != 0){
      vxa /= l;
      vya /= l;
   }
   if((p->pp_flags & PFCLOAK) && (j->p_ship.s_type != STARBASE) &&
      !(j->p_flags & PFORBIT)){
      hspeed += 2;
   }

   vxs = Cos[j->p_dir] * hspeed * WARP1;
   vys = Sin[j->p_dir] * hspeed * WARP1;
   dp = vxs*vxa + vys * vya;
   dx = vxs - dp * vxa;
   dy = vys - dp * vya;
   l = (double)ihypot(dx,dy);
   dp = (double)(me->p_speed * WARP1);	/* current speed or destination sp? */
   l = (dp*dp - l*l);
   if(l > 0){
      l = sqrt(l);
      vxt = l * vxa + dx;
      vyt = l * vya + dy;
      *crs = get_course((int) vxt+mx, (int)vyt+my);
      return 1;
   }
   *crs = get_course(x,y);
   return 0;
}
