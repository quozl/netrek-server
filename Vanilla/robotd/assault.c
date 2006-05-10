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

unsigned char assault_course();
unsigned char get_pl_course();

/* assault planet */

assault()
{
   if(!_state.assault || !_state.assault_planet){
      return;
   }

   if(me->p_armies == 0 && _state.assault_req != ASSAULT_BOMB){
      unassault_c("no armies");
      return;
   }

   if(orbiting(_state.assault_planet))
      assault_planet();
   else{
      if(DEBUG & DEBUG_ASSAULT){
	 printf("not arrived at planet.\n");
      }
      if(_state.assault_req == ASSAULT_TAKE){
	 /*
	 if(me->p_flags & PFORBIT)
	    fprintf(stderr, "escort req may be ignored by robots\n");
	 */
	 notify_take(_state.assault_planet, 1);
      }
      goto_assault_planet();
   }
}

goto_assault_planet()
{
   Player		*e = _state.closest_e;
   struct player	*j = e?e->p:NULL;
   struct planet	*pl = _state.assault_planet;
   unsigned char	pcrs, ecrs, crs_r, new_course;
   int			pdist, edist, dectime, lvorbit;
   double		dx,dy;
   int			speed, speed_r, hittime, c, disengage;
   int			cloak;

   pdist = pl->pl_mydist;
   pcrs = get_wrapcourse(pl->pl_x, pl->pl_y);

   dectime = (pdist-(ORBDIST/2) < (14000 * (me->p_speed) * (me->p_speed))/
      SH_DECINT(me));

   if(j && isAlive(j))
      edist = e->dist;
   else
      edist = GWIDTH;

   new_course = get_pl_course(pl, &speed, &cloak);
   pcrs = new_course;

   /* bug -- crashes into people */
   /* don't cloak if we're far from planet */
   /* start within assuming you don't need cloak */
   cloak = 0;

   if(!inl  && pdist > 20000)
      cloak = 0;

   /* cloak bomb near enemy core, so you don't get res-killed */

   if (pdist < 7000) {
     if (pl && pl->pl_flags&PLHOME)
       cloak=1;

     if (pl && pl->pl_no == 7) /* Altair */
       cloak=1;

     if (pl && pl->pl_no == 16) /* Draconis */
       cloak=1;

     if (pl && pl->pl_no == 26) /* Scorpii */
       cloak=1;
   }

   if(pdist < 10000 && edist < 18000)
      cloak = 1;
   
   /* experiment */
   if(inl && cloak && (his_ship_faster(e) > 2)) cloak = 0;

   if(cloak)
      req_cloak_on();
   else
      req_cloak_off("no cloak in goto_assault_planet()");

   if(DEBUG & DEBUG_ASSAULT){
      if(pdist > 20000){
	 printf("assault planet farther than 20000 -- speed %d\n", speed);
      }
   }

   if(pdist < 8000 && !dectime){
      _state.lock = 0;
   }
   else if(dectime){
      if(!_state.lock && _state.do_lock){
	 if(DEBUG & DEBUG_ASSAULT){
	    printf("lock request: %s\n", pl->pl_name);
	 }
	 req_planetlock(pl->pl_no);
	 _state.lock = 1;
      }
      else{
	 speed = myfight_speed();
	 _state.lock = 0;
      }
   }

   if(!_state.lock){
      compute_course(pcrs, &crs_r, speed, &speed_r, &hittime, &lvorbit); 
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
   }
}

/* UNUSED */
do_cloak(pdist)
   
   int	pdist;	/* unused */
{
   Player	*e = _state.closest_e;

   if(!e) 		return 0;
   if(e->dist > 17500) 	return 0;
   if(e->fuel < 20 && me->p_speed > 0) return 0;
   if(pdist > 25000)	return 0;

   if(!(me->p_flags & PFCLOAK)){
      req_cloak_on();
      return 1;
   }
   return 1;
}

assault_planet()
{
  Player         *e = _state.closest_e;
  struct player	 *j = e?e->p:NULL;
  struct planet  *pl = _state.assault_planet;
  int            cloak;
  int            armies = pl->pl_armies;

#ifdef nodef
   if(!do_cloak(0))
      req_cloak_off();
#endif

   cloak=0; /* start assuming you don't need to cloak */

   if (e && e->p && isAlive(e->p)){ /* cloak when enemy is near */
     if(e->dist < 10000) {
	cloak=1;
     }
   }

   /* cloak bomb near enemy core, so you don't get res-killed */
   if (pl && pl->pl_flags&PLHOME)
     cloak=1;

   if (pl && pl->pl_no == 7) /* Altair */
     cloak=1;

   if (pl && pl->pl_no == 16) /* Draconis */
     cloak=1;

   if (pl && pl->pl_no == 26) /* Scorpii */
     cloak=1;

   if(cloak)
      req_cloak_on();
   else
      req_cloak_off("no cloak in assault_planet()");

   if(armies > 4){
      req_bomb();
      return;
   }

#ifdef nodef
   if(me->p_armies < armies){
      if(j && e->dist < 7000){
	 return;
      }
   }
#endif

   if(me->p_armies == 0){
      req_shields_up();
      unassault_c("no armies");
      if(_state.assault_req == ASSAULT_BOMB)
	 _state.assault_req = 0;
      return;
   }

   req_beamdown();
}

/*
 * Called from s_defense when orbiting a planet we want to assault.
 */

defend_enemy_planet()
{
   Player		*e = _state.closest_e;
   int			num_hits;
   int			speed, hittime, lvorbit, damage, avdir;
   unsigned char	newc;
   int			break_off = 0, hship;

   if(me->p_ship.s_type == SCOUT)
      hship = 1;
   else if(me->p_ship.s_type == ASSAULT)
      hship = 4;
   else
      hship = 3;

   _state.no_assault = 0;
   _state.do_lock = 1;

   if(!(_state.torp_danger || _state.recharge_danger || _state.ptorp_danger))
      return;
   
   /* how many hits without leaving orbit? */
   num_hits = update_torps(&avdir, _state.p_desdir, _state.p_desspeed,
      &hittime, &damage, 0);
   
   if(!inl && _state.closest_e){
      int	i;
      i = _udcounter - _state.closest_e->phittime_me;
      if(i > 5 && i < 20){
	 _state.no_assault = 1;
	 req_shields_up();
	 break_off = 1;
      }
   }
   
   if(num_hits > 1 && hittime < 2){
      _state.no_assault = 1;
      _state.do_lock = 0;
      req_shields_up();
   }
   if(MYFUEL() > 25)
      check_dethis(num_hits, hittime);

   /* I don't think this is clueful behavior */
#ifdef nodef
   /* jump off planet to avoid phasers or mutliple hits */
   if(break_off || num_hits > hship){
      num_hits = compute_course(_state.p_desdir, &newc, _state.p_desspeed, 
	 &speed, &hittime, &lvorbit);
      if(lvorbit){
	 speed = 3;
	 mprintf("lvorbit true\n");
	 _state.no_assault = 1;
	 _state.do_lock = 0;
	 req_shields_up();
	 req_set_course(newc);
	 req_set_speed(speed, __LINE__, __FILE__);
      }
   }
#endif
}

/* NOT USED */
unsigned char assault_course(pl, pdist, crs, c)

   struct planet	*pl;
   int			pdist;
   unsigned char	crs;
   int			*c;
{
   unsigned char	hcrs, bcrs;
   int			avcrs;
   register Player	*e;
   register int		i, q=0;
   int			first = 1, cd=0;

   for(avcrs= 0,i=0,*c=0 ; i< MAXPLAYER; i++){
      e = &_state.players[i];
      if(!e->enemy || !e->p || !isAlive(e->p)) continue;
      if(e->dist > _state.assault_dist) continue;

      /* newdir is course directly to planet */
      if(me_p->attacking[i] || _state.attacking_newdir[i]) {

	 if(DEBUG & DEBUG_ASSAULT){
	    printf("ATTACKING %s, ", e->p->p_mapchars);
	    printf("(newdir: %s, ", _state.attacking_newdir[i]?"true":"false");
	    printf("olddir: %s)\n", me_p->attacking[i]?"true":"false");
	 }
	 /*
	 printf("attacking %s\n", e->p->p_mapchars);
	 */

         if( (PL_FUEL(e) > 10 && !starbase(e)) || starbase(e)){
            bcrs = e->crs;	/* experiment -- icrs -vs- crs */
	    /*
            printf("me attacking %s(%d) on course %d\n", e->p->p_name, 
	       e->p->p_no, bcrs);
	    */
	    if(first) {
	       avcrs = bcrs;
	       first = 0;
	       cd = _state.assault_dist - e->dist;
	    }
	    else
	       avcrs = accavdir(avcrs, cd, bcrs, _state.assault_dist - e->dist);
	    cd += _state.assault_dist - e->dist;

            (*c)++;
         }
      }
      else if(e->attacking[me->p_no] && e->dist < e->hispr+2000){
         if( (PL_FUEL(e) > 10 && !starbase(e)) || starbase(e)){
	    /*
	    printf("being attacked by %s\n", e->p->p_mapchars);
	    */
	    
	    bcrs = e->icrs;
	    if(first) {
	       avcrs = bcrs;
	       first = 0;
	       cd = _state.assault_dist - e->dist;
	    }
	    else
	       avcrs = accavdir(avcrs, cd, bcrs, _state.assault_dist - e->dist);
	    cd += _state.assault_dist - e->dist;
	    q++;
	 }
      }
   }
   if((*c == 0 && q == 0 )|| pdist < 3000) return crs;

   _state.newdir = crs;

   /*
   printf("my course %d\n", me->p_dir);
   */
   if(DEBUG & DEBUG_ASSAULT){
      printf("me attacking average direction %d\n", NORMALIZE(avcrs));
      printf("original course: %d, ", crs);
   }
   /*
   printf("average direction to avoid: %d\n", NORMALIZE(avcrs));
   printf("\tcompared to closest_enemy crs: %d\n", _state.closest_e->crs);
   printf("\tcompared to closest_enemy icrs: %d\n", _state.closest_e->icrs);
   */

   /*
   if(cd/ *c > _state.assault_dist/2)
   */
   avoiddir(NORMALIZE(avcrs), &crs, _state.assault_range);

   if(DEBUG & DEBUG_ASSAULT)
      printf("new course %d\n", crs);
   
   mprintf("new course: %d, compared to course to closest: %d (%s) (=> %d)\n",
      crs, _state.closest_e->icrs, _state.closest_e->p->p_mapchars,
      angdist(crs, _state.closest_e->icrs));
   
   return crs;
}

/* NOTUSED */
accavdir_nd(avdir, dir)

   int			avdir;
   unsigned char	dir;
{
   register int	dif = angdist((unsigned char)avdir, dir);
   register	n;

   n = dir+dif;
   if(NORMALIZE(n) == avdir)
      avdir = dir + dif/2;
   else
      avdir = avdir + dif/2;

   return NORMALIZE(avdir);
}

unsigned char get_pl_course(pl, speed, cloak)

   struct planet	*pl;
   int			*speed;
   int			*cloak;
{
   int			pldist, edist;
   register		i, de=0;
   register Player	*p;
   unsigned char	pdcrs, pcrs;
   bool			first = 1;
   int			sp, dam_weight = 0;
   int			avcrs;
   Player		*ce = _state.closest_e;
   int			ad;
   int			mcd = INT_MAX, cd;
   int			dam;

   *cloak = 0;
   *speed = myemg_speed();

   if(!pl)
      pl = home_planet();

   pldist = pl->pl_mydist;
   pdcrs = get_wrapcourse(pl->pl_x, pl->pl_y);
   
   for(i=0,p=_state.players; i< MAXPLAYER; i++,p++){
      if(!p || !p->p || !isAlive(p->p)) continue;
      if(p->enemy && ((PL_FUEL(p) > 10 && !starbase(p)) || starbase(p))
		  && PL_DAMAGE(p) < 90){

	 /* starbase kludge */
	 if(starbase(p))
	    edist = p->dist - 5000;
	 else
	    edist = p->dist;

	 if(check_ship_speed(p, edist) < pldist){
	    
	    /*
	    if(edist < 15000)
	       pcrs = p->icrs;
	    else
	    */
	       pcrs = p->crs;

	    dam = 20000 - edist;
	    if(dam < 0) dam = 0;

	    if(first){
	       avcrs = pcrs;
	       first = 0;
	       dam_weight = dam;
	    }
	    else{
	       avcrs = accavdir(avcrs, dam_weight, pcrs, dam);
	       dam_weight += dam;
	    }
	    de++;
	 }
	 cd = edist - TRACTRANGE(p->p);
	 if(cd < mcd)
	    mcd = cd;
      }
   }

   if(mcd < 8000)
      *cloak = 1;

   if(!de) {
      /*
      printf("%s: no one close to planet.\n\n", pl->pl_name);
      */
      ad = angdist(me->p_dir, pdcrs);
      *speed -= (ad/8);
      if(*speed < 2) *speed = 2;
      return pdcrs;
   }

   if(!dam_weight){
      ad = angdist(me->p_dir, pdcrs);
      *speed -= (ad/8);
      if(*speed < 2) *speed = 2;
      return pdcrs;
   }

   avcrs = NORMALIZE(avcrs);

   /*
   printf("%s: planet course: %d\n", pl->pl_name, pdcrs);
   printf("weighted average direction to avoid = %d\n", avcrs);
   */


   avoiddir(avcrs, &pdcrs, (unsigned char)_state.assault_range);

   /* "oscillation damper" */

   if(!ce || ce->dist > 10000){
      i = angdist(pdcrs, _state.p_desdir);
      if(i > 0){
	 pcrs = _state.p_desdir + i;
	 if(pcrs == pdcrs){
	    pdcrs -= i/2;
	    /*
	    printf("osc damped %d ccw\n", i/2);
	    */
	 }
	 else{
	    pdcrs += i/2;
	    /*
	    printf("osc damped %d cw\n", i/2);
	    */
	 }
      }
   }

   /*
   printf("returned course: %d\n\n", pdcrs);
   */

   if(!_state.wrap_around)
      check_gboundary(&pdcrs, 0);

   ad = angdist(me->p_dir, pdcrs);
   *speed -= (ad/8);
   if(*speed < 2) *speed = 2;

   return pdcrs;
}

check_ship_speed(p, v)

   Player	*p;
   int		v;
{
   int	val = his_ship_faster(p), r;
   if(val > 0){
      r = v - 500*val;
   }
   else if(val < 0){
      r = v + 500*(-val);
   }
   else
      r = v;
   
   if(r < 0) r = 1000;

   return r;
}
