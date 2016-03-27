#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

/* XXX */
void init_timers();
static int	in_game;

/*
 * Stuff that we don't get from the server about ships.
 */

int		_battle[MAXPLAYER];
Player		_players[MAXPLAYER];
PlanetRec	_planets;
TorpQ		_torpq;
Player		*me_p;
int		Xplhit;

State 	_state;
Timers	_timers;	/* some things we don't want to do on every cycle */

void R_NextCommand() 
{
   int			btime, t1, t2;

   if(_state.state == 0) {
      _state.state = S_UPDATE;

      init_timers();
      if(!in_game){
	 sleep(1);
	 in_game = 1;
	 return;
      }
   }
   if(me->p_status == POUTFIT)
      return;

   switch(_state.command){

      case C_QUIT:
	 if(_state.state != S_QUIT)
	    sendQuitReq();
	 _state.state = S_QUIT;
	 return;
      
      case C_RESET:
	 {  int team_r, ship_r;
	    reset_r_info(&team_r, &ship_r, 0);
	 }
	 _state.command = 0;
      
	 break;
   }

   /* state machine */
   if(DEBUG & DEBUG_STATE){
      printf("state: %s\n", state_name(_state.state));
      fflush(stdout);
   }
   if(DEBUG & DEBUG_TIME){
      btime = mtime(1);
   }

   do_alert();

   phaser_plasmas();
   warfare(WPHASER);		/* warfare.c */
   warfare(WTORP);		/* warfare.c */

   handle_unknown_problems();
   init_dodge();	/* dodge.c */

   do_defense();
   goto_state();

   if(DEBUG & DEBUG_TIME){
      t1 = mtime(1);
      printf("fight cycle took %d ms\n", t1-btime); 
      fflush(stdout);
   }

   update_players();
   update_planets();
   decide();

   if(DEBUG & DEBUG_TIME){
      t2 = mtime(1);
      printf("update cycle took %d ms\n", t2 - t1);

      printf("TOTAL: %d ms\n", t2-btime);
   }
   
   _state.lifetime ++;
   _waiting_for_input = mtime(0);
}

goto_state()

{
   switch(_state.state){
      
      case S_REFIT:
	 s_refit();
	 break;
      
      case S_ENGAGE:
	 s_engage();
	 break;
      
      case S_ASSAULT:
	 s_assault();
	 break;
      
      case S_GETARMIES:
	 s_getarmies();
	 break;

      case S_OGG:
	 s_ogg();
	 break;
      
      case S_ESCORT:
	 s_escort();
	 break;
      
      case S_UPDATE:
      case S_DEFENSE:
	 s_defense();
	 s_update();
	 break;
      
      case S_DISENGAGE:
	 s_disengage();
	 break;
      
      case S_RECHARGE:
	 s_recharge();
	 break;
      
      case S_COMEHERE:
	 s_comehere();
	 break;
      
      case S_QUIT:
	 /* prepare it for next time. */
	 break;
      
      case S_NO_HOSTILE:
	 s_no_hostile();
	 break;
   }
}

/* NS: 	S_UPDATE */
s_no_hostile()
{
   int	f = MYFUEL() < 90;
   int  d = MYDAMAGE() > 0 || MYSHIELD() < 100;
   int	lvorbit;

   if(d | f){
      disengage_c(ENONE, NULL, "damage or low fuel (no hostile)");
      _timers.disengage_time = mtime(0);
      _state.diswhy = ENONE;
      if(_state.protect)
	 _state.planet = _state.protect_planet;
      else
	 _state.planet = NULL;
   }
   else{
      unsigned char	crs_r, crs;
      int		speed_r, hittime, speed;

      if(!in_home_area()){
	 crs = home_crs();
	 speed = 5; 	/* xx */
      }
      else{
	 crs = 0;
	 speed = 0;
      }

      /* should orbit something.. */

      compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
   }
   /* in case torps flying around */
   _state.state = S_DEFENSE;
}

/* just entered */
/* NS: S_ENGAGE, S_UPDATE */
s_update()
{
   int	f = fuel_check(), d = repair_check();

   if(!isAlive(me) && !_state.dead){
      _state.dead = 1;
      _state.kills = 0.0;
      mprintf("dead.\n");

      if(inl && !_state.override)
	 rall_c("dead.  start over.");

#ifdef nodef
      if(_state.human)
	 sleep(2);
#endif
      return;
   }
   /* dead reset by reset_r_info */

   if(_state.assault && !hostilepl(_state.assault_planet) && 
      _state.assault_planet->pl_armies > 0 && 
      _state.assault_planet->pl_mydist > 10000 && 
      !unknownpl(_state.assault_planet)){

      /* planet took over */
      unassault_c("planet took over.");
   }

   if(_state.command == C_COMEHERE)	/* overrides everything else */
      _state.state = S_COMEHERE;
   else if(_state.recharge)
      _state.state = S_RECHARGE;
   else if(_state.ogg_req || _state.ogg)
      _state.state = S_OGG;
   else if(_state.escort_req)
      _state.state = S_ESCORT;
   else if(_state.disengage)
      _state.state = S_DISENGAGE;
   else if(_state.refit)
      _state.state = S_REFIT;
   else if((f || d) && !enemy_near_dead(_state.current_target) && 
      !_state.assault && !_state.refit_req){
      _timers.disengage_time = mtime(0);
      _state.diswhy = d ? EDAMAGE : ELOWFUEL;
      if(_state.protect)
	 _state.planet = _state.protect_planet;
      else
	 _state.planet = NULL;
      disengage_c(_state.diswhy, _state.planet, "damage or low fuel");
   }
   else if(me->p_kills > (_state.kills + .99) && !_state.defend && 
      !_state.chaos && !_state.assault_req && !_state.refit_req){
      /* just made a kill */
      if(me->p_ship.s_type != STARBASE && (MYFUEL() < 90 || MYSHIELD() < 75)){
	 _timers.disengage_time = mtime(0);
	 _state.diswhy = EKILLED;	/* xx */
	 if(_state.protect)
	    _state.planet = _state.protect_planet;
	 else
	    _state.planet = NULL;
	 disengage_c(_state.diswhy, _state.planet, "kill");
      }
      _state.kills = me->p_kills;
   }
   else if(MYWTEMP() > 80 && !_state.assault_req && !_state.refit_req){
      _state.disengage = 1;
      _timers.disengage_time = mtime(0);
      _state.diswhy = EOVERHEAT;
      if(_state.protect)
	 _state.planet = _state.protect_planet;
      else
	 _state.planet = NULL;
      disengage_c(_state.diswhy, _state.planet, "overheat");
   }
   else if(_state.refit_req){
      _state.disengage = 1;
      _timers.disengage_time = mtime(0);
      _state.diswhy = EREFIT;
      _state.planet = home_planet();
      _state.state = S_DISENGAGE;
      disengage_c(_state.diswhy, _state.planet, "refit_req");
   }
   else if(_state.assault_req==ASSAULT_TAKE && !_state.assault)
      _state.state = S_GETARMIES;
   else if(_state.assault)
      _state.state = S_ASSAULT;
   else if(_state.current_target || _state.protect || _state.defend)
      _state.state = S_ENGAGE;
   else
      _state.state = S_NO_HOSTILE;
}

s_refit()
{
   static int	refitting;

   if(me->p_flags & PFREFITTING){
      refitting = 1;
      if(DEBUG & DEBUG_DISENGAGE){ 
	 printf("refitting...\n");
      }
   }

   /* check */
   if(!refitting){
      if(!(me->p_flags & PFORBIT) && !(me->p_flags & PFDOCK)){
	 _state.disengage = 1;
	 _timers.disengage_time = mtime(0);
	 _state.diswhy = EREFIT;
	 _state.refit = 0;
	 _state.state = S_UPDATE;
	 if(DEBUG & DEBUG_DISENGAGE){
	    printf("breaking off refit: not orbiting\n");
	 }
	 return;
      }
      /* is it safe? */
      if(_state.recharge_danger){
	 _state.disengage = 1;
	 _timers.disengage_time = mtime(0);
	 _state.diswhy = EREFIT;
	 _state.refit = 0;
	 _state.state = S_UPDATE;
	 if(DEBUG & DEBUG_DISENGAGE){
	    printf("breaking off refit: recharge danger.\n");
	 }
	 return;
      }
      if(_state.closest_e && 
	 ((_state.closest_e->dist < 10000 && edang(_state.closest_e, 64)) ||
	  (_state.closest_e->dist < 8000))){
	 _state.disengage = 1;
	 _timers.disengage_time = mtime(0);
	 _state.diswhy = EREFIT;
	 _state.refit = 0;
	 _state.state = S_UPDATE;
	 if(DEBUG & DEBUG_DISENGAGE){
	    printf("breaking off refit: enemy too close.\n");
	 }
	 return;
      }
   }

   if(!refitting){
      sendRefitReq(_state.ship);
      if(DEBUG & DEBUG_DISENGAGE){ 
	 printf("sending refit request. ship %d\n", _state.ship);
      }
      _state.state = S_REFIT;
   }
   else if(refitting && !(me->p_flags & PFREFITTING)){
      /* first check to see if we got the ship */

      if(DEBUG & DEBUG_DISENGAGE){ 
	 printf("refit done.\n");
      }

      if(me->p_ship.s_type != _state.ship)
	 mprintf("refit error: didn't get ship.\n");

      /* done */
      refitting = 0;
      _state.refit = 0;
      _state.refit_req = 0;
      _state.diswhy = ENONE;
      _state.state = S_UPDATE;
   }
}

/* plot course towards player */
/* NS: S_DEFENSE, S_UPDATE */
s_engage()
{
   Player		*e = _state.current_target;

   _state.state = S_DEFENSE;

   engage(e);	/* engage.c */
}

s_assault()
{
   _state.state = S_DEFENSE;

   assault();
}

s_getarmies()
{
   _state.state = S_DEFENSE;

   getarmies();
}

s_ogg()
{
   _state.state = S_DEFENSE;

   ogg();
}

s_escort()
{
   _state.state = S_DEFENSE;

   escort();
}

/* go to controller */
s_comehere()
{
   struct player	*j;
   unsigned char	crs, crs_r;
   int			speed, speed_r, hittime, lvorbit;
   double		dx,dy;
   int			tdist;

   if(!_state.controller){
      _state.command = C_NONE;
      _state.state = S_UPDATE;
      return;
   }

   j = &players[_state.controller-1];
   crs = get_wrapcourse(j->p_x, j->p_y);

   dx = j->p_x - me->p_x;
   dy = j->p_y - me->p_y;
   tdist = (int)ihypot(dx,dy);

   if(tdist < 8000){
      /* close enough */
      _state.command = C_NONE;
      _state.state = S_UPDATE;
      return;
   }

   if(MYFUEL() > 40)
      speed = mymax_safe_speed();
   else
      speed = mylowfuel_speed();

   compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
   req_set_course(crs_r);
   req_set_speed(speed_r, __LINE__, __FILE__);
   /* defense will do compute course */
   _state.state = S_DEFENSE;
}

do_defense()
{
   Player		*e = _state.closest_e;
   int			speed = _state.p_desspeed, edist, phrange;

   if(!e || !e->p){
      edist = GWIDTH;
      phrange = GWIDTH/2;
   }
   else {
      edist = edist_to_me(e->p);
      phrange = PHRANGE(e) + 1000;
   }

   if((_state.assault && orbiting(_state.assault_planet)))
      return;

   if(_state.torp_danger || _state.recharge_danger || _state.ptorp_danger ||
      _state.pl_danger || (edist < phrange)){
      if(me->p_ship.s_type == STARBASE){
	 if(MYDAMAGE() < 40 && MYSHIELD() < 60)
	    req_shields_down();
	 else
	    req_shields_up();
      }
      else
	 req_shields_up();


      /* NEW  -- conservative */
      if(DEBUG & DEBUG_DISENGAGE){
	 if(_state.recharge){
	    printf("breaking off recharge -- recharge danger2\n");
	 }
      }
      if(speed < myfight_speed())
	 speed = myfight_speed();
      recharge_off("recharge dangern2");
      req_set_speed(speed, __LINE__, __FILE__);		/* xx */
   }
   else if(edist > phrange && !_state.torp_danger && !_state.pl_danger
      && !_state.recharge_danger && !_state.ptorp_danger && !explode_danger){
      req_shields_down();
   }

#ifdef nodef	/* this ain't workin */
   /* new */
   if( (_state.disengage || _state.recharge) && (_state.attack_diff > 0)){
      if(DEBUG & DEBUG_DISENGAGE){
	 printf("breaking off disengage || recharge, attack diff\n");
      }
      _state.planet = NULL;	/* experiment */
      if(speed < myfight_speed())
	 speed = myfight_speed();
      recharge_off("unused");
      req_set_speed(speed, __LINE__, __FILE__);		/* xx */
   }
#endif
}

/* check for incoming and ready phaser & torps */
s_defense()
{
   Player		*e = _state.closest_e;
   int			num_hits;
   unsigned char	newc;
   int			speed, hittime, edist;
   int			myspeed = _state.p_desspeed;
   unsigned char	mycrs = _state.p_desdir;
   int			phrange;
   int			lvorbit;

   _state.state = S_UPDATE;

   if(_state.assault && orbiting(_state.assault_planet)){
      defend_enemy_planet();
      return;
   }

   if(_state.ogg_req)
      return;

   if(!e || !e->p){
      edist = GWIDTH;
      phrange = GWIDTH/2;
   }
   else {
      edist = edist_to_me(e->p);
      phrange = PHRANGE(e) + 1000;
   }

#ifdef nodef
   if(_state.torp_danger || _state.recharge_danger)
      check_me_explode();
#endif

#ifdef nodef
   if(me->p_ship.s_type != STARBASE && !starbase(e))
      check_pressor(e);
#endif

   num_hits = compute_course(mycrs, &newc, myspeed, &speed, &hittime, &lvorbit);
   handle_disvars(e, &myspeed);
   
   if(_state.recharge && (num_hits || ((me->p_flags & PFORBIT) && lvorbit))){

      if(DEBUG & DEBUG_DISENGAGE){
         printf("breaking off recharge -- recharge_danger\n");
      }

      if(speed < myfight_speed())
	 speed = myfight_speed();
      recharge_off("hits");
   }

   if(!_state.recharge && !_state.lock){
	
      /*
      printf("in defense setting course & speed\n");
      */
      req_set_course(newc);
      req_set_speed(speed, __LINE__, __FILE__);
   }
}

check_me_explode()
{
   Player	*p = _state.closest_f;
   int		mdam;

   _state.pressor_override = 0;

   if(p && p->p && isAlive(p->p)){
      
      switch(me->p_ship.s_type){
	 case BATTLESHIP: mdam = 70; break;
	 case STARBASE:   mdam = 90; break;
	 default:	  mdam = 60; break;
      }
      if(MYDAMAGE() >= mdam){
	 if(p->dist < SHIPDAMDIST){
	    mprintf("check_me_explode: using pressor on %s\n", p->p->p_name);
	    req_pressor_on(p->p->p_no);
	    _state.pressor_override = 1;
	 }
      }
   }
}


check_dethis(num_hits, hittime)

   int	num_hits, hittime;
{
   int	detconst;
#ifdef nodef
   if(_state.ogg && num_hits > 0 && (num_hits == _state.det_torps)){
      printf("num hits %d\n", num_hits);
      printf("_state.det_torps %d\n", _state.det_torps);
      req_detonate("");
   }
   /* best solution */
   else if(num_hits == _state.det_torps && num_hits > 1)
      req_detonate("");
   else if(edist < 5000 && num_hits > 3 && _state.det_torps < 5)
      req_detonate("");
   else if(me->p_ship.s_type == STARBASE){
      /* EXPERIMENTAL */
      if(hittime < 5 && num_hits > 3 && _state.det_torps < 7)
	 req_detonate("");
   }
   else if(me->p_speed == 0 && num_hits > 1 && _state.det_torps < 4 
      && MYFUEL() > 25)
      req_detonate("");
#endif

#ifdef nodef
   /* debug */
   if(num_hits > 0 && me->p_speed > 6){
      printf("num_hits %d, det_damage %d, max damge %d\n", num_hits,
	 _state.det_damage, 50*num_hits);
      printf("my damage + detdamage = %d\n", MYDAMAGE() + _state.det_damage);
   }
#endif
   if(_state.det_torps == 0) return;

   if(!_state.det_const){
      if(me->p_ship.s_type == STARBASE)
	 detconst = 100;
      else if(me->p_ship.s_type == BATTLESHIP)
	 detconst = 80;
      else
	 detconst = 50;
   }
   else
      detconst = _state.det_const;
   
   if(num_hits > 0 && (_state.det_damage < detconst*num_hits))
      req_detonate("det");
}

s_disengage()
{
   disengage();		/* disengage.c */

   _state.state = S_DEFENSE;
}

/* NS: S_DEFENSE */
s_recharge()
{
   /* add shields? */
   int	damage = MYDAMAGE();
   int  fuel   = MYFUEL();
   int	shield = MYSHIELD();
   int  wtemp  = MYWTEMP();
   int	etemp  = MYETEMP();
   int	fck,rck,sck,wt,et;
   int	max_distance = (_state.maxspeed * 15000)/3, pdist;
   struct planet	*pl = _state.planet;

   if(_state.defend && _state.protect_player->dist > defend_dist(_state.protect_player)+1000){
      if(DEBUG & DEBUG_PROTECT){
	 printf("breaking recharge because defend player too far away.\n");
      }
      disengage_c(_state.diswhy, _state.planet, "defend too far away");
      _state.disengage = 1;
      _timers.disengage_time = mtime(0);
      _state.state = S_UPDATE;
      recharge_off("defend player too far away");
   }

#ifdef nodef
   if((me->p_flags & PFORBIT) && _state.assault && !_state.planet)
      pl = _state.assault_planet;
#endif

   if(pl && !(me->p_flags & PFORBIT)){
      disengage_c(_state.diswhy, _state.planet, "planet but no orbit");
      _state.disengage = 1;
      _timers.disengage_time = mtime(0);
      _state.state = S_UPDATE;
      recharge_off("supposed to be orbiting");
      return;
   }
   else if(pl && (me->p_flags & PFORBIT) && hostilepl(pl)){
      mprintf("orbiting hostile planet in recharge..\n");
      req_shields_up();
      /* can we declare peace? */
      if(!(me->p_swar & pl->pl_owner)){
	 int	newh = me->p_hostile;
	 newh ^= pl->pl_owner;
	 /* This code never gets executed, dunno why */
	 sendWarReq(newh);
	 _state.state = S_UPDATE;
	 return;
      }
      else {
	 disengage_c(_state.diswhy, _state.planet, "hostile_planet");
	 _state.disengage = 1;
	 _timers.disengage_time = mtime(0);
	 _state.state = S_UPDATE;
	 recharge_off("hostile planet");
	 _state.planet = NULL;
	 return;
      }
   }
   if((me->p_flags & PFCLOAK) &&
      me->p_ship.s_type != ASSAULT && (!pl || !(pl->pl_flags & PLFUEL))){
      req_cloak_off("recharge: no fuel planet");
   }

   if(_state.pl_danger){
      disengage_c(_state.diswhy, _state.planet, "plasma danger");
      _state.disengage = 1;
      _timers.disengage_time = mtime(0);
      _state.state = S_UPDATE;
      recharge_off("pl_danger");
      return;
   }

   if(!(me->p_flags & PFORBIT) && !(me->p_flags & PFDOCK)){
      struct planet *p;
      if((p=find_safe_planet(NULL, &pdist, 0)) && pdist < max_distance){
	 
	 if(fuel < 400000/pdist || 
	    damage > (50*pdist)/5000 ||
	    shield < 300000/pdist){

	    _state.disengage = 1;
	    _timers.disengage_time = mtime(0);
	    recharge_off("trying a closer planet");
	    _state.planet = p;
	    _state.state = S_UPDATE;
	    disengage_c(_state.diswhy, _state.planet, "try close planet?");
	    if(DEBUG & DEBUG_DISENGAGE)
	       printf("leaving recharge to try close planet.\n");
	    return;
	 }
      }
   }

   if(!(_state.closest_e && (_state.closest_e->p->p_flags & PFCLOAK)
      && _state.closest_e->dist < PHRANGE(_state.closest_e)+1000)){
      if(damage > 0 || shield < 100){
	 req_repair_on();
      }
      else{
	 req_shields_down();
      }
   }
   wt = et = 100;

   switch(_state.diswhy){
      case ENONE:
      case EPROTECT:
	 fck = sck = 100;
	 rck = 0;
	 break;
      case EKILLED:
	 fck = sck = 100;
	 rck = 0;
	 if(me->p_ship.s_type == STARBASE)
	    wt = 0;
	 et = 0;
	 break;
      case EREFIT:
	 fck = sck = 100;	/* more -- wtemp, etemp */
	 rck = 0;
	 break;
      case EOVERHEAT:
	 fck = sck = 100;	/* more -- wtemp, etemp */
	 rck = 0;
	 if(me->p_ship.s_type == STARBASE)
	    wt = 0;
	 et = 0;
	 break;

      default:
	 fck = 80;
	 rck = 25;
	 sck = 75;
	 if(me->p_ship.s_type == STARBASE)
	    wt = 25;
	 et = 10;
	 break;
   }
   if(_state.assault_req == ASSAULT_BOMB && _state.assault)
      rck = 50;

   if(_state.planet && (_state.planet->pl_flags & PLFUEL))
      fck = 100;

   if(_state.planet && (_state.planet->pl_flags & PLREPAIR)){
      rck = 0;
      sck = 100;
      et = 0;
   }
   
   if(_state.protect){		/* XX */
      sck = 100;
      fck = 100;
      rck = 0;
   }

   if(me->p_flags & PFDOCK){
      sck = 100;
      fck = 100;
      rck = 0;
   }

   if(_state.protect && orbiting(_state.protect_planet) && me->p_armies > 0){
      sendBeamReq(2);
   }

   if(_state.refit_req && orbiting(home_planet())){
      fck = 75;
      sck = 75;
      rck = 75;
      wt = 100;
      et = 100;
   }

   if(fuel >= fck && damage <= rck && shield >= sck && wtemp <= wt){
      if(!(_state.protect && orbiting(_state.protect_planet) &&
	 me->p_armies > 0)){
	 recharge_off("done");

	 rdisengage_c("done.");
	 _state.disengage = 0;
	 _timers.disengage_time = 0;
	 _state.closing = 0;
	 _state.p_desspeed = me->p_speed;
	 _state.p_desdir = me->p_dir;

	 if(DEBUG & DEBUG_DISENGAGE){
	    printf("recharge done: limit: fuel: %d, damage: %d, shield: %d\n",
	       fck, rck, sck);
	    printf("actual: fuel: %d, damage: %d, shield: %d\n",
	       fuel, damage, shield);
	 }

	 if(_state.refit_req){
	    if(orbiting_home()){
	       _state.refit = 1;
	    }
	    else {
	       refit_c("from recharge");
	       if(DEBUG & DEBUG_DISENGAGE){
		  printf("refit req but not orbiting home.\n");
	       }
	    }
	 }
      }
      _state.diswhy = 0;

      _state.state = S_UPDATE;
   }
   else
      _state.state = S_DEFENSE;
}

/*
 * Utils.
 */

/* find closest planet away from enemy's course */
struct planet *find_safe_planet(e, dist, team)

   Player		*e;
   int			*dist;	/* returned */
   int			team;
{
   register		k;
   struct planet	*pl, *cpl=NULL;
   Player		*p;
   int			pdist, mindist=INT_MAX, ed;
   unsigned char	pcrs;
   int			fl, dm, nonhostile;

   if(_state.refit_req){
      cpl = home_planet();
      if(hostilepl(cpl) || unknownpl(cpl))
	 _state.refit_req = 0;
      else{
	 *dist = cpl->pl_mydist;
	 return cpl;
      }
   }

   if(_state.defend)
      p = _state.protect_player;
   else
      p = me_p;
   
   if(me->p_ship.s_type == STARBASE && MYDAMAGE() > 10 && MYSHIELD() < 50){
      cpl = home_planet();
      if(!(hostilepl(cpl) || unknownpl(cpl))){
	 *dist = cpl->pl_mydist;
	 return cpl;
      }
   }
      
   /* which planet type is more important */
   if(me->p_ship.s_type != STARBASE){
      dm = (MYDAMAGE() > 50);
      fl = (MYFUEL() < 50);
   }
   else
      dm = 1;

   if(_state.chaos)
      fl = (MYFUEL() < 10);

   if(me->p_ship.s_type == STARBASE)
      team = 1;
   
   for(k=0, pl=planets; k< MAXPLANETS; k++,pl++){
      
      if(team)
	 nonhostile = (me->p_team == pl->pl_owner);
      else{
	 nonhostile = (me->p_team == pl->pl_owner || 
	    !((me->p_swar | me->p_hostile)& pl->pl_owner));

	 if(unknownpl(pl)&& me->p_ship.s_type != STARBASE){
	    nonhostile = !(guess_team(pl) & (me->p_swar | me->p_hostile) );
	    /* kludge */
	    if(nonhostile)
	       pl->pl_flags |= (PLFUEL | PLREPAIR);
	    /*
	    printf("guessing %s as %s\n", pl->pl_name,
	       nonhostile?"nonhostile":"hostile");
	    */
	 }
      }

      if(!nonhostile) continue;
      if(!unknownpl(pl) && (
	 !((pl->pl_flags & PLFUEL) || (pl->pl_flags & PLREPAIR))))
	 continue;
      
      pdist = dist_to_planet(p, pl);

      /* repair gets top priority unless fuel planet is very close */
      /* add in repair check */
      if(dm && !(pl->pl_flags & PLREPAIR) && pdist > rship_dist())
	 continue;
      else if(!dm && fl && !(pl->pl_flags & PLFUEL))
	 continue;

      if(pdist < mindist){
	 pcrs = get_wrapcourse(pl->pl_x, pl->pl_y);
	 if(not_safe(pl, pcrs, pdist))
	    continue;
	 cpl = pl;
	 mindist = pdist;
      }
   }
   *dist = mindist;

   if(_state.defend && mindist > 12000)
      return NULL;

   return cpl;
}

int pltype_needed()
{
   int	r = 0;

   if(MYDAMAGE() > 10 || MYSHIELD() < 80)
      r |= PLREPAIR;
   if(MYFUEL() < 80)
      r |= PLFUEL;

   return r;
}

plmatch(pl, r)

   struct planet	*pl;
   int			r;
{
   if( (r & PLFUEL)  && !(pl->pl_flags & PLFUEL)) return 0;
   if( (r & PLREPAIR)  && !(pl->pl_flags & PLREPAIR)) return 0;
   return 1;
}

struct planet *nearest_safe_planet()
{
   register			i;
   register struct planet	*pl;
   PlanetRec			*pls = _state.planets;
   int				r = pltype_needed();

   if(home_dist() > GWIDTH/2){
      for(i=0; i< pls->num_safesp; i++){
	 pl = pls->safe_planets[i];
	 if(plmatch(pl, r))
	    return pl;
      }
   }
   else {
      for(i=0; i< pls->num_teamsp; i++){
	 pl = pls->team_planets[i];
	 if(plmatch(pl, r))
	    return pl;
      }
   }
   return home_planet();
}

rship_dist()
{
   int	dm = MYDAMAGE();
   switch(me->p_ship.s_type){
      case STARBASE:	/* don't consider non-repair planets */
	 return -1;	
      case BATTLESHIP:
	 return 3000;
      case CRUISER:
	 return 7000;
      case DESTROYER:
	 return 8000;
      case ASSAULT:
	 return 1000;
      case SCOUT:
	 return 10000;
      case GALAXY:
	 return 3000;
   }
   return 5000;
}

not_safe(pl, pcrs, pdist)

   struct planet	*pl;
   unsigned char	pcrs;
   int			pdist;
{
   register		i;
   register Player	*p;
   struct planet	*team_planet();

   if(DEBUG & DEBUG_DISENGAGE)
      printf("Is planet %s safe: ", pl->pl_name);

   /* never */
   if(pl == team_planet(_state.warteam)){
      if(DEBUG & DEBUG_DISENGAGE)
	 printf("not safe because team enter planet\n");
      return 1;
   }

   if(in_team_area(_state.warteam) && dangerous_dir(pcrs, 64)){
      if(DEBUG & DEBUG_DISENGAGE)
	 printf("not safe becuase in team area and dangerous direction\n");
      return 1;
   }

   for(i=0,p=_state.players; i < MAXPLAYER; i++,p++){
      if(!p || !p->p || !isAlive(p->p) || !p->enemy) continue;
      if(PL_FUEL(p) < 10 || PL_DAMAGE(p) > 90) continue;
      /*
      if(running_away(p, p->crs, 32)) continue;
      */

      if(p->dist < pdist && angdist(pcrs, p->icrs) < 32){
	 if(DEBUG & DEBUG_DISENGAGE)
	    printf("not safe because player closer then planet and same dir\n");
	 return 1;
      }

      /* not safe */
      if(p->closest_pl && p->closest_pl == pl && p->dist < 10000){
	 if(DEBUG & DEBUG_DISENGAGE)
	    printf("not safe because player closest to that planet\n");
	 return 1;
      }
   }
   if(DEBUG & DEBUG_DISENGAGE)
      printf("planet safe\n");
   return 0;
}


/* TODO: incorporate in dodge.c */
phaser_plasmas()
{
   register struct plasmatorp *pt;
   register int i, x, y;
   int myphrange;

   unsigned char	pcrs;
   int			pspeed;
   Player		*e;
   _state.ptorp_danger = 0;

   /* XX */
   if(_server != SERVER_GRIT){
      if(Xplhit < 50) return;
   }

    myphrange = PHASEDIST * me->p_ship.s_phaserdamage / 100;
    /*
    if(_state.human) myphrange -= RANDOM() % 1000;
    */
    for (i = 0, pt = &plasmatorps[0]; i < MAXPLASMA * MAXPLAYER; i++, pt++) {
      if (pt->pt_status != PTMOVE) continue;

      if(_server == SERVER_GRIT || _state.torp_bounce){
	 if(!(pt->pt_war & me->p_team))
	    continue;
      }
      else{
	 if (pt->pt_owner == me->p_no) continue;
	 if(!( (pt->pt_war & me->p_team) || (players[pt->pt_owner].p_team &
	    (me->p_hostile | me->p_swar))))
	    continue;
      }
      
      /* NOTE: 6 updates if cloaked */
      if (abs(pt->pt_x - me->p_x) > myphrange) continue;
      if (abs(pt->pt_y - me->p_y) > myphrange) continue;
      if ((int)hypot((double) (pt->pt_x - me->p_x), (double) (pt->pt_y - me->p_y)) 
	 > myphrange) continue;

      _state.ptorp_danger = 1;

      /* 5 updates reaction time? */
      if(_state.human && (pt->pt_fuse < 8)) { mprintf("h:ign\n");continue;}
	 
      /* ignore the torp if it's already past */
      {
	 unsigned char	tdir = get_acourse(me->p_x, me->p_y,
	    pt->pt_x, pt->pt_y);
	 int			ad = angdist(tdir, pt->pt_dir);

	   /*
	   printf("pl ang dist %d\n", ad);
	   */
	 if(ad > 20)
	    continue;
      }
      _state.ptorp_danger = 1;
      req_repair_off();
      req_shields_up();
      _state.torp_danger = 1;	/* might not work */

      req_cloak_off("phaser_plasmas");
	 
      if(_state.human){
	 e = &_state.players[pt->pt_owner];
	 /* TMP -- human factor */
	 if(e->dist < myphrange + 1000)
	    continue;
      }

      pspeed = SH_PLASMASPEED(&players[pt->pt_owner]);

      x = pt->pt_x +
	 _avsdelay * (double)pspeed * Cos[pt->pt_dir] * WARP1;
      y = pt->pt_y +
	 _avsdelay * (double)pspeed * Sin[pt->pt_dir] * WARP1;
      pcrs = get_course(x,y);
      req_phaser(pcrs,1);
      break;
    }
}

/* needs repair */
repair_check()
{
   int	v = MYDAMAGE(), s = MYSHIELD(), vd = 0;
   switch(me->p_ship.s_type){
      case SCOUT: 
	 vd = 20;
	 break;
      case DESTROYER:
	 vd = 25;
	 break;
      case CRUISER:
	 vd = 30;
	 break;
      case BATTLESHIP:
	 vd = 35;
	 break;
      case GALAXY:
	 vd = 30;
	 break;
      case ASSAULT:
	 vd = 40;
	 break;
      case STARBASE:
	 if(v || s < 90)
	    vd = 1;		/* stay near a repair planet for now */
	 break;
   }
   if(_state.current_target && _state.current_target->p){
      switch(_state.current_target->p->p_ship.s_type){
	 case SCOUT:
	    vd += 20;
	    break;
	 case DESTROYER:
	    vd += 10;
	    break;
	 case CRUISER:
	    break;
	 case BATTLESHIP:
	    vd -= 10;
	    break;
	 case GALAXY:
	    break;

	 case ASSAULT:
	    vd += 20;
	    break;
	 case STARBASE:
	    break;
      }
   }
   if(_state.defend)
      vd = 70;

   switch(_state.player_type){
      case PT_CAUTIOUS:
	 vd += 10;
	 break;
      case PT_RUNNERSCUM:
	 vd += 20;
	 if(s < 10)		/* shields */
	    return 1;
	 break;
      case PT_NORMAL:
      case PT_OGGER:
      default:
	 break;
   }

   if(v >= vd){
      return v;
   }
   else
      return 0;
}

/* needs fuel */
fuel_check()
{
   int v = MYFUEL();

   if(_state.defend)
      return v < 90;

   if(_state.chaos){
      if(v < 10)
	 return (100-v)==0?1:(100-v);
   }
   switch(_state.player_type){
      case PT_CAUTIOUS:
	 if(v < 40)
	    return (100-v)==0?1:(100-v);
	 break;
      case PT_RUNNERSCUM:
	 if(v < 50)
	    return (100-v)==0?1:(100-v);
	 break;
      case PT_NORMAL:
      case PT_OGGER:
      default:
	 if(v < 35)
	    return (100-v)==0?1:(100-v);
	 break;
   }
   return 0;
}

output_stats()
{
   char	buf[80];
   /* include temperatures here*/
   sprintf(buf, "S: %d%%, D: %d%%, F: %d%%, WT: %d%%, ET: %d%%", 
      (100*me->p_shield)/me->p_ship.s_maxshield,
      MYDAMAGE(),
      MYFUEL(), MYWTEMP(), MYETEMP());
   response(buf);
}

output_pstats(p, s)
   
   Player	*p;
   char		*s;
{
   char	buf[80];
   extern char	*bp();

   if(p && p->p){
      sprintf(buf, "%s:%s(%d): S: %d%%, D: %d%%, F: %d%% W: %d%% (a: %d) (bp: %s)", 
	 s?s:"",
	 p->p->p_name, p->p->p_no, PL_SHIELD(p), PL_DAMAGE(p), PL_FUEL(p),
	 PL_WTEMP(p),
	 p->armies, _state.borg_detect?bp(p):"off");
      response(buf);
   }
   else{
      response("no info.");
      return;
   }
}

char *bp(p)

   Player	*p;
{
   static char	s[80];
   sprintf(s, "(%d,%d,%d,c%d) = %g", 
      p->bp1, p->bp2, p->bp3,p->bdc,
      p->borg_probability);
   return s;
}

/* bomb & take stats */
output_astats()
{
   register		i;
   register Player	*p;

   mprintf("team bombers: ");
   for(i=0, p=_state.players; i< MAXPLAYER; i++,p++){
      if(!p->p || !isAlive(p->p) || p->enemy) continue;
      if(p->bombing > 0){
	 /* 2 minutes */
	 if(_udcounter - p->bombing < 2*60*10)
	    mprintf("%s ", p->p->p_mapchars);
      }
   }
   mprintf("\n");
   mprintf("enemy bombers: ");
   for(i=0, p=_state.players; i< MAXPLAYER; i++,p++){
      if(!p->p || !isAlive(p->p) || !p->enemy) continue;
      if(p->bombing > 0){
	 /* 2 minutes */
	 if(_udcounter - p->bombing < 2*60*10)
	    mprintf("%s ", p->p->p_mapchars);
      }
   }
   mprintf("\n");
   mprintf("team pl takers: ");
   for(i=0, p=_state.players; i< MAXPLAYER; i++,p++){
      if(!p->p || !isAlive(p->p) || p->enemy) continue;
      if(p->bombing > 0){
	 /* 5 minutes */
	 if(_udcounter - p->taking < 5*60*10)
	    mprintf("%s ", p->p->p_mapchars);
      }
   }
   mprintf("\n");
   mprintf("enemy pl takers: ");
   for(i=0, p=_state.players; i< MAXPLAYER; i++,p++){
      if(!p->p || !isAlive(p->p) || !p->enemy) continue;
      if(p->bombing > 0){
	 /* 5 minutes */
	 if(_udcounter - p->taking < 5*60*10)
	    mprintf("%s ", p->p->p_mapchars);
      }
   }
   mprintf("\n");
}

req_set_speed(s, l, f)

   int	s;
   int	l;
   char	*f;
{
   /*
   printf("%s:%d %d\n", f, l, s);
   */

   if(s > _state.maxspeed) s = _state.maxspeed;

   if(inl && !status->tourn && !_state.itourn && !ignoreTMode)
      return;

   if(_state.p_desspeed != s){
      _state.p_desspeed = s;
      sendSpeedReq(s);
      if((DEBUG & DEBUG_DISENGAGE) && (me->p_flags & (PFORBIT|PFDOCK))){
	 printf("breaking dock/orbit for speed %d\n", s);
      }
      return 1;
   }
   return 0;
}

/* human variables */
static int		tfired, pfired, turned;
static unsigned char	turned_dir, tfire_dir, pfired_dir;


req_set_course(c)

   unsigned char	c;
{
   if(me->p_speed == 0) return 0;

   if(_state.p_desdir != c){
      if(_state.human){
#ifdef nodef
	 /* experiment */
	 if(RANDOM()%4 < _state.human) 
	    return 0;

#endif
	 if(_udcounter - tfired < _state.human){
	    if(angdist(c, tfire_dir) > 20+ (RANDOM()%20))
	       return 0;
	 }

	 if(_udcounter - pfired < _state.human){
	    if(angdist(c, pfired_dir) > 20+ (RANDOM()%20))
	       return 0;
	 }

	 /* was _state.human/2 */
	 if(_udcounter - turned < (RANDOM()%_state.human))
	    return 0;

	 turned = _udcounter;
	 turned_dir = c;
      }
      _state.p_desdir = c;
      sendDirReq(c);
      if((DEBUG & DEBUG_DISENGAGE) && (me->p_flags & (PFORBIT|PFDOCK))){
	 printf("breaking dock/orbit for course %d\n", c);
      }
      return 1;
   }
   return 0;
}

req_torp(crs)

   unsigned char	crs;
{
   extern	int	_tsent;
   int			now = mtime(0), t=0;

   if(expltest){
      if(MYDAMAGE() < 50) return;
   }

   if(_state.no_weapon) return 0;

   if(me->p_flags & PFCLOAK) return 0;

   if(me->p_ship.s_type == STARBASE && MYDAMAGE() < 60 &&
      (_udcounter - pfired < 7)) 
      return 0;

   /* experiment */
   if(_state.human){
      /*
      if((RANDOM()%4) == 3)
	 crs += rrnd(1);
      */
      
      if(_udcounter - pfired < 5) 
	 return 0;
      
      if(_state.human > 2 && (RANDOM()%4)==1){
	 if(_udcounter - tfired < 2)
	    return 0;
      }

#ifdef nodef
      if(_udcounter - turned < 5){
	 if(angdist(crs, turned_dir) > 40)
	    return 0;
      }
#endif
   }

   /*
   if(now - _state.lasttorpreq < 100){
      mprintf("fire too short: %d.\n", now-_state.lasttorpreq);
      t = 100 - (now - _state.lasttorpreq);
      usleep(t);
   }
   mprintf("req_torp at %d\n", now);
   */

   if(me->p_ntorp < 8 && 
      me->p_ship.s_torpcost <= me->p_fuel){

      _state.lasttorpreq = now + t;
      tfire_dir = crs;
      sendTorpReq(crs);
      tfired = _udcounter;
      _tsent = _state.lasttorpreq;
      return 1;
   }
   else
      return -1; /* low on fuel or too many torps */
}

req_pltorp(dir)

   unsigned char	dir;
{
   if(_state.no_weapon) return;
   if(me->p_flags & PFCLOAK) return;

   /* too dangerous */
   if(me->p_ship.s_type == STARBASE) return;

   if(me->p_nplasmatorp != 0) return;
   if(angdist(dir, me->p_dir) > 20) return;
   if(me->p_ship.s_type == STARBASE)
      if(MYWTEMP() > 20) return;
   else
      if(MYWTEMP() > 50) return;

   sendPlasmaReq(dir);
}

req_phaser(crs, f)

   unsigned char	crs;
   int			f;
{
   if(expltest){
      if(MYDAMAGE() < 90) return;
   }

   if(_state.no_weapon /*&& !Xplhit*/) return 0;
   if(me->p_flags & PFCLOAK) return 0;


   if(phasers[me->p_no].ph_status == PHFREE){

      if(me->p_ship.s_type == STARBASE && !f && MYDAMAGE() < 80 && MYWTEMP() > 50){
	 if(MYWTEMP() < 60) {
	    if((RANDOM() % 6) < 1) return 0;
	 }
	 if(MYWTEMP() < 70) {
	    if((RANDOM() % 6) < 2) return 0;
	 }
	 if(MYWTEMP() < 80) {
	    if((RANDOM() % 6) < 3) return 0;
	 }
	 else{
	    if((RANDOM() % 6) < 4) return 0;
	 }
      }

      if(me->p_ship.s_phasercost <= me->p_fuel){
	 if(!(me->p_flags & PFREPAIR)){

	    /* experiment */
	    if(_state.human){
	       int	r;

#ifdef nodef
	       if(_udcounter - turned < 5){
		  if(angdist(crs, turned_dir) > 40)
		     return 0;
	       }
#endif
	       if(_udcounter - tfired < 5 || (me->p_ship.s_type != STARBASE 
		  && _udcounter - pfired < 11 + (RANDOM()%10)))
		  return 0;

	       r = RANDOM()%10;
	       if(r == 5)
		  return 0;
	       if(r > 6){
		  if(RANDOM()%2)
		     crs += rrnd(5);
		  else
		     crs -= rrnd(5);
	       }
		/* experiment */
		if(RANDOM()%10 < _state.human)
		  return 0;
   
	    }

	    _state.lastphaserreq = _udcounter;
	    sendPhaserReq(crs);

	    pfired = _udcounter;
	    pfired_dir = crs;
	    return 1;
	 }
      }
   }
   return 0;
}

req_planetlock(n)

   int			n;
{
   sendPlanlockReq(n);
   if((DEBUG & DEBUG_DISENGAGE) && (me->p_flags & (PFORBIT|PFDOCK))){
      printf("breaking dock/orbit for planet lock %d\n", n);
   }
   /*
   printf("me->p_planet = %d\n", n);
   */
   me->p_planet = n;
   if(rsock != -1) updateR(me);
   return 1;
}

req_playerlock(n)

   int			n;
{
   sendPlaylockReq(n);
   if((DEBUG & DEBUG_DISENGAGE) && (me->p_flags & (PFORBIT|PFDOCK))){
      printf("breaking dock/orbit for player lock %d\n", n);
   }
   me->p_playerl = n;
   if(rsock != -1) updateR(me);
   return 1;
}

req_shields_up()
{
   if(!(me->p_flags & PFSHIELD)){
      if(DEBUG & DEBUG_DISENGAGE){
	 printf("shields down.\n");
      }
      sendShieldReq(1);
      return 1;
   }
   return 0;
}

req_shields_down()
{
   if(me->p_flags & PFSHIELD){
      sendShieldReq(0);
      return 1;
   }
   return 0;
}

req_repair_on()
{
   if(!(me->p_flags & PFREPAIR)){
      if(DEBUG & DEBUG_DISENGAGE){
	 printf("recharge: repair on.\n");
      }
      sendRepairReq(1);
      _state.p_desspeed = 0;
      emptytorpq();
      return 1;
   }
   return 0;
}

req_detonate(s)
   
   char	*s;
{
   if(s)
      mprintf("%s\n", s);
   sendDetonateReq();
   return 1;
}

req_repair_off()
{
   /* just in case */
   _state.p_desspeed = me->p_speed;
   _state.p_desdir = me->p_dir;

   if(me->p_flags & PFREPAIR){
      sendRepairReq(0);
      return 1;
   }
   return 0;
}

req_orbit()
{
   if(!(me->p_flags & PFORBIT)){
      /* note: _state.p_desdir is wrong after this */
      sendOrbitReq(1);
      _state.p_desspeed = 0;
      return 1;
   }
   return 0;
}

req_det_mytorps()
{
   register int i;

   for (i = 0; i < MAXTORP; i++) {
      if (torps[i + (me->p_no * MAXTORP)].t_status == TMOVE) {
	 sendDetMineReq(i + (me->p_no * MAXTORP));
      }
   }
}

req_det_mytorp(i)

   int	i;
{
   sendDetMineReq(i + (me->p_no * MAXTORP));
}

req_tractor_on(n)

   int	n;
{
   if(_state.no_weapon) return;

   if(me->p_flags & (PFTRACT | PFPRESS)){
      return;
   }
   if(me->p_flags & PFCLOAK) return;

   if(DEBUG & DEBUG_WARFARE){
      printf("tractor ON %s\n", players[n].p_name);
   }
   _timers.tractor_time = _udcounter;
   me->p_tractor = n;
   if(rsock != -1) updateR(me);
   sendTractorReq(1, n);
}

req_tractor_off(s)
   
   char	*s;
{
   if(me->p_flags & (PFTRACT | PFPRESS)){
      sendTractorReq(0, me->p_no);
      if(DEBUG & DEBUG_WARFARE){
	 printf("tractor OFF \"%s\"\n", s);
      }
   }
   _timers.tractor_limit = TRACTOR_TIME + rrnd(10);
}

req_pressor_on(n)

   int	n;
{
   if(_state.no_weapon) return;
   if(me->p_flags & (PFTRACT | PFPRESS)){
      return;
   }
   if(DEBUG & DEBUG_WARFARE){
      printf("pressor ON %s\n", players[n].p_name);
   }
   if(me->p_flags & PFCLOAK) return;
   _timers.tractor_time = _udcounter;
   me->p_tractor = n;
   if(rsock != -1) updateR(me);
   sendRepressReq(1, n);
}

req_pressor_off()
{
   if(me->p_flags & (PFTRACT | PFPRESS)){
      sendRepressReq(0, me->p_no);
      if(DEBUG & DEBUG_WARFARE){
	 printf("pressor OFF\n");
      }
   }
   _timers.tractor_limit = TRACTOR_TIME + rrnd(10);
}

req_cloak_on()
{
   if(no_cloak) return 0;

   if(!(me->p_flags & PFCLOAK)){
      sendCloakReq(1);
      return 1;
   }
   return 0;
}

req_cloak_off(s)
   char	*s;
{
   if(me->p_flags & PFCLOAK){
      if(s) mprintf("req_cloak_off: %s\n", s);
      sendCloakReq(0);
      return 1;
   }
   return 0;
}

req_dock_on()
{
   if(!(me->p_flags & PFDOCKOK)){
      sendDockingReq(1);
      return 1;
   }
   return 0;
}

req_dock_off()
{
   if(me->p_flags & PFDOCKOK){
      sendDockingReq(0);
      return 1;
   }
   return 0;
}

req_bomb()
{
   if(!(me->p_flags & PFBOMB)){
      sendBombReq(1);
      return 1;
   }
   return 0;
}

req_beamdown()
{
   if(!(me->p_flags & PFBEAMDOWN)){
      sendBeamReq(2);
      return 1;
   }
   return 0;
}

req_beamup()
{
   if(!(me->p_flags & PFBEAMUP)){
      sendBeamReq(1);
      return 1;
   }
   return 0;
}


mtime(x)
   int	x;
{
   struct timeval tm;
   static int mtime_cache;
   int		v;
   if(x){
      gettimeofday(&tm, NULL);
      /* mask off 16 high bits and add in milliseconds */
      v = (tm.tv_sec & 0x0000ffff)*1000+tm.tv_usec/1000;
      mtime_cache = v;
   }
   return mtime_cache;
}

reset_r_info(team_r, ship_r, first, login)
   
   int	*team_r, *ship_r, first;
   char	*login;
{

   if(first){
      _reset_initial(team_r, ship_r, login);
      return 1;
   }
   else {
      int		controller, debug, df, td, protect;
      struct planet	*protect_planet;
      ecommand		command;
      float		timer_delay_ms;
      int		k;
      Player		*protect_player, *ld, *esc_player;
      int		defend;
      char		ignore_e[MAXPLAYER+1];
      int		vt, wa, kc, tw, ad, od, ts, ns, is, wt, it, nw, ar;
      int		sc, ov, vq, add;
      bool		hu, tb, ga, bd, dr, pt, tn, es;
      int		os, dc, ogs;

      vt 		= _state.vector_torps;
      wa 		= _state.wrap_around;
      kc 		= _state.chaos;
      tw 		= _state.torp_wobble;
      ad		= _state.assault_dist;
      ar		= _state.assault_range;
      od		= _state.ogg_dist;
      ts		= _state.torp_seek;
      tb		= _state.torp_bounce;
      ga 		= _state.galaxy;
      ns		= _state.no_speed_given;
      is		= _state.ignore_sudden;
      wt		= _state.warteam;
      it		= _state.itourn;
      nw		= _state.no_weapon;
      hu		= _state.human;
      bd		= _state.borg_detect;
      dr		= _state.do_reserved;
      sc		= _state.seek_const;
      ov		= _state.override;
      vq		= _state.vquiet;
      pt		= _state.player_type;
      add		= _state.attack_diff_dist;
      tn		= _state.take_notify;
      os		= _state.ogg_pno;
      ld		= _state.last_defend;
      es		= _state.escort_req;
      esc_player	= _state.escort_player;
      dc		= _state.det_const;
      ogs		= _state.ogg_speed;

      if(_state.command == C_QUIT)
	 return 0;

      /* save various things */
      controller 	= _state.controller;
      debug 		= _state.debug;
      df 		= _state.lookahead;
      td 		= _state.tdanger_dist;
      protect_planet 	= _state.protect_planet;
      protect 		= _state.protect;
      *team_r 		= _state.team;
      *ship_r 		= _state.ship;
      command		= _state.command;
      timer_delay_ms	= _state.timer_delay_ms;
      k			= _state.killer;
      defend		= _state.defend;
      protect_player	= _state.protect_player;
      strcpy(ignore_e, _state.ignore_e);

      bzero(&_state, sizeof(_state));
      bzero(&_torpq, sizeof(_torpq));
      bzero(_battle, sizeof(_battle));

      _state.planets	= &_planets;
      _state.players 	= _players;
      _state.torpq 	= &_torpq;
      _state.battle 	= _battle;

      _state.controller 	= controller;
      _state.debug 		= debug;
      _state.lookahead 		= df;
      _state.tdanger_dist	= td;
      _state.protect_planet	= protect_planet;
      _state.protect		= protect;
      _state.team 		= *team_r;
      _state.ship 		= *ship_r;
      _state.command		= command;
      _state.timer_delay_ms	= timer_delay_ms;
      _state.killer		= k;
      _state.defend		= defend;
      _state.protect_player	= protect_player;
      _state.vector_torps 	= vt;
      _state.wrap_around 	= wa;
      _state.chaos 		= kc;
      _state.torp_wobble 	= tw;
      _state.assault_dist	= ad;
      _state.assault_range	= ar;
      _state.torp_seek		= ts;
      _state.galaxy		= ga;
      _state.torp_bounce	= tb;
      _state.ogg_dist		= od;
      _state.no_speed_given	= ns;
      _state.ignore_sudden	= is;
      _state.warteam		= wt;
      _state.itourn		= it;
      _state.human		= hu;
      _state.borg_detect	= bd;
      _state.do_reserved	= dr;
      _state.no_weapon		= nw;
      _state.seek_const		= sc;
      _state.override		= ov;
      _state.vquiet		= vq;
      _state.player_type	= pt;
      _state.attack_diff_dist	= add;
      _state.take_notify	= tn;
      _state.last_defend	= ld;
      _state.escort_req		= es;
      _state.escort_player	= esc_player;
      _state.det_const		= dc;
      _state.ogg_speed		= ogs;
      if(_state.player_type == PT_OGGER)
	 _state.ogg_pno = os;
      strcpy(_state.ignore_e, ignore_e);

      bzero(&_timers, sizeof(_timers));

      /* only happens when robot is dead or when someone sends a reset
      command to the robot.  If robot is dead, declare_intents does
      nothing. */
      declare_intents(0);
      req_cloak_off("reset");
      return 1;
   }
}

_reset_initial(team_r, ship_r, login)

   int	*team_r, *ship_r, *login;
{
   /* debug */
   extern int dgo;
   bzero(&_state, sizeof(_state));
   bzero(_players, sizeof(_players));
   bzero(&_planets, sizeof(_planets));
   bzero(&_torpq, sizeof(_torpq));
   bzero(_battle, sizeof(_battle));

   _state.players = _players;
   _state.planets = &_planets;
   _state.torpq = &_torpq;
   _state.battle = _battle;

   _state.warteam 	= -1;

   _state.team = *team_r;
   _state.ship = *ship_r;

   _state.lookahead = MAXFUSE-1;

   _state.tdanger_dist = 1500;
   _state.assault_dist = 20000;
   _state.assault_range = 50;
   if(inl)
      _state.ogg_dist = 7000;
   else
      _state.ogg_dist = 6000;
   _state.timer_delay_ms = updates;
   _state.kills = 0.0;
   _state.seek_const = 5000;		/* torp seek */
   _state.override = override;		/* from command line */
   _state.do_reserved = doreserved;	/* from command line */
   _state.take_notify = 1;

   _state.attack_diff_dist = 10000;

   ignore_edefault(_state.ignore_e);

   /*
   _serverdelay = 0.0;
   */

   init_servername();
   init_ships();
   init_playertype(login);
}

send_initial()
{
   mprintf("sending update %d\n", (int)(_state.timer_delay_ms * 100000.));

   sendUpdatePacket((int)(_state.timer_delay_ms * 100000.));
   init_comm();
}

ignore_edefault(s)

   char	*s;	/* length MAXPLAYER+1 */
{
   register	i;
   for(i=0; i< MAXPLAYER; i++)
      s[i] = '0';
   s[i] = '\0';
}

ignore_enemy(p)

   int	p;
{
   return (_state.ignore_e[p] == '1');
}

ignoring_e(e)

   Player	*e;
{
   if(!e || !e->p || !isAlive(e->p))
      return 0;

   if(_state.ignore_sudden && should_ignore(e,e->p,e->dist)){
      return 1;
   }

   return _state.ignore_e[e->p->p_no] == '1';
}

set_team(t)

   int	t;
{
   _state.team = t;
}

mymax_safe_speed()
{
   int	s;
   switch(me->p_ship.s_type){
      case SCOUT:
	 s = 9;	break;
      case DESTROYER:
	 s = 8; break;
      case CRUISER:
	 s = 7; break;
      case BATTLESHIP:
	 s = 7; break;
      case GALAXY:
	 s = 6; break;
      case ASSAULT:
	 s = 7; break;
      case STARBASE:
	 if(_state.chaos) {
	    s = 3;
	 }
	 else
	    s = 2; break;
   }
   if(MYETEMP() > 90)
      s -= 2;
   else if(MYETEMP() > 80)
      s --;

   return s;
}

myemg_speed()
{
   int	s;

   if(MYFUEL() > 20 && MYETEMP() < 80)
      s =  me->p_ship.s_maxspeed;
   else if(MYETEMP() < 85)
      s = me->p_ship.s_maxspeed -2;
   else if(MYETEMP() < 90)
      s = me->p_ship.s_maxspeed -3;
   else
      s = me->p_ship.s_maxspeed -4;
   
   if(s < 3) s = 3;

   return s;
}
      
mylowfuel_speed()
{
   int	s;
   switch(me->p_ship.s_type){
      case SCOUT:
	 s = 7; break;
      case DESTROYER:
	 s = 6; break;
      case CRUISER:
	 s = 5; break;
      case BATTLESHIP:
	 s = 5; break;
      case GALAXY:
	 s = 6; break;
      case ASSAULT:
	 s = 6; break;
      case STARBASE:
	 if(_state.chaos){
	    s = 3;
	 }
	 else 
	    s = 2; 
	 break;
   }
   if(MYFUEL() < 10)
      s--;

   if(MYETEMP() > 90)
      s -= 2;
   else if(MYETEMP() > 80)
      s --;

   return s;
}

myfight_speed()
{
   int	s;
   switch(me->p_ship.s_type){
      case SCOUT:
	 s = 5;
	 break;
      case DESTROYER:
	 s= 5;
	 break;
      case CRUISER:
	 s = 4;
	 break;
      case BATTLESHIP:
      case GALAXY:
	 s = 3;
	 if(_state.torp_seek) s++;
	 break;
      case ASSAULT:
	 s = 4;
      case STARBASE:
	 if(_state.chaos){
	    s = 3;
	 }
	 else
	    s = 2;
   }
   return s;
}

void init_timers()
{
   /* intervals */
   bzero(&_timers, sizeof(_timers));
   _state.torp_i = MIN_TORP_I;
}

char *state_name(s)

   estate	s;
{
   switch(s){
      
      case S_NOTHING: 		return "NOTHING";
      case S_UPDATE: 		return "UPDATE";
      case S_ENGAGE:		return "ENGAGE";
      case S_DEFENSE:		return "DEFENSE";
      case S_DISENGAGE:		return "DISENGAGE";
      case S_RECHARGE:		return "RECHARGE";
      case S_NO_HOSTILE:	return "NO_HOSTILE";
      case S_QUIT:		return "QUIT";
      case S_COMEHERE:		return "COMEHERE";
      case S_REFIT:		return "REFIT";
      case S_GETARMIES:		return "GETARMIES";
      case S_ASSAULT:		return "ASSAULT";
      case S_ESCORT:		return "ESCORT";
      default:
	return "unknown";
   }
}

/* every once in a while the server misses a crucial packet. */
handle_unknown_problems()
{
   static int	last;

   if(!_state.lock && !(me->p_flags & (PFORBIT | PFREPAIR | PFDOCK))){
      if(_udcounter - last > 50){
	 sendSpeedReq(_state.p_desspeed);
	 /*
	 sendDirReq(_state.p_desdir);
	 */
	 last = _udcounter;
      }
   }

   if(me->p_flags & (PFORBIT|PFDOCK))
      _state.lock = 0;
}

enemy_near_dead(e)

   Player	*e;
{
   if(!e || !e->p || !isAlive(e->p))
      return 0;
   
   if(e->p->p_speed >= _state.maxspeed)
      return 0;

   /* much more needs to be added here */
   if((PL_DAMAGE(e) > MYDAMAGE()) && MYFUEL() > 10 && PL_DAMAGE(e) > 70)
      return 1;
   else if(PL_DAMAGE(e) > 60 && (PL_FUEL(e) < MYFUEL()))
      return 1;
   else
      return 0;
}

init_servername()
{
   extern char	*serverName;
   char		*s = serverName;

   _state.vector_torps  = 0;
   _state.wrap_around	= 0;
   _state.chaos         = 0;
   _state.torp_wobble 	= 1;
   _state.ignore_sudden = 1;
   _state.human		= 0;
   _state.vquiet	= 1;
   _state.try_udp	= 1;
   _state.take_notify 	= 1;

   if(strcmp(s, "rwd4.mach.cs.cmu.edu") == 0){
      mprintf("rwd4.mach.cs.cmu.edu\n");
      _server = SERVER_LOCAL;   /* xx */
      _state.vector_torps = 1;
   }
   else if(strcmp(s, "calvin.usc.edu")==0){
      mprintf("calvin.usc.edu\n");
      _state.take_notify = 0;
      _state.try_udp = 1;
   }
   else if(strcmp(s, "vlsi")==0){
      _state.try_udp = 1;
   }
   else
      _server = SERVER_LOCAL;   /* xx */
}

/* player type based on name */
init_playertype(login)
   
   char	*login;
{
   int	v = -1;
   if(!login) return;

   sscanf(login, "r1h%d", &v);
   if(v > 0)
      _state.human = v;

   if(strlen(login) > 4){
      if(login[4] == 'o'){
	 _state.player_type = PT_OGGER;
	 mprintf("player type ogger\n");
      }
      else if(login[4] == 'p') 
	 _state.player_type = PT_CAUTIOUS;
      else if(login[4] == 's') 
	 _state.player_type = PT_RUNNERSCUM;
      else if(login[4] == 'd'){
	 _state.player_type = PT_DEFENDER;
	 mprintf("player type defender\n");
      }
      else if(login[4] == 'f' || login[5] == 'f'){
	 _state.player_type = PT_DOGFIGHTER;
	 tractt_angle = 16;
	 mprintf("player type dogfighter\n");
      }
   }
}
