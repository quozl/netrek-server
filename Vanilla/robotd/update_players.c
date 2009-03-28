#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include "defs.h"
#undef hypot
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

/* These are the flags we get from every player */
#define FLAGMASK (PFSHIELD|PFBOMB|PFORBIT|PFCLOAK|PFROBOT|PFBEAMUP|PFBEAMDOWN|PFPRACTR|PFDOCK|PFTRACT|PFPRESS|PFDOCKOK)

#define NO_PFTRACT


Player 		*get_target();
struct planet	*closest_planet();

update_players()
{
   register struct player	*j, *closest = NULL;
   register Player		*p, *ce = NULL, *cf = NULL, *ct = NULL;
   register			i;
   int				mce = INT_MAX, mcf = INT_MAX, mct = INT_MAX, d;
   int				pldist;
   int				predictx, predicty, rx,ry;
   Player			*nt;

   if(_state.closest_e && _state.closest_e->p && 
      _state.closest_e->p->p_status == PEXPLODE){
      explode_danger = 5/(int)updates + 1;
   }
   if(explode_danger) explode_danger --;

   _state.total_enemies = 0;
   _state.total_wenemies = 0;
   _state.closest_e = NULL;
   _state.total_friends = 0;
   _state.closest_f = NULL;

   _state.num_tbombers = 0;
   _state.num_ttakers = 0;

   shmem_updmyloc(me->p_x, me->p_y);

   for(i=0, j= &players[i]; i< MAXPLAYER; i++,j++){
      p = &_state.players[j->p_no];

      switch(j->p_status){
	 case PFREE:
	    if(p->last_init){	/* any field not nulled below */
	       if(j->p_no == _state.controller-1){
		  _state.controller = 0;
		  locked = 0;
	       }
	       if(_state.last_defend && _state.last_defend->p &&
		  _state.last_defend->p->p_no == j->p_no)
		  _state.last_defend = NULL;

	       bzero(p, sizeof(Player));
	    }
	    continue;
	 case POUTFIT:
	    p->p = NULL;
	    p->dist = GWIDTH;
	    p->p_x = -GWIDTH;
	    p->p_y = -GWIDTH;
	 case PDEAD:
	 case PEXPLODE:
	    if(WAR(j)){
	       _state.total_enemies ++;
	    }
	    p->alive = 0;
	    p->plcarry = 0.0;
	    if(j->p_team == _state.warteam)
	       _state.total_wenemies ++;
	    continue;
      }
      if(j->p_team == _state.warteam)
	 _state.total_wenemies ++;

      if(j->p_status == PEXPLODE && j->p_explode == 0){
	 do_expdamage(j);
	 j->p_explode ++;
      }

      /*
      if(j == me) continue;
      */

      if((j!= me) && WAR(j)){
	 _state.total_enemies ++;
	 p->enemy = 1;
      }
      else {
	 _state.total_friends ++;
	 p->enemy = 0;
	 shmem_rloc(j->p_no, &j->p_x, &j->p_y);
      }

      if(!p->alive){
	 /* reset all attributes */
	 p->p = j;
	 init_sstats(p);
      }
      p->alive ++;

      p->p = j;
      if(j->p_x < 0 || j->p_y < 0){
	 /* very little information available */
         if ( ! p->invisible ) {
	    p->invisible = 1;
	    /*
	    p->last_init = _udcounter - 1;
	    p->dist = GWIDTH;
	    continue;
            */
	 }
      }
      else
	 p->invisible = 0;

#ifdef NO_PFTRACT	/* if server doesn't give tractor info */
      /* tractor_check resets or sets this flag */
      if(p->pp_flags & PFTRACT){
	 p->pp_flags = j->p_flags;
	 p->pp_flags |= PFTRACT;
      }
#endif /* NO_PFTRACT */
      p->robot = j->p_flags & PFROBOT;
      if(p->robot)
	 p->fuel = j->p_ship.s_maxfuel;

      if(j != me){
	 orbit_check(p, j);
	 army_check1(p, j);
	 army_check2(p, j);
         if (hm_cr)
            army_check3(p, j);
      }

      if(p->invisible) {
         p->closest_pl = p->closest_pl; /* keep stale info JKH */
      } else {
	  p->closest_pl = closest_planet(j, &pldist, p->closest_pl);
	  p->closest_pl_dist = pldist;
      }

      if(j->p_flags & PFBOMB)
	 p->bombing = _udcounter;

      /* the old information is in p not j */
      if(p->invisible)
	 d = GWIDTH;
      else
	 d = edist_to_me(j);

      p->lastdist = p->dist;
      p->dist = d;
      p->hispr = PHRANGE(p);

      if(p->invisible){
	 p->crs = get_wrapcourse(p->p_x, p->p_y);
	 p->icrs = p->crs;	/* xxx */
      }
      else{
	 p->crs = get_wrapcourse(j->p_x, j->p_y);
	 get_intercept_course(p, j, p->dist, &p->icrs);
      }

      if(p->enemy && d < mce){
	 ce = p;
	 mce = d;

	 if(!ignoring_e(p)){
	    ct = p;
	    mct = d;
	 }
      }
      if(p->enemy && d < mct && !ignoring_e(p)){
	 ct = p;
	 mct = d;
      }
      if(!p->enemy && p->p != me && d < mcf){
	 cf = p;
	 mcf = d;
      }

      if(j != me)
	 cloak_check(p, j);

      if(_udcounter - p->last_init > 450){	/* roughly 45 seconds */
	 init_sstats(p);
      }
      else if(_udcounter - p->last_init == 1){
	 /* common case for close enemies */
	 update_sstats(p, 100);
      }
      else
	 /* should be actual time in ms, I think */
	 update_sstats(p, 100 * (_udcounter - p->last_init));
      
      if(p->enemy && !(j->p_flags & PFCLOAK)){
	 p->thittime = 0; p->phittime = 0;
	 p->thit = p->phit = 0;
      }

      if(!p->enemy){
	 /* what's he doing */
	 if(_udcounter - p->bombing < 3*60*10)
	    _state.num_tbombers++;
      }

      p->last_init = _udcounter;
	 
#ifdef nodef
      if(DEBUG & DEBUG_ENEMY)
	 print_player(p);
#endif

      /* don't update unless it's a value number */
      if ( ! p->invisible ) {
      p->p_x = j->p_x;	/* last x */
      p->p_y = j->p_y;	/* last y */
      }
   }

   _state.closest_e		= ce;
   _state.closest_f		= cf;

   if(ce)
      tractor_check(ce, ce->p);

   nt = get_target(ct);
   if(nt != _state.current_target){
      if(sync_check(&nt)){
	 _state.current_target = nt;
	 shmem_updmytarg(nt->p->p_no);
	 sendOggVPacket();
      }
   }

#ifdef nodef
   check_server_response(&predictx, &predicty, nint(_serverdelay), NULL, NULL);

   rx = ABS(me->p_x - predictx);
   ry = ABS(me->p_y - predicty);
   /*
   printf("(sd: %lg) diff: (%d,%d)\n", _serverdelay, rx,ry);
   */
   printf("server (s %d, d %d), local (s %d, d %d), diff (s %d, d %d)\n",
      _state.sp_subspeed, _state.sp_subdir,
      _state.p_subspeed, _state.p_subdir,
      _state.sp_subspeed - _state.p_subspeed,
      _state.sp_subdir - _state.p_subspeed);
#endif

   /* update my information */
   _state.last_x	= me->p_x;
   _state.last_y	= me->p_y;
   _state.last_speed	= me->p_speed;
   _state.last_desspeed	= _state.p_desspeed;
   _state.last_dir	= me->p_dir;
   _state.last_desdir	= _state.p_desdir;
   _state.last_subspeed = _state.p_subspeed;
   _state.last_subdir   = _state.p_subdir;

   last_udcounter	= _udcounter;

   update_player_density();

   check_active_enemies();
}

check_active_enemies()
{
   static timer;

   if(_state.total_enemies == 0){
      if(_state.player_type == PT_OGGER || _state.player_type == PT_DOGFIGHTER){
	 timer ++;
	 if(timer == 100){
	    mfprintf(stderr, "nobody to fight\n");
	    exitRobot(0);
	 }
      }
      if(_state.timer_delay_ms < 10.0){
	 mprintf("No enemies: setting 1 update per second\n");
	 sendUpdatePacket(999999);
	 _state.timer_delay_ms = 10.0;
      }
   }
   else {
      timer = 0;
   
      if(_state.timer_delay_ms > updates){
	 _state.timer_delay_ms = updates;
	 mprintf("Enemies: setting %d updates per second\n",
		(int)(1000000./(updates * 100000.)));
	 sendUpdatePacket((int)(updates * 100000.));
      }
   }
}

orbit_check(p, j)

   Player		*p;
   struct player	*j;
{



/* begin -- only if PFORBIT not given by server */
#ifdef NO_PFORBIT
   /* assume nothing has changed. */
   if(j->p_speed == 0 && (j->p_flags & PFORBIT)){
      if (p->invisible) /* keep old x & y coordinates */
         return;
      p->p_x = j->p_x;
      p->p_y = j->p_y;
      return;
   }

   j->p_flags &= ~PFORBIT;

   /* Assume the worst if player is invisible JKH */
   if(p->invisible) {
      if ( p->closest_pl ){ /* grab stale info */
         j->p_planet = p->closest_pl->pl_no;
      } else {
         j->p_planet = -1;
      }
      if(j->p_planet > -1){
         j->p_flags |= PFORBIT;
         p->pl_armies = planets[j->p_planet].pl_armies;
         if(_state.no_speed_given)
            j->p_speed = 0;
         if(DEBUG & DEBUG_ENEMY){
            printf("%s invisible orbiting %s\n", j->p_mapchars,
                   planets[j->p_planet].pl_name);
         }
      }
      return;
   }

   /* Less info is given about cloaked players too
      so we need to be pessimistic also JKH */
   if (j->p_flags & PFCLOAK) {
      if ( p->closest_pl && p->closest_pl_dist < 3000 ){
         j->p_planet = p->closest_pl->pl_no;
      } else {
         j->p_planet = -1;
      }            
      if(j->p_planet > -1){
         j->p_flags |= PFORBIT;
         p->pl_armies = planets[j->p_planet].pl_armies;
         if(_state.no_speed_given)
            j->p_speed = 0;
         if(DEBUG & DEBUG_ENEMY){
            printf("%s Cloaked orbiting %s\n", j->p_mapchars,
                   planets[j->p_planet].pl_name);
         }
      }
      return;
   }

   if(j->p_speed == 0 && !_state.no_speed_given){
      if(j->p_x != p->p_x || j->p_y != p->p_y){
	 /* NOTE: could also be tractor */
	 j->p_planet = planet_from_ppos(j);
	 if(j->p_planet > -1){
	    j->p_flags |= PFORBIT;
	    p->pl_armies = planets[j->p_planet].pl_armies;
	    if(_state.no_speed_given)
	       j->p_speed = 0;
	    if(DEBUG & DEBUG_ENEMY){
	       printf("%s orbiting no PFORBIT %s\n", j->p_mapchars,
		  planets[j->p_planet].pl_name);
	    }
	 }
      }
   }
#endif
/* end -- only if PFORBIT not given by server */

   if(j->p_flags & PFORBIT){
      j->p_planet = planet_from_ppos(j);
      if(j->p_planet > -1){
	 if(!p->pl_armies){
	    p->pl_armies = planets[j->p_planet].pl_armies;
	 }
	 if(_state.no_speed_given) j->p_speed = 0;
	 if(DEBUG & DEBUG_ENEMY){
	    printf("%s orbiting %s with PFORBIT\n", j->p_mapchars, 
                   planets[j->p_planet].pl_name);
	 }
      }
      else
	 p->pl_armies = 0;
   }
   else
      p->pl_armies = 0;
}

army_check1(p, j)

   Player		*p;
   struct player	*j;
{
   struct planet	*pl;

   if(!(j->p_flags & PFBEAMUP) && !(j->p_flags & PFBEAMDOWN)) return;

   /* don't track robot carriers if told not to JKH */
   if ( robdc && !NotRobot(p,j) ) { 
      p->armies=0;
      p->plcarry=0.0;
      return;
   }

   if(j->p_flags & PFBEAMUP){
      if(_udcounter - p->beam_fuse >= 8){
	 p->armies ++;
	 p->plcarry = 100.;
	 p->beam_fuse = _udcounter;
	 if(DEBUG & DEBUG_ENEMY)
	    printf("%s beamed up 1\n", j->p_mapchars);
      }
   }
   if(j->p_flags & PFBEAMDOWN){
      if(_udcounter - p->beam_fuse >= 8){
	 p->armies --;
	 p->beam_fuse = _udcounter;
	 if(DEBUG & DEBUG_ENEMY)
	    printf("%s beamed down 1\n", j->p_mapchars);
      }
   }
   if((j->p_flags & PFORBIT) && j->p_planet > -1){
      pl = &planets[j->p_planet];
      if((j->p_swar | j->p_hostile) & pl->pl_owner)
	 if(j->p_flags & PFBEAMDOWN)
	    p->taking = _udcounter;
   }

   if(p->armies < 0) 
      p->armies = 0;
   if(p->armies > troop_capacity(j))
      p->armies = troop_capacity(j);

}

army_check2(p, j)

   Player		*p;
   struct player	*j;
{
   struct planet	*pl;
   bool			ohostile = 0, opl = 0;
   int			pa;
   float                ptrate; /* planet taking rate */
   static int count=0;

   if(!(j->p_flags & PFORBIT)) return;

   /*
   if(!(j->p_flags & PFBEAMUP) && !(j->p_flags & PFBEAMDOWN)) return;
   */

   if(troop_capacity(j) == 0) return;

   /* don't track robot carriers if told not to JKH */
   if ( robdc && !NotRobot(p,j) ) { 
      p->armies=0;
      p->plcarry=0.0;
      return;
   }

   pl = &planets[j->p_planet];
   if((j->p_swar | j->p_hostile) & pl->pl_owner)
      ohostile = 1;
   if(j->p_team == pl->pl_owner)
      opl = 1;
   
   if(!opl && !ohostile) return;

   if(p->pl_armies == pl->pl_armies) return;

   /* store old army count in pa */
   /* update army count in player based on planet */
   pa = p->pl_armies;
   p->pl_armies = pl->pl_armies;

   if(pa > pl->pl_armies){
      if(ohostile){
         if ( pa <= 4) { /* bombing otherwise */
            p->armies -= (pa - pl->pl_armies);
            if(DEBUG & DEBUG_ENEMY) {
               printf("%s(%s) beaming DOWN %d armies to hostile %s\n",
                      j->p_name, j->p_mapchars, (pa - pl->pl_armies),
                      pl->pl_name);
               printf("%s(%s) has %d armies.\n",
                      j->p_name, j->p_mapchars, p->armies);
            }
         }
      } else {
         /* beaming up armies */
         /* or someone is bombing under the guy */
	 p->armies += (pa - pl->pl_armies);
	 if(DEBUG & DEBUG_ENEMY) {
            printf("%s(%s) beaming UP %d armies from peaceful %s\n",
                   j->p_name, j->p_mapchars, (pa - pl->pl_armies),
                   pl->pl_name);
            printf("%s(%s) has %d armies.\n",
                   j->p_name, j->p_mapchars, p->armies);
         }
      }
   } else {
      if(ohostile) 
	 return; /* planet is popping */
      else{
         /* either planet is popping or player is */
         /* ferrying armies JKH */
         /* as the planet can pop, let's be pessismistic */
         /* and just assume it popped */ 

	 /* p->armies -= (pl->pl_armies - pa);  */
	 if(DEBUG & DEBUG_ENEMY) {
            printf("%s(%s) beaming DOWN %d armies to peaceful %s\n",
                   j->p_name, j->p_mapchars, (pl->pl_armies - pa),
                   pl->pl_name);
            printf("%s(%s) has %d armies.\n",
                   j->p_name, j->p_mapchars, p->armies);
         }
      }
   }
   if(p->armies < 0) p->armies = 0;
   if(p->armies > troop_capacity(j))
      p->armies = troop_capacity(j);


}


/* A more intuitive way of figuring out who carries in 
   a pickup game. Only works with humans */
army_check3(p, j)

   Player		*p;
   struct player	*j;
{
   float                ptrate; /* planet taking rate */

   /* a human alive for long time with enough kills to drop */
   /* just assume he carries as he can trick a bot */
   if ( j->p_kills == 0 ) /* reset the fuse on death */
      p->killfuse = 0;
   
   if ( j->p_kills >= 0.51 ) /* increment the fuse tunable JKH */
      p->killfuse = p->killfuse + j->p_kills;

   if ( NotRobot(p,j) ) { 
      if (p->killfuse >= 3000) { /* about 5 min with 1 kill */
         p->armies = troop_capacity(j);
         p->plcarry = 100.0;
      }
   }

   /* > 1.5 kill & high planet rating on player database */
   if ( j->p_kills >= 1.5 ) {
      ptrate = (j->p_stats.st_tplanets * 100000) / j->p_stats.st_tticks;
      if ( ptrate > 7.0 ) { /* about a planet rating of 4 */
         if ( NotRobot(p,j) ) {
            p->armies = troop_capacity(j);
            p->plcarry = 100.0;
         }
      }
   }

}

NotRobot(p, j)

   Player		*p;
   struct player	*j;
{

    if ( (strcmp(j->p_login,"robot!") != 0) || !(j->p_flags&PFBPROBOT) ) { 
       return 1;
    } else {
       return 0;
    }

}

calc_speed(p, j)
   
   Player		*p;
   struct player	*j;
{
   double	x_speed, y_speed;
   int		s;

   if(!on_screen(p)) return 0;

   x_speed = ((double)(j->p_x - p->p_x))/(Cos[j->p_dir] * WARP1);
   y_speed = ((double)(j->p_y - p->p_y))/(Sin[j->p_dir] * WARP1);

   if(j->p_x == p->p_x) 
      s = nint(y_speed);
   else if(j->p_y == p->p_y) 
      s = nint(x_speed);
   else
      s = nint(sqrt(x_speed * x_speed + y_speed * y_speed));
   
   /* fixes */
   if(s > j->p_speed+1) s /= 2;
   if(s > j->p_ship.s_maxspeed) s /= 2;

   /* make sure speed hasn't increased/decreased by more than 1 */
   if(s > j->p_speed + 1) 
      s = j->p_speed + 1;
   else if(s < j->p_speed-1)
      s = j->p_speed - 1;
   
   if(s > j->p_ship.s_maxspeed) s = j->p_ship.s_maxspeed;
   if(s < 0) s = 0;

   return s;
}

cloak_check(p, j)

   Player		*p;
   struct player	*j;
{
   int		dist, ns;
   /* speed & direction is same as when started cloaking unless phaser
      hit */
   
   if(p->invisible) return;

   p->pp_flags &= ~PFCLOAK;

   if((j->p_flags & PFCLOAK) && j->p_cloakphase ++ < CLOAK_PHASES - 1)
      return;

   /* not cloaked after phit, for all practical purposes */
   if( (j->p_flags & PFCLOAK) && (_udcounter - p->phittime < 12))
      p->pp_flags &= ~PFCLOAK;
   else if(j->p_flags & PFCLOAK)
      p->pp_flags |= PFCLOAK;

   if(!(j->p_flags & PFCLOAK) || !(p->pp_flags & PFCLOAK))
      return;

   if(shmem && (ABS(mtime(0) - j->p_lcloakupd) < 100)){
      if(shmem_cloakd(j)){
	 p->pp_flags &= ~PFCLOAK;
	 return;
      }
   }

   if((j->p_flags & PFORBIT) || (j->p_flags & PFBOMB)){
      j->p_speed = 0;
      /* force fire all over planet */
      j->p_dir = (unsigned char) (RANDOM() % 256);
      return;
   }
   
#ifdef nodef
   if(p->p_x == j->p_x && p->p_y == j->p_y)
      /* not updated */
      return;
#endif

   if(_state.human){
      /* randomize locations a little more */
      if(_state.human < 11){
	 j->p_x = j->p_x + rrnd(_state.human * 1500);
	 j->p_y = j->p_y + rrnd(_state.human * 1500);
      }
      else {
	 p->invisible = 1;
	 return;
      }
   }

   j->p_x += (double) (j->p_speed * WARP1) * Cos[j->p_dir];
   j->p_y += (double) (j->p_speed * WARP1) * Sin[j->p_dir];
   
   j->p_dir = get_awrapcourse(p->p_x, p->p_y, j->p_x, j->p_y);
   if(_state.wrap_around)
      dist = wrap_dist(j->p_x, j->p_y, p->p_x, p->p_y);
   else
      dist = (int)hypot((double)(j->p_x-p->p_x), (double)(j->p_y-p->p_y));

   ns = dist/(10*WARP1) - 5;

   if(ns > j->p_speed + 1) 
      ns = j->p_speed + 1;
   else if(ns < j->p_speed - 1)
      ns = j->p_speed - 1;

   if(ns > j->p_ship.s_maxspeed)
      ns = j->p_ship.s_maxspeed;
   if(ns < 0)
      ns = 0;
   
   j->p_speed = ns;

   /*
   printf("speed: %d, direction: %d\n", j->p_speed, j->p_dir);
   */
}

init_sstats(p)

   Player	*p;
{
   register struct player	*j = p->p;

   /*
   printf("initing %d\n", j?j->p_no:-1);
   */

   p->fuel 	= j->p_ship.s_maxfuel;
   p->shield	= j->p_ship.s_maxshield;
   p->damage	= 0;
   p->wpntemp	= 0;

   p->last_init = _udcounter;
   p->run_t = 0;
   p->phit = p->thit = 0;
   p->thittime = p->phittime = 0;
   p->phit_me = p->phittime_me = 0;

   p->armies 	= 0;
   p->pl_armies	= 0;
}

update_sstats(e, ct)

   Player	*e;
   int		ct;
{
   register struct player	*j = e->p;

   e->fuel -= subfuel(e->p, ct);
   if(e->fuel < 0) e->fuel = 0;

   e->fuel += addfuel(e->p, ct);
   if(e->fuel > j->p_ship.s_maxfuel)
      e->fuel = j->p_ship.s_maxfuel;
   if(e->shield < j->p_ship.s_maxshield)
      repair_shield(j, e, ct);
   if(e->damage > 0)
      repair_damage(j, e, ct);
   
   /* if player dies after we die */
   if(e->damage > (e->p->p_ship.s_maxdamage + 50)){
      e->damage = e->subdamage = 0;
   }

   e->wpntemp -= SH_COOLRATE(e->p);
   if(e->wpntemp < 0) e->wpntemp = 0;
}

addfuel(k, cycletime)

   struct player	*k;
   int			cycletime;
{
   if((k->p_flags & PFORBIT) && fuel_planet(k->p_planet)){
      return (cycletime * 8 * SH_RECHARGE(k))/100;
   }
   else if(k->p_flags & PFDOCK)
      /* could eventually deduct sb fuel here also */
      return (cycletime * 12 * SH_RECHARGE(k))/100;
   else
      return (cycletime * 2 * SH_RECHARGE(k))/100;
}

/* xx -- put in util.c */
fuel_planet(n)

   int	n;
{
   struct planet	*pl = &planets[n];
   if(unknownpl(pl)) return 1;	/* benefit of the doubt */

   return (pl->pl_flags & PLFUEL);
}

repair_planet(n)

   int	n;
{
   struct planet	*pl = &planets[n];
   if(unknownpl(pl)) return 1;	/* benefit of the doubt */

   return (pl->pl_flags & PLREPAIR);
}

subfuel(k, cycletime)
   
   struct player	*k;
   int			cycletime;
{
   int	dfuel;

   dfuel =  (cycletime * SH_WARPCOST(k) * k->p_speed)/100;
   
   if(k->p_flags & PFSHIELD){
      switch(k->p_ship.s_type){
	 case SCOUT:
	    dfuel += (cycletime * 2)/100;
	    break;
	 case STARBASE:
	    dfuel += (cycletime * 6)/100;
	 default:
	    dfuel += (cycletime * 3)/100;
      }
   }
   if(k->p_flags & PFCLOAK)
      dfuel += (cycletime * SH_CLOAKCOST(k))/100;
   
   if(k->p_flags & PFTRACT)
      dfuel += TRACTCOST;

   return dfuel;
}

repair_shield(k,e, cycletime)

   struct player	*k;
   Player		*e;
   int			cycletime;
{
   if(k->p_speed == 0 && !(k->p_flags & PFSHIELD)){
      /* assume he's repairing */
      e->subshield += (cycletime * SH_REPAIR(k) * 4)/100;
      if((k->p_flags & PFORBIT) && repair_planet(k->p_planet)){
	 e->subshield += (cycletime * SH_REPAIR(k) * 4)/100;
      }
      if(k->p_flags & PFDOCK)
	 e->subshield += (cycletime * SH_REPAIR(k) * 6)/100;
   }
   else
      e->subshield += (cycletime * SH_REPAIR(k) * 2)/100;
   if(e->subshield / 1000){
      e->shield += e->subshield / 1000;
      e->subshield %= 1000;
   }
   if(e->shield > k->p_ship.s_maxshield){
      e->shield = k->p_ship.s_maxshield;
      e->subshield = 0;
   }
}

repair_damage(k, e, cycletime)

   struct player	*k;
   Player		*e;
   int			cycletime;
{
   if(!(k->p_flags & PFSHIELD)){
      if(k->p_speed == 0){	/* assume repairing */
	 e->subdamage += (cycletime * SH_REPAIR(k)*2)/100;
	 if((k->p_flags & PFORBIT) && repair_planet(k->p_planet))
	    e->subdamage += (cycletime * SH_REPAIR(k)*2)/100;
	 if(k->p_flags & PFDOCK)
	    e->subdamage += (cycletime * SH_REPAIR(k)*3)/100;
      }
      else
	 e->subdamage += (cycletime * SH_REPAIR(k))/100;
      if(e->subdamage / 1000) {
	 e->damage -= e->subdamage / 1000;
	 e->subdamage %= 1000;
      }
      if(e->damage < 0){
	 e->damage = 0;
	 e->subdamage = 0;
      }
   }
}

/* called from init_dodge */
add_damage(t, td, e)

   struct torp	*t;
   int		td;
   Player	*e;
{
   int			ddist;
   int			dx,dy;
   struct player	*j;
   int			dam = 0;

   j = e->p;

   if(e->enemy && (j->p_flags & PFCLOAK)){
      /* can't guarantee damage but close
	 on location */
      if(_udcounter - e->phittime > 10 && !edge_expl(t)){
	 /* this is more uptodate */
	 e->thit = 1;
	 e->thittime = _udcounter;
	 e->cx = t->t_x;
	 e->cy = t->t_y;
      }
   }

   dx = t->t_x - j->p_x;
   dy = t->t_y - j->p_y;

   if(ABS(dx) > DAMDIST || ABS(dy) > DAMDIST) return;

   ddist = dx*dx + dy*dy;

   if(ddist > DAMDIST * DAMDIST) return;

   if(ddist > EXPDIST * EXPDIST){
      dam = td * (DAMDIST - sqrt((double) ddist)) /
	 (DAMDIST - EXPDIST);
   }
   else 
      dam = td;

   if(j == me && dam > 0)
      hit_by_torp = 1;

   if(DEBUG & DEBUG_ENEMY)
      printf("torp dam to %s(%d): %d\n", j->p_name, j->p_no, dam);

   dodamage(e, j, dam);
}

dodamage(e, j, dam)

   Player		*e;
   struct player	*j;
   int			dam;
{
   /*
   if(e->p->p_ship.s_type == STARBASE){
      printf("dam before: %d, shields before: %d, dam: %d, max dam: %d\n",
	 e->damage, e->shield, dam, j->p_ship.s_maxdamage);
   }
   */
   if(dam > 0){
      if(j->p_flags & PFSHIELD){
	 e->shield -= dam;
	 if(e->shield < 0){
	    e->damage -= e->shield;
	    e->shield = 0;
	 }
      }
      else{
	 e->damage += dam;
	 if(e->damage > j->p_ship.s_maxdamage)
	    e->damage = j->p_ship.s_maxdamage;
      }
   }
}

/* sh just exploded */

do_expdamage(sh)

   struct player	*sh;
{
   register			i;
   int				dx,dy,dist;
   int				damage;
   register struct player	*j;
   register Player		*e;

   for(i=0, e=_state.players; i < MAXPLAYER; i++, e++){
      if(!e->p || (e->p->p_status != PALIVE) || (e->invisible))
	 continue;
      j = e->p;

      if (j == sh)
	 continue;

      dx = sh->p_x - j->p_x;
      dy = sh->p_y - j->p_y;
      if (ABS(dx) > SHIPDAMDIST || ABS(dy) > SHIPDAMDIST)
	 continue;
      dist = dx * dx + dy * dy;
      if (dist > SHIPDAMDIST * SHIPDAMDIST)
	 continue;
      if (dist > EXPDIST * EXPDIST) {
	 if (sh->p_ship.s_type == STARBASE)
	    damage = 200 * (SHIPDAMDIST - sqrt((double) dist)) /
	       (SHIPDAMDIST - EXPDIST);
	 else if (sh->p_ship.s_type == SCOUT)
	    damage = 75 * (SHIPDAMDIST - sqrt((double) dist)) /
	       (SHIPDAMDIST - EXPDIST);
	 else
	    damage = 100 * (SHIPDAMDIST - sqrt((double) dist)) /
	       (SHIPDAMDIST - EXPDIST);
      }
      else {
	 if (sh->p_ship.s_type == STARBASE)
	    damage = 200;
	 else if (sh->p_ship.s_type == SCOUT)
	    damage = 75;
	 else
	    damage = 100;
      }
      if (damage > 0){
	 dodamage(e, j, damage);
	 if(DEBUG & DEBUG_ENEMY)
	    printf("ship explode dam to %s(%d): %d\n", j->p_name, j->p_no, 
	       damage);
      }
   }
}

/* war & peace.  Done initially */

/* Times to declare war:

   1. When t-mode first starts or when t-mode ended and restarted(done)
   2. When you get forced onto another team due to timercide (done)
   3. When 3rd space scummer enters game (only happens before t-mode starts
      So it is covered on step 1.)
   4. When 3rd space scummers leave the game. (I have bots declare peace
      everytime after death, and so this should cover this.)

*/
int declare_intents(int peaceonly)
{
   register			i;
   register struct player	*j;
   int				teams[16], maxt=0, maxcount=0;
   int				maxt2=0, maxcount2=0;
   int				newhostile, pl=0;
   extern char			*team_to_string();

   if(me_p->alive < 100 || !isAlive(me))	/* 10 seconds into game */
      return 0;
   
   if(_state.player_type == PT_OGGER || _state.player_type == PT_DOGFIGHTER)
      return 0;

   newhostile = me->p_hostile;
   bzero(teams, sizeof(teams));

   for(i=0, j=players; i<MAXPLAYER; i++,j++){

      if(j->p_status == PFREE) continue;
      if(j->p_flags & PFROBOT) continue;
      if(me->p_team == j->p_team || j->p_team == 0 || j->p_team == ALLTEAM)
	 continue;
      teams[j->p_team]++;
      pl++;
   }

   if (!pl) return 0;

   if(pl){
      for(i=1; i< 9; i *= 2) {
	 if(teams[i] > maxcount) {
	    maxt = i;
	    maxcount = teams[i];
	 }
      }
      mprintf("max team = %s\n", team_to_string(maxt));
      for(i=1; i< 9; i *= 2) {
	 if( (teams[i] > maxcount2) && (i != maxt) ) {
	    maxt2 = i;
	    maxcount2 = teams[i];
	 }
      }
      mprintf("max team2 = %s\n", team_to_string(maxt2));
   }

   /* peace */
   for(i=1; i< 9; i *= 2){
      if(i == maxt || i == maxt2) continue;
      if((i & me->p_hostile) && !(i & me->p_swar)){
	 /* is hostile to and not at war with */
	 mprintf("declaring peace with %s\n", team_to_string(i));
	 newhostile ^= i;
      }
   }

   if (peaceonly) {
      if(newhostile != me->p_hostile)
	 sendWarReq(newhostile);
      return 1;
   }

   _state.warteam = maxt;
   
   /* war */
   if(maxt != 0) {
      if(!((maxt & me->p_swar) && !(maxt & me->p_hostile))) {
	 mprintf("declare war with %s\n", team_to_string(maxt));
	 newhostile |= maxt;
      }
   }

   /* Being hostile to more teams is better than less teams. */
   if(maxt2 != 0) {
      if(!((maxt2 & me->p_swar) && !(maxt2 & me->p_hostile))) {
	 mprintf("declare war with %s\n", team_to_string(maxt2));
	 newhostile |= maxt2;
      }
   }

   if(newhostile != me->p_hostile)
      sendWarReq(newhostile);

   return 2;
}

/* DEBUG*/

print_player(e)

   Player	*e;
{
   printf("name: \"%s\"(%d)\n", e->p->p_name, e->p->p_no);
   printf("%s\n", e->enemy?"enemy":"friend");
   printf("last_init %d, (current %d)\n", e->last_init, _udcounter);
   printf("robot: %s\n", e->robot?"TRUE":"FALSE");
   printf("invisible: %s\n", e->invisible?"TRUE":"FALSE");
   printf("fuel: %d%%\n", PL_FUEL(e));
   printf("shield: %d%%\n", PL_SHIELD(e));
   printf("damage: %d%%\n", PL_DAMAGE(e));
}


Player *get_target(closest_target)
   
   Player	*closest_target;

{
   Player	*l;
   Player	*getany(), *getabase();
   int	dummy;

   if(locked && _state.controller > 0){
      l = &_state.players[_state.controller-1];
      if(!l || !l->p || !WAR(l->p)){
	 locked = 0;
	 _state.controller = 0;
      }
      else
	 return l;
   }

   if(_state.player_type == PT_OGGER)
      closest_target =  getabase();

   if(!closest_target) {
      /*
      _state.ogg_req = 0;
      */
      return getany();
   }

   if(_state.ogg_req){
      if(!_state.current_target){
	 if(DEBUG & DEBUG_OGG)
	    printf("ogg: no target, resetting\n");
	 /*
	 _state.ogg_req = 0;
	 */
      }
      else
	 return _state.current_target;
   }

   if(_state.protect && closest_target->dist > closest_target->hispr-500){
      if(hostilepl(_state.protect_planet)){
	 unprotectp_c("hostile planet");
	 if(DEBUG & DEBUG_PROTECT){
	    printf("hostile planet -- unprotecting.\n");
	 }
	 return closest_target;
      }
      return get_nearest_to_pl(_state.protect_planet);
   }
   else if(_state.defend && closest_target->dist > closest_target->hispr-500){
      if(!_state.protect_player->p || !isAlive(_state.protect_player->p)){

	 _state.defend = 0;
	 _state.protect_player = NULL;
	 if(DEBUG & DEBUG_PROTECT){
	    printf("protect player dead.  No longer defending.\n");
	 }
	 return closest_target;

      }
      return get_nearest_to_p(_state.protect_player, &dummy);
   }
   else if(!closest_target){
      return getany();
   }
   else
      return closest_target;
}

process_general_message(message, flags, from, to)
   
   char			*message;
   unsigned char	flags,from,to;
{
   Player		*p;
   if(from == 255) {
      /* god */
      switch(flags & (MTEAM|MINDIV|MALL)){
	 case MTEAM:
	    break;
	 case MINDIV:
	    break;
	 default:
	    break;
      }
      return;
   }
   if(/* from < 0 ||*/ from >= MAXPLAYER){
      mfprintf(stderr, "process_general_message: unknown player %d\n", from);
      return;
   }
   p = &_state.players[from];
   switch(flags & (MTEAM|MINDIV|MALL)){
      case MTEAM:
	 parse_message(&message[10], p, MTEAM);
	 break;
      case MINDIV:
	 parse_message(&message[10], p, MINDIV);
	 break;
      default:
	 break;
   }
}

tractor_check(e, j)
   
   Player		*e;
   struct player	*j;
{

   int			predictx, predicty, 
			predict_trx, predict_try, edist;
   register		currx, curry, rtx, rty, rx, ry;
   int			trx, try;


#ifndef NO_PFTRACT
   e->pp_flags &= ~PFTRACT;
   e->pp_flags &= ~PFPRESS;

   if(j->p_flags & PFTRACT){
      e->pp_flags |= PFTRACT;
      printf("enemy tractor\n");
   }
   if(j->p_flags & PFPRESS)
      e->pp_flags |= PFPRESS;

#else 		/* if server doesn't give PFTRACT/PFPRESS */
   /*
      printf("cycles: (%g, %d)\n", _serverdelay, nint(_serverdelay));
      printf("Last loc (%d,%d), last speed (%d), last dir (%d)\n",
	 _state.last_x, _state.last_y, _state.last_speed, _state.last_dir);
      printf("Present loc (%d,%d), present speed (%d), present dir (%d)\n",
	 me->p_x, me->p_y, me->p_speed, me->p_dir);
      printf("Predicted loc (%d,%d)\n", predictx, predicty);
      printf("%d, %d\n", rx,ry);
      printf("\n");
   */

   if((double)e->dist < ((double)TRACTDIST)*SH_TRACTRNG(j)){

      int delt = _udcounter - last_udcounter;

      if(delt <= 0) delt = 1;

      /* without him tractoring */
      check_server_response(&predictx, &predicty, delt, NULL, NULL);

      /* with him tractoring */
      check_server_response(&predict_trx, &predict_try, delt, e, j);
      
      /* difference if tractor */
      rtx = ABS(me->p_x - predict_trx);
      rty = ABS(me->p_y - predict_try);

      /* difference if no tractor -- ideally 0 */
      rx = ABS(me->p_x - predictx);
      ry = ABS(me->p_y - predicty);

      if(rtx < rx/2 && rty < ry/2){

	 if(DEBUG & DEBUG_WARFARE){
	    printf("tractor from %s\n", j->p_name);
	 }
	 /*
	 printf("tractor from %s\n", j->p_name);
	 printf("rtx: %d, rty: %d, rx: %d, ry: %d\n",
	    rtx, rty, rx, ry);
	 */
	 e->pp_flags |= PFTRACT;
      }
      else{
	 e->pp_flags &= ~PFTRACT;
      }
   }
   else{
      e->pp_flags &= ~PFTRACT;
   }
#endif /* NO_PFTRACT */
}

update_player_density()
{
   register			i,j;
   register Player		*p,*e;
   register struct player	*fr, *en;
   int				st;
   unsigned char		crs;
   int				dist;

   if(DEBUG & DEBUG_TIME){
      st = mtime(0);
   }

   for(i=0, p= _state.players; i < MAXPLAYER; i++, p++){
      if(!p->p) continue;
      bzero(p->attacking, sizeof(p->attacking));
      if(p->p == me)
	 bzero(_state.attacking_newdir, sizeof(_state.attacking_newdir));
      for(j=0; j< MAXPLAYER; j++)
	 p->distances[j] = INT_MAX;
   }

   for(i=0, p= _state.players; i < MAXPLAYER; i++, p++){
      
      if(!p->p || !isAlive(p->p)) continue;
      
      if(!p->enemy){
	 
	 fr = p->p;
	 
	 for(j=0, e=_state.players; j < MAXPLAYER; j++,e++) {
	    
	    if(!e->p) continue;
	    if(!e->enemy) continue;

	    en = e->p;

	    /* enemy course to friend */
	    crs = get_awrapcourse(fr->p_x, fr->p_y, en->p_x, en->p_y);
	    dist = pdist_to_p(fr, en);
	    /* note: independent of distance */
	    p->distances[j] = dist;
	       
	       /*
	       printf("%s close to %s\n", fr->p_name, en->p_name);
	       printf("ecrs %d, frcrs %d, line %d\n",
		  en->p_dir, fr->p_dir, crs);
	       */

	    /* this information won't be useful until close however */

	    if(attacking(e, (unsigned char)(crs-128), 32))
	       e->attacking[i] = p;

	    if(attacking(p, crs, 32))
	       p->attacking[j] = e;

	    if(p->p == me){
	       unsigned char odir = p->p->p_dir;
	       p->p->p_dir = _state.newdir;
	       if(attacking(p, crs, 32))
		  _state.attacking_newdir[j] = e;
	       p->p->p_dir = odir;
	    }

#ifdef nodef
	    /* debug */
	    if(p->p == me && p->attacking[j] && dist < 6000){
	       printf("dens: me attack %s (mycrs: %d), (hiscrs: %d)\n",
		  en->p_mapchars, en->p_no, me->p_dir, en->p_dir);
	    }
	    else if(p->p == me && dist < 6000){
	       printf("dens: NOT attacking %s\n", en->p_mapchars);
	    }
#endif
	 }
      }
   }

   calc_attackdiff();

   if(DEBUG & DEBUG_TIME){
      printf("update_player_density() took %dms\n", mtime(0)-st);
   }

   /* DEBUG */

#ifdef nodef
   for(i=0, p = _state.players; i < MAXPLAYER; i++, p++){
      
      if(!p->p) continue;

      printf("%s attacking: ", p->p->p_name);

      for(j=0; j< MAXPLAYER; j++){
	 
	 if(p->attacking[j])
	    printf("%s, ", p->attacking[j]->p->p_name);
      }
      printf("\n");
   }
#endif
}

calc_attackdiff()
{
   register		i,j;
   register Player	*p;
   Player		*pe[MAXPLAYER];
   int	    		my_pno = me->p_no;
   int			at = 0, df = 0;

   /* find enemies */
   for(i=0, p= _state.players; i < MAXPLAYER; i++, p++){

      pe[i] = NULL;

      if(!p->p || !isAlive(p->p) || !p->enemy) continue;

      /* int     p->distances[]  distance from each player
	 Player *p->attacking[]  'p' attacking player. */
      
      if(p->dist < _state.attack_diff_dist){
	 if(p->attacking[my_pno] && PL_FUEL(p) > 10){
	    at ++;
	    pe[i] = p;
	 }
      }
   }
   /* check friends */
   for(i=0, p= _state.players; i < MAXPLAYER; i++, p++){

      if(!p->p || !isAlive(p->p) || p->enemy) continue;

      if(p->dist < _state.attack_diff_dist && PL_FUEL(p) > 20){
	 for(j=0; j< MAXPLAYER; j++){
	    if(pe[j]){
	       /* attacking one of my enemies */
	       if(p->attacking[j]){
		  df ++;
		  break;
	       }
	    }
	 }
      }
   }
   _state.attackers = at;
   _state.defenders = df;
   _state.attack_diff = at - df;

#ifdef nodef
   /* tmp */
   printf("att %d, def %d\n", at, df);
#endif
}

/* SSS: speed critical */
struct planet	*closest_planet(j, dist, opl)

   struct player	*j;
   int			*dist;
   struct planet	*opl;

{
   register			k;
   register struct planet	*pl, *rp = opl;
   register			d, mdist = INT_MAX;

   if(opl && (mdist = ihypot((double)(j->p_x - opl->pl_x), 
			     (double)(j->p_y - opl->pl_y))) < 3000){
      *dist = mdist;
      return opl;
   }

   for(k=0, pl=planets; k < MAXPLANETS; k++, pl++){

      d = ihypot((double)(j->p_x - pl->pl_x), (double)(j->p_y - pl->pl_y));
      if(d < mdist){
	 mdist = d;
	 rp = pl;
	 if(mdist < 3000)
	    break;
      }
   }
   /*
   printf("%s closest to %s\n", j->p_mapchars, rp?rp->pl_name:"(NULL)");
   */
   *dist = mdist;
   return rp;
}

Player *getany()
{
   register 		i;
   register Player	*p;
   struct planet	*hp, *team_planet();

   for(i=0, p = _state.players; i < MAXPLAYER; i++, p++){
      if(!p->p || !isAlive(p->p) || !p->enemy) continue;

      if(p->invisible){
	 hp = team_planet(p->p->p_team);
	 if(!hp)
	    continue;
	 p->p->p_x = hp->pl_x;
	 p->p->p_y = hp->pl_y;
#ifdef nodef
	 printf("choosing invisible player with hp %s (%d,%d)\n",
	    hp->pl_name, hp->pl_x, hp->pl_y);
#endif
	 p->invisible = 0;	/* xx */
      }
      return p;
   }
   return NULL;
}

Player *getabase()
{
   register 		i;
   register Player	*p;
   struct planet	*hp, *team_planet();
   int			sbs[MAXPLAYER], sbf = 0;

   for(i=0, p = _state.players; i < MAXPLAYER; i++, p++){
      if(!p->p || !isAlive(p->p) || !p->enemy) continue;
      if(p->p->p_ship.s_type != STARBASE) continue;

      sbs[sbf++] = i;

      if(p->invisible){
	 hp = team_planet(p->p->p_team);
	 if(!hp)
	    continue;
	 p->p->p_x = hp->pl_x;
	 p->p->p_y = hp->pl_y;
#ifdef nodef
	 printf("choosing invisible player with hp %s (%d,%d)\n",
	    hp->pl_name, hp->pl_x, hp->pl_y);
#endif
	 p->invisible = 0;	/* xx */
      }
   }
   if(!sbf)
      return NULL;
   else
      return &_state.players[ sbs[random()%sbf] ];
}

/* make sure edge didn't set off explosion */

edge_expl(t)

   struct torp	*t;
{
   register	i = t->t_x,
		j = t->t_y;
   
   return (!i || i == GWIDTH || !j || j == GWIDTH);
}

sync_check(t)
   
   Player	**t;
{
   Player	*targ = *t;
   Player	*him, *his_targ;
   int		ht;

   if(!targ || !targ->p) {
      *t = NULL;
      return 0;
   }
   if(!shmem) return 1;
   if(!_state.ogg_pno) return 1;

   check_sync_loop();

   ht = shmem_rtarg(_state.ogg_pno-1);
   if(ht >= 0){
      his_targ = &_players[ht];
      if(his_targ && his_targ->p){
	 *t = his_targ;
      }
      return 1;
   }
   return 1;
}

check_sync_loop()
{
   struct player	*j;
   char			buf[30];
   register		i = _state.ogg_pno-1, k=0, q;
   for(k=0; k< MAXPLAYER+1; k++){
      q = shmem_rsync(i);
      /*
      sprintf(buf, "%d synced with %d", i, q);
      response(buf);
      */
      i = q;
      if(q >= 0){
	 continue;
      }
      else
	 return;	/* no loop */
   }
   /* otherwise, loop */
   fprintf(stderr, "Sync loop detected.\n");
   response("sync loop detected");
   _state.ogg_pno = 0;
   shmem_updmysync(-1);
}
