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

static int	_special_tr;

#define PDEFEND	(_state.defend || _state.player_type == PT_DEFENDER)

pwarfare(type, e, j, edist)

   int			type;
   Player		*e;
   struct player	*j;
   int			edist;
   
{
   if(_state.ignore_sudden && should_ignore(e,j,edist)){
      return;
   }

   if(j && j->p_status != PALIVE)
      return;

   if(_state.ogg || _state.escort == 3) { 
      oggwarfare(type, e, j, edist);
      return;
   }
   else if(_state.escort_req){
      if(_state.closest_e && too_close(_state.closest_e))
	    handle_tractors(_state.closest_e, 
	       _state.closest_e->p, _state.closest_e->dist, 0, 
		"escort pressor", TR_ESCORT_PRESSOR);
      if(MYSHIELD() > 30)
	 return;
   }
      

   if(me->p_flags & PFCLOAK)
      return;

#ifdef nodef
   if(type == WPHASER){		/* since this gets called twice in same cycle */
      if((e->pp_flags & PFTRACT) && !_special_tr){
	 if(_state.protect || _state.disengage)
	    handle_tractors(e, j, edist, 0, "enemy tractor (pressor)",
	       TR_ENEMY_TRACTOR);
	 else if(_state.killer)
	    handle_tractors(e, j, edist, 1, "enemy tractor (tractor)",
	       TR_ENEMY_TRACTOR);
	 else{
	    if(me->p_kills > .99){
	       if(RANDOM()%128 < 32)
		  handle_tractors(e, j, edist, 1, "enemy tractor (tractor)",
		     TR_ENEMY_TRACTOR);
	       else
		  handle_tractors(e, j, edist, 0, "enemy tractor (pressor)",
		     TR_ENEMY_TRACTOR);
	    }
	    else
	       handle_tractors(e, j, edist, RANDOM()%2, 
		  "enemy tractor (RANDOM)", TR_ENEMY_TRACTOR);
	 }
      }
      else{
	 if(ttime() && !_special_tr){
	    if(!_state.killer)
	       req_tractor_off();
	    req_pressor_off();
	 }
      }
   }
#endif

#ifdef nodef
   if(j->p_flags & PFCLOAK)
      printf("%s: speed: %d, dir %d\n", j->p_name, j->p_speed, j->p_dir);
#endif

   if(e->invisible)
      return;

   if(type == WTORP){
      ptorp_attack(e, j, edist);
      if(!e->robot)
	 plasma_attack(e,j,edist);
   }
   else{
      /*
      if(_cycletime < 300)
      */
      pphaser_attack(e, j, edist);
   }

   if(_state.defend){
      Player	*p = _state.protect_player;
      if(defend_det_torps > 0 && (me->p_ship.s_type == STARBASE ||
	 (in_danger(p) || (_udcounter - p->lastweap) > 5*10))){
	 req_detonate("det for defend");
      }
   }
}

should_ignore(e,j,edist)
   
   Player		*e;
   struct player	*j;
   int			edist;
{
   if(e->robot) return 0;
   if(PL_FUEL(e) < 97) return 0;
   if(me_p->alive > 30 && e->alive < 30) return 1;
   return 0;
}

ttime()
{
   return (_udcounter - _timers.tractor_time) >= _timers.tractor_limit;
}

handle_tractors(e, j, edist, tractor, s, m)

   Player		*e;
   struct player	*j;
   int			edist;
   int			tractor;
   char			*s;
   int			m;
{
   if(_state.assault && tractor) return;

   if((double)edist > ((double)TRACTDIST)*SH_TRACTRNG(me)){
      return;
   }

   if((me->p_flags & PFTRACT) || (me->p_flags & PFPRESS)){
      return;
   }

   if(DEBUG & DEBUG_WARFARE){
      printf("handle_tractors: %s\n", s);
   }

   if(tractor){
      req_tractor_on(j->p_no);
      last_tract_req = m;
   }
   else{
      req_pressor_on(j->p_no);
      last_press_req = m;
   }
}

check_pressor(e)

   Player	*e;
{
   struct player	*j;

   if(!e || !e->p || !isAlive(e->p))
      return;
   
   j = e->p;
   
   if((!PDEFEND && _state.defend) || _state.killer || _state.ogg) return;
   /* handled in pwarfare */
   if(_state.protect && (j->p_flags & PFORBIT)) return;
   if(j->p_flags & PFCLOAK) return;

   if(MYDAMAGE() <= PL_DAMAGE(e) + 30) return;
   if(MYFUEL() < 20 || MYFUEL() >= PL_FUEL(e) + 20) return;

   if(e->dist <= ((double)TRACTDIST)*SH_TRACTRNG(me)){

      if(me->p_flags & PFTRACT)
	 req_tractor_off("prepare to press");

      if(!(me->p_flags & PFPRESS)){
	 req_pressor_on(j->p_no);
      }
   }
}
   
ptorp_attack(e, j, edist)

   Player		*e;
   struct player 	*j;
   int			edist;
{
   static int		tcount;
   unsigned char	tcrs, dcrs;
   int			size, urnd, tht, pht, hispr,tf;
   int			k = (_state.killer || e->robot || starbase(e));
   int			pf=100, ndist=GWIDTH, cdist;

   if(_state.defend){
      struct player	*dp = _state.protect_player->p;
      get_torp_course(dp, &dcrs, me->p_x, me->p_y);
   }

   size = 0;
   urnd = 0;

   _special_tr = 0;

   if(k){
      tf = me->p_fuel;
      me->p_fuel = 500000;
   }

   if(_state.defend){
      ndist = pdist_to_p(j, _state.protect_player->p);
      pf = PL_FUEL(_state.protect_player);
   }

   hispr = e->hispr;

   if(starbase(e))
      /* long range firing */
      hispr += 3000;
   

   /* Might want to later increase that two 2*torp damage */

   if((((100 - PL_DAMAGE(e) + PL_SHIELD(e)) 
      <= SH_TORPDAMAGE(me)) && !_state.killer &&
      (e->dist <= SHIPDAMDIST - 1000)) ||
	 (me->p_ship.s_type == STARBASE && e->dist <= SHIPDAMDIST+2000)){

      if(DEBUG & DEBUG_WARFARE){
	 printf("player about to explode-- using pressor.\n");
      }
      if((me->p_flags & PFTRACT) || (me->p_flags & PFPRESS)){
	 if(me->p_tractor != j->p_no){
	    req_tractor_off("prepare to TR_EXPLODE");
	 }
      }
      handle_tractors(e, j, edist, 0, "about to blow up", TR_EXPLODE);
#ifdef nodef
      if(me->p_flags & PFTRACT){
	 req_tractor_off();
      }
      else if((me->p_flags & PFPRESS) && (me->p_tractor != j->p_no)){
	    req_tractor_off();
      }
#endif
      _special_tr = 1;
   }

   cdist = get_fire_dist(e, j, edist);
   
   if(_state.chaos) cdist += 2000;

   if(me->p_ship.s_type == STARBASE){
      if(MYWTEMP() < 20)
	 cdist = 9000;
      else if(MYWTEMP() < 50)
	 cdist = 7000;
      else if(MYWTEMP() < 80)
	 cdist = 5000;
      else cdist = 4000;
   }

   if(inl && me->p_ship.s_type != STARBASE && _state.protect &&
      _state.protect_planet && (e->pp_flags & PFCLOAK) && 
      dist_to_planet(e,_state.protect_planet) > 5000) 	/* conserve fuel */
      return;

   if((e->pp_flags & PFCLOAK) && edist < cdist && 
      ((PL_FUEL(e) < MYFUEL())||edist < 3500)){
      
      tht = _udcounter - e->thittime;
      pht = _udcounter - e->phittime;

      if(tht > 2 && pht > 2){
	 static int	ctu;
	 static int	nctu;
	 static char	toggle;

	 if(DEBUG & DEBUG_WARFARE){
	    printf("cloak torpfire unknown\n");
	 }
#ifdef nodef
	 urnd = (8*edist)/7500 + 2;
#endif
	 if(j->p_flags & PFORBIT){
	    get_torp_course(j, &tcrs, me->p_x, me->p_y);
	    if(!_state.defend || angdist(tcrs, dcrs) > 32)
	       req_torp(tcrs);
	 }
	 else {
	    if(_udcounter - ctu > nctu){
	       _state.torp_i = 100;
	       get_torp_course(j, &tcrs, me->p_x, me->p_y);
	       if(!_state.defend || angdist(tcrs, dcrs) > 32)
		  req_torp(tcrs);
	       nctu = (((RANDOM()%10)+6)*edist)/cdist;
	       if(me->p_ship.s_type == STARBASE && MYWTEMP() < 20)
		  nctu = 2;

	       toggle = 1;
	       ctu = _udcounter;
	    }
	    else if(toggle){
	       _state.torp_i = 100;
	       j->p_speed += 2;
	       get_torp_course(j, &tcrs, me->p_x, me->p_y);
	       j->p_speed -= 2;
	       if(!_state.defend || angdist(tcrs, dcrs) > 32)
		  req_torp(tcrs + (RANDOM()%3)-1);
	       toggle = 0;
	    }
	 }

	 if(k){
	    me->p_fuel = tf;
	 }
	 return;
      }
      else {
	 if(pht < 15){	/* might be wrong */
	    /* position has been updated by the server*/
	    if(DEBUG & DEBUG_WARFARE){
	       printf("cloak fire after phit\n");
	    }
	    size = 1;
	    urnd = 0;
	    _state.torp_i = 100;
	 }
	 else{
	    /* use the value in e */
	    j->p_x = e->cx;	/* might be wrong for thit? */
	    j->p_y = e->cy;

	    if(DEBUG & DEBUG_WARFARE){
	       printf("cloak fire after thit\n");
	    }

	    get_torp_course(j, &tcrs, me->p_x, me->p_y);
	    if(!_state.defend || angdist(tcrs, dcrs) > 32)
	       req_torp(tcrs);
	    _state.torp_i = 225;
	    size = 1;
	    urnd = 0;
	 }
      }
   }
   else {
      /* possibly cloaked */
      /* damage check first */
#ifdef nodef
      /* debug */
      if(edist < 9000){
	 _state.torp_i = 800;
	 get_torp_course(j, &tcrs, me->p_x, me->p_y);
	 if(!_state.defend || angdist(tcrs, dcrs) > 32)
	    req_torp(tcrs);
      }
      return;
#endif

      if(_state.protect && (j->p_flags & PFORBIT) && !(j->p_flags & PFCLOAK) &&
	 (j->p_planet == _state.protect_planet->pl_no) && edist < 9000){
	 if(DEBUG & DEBUG_WARFARE){
	    printf("using tractor on orbiting player\n");
	 }
	 size = 1;
	 _state.torp_i = 100;
	 if((me->p_flags & PFTRACT) || (me->p_flags & PFPRESS)){
	    if(me->p_tractor != j->p_no){
	       req_tractor_off("prepare to TR_ORBIT");
	    }
	 }
	 handle_tractors(e, j, edist, 1, "orbiting", TR_ORBIT);
	 _special_tr = 1;
	 get_torp_course(j, &tcrs, me->p_x, me->p_y);
	 if(!_state.defend || angdist(tcrs, dcrs) > 32)
	    req_torp(tcrs);
	 if(k){
	    me->p_fuel = tf;
	 }
	 return;
      }

      if(PL_DAMAGE(e) > (MYDAMAGE()+40) && !(j->p_flags & PFCLOAK) && 
	 MYFUEL() > 10 && !_state.disengage &&
	 !_state.killer &&
	 !_state.protect && !_state.defend && (e->dist > SHIPDAMDIST-1000)){
	 if((me->p_flags & PFTRACT) || (me->p_flags & PFPRESS)){
	    if(me->p_tractor != j->p_no)
	       req_tractor_off("prepare to TR_DAMAGED");
	 }
	 handle_tractors(e, j, edist, 1, "damaged", TR_DAMAGED);
	 _special_tr = 1;
      }

      if(PL_FUEL(e) < 20 && MYFUEL() > 40 && MYDAMAGE() < 50 && 
	 !(j->p_flags & PFCLOAK) && !_state.disengage &&
	 !_state.killer &&
	 !_state.protect && !_state.defend && (e->dist > SHIPDAMDIST)){
	 if((me->p_flags & PFTRACT) || (me->p_flags & PFPRESS)){
	    if(me->p_tractor != j->p_no)
	       req_tractor_off("prepare to TR_NOFUEL");
	 }
	 handle_tractors(e, j, edist, 1, "empty", TR_NOFUEL);
	 _special_tr = 1;
      }

#ifdef nodef
      printf("damage: %d\n", 100-PL_DAMAGE(e) + PL_SHIELD(e));
      printf("td: %d\n", SH_TORPDAMAGE(me));
      printf("dist: %d\n", e->dist);
#endif
      

      if(_state.defend){
	 /* about to blow up on player */
	 if(PL_DAMAGE(e) > 60 && ndist < 3000){
	    /* check to see which is better, push or pull */
	    if(_state.protect_player->dist >= edist){
	       /* use tractor */
	       if((me->p_flags & PFTRACT) || (me->p_flags & PFPRESS)){
		  if(me->p_tractor != j->p_no)
		     req_tractor_off("prepare to TR_DEFEND");
	       }
	       handle_tractors(e, j, edist, 1, "defend blow up (tractor)",
		  TR_DEFEND);
	       _special_tr = 1;
	    }
	    else {
	       if((me->p_flags & PFTRACT) || (me->p_flags & PFPRESS)){
		  if(me->p_tractor != j->p_no)
		     req_tractor_off("prepare to TR_DEFEND");
	       }

	       /* pressor */
	       handle_tractors(e, j, edist, 0, "defend blow up (pressor)",
		  TR_DEFEND);
	       _special_tr = 1;
	    }
	 }
      }

      if(!PDEFEND && (PL_DAMAGE(e) > 60 || (edang(e, 80) && dng_speed(e)))){

	 if(starbase(e) && edist < 9000 && !_state.assault)
	    size = 1;
	 else if(PL_DAMAGE(e) > 90 && edist < 9000 && !_state.assault){
	    size = 1;
	    _state.torp_i = (400 * edist)/9000;
	    if(DEBUG & DEBUG_WARFARE){
	       printf("damage 90 fire.\n");
	    }
	 }
	 else if(PL_DAMAGE(e) > 70 && edist < 8000 && !_state.assault){
	    size = 1;
	    _state.torp_i = (300 * edist)/9000;
	    if(DEBUG & DEBUG_WARFARE){
	       printf("damage 70 fire.\n");
	    }
	 }
	 else if(PL_DAMAGE(e) > 60 && edist < 6500 && !_state.assault){
	    size = 1;
	    _state.torp_i = (200 * edist)/6500;
	    if(DEBUG & DEBUG_WARFARE){
	       printf("damage 60 fire.\n");
	    }
	 }
	 else{
	    /* attacking at dangerous speed */
	    if(edist < dng_speed_dist(e) && !(e->pp_flags & PFCLOAK)){
	       if(!_state.assault){
		  size = 1;
		  _state.torp_i = 100;
		  if(DEBUG & DEBUG_WARFARE){
		     printf("dangerous speed fire.\n");
		  }
	       }
	       else if((RANDOM()%5) == 0){
		  size = 1;
		  _state.torp_i = 100;
	       }
	    }
	 }
	 /* no torp */
	 if(_state.defend && ndist < 4000 && pf > 10){
	    unsigned char	pcrs = get_awrapcourse(j->p_x, j->p_y, 
	       _state.protect_player->p->p_x,
	       _state.protect_player->p->p_y);
	    if(attacking(e, pcrs, 64))
	       size = 0;
	 }
      }
      else if(edist < hispr){
	 if(PL_FUEL(e) < MYFUEL() - ((_state.protect||_state.defend)?50:0) ){
	    size = 1;
	    if((RANDOM() % 4) == 3)
	       urnd = (RANDOM()%3);
	    else
	       urnd = 0;

	    if(e->pp_flags & PFCLOAK)
	       _state.torp_i = (500*edist)/7500;
	    else
	       _state.torp_i = (100*edist)/5000;
	    if(DEBUG & DEBUG_WARFARE){
	       printf("more fuel fire.\n");
	    }
	 }
	 else if(MYFUEL() > 70 && ((RANDOM() % 100) < 10)){
	    size = 1;

	    if(e->pp_flags & PFCLOAK)
	       _state.torp_i = (500*edist)/7500;
	    else
	       _state.torp_i = (200*edist)/5000;
	    if(DEBUG & DEBUG_WARFARE){
	       printf("RANDOM fire.\n");
	    }
	 }
	 else if(_state.defend && ((ndist < 4750 && ndist > 2500)||pf<10)){
	    size = 1;
	    _state.torp_i = (100*edist)/2000;
	    if(DEBUG & DEBUG_WARFARE){
	       printf("enemy too close fire.\n");
	    }
	 }
	 else if(!_state.defend && edist < 3550 && !(e->pp_flags & PFCLOAK)){
	    size = 1;
	    _state.torp_i = (100*edist)/2000;
	    if(DEBUG & DEBUG_WARFARE){
	       printf("enemy too close fire.\n");
	    }
	 }
	 else if(_state.defend && ndist < 3550 && (e->pp_flags & PFCLOAK)){
	    size = 1;
	    _state.torp_i = (100*edist)/2000;
	 }
      }
   }
   /* XX */
   _state.torp_i = 100;

   /* desperation */
   if(size == 0 && MYDAMAGE() > 75 && MYSHIELD() < 10 && 
      e->dist < e->hispr-1000)
      size = 1;

   if(size > 0){
      if(!_state.torp_wobble){
	 if(tcount == (RANDOM() % 3)){
	    tcrs --;
	    tcount ++;
	 }
	 else if(tcount == 4 + (RANDOM() % 3)){
	    tcrs ++;
	    tcount --;
	 }
	 if(tcount < 0) tcount = 0;
	 if(tcount > 8) tcount = 0;
      }
      
      get_torp_course(j, &tcrs, me->p_x, me->p_y);
      if(!_state.defend || angdist(tcrs, dcrs) > 32)
	 req_torp(tcrs);
   }

   if(k){
      me->p_fuel = tf;
   }
}

plasma_attack(e,j,edist)

   Player		*e;
   struct player	*j;
   int			edist;
{
   if(!have_plasma()) return;
   if(MYFUEL() < 20 || MYWTEMP() > 80) return;

   /* check for other enemies within phaser range of my players.. */

   if(e->pp_flags & PFCLOAK){
      if(edist < 8000 && e->p->p_speed > 7 && e->p->p_ship.s_type != SCOUT &&
	 e->p->p_ship.s_type != DESTROYER)
	 req_pltorp(e->crs);
      return;
   }
   if(me->p_ship.s_type == STARBASE) return;

   if((RANDOM() % (100-MYFUEL()+20)) < 10){
      if(_state.torp_bounce){
	 if(edist < 20000 && edist > (e->hispr + e->hispr/2))
	    req_pltorp(e->icrs);
      } else if(edist < 10000 && edist > e->hispr+2000){
	 req_pltorp(e->icrs);
      }
   }
}

have_plasma()
{
   if(_state.chaos){
      switch(me->p_ship.s_type){
	 case DESTROYER:
	 case CRUISER:
	 case BATTLESHIP:
	 case STARBASE:
	 case GALAXY:
	    return 1;
	 default:
	    return 0;
      }
   }
   else if(me->p_ship.s_type == STARBASE)
      return 1;
   else
      return _state.have_plasma;
}

pphaser_attack(e, j, edist)

   Player		*e;
   struct player	*j;
   int			edist;
{
   int			x,y, tf;
   unsigned char	pcrs;
   int			pd = PHASEDIST * me->p_ship.s_phaserdamage/100, prange;
   int			k = (_state.killer || e->robot || starbase(e));
   int			ndist, pf = 100;
   int			epm;

   if(k){
      tf = me->p_fuel;
      me->p_fuel = 500000;
   }

   epm = (e->phit_me > 0) && edist < pd-500;

   if(_state.defend){
      ndist = pdist_to_p(j, _state.protect_player->p);
      pf = PL_FUEL(_state.protect_player);
   }

   if(me->p_ship.s_type == STARBASE)
      prange = pd - 2000;
   else if(!_state.human)
      prange = pd - 1000-(RANDOM()%1000);	/* higher point range */
   else
      prange = pd - 1000;

   if(e->pp_flags & PFCLOAK){
      if(edist < pd){
	 if(_udcounter - e->phittime <= 11 && (phasers[me->p_no].ph_status
	    == PHFREE)){

	    /*
	    if(!_state.defend || pf < 10 || (_state.defend && ndist > 2500)){
	    */
	       if(DEBUG & DEBUG_WARFARE){
		  printf("phaser on cloak after phaser hit.\n");
	       }
	       if(!(me->p_ship.s_type == STARBASE && MYWTEMP() > 70))
		  req_phaser(get_course(j->p_x, j->p_y),0);
	       e->thit = 0;
	       e->thittime = 0;
	    /*
	    }
	    */
	 }
	 else if(_udcounter - e->thittime < 4) {
	    if(phasers[me->p_no].ph_status == PHFREE){

	       /*
	       if(!_state.defend || pf < 10 || (_state.defend && ndist > 2500)){
	       */
		  if(DEBUG & DEBUG_WARFARE){
		     printf("firing anticloak phasers on thit..\n");
		  }
		  /* fire at location of hit */
		  if(!(me->p_ship.s_type == STARBASE && MYWTEMP() > 70))
		     req_phaser(get_course(e->cx, e->cy),0);
		  e->thit = 0;
		  e->thittime = 0;
	       /*
	       }
	       */
	    }
	 }
	 else if(_state.protect && edist < 3000){
	    if(DEBUG & DEBUG_WARFARE){
	       printf("random phaser -- enemy too close.\n");
	    }
	    req_phaser(get_course(j->p_x, j->p_y), _state.attackers>1 &&
	       MYWTEMP() < 50);
	    e->thit = 0;
	    e->thittime = 0;
	 }
      }
   }
   /* NOTE -- took server delay out of phaser req */
   else if(edist < pd){
      if((me->p_ship.s_type == STARBASE && !(j->p_flags & PFCLOAK) &&
	 PL_FUEL(e) < 10 && active(_state.closest_f) && !_state.protect &&
	 _state.closest_f->dist < 6000 && PL_FUEL(_state.closest_f)> 20) ||
	 (_state.player_type == PT_DEFENDER && PL_FUEL(e) < 10 && 
	 active(_state.protect_player) && !starbase(_state.protect_player) &&
	 !_state.escort && 
	 !_state.escort_req) && PL_FUEL(_state.protect_player)>20){
	 mprintf("free kill\n");
	 return;
      }

      if(PL_DAMAGE(e) > 50 && edist < pd - 750){
	 /*
	 if(!_state.defend || pf < 10 || (_state.defend && ndist > 2500)){
	 */
	    x = j->p_x +
	       _avsdelay * (double)j->p_speed * Cos[j->p_dir] * WARP1;
	    y = j->p_y +
	       _avsdelay * (double)j->p_speed * Sin[j->p_dir] * WARP1;
	    pcrs = get_course(x,y);
	    req_phaser(pcrs,_state.attackers > 2);
	    if(e->phit_me > 0)
	       e->phit_me --;
	    if(DEBUG & DEBUG_WARFARE){
	       printf("damage 50 phaser fire.\n");
	    }
	 /*
	 }
	 */
      }
      else if((PL_FUEL(e) <= MYFUEL()) || edist < prange || epm){
	 /*
	 if(!_state.defend || pf < 10 || (_state.defend && ndist > 2500) ||epm){
	 */
	    if((phaser_condition(e, edist) && edist < prange) || epm){
	       x = j->p_x +
		  _avsdelay * (double)j->p_speed * Cos[j->p_dir] * WARP1;
	       y = j->p_y +
		  _avsdelay * (double)j->p_speed * Sin[j->p_dir] * WARP1;
	       pcrs = get_course(x,y);
	       req_phaser(pcrs,_state.attackers > 2);
#ifdef nodef
	       if(DEBUG & DEBUG_WARFARE){
		  printf("hitme = %d\n", e->phit_me);
	       }
#endif
	       if(e->phit_me > 0)
		  e->phit_me --;
	    }
	 /*
	 }
	 */
      }
      else
      if(_state.protect && (j->p_flags & PFORBIT) && !(e->pp_flags & PFCLOAK) &&
	 (j->p_planet == _state.protect_planet->pl_no)){
	 if(edist < pd-750){
	    /*
	    if(!_state.defend || pf < 10 || (_state.defend && ndist > 2500)){
	    */
	       x = j->p_x +
		  _avsdelay * (double)j->p_speed * Cos[j->p_dir] * WARP1;
	       y = j->p_y +
		  _avsdelay * (double)j->p_speed * Sin[j->p_dir] * WARP1;
	       pcrs = get_course(x,y);
	       req_phaser(pcrs,_state.attackers > 2);
	    /*
	    }
	    */
	 }
      }
      else if(_state.defend && in_danger(_state.protect_player)){
	 x = j->p_x +
	    _avsdelay * (double)j->p_speed * Cos[j->p_dir] * WARP1;
	 y = j->p_y +
	    _avsdelay * (double)j->p_speed * Sin[j->p_dir] * WARP1;
	 pcrs = get_course(x,y);
	 req_phaser(pcrs,_state.attackers > 2);
      }
   }

   if(k){
      me->p_fuel = tf;
   }
}

active(p)

   Player	*p;
{
   if(!p || !p->p || !isAlive(p->p) || PL_FUEL(p) < 5)
      return 0;
   return 1;
}

starbase(e)

   Player	*e;
{
   if(!e || !e->p || !isAlive(e->p)) return 0;

   return e->p->p_ship.s_type == STARBASE;
}

oggwarfare(type, e, j, edist)

   int			type;
   Player		*e;
   struct player	*j;
   int			edist;
{
   unsigned char	tcrs;

   if(type == WPHASER){
      if(me->p_flags & PFPRESS)
	 req_pressor_off();
   
      handle_tractors(e, j, edist, 1, "ogg tractors", TR_OGG);
      if(e->dist < (PHASEDIST*me->p_ship.s_phaserdamage/100 - 1500))
	 req_phaser(get_course(j->p_x, j->p_y), 1);

   }
   else if (type == WTORP){
      get_torp_course(j, &tcrs, me->p_x, me->p_y);
      _state.torp_i = 100;
      req_torp(tcrs);
   }
}

too_close(e)

   Player	*e;
{
   return e->dist < e->hispr+2000;
}

/* was 5000 */

get_fire_dist(e, j, edist)
   
   Player		*e;
   struct player	*j;
   int			edist;
{
   int	t = me->p_ship.s_torpspeed, turns;

   if(!e || !j || j->p_status != PALIVE)
      return 0;

   return 4000;
}

in_danger(p)

   Player	*p;
{
   if(PL_DAMAGE(p) > 50 && PL_SHIELD(p) < 50)
      return 1;
   if(PL_FUEL(p) < 10)
      return 1;
   if(p->armies > 0)		/* test */
      return 1;
   
   if(starbase(p) && PL_WTEMP(p) > 60)
      return 1;
   
   return 0;
}
