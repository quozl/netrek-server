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

#define OCOURSE_DIST		20000

unsigned char ocourse();

static int 	odist;

/* ogg player */

/* actual firing doesn't occur until _state.ogg = 1 */

ogg()
{
   Player	*e = _state.current_target;
   int		p;

   if(!e || !e->p || !isAlive(e->p)){
      if(DEBUG & DEBUG_OGG)
	 printf("ogg: no enemy.\n");
      _state.ogg_req = 0;
      req_cloak_off("ogg: no enemy");
      _state.ogg = 0;
      return;
   }

   if(_state.borg_detect && e->borg_probability > .15)
      odist = _state.ogg_dist + 2000;
   else
      odist = _state.ogg_dist;

#ifdef nodef
   if(starbase(e))
      _oggdist = e->hispr-2000;
   else
      _oggdist = 4000;	/* experiment */
#endif

   if((MYFUEL() < 10 || MYDAMAGE() > 50) && e->dist > 7000){
      if(DEBUG & DEBUG_OGG)
	 printf("ogg aborted.\n");
      _state.ogg_req = 0;
      req_cloak_off("ogg: aborted");
      _state.ogg = 0;
      return;
   }


   goto_ogg(e);
   p = wait_on_sync();

/* -1: don't wait
   0: wait 
   1: uncloak */

   if(!_state.ogg){
      if( (p == -1 && e->dist < odist) ||
	  (p == 1 && e->dist < 7000)){

	 if(DEBUG & DEBUG_OGG)
	    printf("beginning active ogg\n");
	 req_cloak_off("ogg: beginning active ogg");
	 _state.ogg = 1;
      }
      else if(_udcounter - e->phittime_me < phaser_int && e->dist < 6000){
	 if(DEBUG & DEBUG_OGG)
	    printf("beginning active ogg from phaser hit\n");
	 req_cloak_off("ogg: beginning active ogg");
	 _state.ogg = 1;
      }
      else if(e->dist < 6000 && friend_uncloak(e->p->p_no)){
	 if(DEBUG & DEBUG_OGG)
	    printf("beginning active ogg on auto sync\n");
	 req_cloak_off("ogg: beginning active ogg");
	 _state.ogg = 1;
      }
   }
}

friend_uncloak(e_num)
   int	e_num;
{
   Player		*p = _state.closest_f;
   struct player	*j;
   if(!p || !p->alive)
      return 0;
   j = p->p;
   
   if(p->distances[e_num] <= 7000 && !(j->p_flags & PFCLOAK))
      return 1;
   
   return 0;
}

goto_ogg(e)

   Player	*e;
{
   unsigned char	crs, crs_r, ogg_course();
   int			speed, speed_r, hittime, edist, lvorbit;
   int			i, sb = (me->p_ship.s_type == STARBASE);

   if(e->dist > cloak_odist){
      speed = mymax_safe_speed();
      req_cloak_off("ogg: enemy not close enough");
   }

   if(e->dist > odist && !_state.ogg){
      crs = ogg_course(e, &i);
      if(i && !(me->p_flags & PFCLOAK) && (me->p_ship.s_type != STARBASE))
	 speed = sb?mymax_safe_speed():mymax_safe_speed()-1;
      if(angdist(me->p_dir, crs) > 32)
	 speed /= 2;
   }
   else{
      crs = e->icrs + _state.ogg_early_crs;
      speed = me->p_ship.s_maxspeed;
      if(angdist(me->p_dir, crs) > 32)
	 speed /= 2;
   }

   if(e->dist < odist || _state.ogg){
#ifdef nodef
      if(starbase(e)){
	 if(angdist(me->p_dir, crs) < 32)	/* was 24 */
	    speed = me->p_ship.s_maxspeed;
	 else
	    speed = mymax_safe_speed()-1;
      }
      else{
	 if(running_away(e, crs, 32))
	    speed = e->p->p_speed+1;
	 else if(attacking(e, crs, 32))
	    speed = myfight_speed();
	 else
	    speed = e->p->p_speed;
      }
      /*
      speed += _state.ogg_speed;
      */
      if(angdist(me->p_dir, crs) > 32)
	 speed /= 2;
   
#endif
   } else if(e->dist <= cloak_odist){
      if(angdist(me->p_dir, crs) < 32)	/* was 24 */
	 speed = me->p_ship.s_maxspeed;
      else
	 speed = mymax_safe_speed()-1;

      if(MYFUEL() > 50 && !_state.ptorp_danger){
	 if(!_state.ogg)
	    req_cloak_on();
      }
      if(_state.ptorp_danger)
	 /* element of surprise is lost */
	 speed /= 2;
   }

   if(_state.ogg_pno){
      speed = modspeed(speed, &_state.players[_state.ogg_pno-1], e);
   }

   compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
   if(_state.ogg && angdist(crs, crs_r) > 64){
      /* trust to det */
      /*
	printf("overriding cc: crs: %d, sp: %d\n", crs, speed);
      */
      req_set_course(crs);
      req_set_speed(speed, __LINE__, __FILE__);
   }
   else {
      /*
      printf("crs: %d (%d), sp: %d (%d)\n", crs_r, crs, speed_r, speed);
      */
      req_set_course(crs_r);
      req_set_speed(speed_r, __LINE__, __FILE__);
   }
}

unsigned char ogg_course(o, c)

   Player	*o;
   int		*c;
{
   unsigned char	crs, hcrs, bcrs;
   int			avcrs;
   Player		*me_p = &_state.players[me->p_no];
   register Player	*e;
   register int		i;
   int			first = 1, cd = 0;

   if(o->invisible || o->dist > MAX(OCOURSE_DIST, 3*evade_radius)){
      crs = o->icrs;
   }
   else if(o->dist <= MAX(OCOURSE_DIST, 3*evade_radius)){
      crs = ocourse(o);
      return crs;
   }
   else{
      crs = o->icrs;
   }

   for(avcrs= 0,i=0,*c=0 ; i< MAXPLAYER; i++){
      e = &_state.players[i];
      if(!e->enemy || !e->p || !isAlive(e->p) || e == o) continue;
      if(e->dist > _state.assault_dist) continue;

      if(me_p->attacking[i] || _state.attacking_newdir[i]) {

         if(DEBUG & DEBUG_OGG)
            printf("wish to avoid %d\n", e->p->p_no);

         if( (PL_FUEL(e) > 10 && !starbase(e)) || starbase(e)){
            bcrs = e->crs;
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
   }
   if(*c == 0) return crs;

   _state.newdir = crs;

   /*
   printf("my course %d\n", me->p_dir);
   */
   if(DEBUG & DEBUG_OGG){
      printf("me attacking average direction %d\n", NORMALIZE(avcrs));
      printf("original course: %d, ", crs);
   }

   /* experiment with less drastic course change */

   avoiddir(NORMALIZE(avcrs), &crs, 20);

   if(DEBUG & DEBUG_OGG)
      printf("new course %d\n", crs);

   return crs;
}

unsigned char ocourse(o)
   
   Player	*o;
{
   int	r, dist;
   struct player	*j = o->p;
   unsigned char	dir;

   if(j->p_ship.s_type != STARBASE){
      return (unsigned char) ((int)o->icrs);
   }


   if(!ogg_state){
      int	a;

      dist = ihypot((double)(me->p_x - (j->p_x + ogg_offx)),
	      (double)(me->p_y - (j->p_y + ogg_offy)));

      if(/*(dist < 10000 && dist > 3000 && ((random()%15)== 0)) || */
	 hit_by_torp){
	 if(DEBUG & DEBUG_OGG)
	    printf("changing ogg variables (ht: %d)\n", hit_by_torp);
	 set_ogg_vars();
      }

      if(dist < 3000){
	 if((RANDOM()%2) == 1)
	    set_ogg_vars();
	 else{
	    if(DEBUG & DEBUG_OGG)
	       printf("REACHED ogg offset point.\n");
	    ogg_state = 1;
	 }
      }
      dir = get_course(j->p_x + ogg_offx, j->p_y + ogg_offy);
      if(angdist(dir, o->icrs) > 90){	/* missed it, no prob */
	 set_ogg_vars();
	 dir = get_course(j->p_x + ogg_offx, j->p_y + ogg_offy);
      }
   }

   if(ogg_state){
      r = (int)o->icrs + _state.ogg_early_crs;
   }
   else{
      r = dir;
   }

   return (unsigned char) r;
}

int modspeed(sp, p, e)
   
   int		sp;
   Player	*p, *e;
{
   int		v;
   int		speed;

   if(me->p_flags & PFCLOAK)
      return sp;

   if(!p || !p->p || !isAlive(p->p) || p->enemy)
      return sp;
   
   if(p->p->p_flags & PFORBIT)
      return sp;		/* probably recharging */

   if(!(p->p->p_flags & PFCLOAK))
      speed = p->p->p_speed;
   else
      speed = sp;
   
   v = angdist(p->crs, e->crs);
   if(v < 64){
      /* means he's in front of me .. should I speed up? */
      if(p->dist < 3000)
	 return speed;
      if(e->dist - p->dist >= p->dist)
	 return myemg_speed();		/* catch up */

      if(e->dist > 10000)
	 return 2;		/* wait */
      
   }
   else if(v > 64){
      /* means he's behind me .. should I slow down? */
      if(p->dist < 5000)
	 return speed;

      if(e->dist > 10000)
	 return 2;		/* wait */
   }

   return speed;
}

/* -1: don't wait
   0: wait 
   1: uncloak
 */

wait_on_sync()
{
   Player	*p;
   static int	timer;

   if(!_state.ogg_pno){
      return -1;
   }
   
   p = &_state.players[_state.ogg_pno-1];
   if(!p || !p->p || /* !isAlive(p->p) || */ p->enemy)
      return -1;
   
   /* too far away */
   if(p->dist > 15000){
      return -1;
   }

   if((timer++ % 50)==0)
      sync_check(&_state.current_target);
   
   if(!(p->p->p_flags & PFCLOAK)){
      return 1;
   }
   
   return 0;
}

unsigned char	ogg_incoming_dir = 0;

set_ogg_vars()
{
   int			r;
   Player		*e = _state.current_target;
   struct player	*j;
   int			hd;
   int			dir, x,y, ra;
   unsigned char	crsc;

   if(!e) return;
   if(e->dist <= 0) e->dist = 1;

   evade_radius = 4000 + rrnd(3000);

   if(evade_radius < odist)
      evade_radius = odist + random()%2000;

   if(!e || !e->p){
      return;
   }
   j = e->p;
   hd = e->dist - 1000 - rrnd(e->dist/2);
   if(hd <= 0) hd = 500;
   if(hd > evade_radius) hd = evade_radius;

   /* hd is the radius of the circle, we choose a random point on the
      half circle */
   
   dir = e->icrs - 128;

   ra = random()%128;
   crsc = (unsigned char)(dir - 64 + ra);

   ogg_incoming_dir = crsc;

   if(DEBUG & DEBUG_OGG){
      printf("INCOMING DIRECTION: %d\n", ogg_incoming_dir);
      printf("evade_radius: %d\n", evade_radius);
   }

   ogg_offx = (int)((double)hd * Cos[ogg_incoming_dir]);
   ogg_offy = (int)((double)hd * Sin[ogg_incoming_dir]);

   x = j->p_x + ogg_offx;
   y = j->p_y + ogg_offy;

   ogg_state = 0;

   _state.ogg_early_crs = rrnd(10);
   _state.ogg_speed = 2 + RANDOM()%5;

   /* NOT USED */
   ogg_early_dist = 12000 + (RANDOM() % 3000);
   aim_in_dist = 4000 + (RANDOM() % 2000);

   if(DEBUG & DEBUG_OGG){
      printf("target: %c%c\n", j->p_mapchars[0], j->p_mapchars[1]);
      printf("ogg_early_crs: %d\n", _state.ogg_early_crs);
      /*
      printf("aiming for point (%d,%d) (off: %d,%d)\n", x,y, ogg_offx,ogg_offy);
      */
      printf("ogg_speed: %d\n", _state.ogg_speed);
   }

   if(_state.player_type == PT_OGGER)
      phaser_int = random()%10;
}
