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

extern unsigned char get_pl_course();

disengage()
{
   /* one way to know we are recharging */
   if((me->p_flags & PFORBIT) && !hostilepl(&planets[me->p_planet])){
      if(!(_state.refit_req && &planets[me->p_planet] != home_planet()) &&
	 !(_state.refit_req && me->p_flags & PFDOCK)){
	 if(DEBUG & DEBUG_DISENGAGE){
	    printf("orbiting.\n");
	 }
	 rdisengage_c("orbiting");
	 _timers.disengage_time = 0;
	 _state.recharge = 1;
	 return;
      }
   }
   else if(me->p_flags & PFDOCK){
      /* for case below when we wish to recharge on a starbase */
      rdisengage_c("docking");
      _timers.disengage_time = 0;
      _state.recharge = 1;
      return;
   }

   if(me->p_flags & PFCLOAK){
      if(!_state.closest_e) req_cloak_off("disengage: no enemy");

      if((_state.closest_e && _state.closest_e->dist > 10000)
	 /* first try at using friends */
#ifdef nodef
	 || (_state.closest_f && _state.closest_f->dist < 7000)
#endif
	 )
	 req_cloak_off("disengage: enemy farther than 10000");
      
#ifdef nodef
      /* last ditch */
      if((me->p_ship.s_type != SCOUT) && MYFUEL() < 10)
	 req_cloak_off("disengage: fuel < 10");
#endif
   }
      

   if(_state.protect)
      goto_planet(_state.protect_planet);
   else if(/*inl &&*/ _state.defend)
      disengage_defend();
   else
      disengage_normal();
}

disengage_defend()
{
   Player		*e = _state.closest_e;
   Player		*p = _state.protect_player;
   struct player	*dp = p->p;
   struct planet	*pl;
   int			danger;
   unsigned char	crs, crs_r;
   int			speed, speed_r, hittime, lvorbit;

   if(dp->p_ship.s_type == STARBASE){
      goto_protect(NULL, p, 1);
      return;
   }

   if(dp->p_flags & PFORBIT){
      pl = &planets[dp->p_planet];
      if(!hostilepl(pl) && 
	 ((pl->pl_flags & PLREPAIR) || (pl->pl_flags & PLFUEL))){
	 if(DEBUG & DEBUG_PROTECT){
	    printf("going to planet %s\n", pl->pl_name);
	 }
	 goto_planet(pl);
	 return;
      }
   }

   if(e && e->p)
      danger = (_state.torp_danger || _state.recharge_danger || 
	 /* ATTACKING */
	 (e->dist < e->hispr+2000 && e->fuel > 10));
   else
      danger = 0;
   
   crs = p->crs;
   
   if(dp->p_speed > mylowfuel_speed())
      speed = mylowfuel_speed()-1;
   else
      speed = dp->p_speed - 1;
   
   if(speed < 0) speed = 0;

   if(speed < 3 && (_state.recharge_danger || _state.torp_danger))
      speed = 3;

   if(p->dist > defend_dist(p)+2000)
      speed = mylowfuel_speed();

   if(DEBUG & DEBUG_PROTECT){
      printf("disenaging at %d\n", speed);
   }

   if(speed == 0 && !danger){
      if(DEBUG & DEBUG_PROTECT){
	 printf("beginning recharge.\n");
      }
      rdisengage_c("defend recharge");
      _timers.disengage_time = 0;
      _state.recharge = 1;
   }
   if(me->p_ship.s_type == STARBASE)
      speed = 3;	/* XX */

   compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
   req_set_course(crs_r);
   req_set_speed(speed_r, __LINE__, __FILE__);
}

disengage_normal()
{
   Player		*e = _state.closest_e, *sb, *sb_check();
   struct planet	*pl;
   int			pdist, sbdist;
   int			max_distance = (me->p_ship.s_maxspeed*30000)/9;

   if(_state.assault_req == ASSAULT_BOMB)
      max_distance = ((_state.maxspeed/2) * 20000)/9;
   
   if(_state.assault_req == ASSAULT_TAKE && !_state.assault &&
      _state.army_planet){
      if((pl = find_safe_planet(e, &pdist, 0)) && pdist >= 
	 _state.army_planet->pl_mydist){
	 _state.planet = _state.army_planet;
      }
   }

   pl = _state.planet;
   if(!pl){
      if(_state.chaos){	/* refuels very fast */
	 if(MYDAMAGE() < 30)
	    max_distance = 10000;
      }
      sb = sb_check(&sbdist);
      if(DEBUG & DEBUG_DISENGAGE){
	 printf("sb check returns %s(%d)\n", sb?sb->p->p_name:"(none)",sbdist);
      }
      if(_state.diswhy == EKILLED || _state.diswhy == ENONE || me->p_ship.s_type == STARBASE)
	 pl = find_safe_planet(e, &pdist, 1);
      else
	 pl = find_safe_planet(e, &pdist, 0);
      if(DEBUG & DEBUG_DISENGAGE){
	 if(pl){ printf("find_safe_planet: %s\n", pl->pl_name); }
      }
      if(pl && (pdist < max_distance /*|| !e || e->dist > 26666-max_distance*/)
	 && pdist < sbdist){
	 if(DEBUG & DEBUG_DISENGAGE){
	    printf("choosing %s\n", pl->pl_name);
	 }
	 _state.planet = pl;
	 goto_planet(pl);
	 return;
      }
      else if(sb && (sbdist < max_distance || !e || e->dist > 
	 26666-max_distance)){
	 _state.arrived_at_player = 0;
	 _state.protect_player = sb;
	 goto_protect(NULL, sb, /* dock */1);	/* xx */
      }
      else{
	 /* run away */
	 if(e)
	    runaway(e);
	 else {
	    rdisengage_c("no planet recharge -- no enemy");
	    _timers.disengage_time = 0;
	    _state.recharge = 1;
	 }
	 return;
      }
   }
   else {
      /* got planet already */
      goto_planet(pl);
      return;
   }
}

Player *sb_check(dist)

   int	*dist;
{
   register			i,j, c = 0;
   register Player		*p, *sb = NULL;

   *dist = INT_MAX;

   if(me->p_ship.s_type == STARBASE) return NULL;

   for(i=0, p=_state.players; i < MAXPLAYER; i++,p++){

      if(!p->p || p->p->p_status != PALIVE || p->p == me || p->enemy)
	 continue;
      
      if(p->p->p_ship.s_type == STARBASE && !p->sb_dontuse){
	 sb = p;
	 break;
      }
   }

   if(!sb) return NULL;
   if(!(sb->p->p_flags & PFDOCKOK)) return NULL;
   if(PL_FUEL(sb) < 18) return NULL;

   /* now determine how much under attack he is */

   for(i=0, p=_state.players; i < MAXPLAYER; i++,p++){
      
      if(!p->enemy || !p->p || p->p->p_status != PALIVE)
	 continue;
      for(j=0; j< MAXPLAYER; j++)
	 if(p->attacking[j] == sb && p->distances[j] < 9000)
	    c++;
   }
   if(c > 0) 
      return NULL;
   
   *dist = sb->dist;
   return sb;
}

runaway(e)

   Player	*e;
{
   unsigned char	crs, crse, crs_r;
   int			speed, speed_r, hittime, cloak;
   struct player	*j = e?e->p:NULL;
   int			danger, lvorbit;
   struct planet	*pl;

   if(!e || !e->p || !isAlive(e->p)){
      rdisengage_c("no enemy - no planet recharge");
      _timers.disengage_time = 0;
      _state.recharge = 1;
      return;
   }

   if(DEBUG & DEBUG_DISENGAGE){
      printf("running away.\n");
   }


   pl = nearest_safe_planet();

   /*
   printf("running away..\n");
   if(pl)
      printf("%s nearest safe\n", pl->pl_name);
   else
      printf("no safe.  Home planet.\n");
   */

   /* speed & cloak are ignored */
   crs = get_pl_course(pl, &speed, &cloak);

   crse = e->crs;
   crs = choose_course(crs, crse);

   speed = disengage_speed(e, j, 0, 0);

   danger = (_state.torp_danger || _state.recharge_danger || _state.pl_danger ||
      (e->dist < e->hispr && e->fuel > 10));

   if(me->p_ship.s_type == STARBASE)
      /*
      danger = (danger || !in_home_area());
      */
      danger = 1;	/* don't sit ever */

   if((e->dist > edistfunc(e,j) && !danger )
      || (e->dist > edistfunc(e,j)-2000 && !danger && !edang(e,32))){
      if(DEBUG & DEBUG_DISENGAGE){
	 printf("enemy far enough away (%d) -- recharging.\n", e->dist);
      }
      speed = 0;
      rdisengage_c("enemy far away, no planet recharge");
      _timers.disengage_time = 0;
      _state.recharge = 1;
   }

   compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
   req_set_course(crs_r);
   req_set_speed(speed_r, __LINE__, __FILE__);
}

edistfunc(e, j)

   Player		*e;
   struct player	*j;
{
   return 10000;
}

goto_planet(pl)

   struct planet	*pl;
{
   Player		*e = _state.closest_e;
   double		dx,dy;
   int			pdist;
   unsigned char	crs, crs_r;
   int			speed, speed_r, hittime, cloak;
   int			lvorbit, lkreq=0, dntlock=0;

   pdist = pl->pl_mydist;

   speed = myfight_speed();

   /* speed ignored */
   crs = get_pl_course(pl, &speed, &cloak);
#ifdef nodef
   crs = get_wrapcourse(pl->pl_x, pl->pl_y);
#endif

   if(_state.recharge_danger || (e && e->dist < 8000) || 
      (e && e ->dist < 10000 && edang(e, 40))){
       dntlock = 1;
       speed = myfight_speed();
   }
   if(dntlock && e && !starbase(e) && PL_FUEL(e) < 10){
      dntlock = 0;
   }

   if(_state.torp_danger){		/* XX */
      dntlock = 1;
      speed = myfight_speed();
   }
   
   dntlock = (dntlock || !_state.do_lock);

   /*
   if(dntlock) printf("Don't lock\n");
   else printf("Lock allowed\n");
   */

   if(pdist - ORBDIST/2 < (14000 * (me->p_speed+1) * (me->p_speed+1)) / 
      SH_DECINT(me)){

      if(!_state.lock && !dntlock){

	 if(DEBUG & DEBUG_DISENGAGE){
	    printf("closing in on %s\n", pl->pl_name);
	    printf("lock request.\n");
	 }
	 req_planetlock(pl->pl_no);
	 _state.lock = 1;
	 lkreq=1;
      }
   }
   else {
      _state.lock = 0;
      if(DEBUG & DEBUG_DISENGAGE){
	 printf("not close enough to planet %s -- continuing.\n",
	    pl->pl_name);
      }
      if(angdist(me->p_dir, crs) > 40) 
	 speed = 2;
      else
	 speed = disengage_speed(e, e?e->p:NULL,
	    1, pdist);
   }
   
   if((!_state.lock && !lkreq) || (me->p_speed == 0 && _state.p_desspeed == 0)){
      compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
   }
}

/* 
 * called from s_defense to reset
 * _state.recharge/_state.disengage, etc. 
 */

handle_disvars(e, speed)
   
   Player	*e;
   int		*speed;
{
   int			edist;
   struct player	*j;

   _state.do_lock = 1;

   if(!e) return;

   /*
   if(_state.disengage && _state.diswhy == ENONE){
      rdisengage_c("no reason");
      return;
   }
   */

   j = e->p;

   edist = edist_to_me(j);

   if(_state.recharge || _state.disengage || (me->p_speed == 0 || 
      _state.p_desspeed == 0)){

      if(PL_FUEL(e) > 10 &&((edist < 10000 && edang(e, 64)) || edist < 8000)){
	 /* one exception to above */
	 if(!( (e->p->p_flags & PFCLOAK) && _state.protect &&
	    (_state.protect_planet->pl_flags & PLFUEL) &&
	    edist > 3000 && !_state.recharge_danger)){

	    if(me->p_speed < myfight_speed())
	       *speed = myfight_speed();
	    _state.do_lock = 0;

	    if(_state.recharge && (DEBUG & DEBUG_DISENGAGE)){
	       printf("breaking off recharge (%d) -- enemy\n", _state.recharge);
	    }
	    recharge_off("enemy close");
	 }
	 req_repair_off();
      }
      if(edist < 8000 && !_state.protect && !_state.defend){

         if(MYFUEL() > 50 && MYDAMAGE() < 20 && me->p_ship.s_type != STARBASE){
	    if(!(_state.disengage && _state.diswhy == EREFIT)){
	       if(RANDOM()%100 > 25) 
		  rdisengage_c("random but good fuel/dam");
	    }
	 }
	 /*
         else if(RANDOM()%100 > 25)
	    disengage_off();
	 */
      }
   }
   if(!_state.protect && !_state.defend){
      switch(_state.diswhy){
	 case EDAMAGE:
	    if(MYDAMAGE() < 25 && (me->p_ship.s_type != STARBASE))
	       rdisengage_c("damage repaired");
	    break;
	 case ELOWFUEL:
	    if(MYFUEL() > 60)
	       rdisengage_c("fuel recharged");
	    break;
	 case EOVERHEAT:
	    if(!((me->p_flags & PFWEP) || (me->p_flags & PFENG)))
	       if(MYWTEMP() < 50 && MYETEMP() < 50)
		  rdisengage_c("temperatures cooled");
	    break;
      }
   }
}

edang(e, r)

   Player	*e;
   int		r;
{
   struct player	*j = e->p;
   unsigned char	cse = e->crs;
   
   if(!attacking(e, cse, r))
      return 0;

   if(j->p_speed < 2)
      return 0;

   return 1;
}

dng_speed(e)

   Player	*e;
{
   int	s = e->p->p_speed;

   switch(e->p->p_ship.s_type){
      
      case SCOUT:
	 return s >= 11;
      case DESTROYER:
	 return s >= 9;
      case CRUISER:
	 return s >= 8;
      case BATTLESHIP:
	 return s >= 7;
      case ASSAULT:
	 return s >= 8;
      case STARBASE:		/* XXX */
	 return s >= 4;
      case GALAXY:
	 return s >= 6;
   }
   return 12;
}

/* When to attack player at dangerous speed */

dng_speed_dist(e)

   Player	*e;
{
   int	s = e->p->p_speed;

   switch(e->p->p_ship.s_type){
      case SCOUT:
	 return (s*5000)/10;
      case DESTROYER:
	 return (s*6500)/9;
      case CRUISER:
	 return (s*8000)/8;
      case BATTLESHIP:
	 return (s*8500)/7;
      case ASSAULT:
	 return (s*8000)/8;
      case STARBASE:
	 return 500;
      case GALAXY:
	 return (s*9000)/7;
   }
   return 6000;	/* XX starbase */
}

recharge_off(s)

   char	*s;

{
   if(_state.recharge){
      _state.recharge = 0;
      _state.p_desspeed = me->p_speed;
      _state.p_desdir = me->p_dir;
      reset_planet();
      req_repair_off();

      /* redo disengage */
      disengage_c(_state.diswhy, NULL, s);
   }
   _state.lock = 0;
}

reset_planet()
{
   if(!_state.protect){
      if(!(_state.disengage && _state.diswhy == EREFIT))
	 _state.planet = NULL;
   }
}

struct {
    int x;
    int y;
} center[] = { {0, 0},
                {GWIDTH / 4, GWIDTH * 3 / 4},           /* Fed */
                {GWIDTH / 4, GWIDTH / 4},               /* Rom */
                {0, 0},
                {GWIDTH * 3 / 4, GWIDTH  / 4},          /* Kli */
                {0, 0},
                {0, 0},
                {0, 0},
                {GWIDTH * 3 / 4, GWIDTH * 3 / 4}};      /* Ori */

home_crs()
{
   return get_wrapcourse(center[me->p_team].x, center[me->p_team].y);
}

home_dist()
{
   double	dx = (me->p_x - center[me->p_team].x),
		dy = (me->p_y - center[me->p_team].y);
   return (int) hypot(dx,dy);
}

#define NEUTRAL_ZONE	500

in_home_area()
{
   return in_team_area(me->p_team);
}

in_team_area(team)
   int	team;
{
   switch(team){
      
      case FED:
	 return (me->p_x < (GWIDTH/2-500) && (me->p_y > (GWIDTH/2+500)));
      
      case ROM:
	 return (me->p_x < (GWIDTH/2-500) && (me->p_y < (GWIDTH/2-500)));
      
      case KLI:
	 return (me->p_x > (GWIDTH/2+500) && (me->p_y < (GWIDTH/2-500)));
      
      case ORI:
	 return (me->p_x > (GWIDTH/2+500) && (me->p_y > (GWIDTH/2+500)));
   }
   return 0;
}

/* true if we have to go towards enemy home area */
dangerous_dir(pcrs, r)

   unsigned char	pcrs;
{
   int			t = _state.warteam;
   unsigned char hcrs = 
      get_wrapcourse(center[t].x, center[t].y);
   
   return angdist(pcrs, hcrs) < r;
}

struct planet *home_planet()
{
   struct planet *team_planet();
   return team_planet(me->p_team);
}

struct planet *team_planet(team)
   int	team;
{
   switch(team){
      case FED:
	 return name_to_planet("earth", 3);
      case ROM:
	 return name_to_planet("romulus", 3);
      case KLI:
	 return name_to_planet("klingus", 3);
      case ORI:
	 return name_to_planet("orion", 3);
   }
   return NULL;
}

unsigned char to_team_planet(team)

   int	team;
{
   struct planet *hp = team_planet(team);
   if(!hp) return get_wrapcourse(GWIDTH/2,GWIDTH/2);
   return get_wrapcourse(hp->pl_x, hp->pl_y);
}

orbiting_home()
{
   struct planet	*h = home_planet();
   if(DEBUG & DEBUG_DISENGAGE){
      printf("home planet: %s\n", h?h->pl_name:"none");
      printf("my planet: %d, h: %d\n", me->p_planet, h?h->pl_no:-1);
   }
   return (me->p_flags & PFORBIT) && me->p_planet == h->pl_no;
}

/* runaway disengage speed */

disengage_speed(e, j, dplanet, pdist)

   Player		*e;
   struct player	*j;
   int			dplanet;	/* bool */
   int			pdist;
{
   int	fuel = MYFUEL();
   int	speed;
   int	heading_home = angdist(me->p_dir, home_crs());

   if(j && isAlive(j)){
      if(e->robot){
	 speed = mylowfuel_speed()+3;
	 return speed;
      }

      if(e->dist < e->hispr){
	 speed = MAX(j->p_speed+2, mylowfuel_speed());
      }
      else if(e->dist < 10000 && attacking(e, me->p_dir, 32)){
	 if(PL_FUEL(e) < fuel)
	    speed = MAX(j->p_speed, mylowfuel_speed());
	 else if(PL_FUEL(e) > fuel-30 && fuel > 15){
	    speed = MAX(j->p_speed+3, mylowfuel_speed());
	 }
	 else
	    speed = MAX(j->p_speed+1, mylowfuel_speed());
      }
      else{
	 if(home_dist() < GWIDTH/8 + 5000)
	    speed = mylowfuel_speed()+3;
	 else
	    speed = mylowfuel_speed()+1;
      }
   }
   else
      speed = mylowfuel_speed()+2;
   
   if(heading_home && home_dist() < GWIDTH/8+5000)
      speed ++;

   if(speed > _state.maxspeed)
      speed = _state.maxspeed;
   
   if(MYETEMP() > 90)
      speed -= 2;
   
   if(me->p_ship.s_type == STARBASE)
      speed = 3;	/* fast as possible */
   
   /* XXX */
   speed = myemg_speed();
   
   return speed;
}
