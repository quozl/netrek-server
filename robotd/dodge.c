#include <stdio.h>
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

#define END		-1

static int 		_myspeed;
static unsigned char	_mydir;
static int		_subspeed, _subdir, _plhit;
static int		_cticks;
static int		_do_tract;
static int		_tr_timer = -1;

struct pos {
   int	po_x,po_y;
};

#define	TPHIT		0x01
#define	TPPLASMA	0x02
#define	TPSEEK		0x04

struct tpos {

   int			tp_x, tp_y;	/* intial start point */
   struct player	*tp_j;		/* owner */
   int			tp_fuse;	/* current fuse */
   char			tp_info;	/* TPHIT,TPPLASMA,TPSEEK */
   int			tp_dist;
   unsigned char	tp_dir, tp_ndir;
   struct pos		tp_fusepos[MAXFUSE+1];
};

struct tpos _etorps[MAXPLAYER * (MAXTORP+MAXPLASMA) + 1];

init_dodge()
{
   /* set max speed if damaged */
   set_maxspeed();
   /* check for nearby hostile planet */
   update_hplanets();

   /* init torpedoes */
   init_torps();
}

/*
 * init_torps()
 * Update enemy torps, friendly damage
 */

init_torps()
{
   int				i;
   register			k,l;
   struct player		*j;
   register struct torp		*t;
   register struct tpos		*tp;
   register struct pos		*fp;
   register int			tx,ty;
   register unsigned char	tdir;
   double			tdx,tdy,dx,dy;
   int				ts, tf, tdist;
   int				maxfuse, tdangerdist;
   int				tdi;
   struct plasmatorp		*pt;

   /* _state.tdanger_dist configurable. Adjusted here according to 
      speed */
   tdangerdist = (_state.tdanger_dist * me->p_speed - 1)/
      (myfight_speed() -1);
   if(tdangerdist < _state.tdanger_dist) tdangerdist = _state.tdanger_dist;

   /* if speed == 1 or 0 we push lookahead to MAXFUSE in order to determine
      immediately if torps are coming our way. */
   if(me->p_speed < 2 || _state.p_desspeed < 2)
      c_lookahead(1);
   /* -- note: MAXFUSE is special -- _state.lookahead is not configurable
      for that value. */
   else if(_state.lookahead == MAXFUSE)
      c_lookahead(0);
   
   _do_tract			= 0;
   
   _state.torp_danger		= 0;
   _state.det_torps		= 0;
   _state.det_damage		= 0;
   _state.pl_danger		= 0;
   _state.recharge_danger	= 0;

   /* planet danger -should be configurable */
   if(_state.hpldist <= PFIREDIST + 500)
      _state.pl_danger = 1;
   
   _state.maxfuse = maxfuse = 0;

   hit_by_torp = 0;
   defend_det_torps = 0;

   for(i=0, j=players, tp=_etorps; i < MAXPLAYER; i++, j++){
      
      if(!WAR(j)){
	 /* friendly torps */
	 init_ftorps(i, j);
	 /* With ping-pong plasma, a friendly player's plasma can become
	 hostile, if an enemy has bounced it. */
	 if(_state.torp_bounce)
	    goto check_plasma;
	 continue;
      }

      /* no torps */
      if(!j->p_ntorp) goto check_plasma;

      for(k=0, t= &torps[i*MAXTORP]; k < MAXTORP; k++, t++){
	 
	 ts = t->t_speed;	/* socket.c */
	 /*
	 printf("t_speed = %d\n", ts);
	 */
	 
	 if(t->t_status == TFREE)
	    continue;
	 if(t->t_status == TEXPLODE){
	    check_damage(1, t, SH_TORPDAMAGE(j));
	    t->t_status = TFREE;
	    j->p_ntorp --;
	    continue;
	 }

	 tx = t->t_x; ty = t->t_y;
	 dx = tx - me->p_x; dy = ty - me->p_y;

#ifdef nodef
	 /* ignore off screen torps (don't get information anyway) */
	 if(dx > SCALE*WINSIDE/2 || dx < -SCALE*WINSIDE/2 || 
	    dy > SCALE*WINSIDE/2 || dy < -SCALE*WINSIDE/2)
	    continue;
#endif
	 
	 tdist = (int)hypot(dx,dy);

	 if(tdist < tdangerdist)
	    _state.torp_danger = 1;
	 if(tdist < DETDIST+1000){
	    _state.det_torps ++;
	    _state.det_damage += SH_TORPDAMAGE(j) *
	       (DAMDIST - sqrt((double) tdist)) / (DAMDIST - EXPDIST);
	    if(_state.defend && _state.protect_player &&
	       isAlive(_state.protect_player->p)){	
		  /* can we det torps intended for defend? */
	       struct player     *dp = _state.protect_player->p;
	       unsigned char	 tcrs;
	       tcrs = get_acourse(dp->p_x, dp->p_y, tx,ty);
	       if(angdist(tcrs, t->t_dir) < 16){
		  defend_det_torps ++;
	       }
	    }
	 }

	 /* goto next torp if this one is already past or won't possibly
	    hit us */
	 tdir = get_acourse(me->p_x, me->p_y, tx, ty);
	 tdi = angdist(tdir, t->t_dir);
	 /* TEST -- was 92 */
	 if(tdi > 92)
	    continue;
#ifdef tmpNODEF
	 if((tdi > 48 && tdist > 5000) || tdi > 64)
	    continue;
	 else if(tdi < 8)	/* torp heading our way */
	    _state.recharge_danger = 1;
#endif
	 /* Experimental */
	 if(tdi < 8 && tdist < 7000)
	    _state.recharge_danger = 1;

	 tdx = (double) ts * Cos[t->t_dir] * WARP1;
	 tdy = (double) ts * Sin[t->t_dir] * WARP1;

#ifdef nodef
	 tx += rint(_avsdelay * tdx);
	 ty += rint(_avsdelay * tdy);
#endif

	 tp->tp_dist 	= tdist;
	 tp->tp_x 	= tx;
	 tp->tp_y	= ty;
	 tp->tp_j	= j;
	 tp->tp_dir	= t->t_dir;
	 tp->tp_info	= 0;
	 if(torp_seek_ship(j->p_ship.s_type))
	    tp->tp_info |= TPSEEK;

	 tf = SH_TORPFUSE(j);
	 fixfuse(tx, ty, &tf, j, tdx, tdy);
	 if(_state.human && tf > _state.lookahead) 
	    tf = _state.lookahead;
	 if(tf > maxfuse)
	    maxfuse = tf;

	 tp->tp_fuse	= tf;

	 /* now move torp ahead in space */

	 for(fp = tp->tp_fusepos, l=0; l<tf; l++,fp++){
	    
	    tx += tdx; ty += tdy;
	    fp->po_x = tx;
	    fp->po_y = ty;
	 }
	 fp->po_x = END;	/* just to make sure */
	 tp++;
      }
check_plasma:;
      /* plasma */
      pt = &plasmatorps[i];
      if(pt->pt_status == PTMOVE){
	 /* Skip non-hostile plasmas */
	 if( !(pt->pt_war & me->p_team) &&
	     !((me->p_hostile|me->p_swar) & pt->pt_team))
	    continue;
	 ts = SH_PLASMASPEED(j);
	 tx = pt->pt_x; ty = pt->pt_y;
	 dx = tx - me->p_x; dy = ty - me->p_y;

         if(dx > SCALE*WINSIDE/2 || dx < -SCALE*WINSIDE/2 ||
            dy > SCALE*WINSIDE/2 || dy < -SCALE*WINSIDE/2)
            continue;

         tdist = (int)hypot(dx,dy);
         if(tdist < tdangerdist)
            _state.ptorp_danger = 1;
         /* goto next torp if this one is already past or won't possibly
            hit us */
         tdir = get_acourse(me->p_x, me->p_y, tx, ty);
         tdi = angdist(tdir, pt->pt_dir);
         /* TEST -- was 92 */
         if(tdi > 92){
            continue;
	 }

         /* Experimental */
         if(tdi < 8 && tdist < 7000)
            _state.recharge_danger = 1;

	 tdx = (double) ts * Cos[pt->pt_dir] * WARP1;
	 tdy = (double) ts * Sin[pt->pt_dir] * WARP1;

         tp->tp_dist    = tdist;
         tp->tp_x       = tx;
         tp->tp_y       = ty;
         tp->tp_j       = j;
         tp->tp_dir     = pt->pt_dir;
	 tp->tp_info	= 0;
	 tp->tp_info	|= (TPPLASMA | TPSEEK);

         tf = j->p_ship.s_plasmafuse;
         fixfuse(tx, ty, &tf, j, tdx, tdy);
         if(_state.human && tf > _state.lookahead)
            tf = _state.lookahead;
         if(tf > maxfuse)
            maxfuse = tf;

         tp->tp_fuse    = tf;

         /* now move torp ahead in space */

	 /* XX -- use only 1 position -- updated later */ 
         fp = tp->tp_fusepos;
	 /* intial position */
	 fp->po_x = tx;
	 fp->po_y = ty;
	 fp++;
         fp->po_x = END;
         tp++;
      }
   }
   tp->tp_x = END;		/* to make sure */
   _state.maxfuse = maxfuse;
}

set_maxspeed()
{
   if(!me->p_damage)
      _state.maxspeed = me->p_ship.s_maxspeed;
   else
      _state.maxspeed = (me->p_ship.s_maxspeed+2) - (me->p_ship.s_maxspeed+1) *
	 ((float)me->p_damage/ (float) (me->p_ship.s_maxdamage));
}

/*
 * d_crs        : desired course
 * d_speed      : desired speed;
 * crs_r        : "safe" course.
 * speed_r      : "safe" speed.
 * hittime_r    : number of cycles before predicted hit.
 *
 * Returns number of immediate hits.
 */

compute_course(d_crs, crs_r, d_speed, speed_r, hittime_r, lvorbit)

   unsigned char	d_crs, *crs_r;
   int			d_speed, *speed_r, *hittime_r;
   int			*lvorbit;
{
   static int	r = -1;
   register	i;
   int		hits;

   if(!(me->p_flags & PFORBIT) && !(me->p_flags & PFDOCK)){
      if(_state.closest_e && !_state.closest_e->robot){
	 i = _udcounter - _state.closest_e->phittime_me;

	 if(i >5 && i < 15){
	    if(!_state.disengage){		/* XX */
	       if(!r){
		  r = RANDOM()%40;
	       }
	       /* do a quick dodge to fake phasers */

	       if(r % 2 == 0)
		  d_crs = me->p_dir - 50 - r;
	       else
		  d_crs = me->p_dir + 50 + r;
	    }
	 }
	 else r = 0;
      }

      /* done in order of importance -- last more important */
      /* TODO: avoiddir in terms of their direction and speed */

      /* BUG -- lets players hide in the corners */
      if(!_state.wrap_around && !_state.assault)
	 check_gboundary(&d_crs, 10);

      /* don't get too close */
      if(me->p_ship.s_type != STARBASE && (_state.closest_f && 
	 _state.closest_f->dist < 2500 && (MYDAMAGE() > 40
	 || (_state.closest_e && (_state.torp_danger || 
	 _state.closest_e->dist < 8000))))){

	 struct player *j = _state.closest_f->p;
	 unsigned char	deg;
	 if(j && isAlive(j) && !(j->p_flags & PFORBIT))
	    avoiddir(_state.closest_f->crs, &d_crs, 100);
      }

      /* hostile planets */
      if(_state.hplanet && _state.hplanet != _state.planet && (
	 !(_state.assault)
	 || (_state.assault && _state.hplanet != _state.assault_planet
	     && _state.hplanet->pl_armies > 4))){
      
#ifdef nodef
      !(_state.assault && _state.assault_planet == _state.hplanet) &&
	 (_state.planet != _state.hplanet)
#endif

	 unsigned char	deg;
	 int		dist = _state.hpldist;

	 if(dist < 750) dist = 750;
	 deg = (70 * PFIREDIST) / (dist+1);
	 avoiddir(get_course(_state.hplanet->pl_x, _state.hplanet->pl_y),
	       &d_crs, deg);
      }

      if(ignoring_e(_state.closest_e)){
	 struct player	*j = _state.closest_e->p;
	 unsigned char	deg;
	 int		e_dist = edist_to_me(j);

	 if(e_dist < 10000){
	    avoiddir(_state.closest_e->crs, &d_crs, 64);
	 }
	 if(e_dist < 7000 && me->p_speed > 5)
	    d_speed = 5;
      }
      /* don't want to blow up on player */
      if(_state.defend && _state.protect_player && (MYSHIELD() < 20 ||
	 (MYDAMAGE() > 40))){
	 struct player	*j = _state.protect_player->p;
	 unsigned char	deg;
	 int		dist = _state.protect_player->dist;

	 if(_state.det_torps > 0){

	    if(dist < SHIPDAMDIST){
	       avoiddir(_state.protect_player->crs, &d_crs, 128);
	       if(d_speed < 2)
		  d_speed ++;
	    }
	 }
      }
   }

   if(d_speed > me->p_ship.s_maxspeed){
      /*
      printf("excess speed warning %d\n", d_speed);
      */
      d_speed = me->p_ship.s_maxspeed;
   }

   hits = _old_compute_course(d_crs, crs_r, d_speed, speed_r, hittime_r,
      lvorbit);
   /* ADDED hittime_r < 20 */
   if(hits && *hittime_r < 40)
      _state.recharge_danger = 1;
   
   if(d_speed == me->p_speed && d_crs == me->p_dir && hits > 0)
      check_dethis(hits, *hittime_r);

   return hits;
}

static int chks[4] = { 4, 8, 16, 32 };

/* NOT USED */
_compute_course(d_crs, crs_r, d_speed, speed_r, hittime_r, lvorbit)

   unsigned char	d_crs, *crs_r;
   int			d_speed, *speed_r, *hittime_r;
   int			lvorbit;
{
   register		i;
   int			hits;
   int			avdir= -1, hittime, damage;
   register		mindamage = INT_MAX, cavdir;


   hits = update_torps(&avdir, d_crs, d_speed, &hittime, &damage, lvorbit);

   if(!hits){
      *crs_r 		= d_crs;
      *speed_r 		= d_speed;
      *hittime_r	= -1;
      return 0;
   }
   else {
      *hittime_r	= hittime;
      mindamage		= damage;
   }

   cavdir = avdir;

   if(DEBUG & DEBUG_HITS){
      printf("INITIALLY found %d hits from average direction %d.\n",
	 hits, avdir);
      printf("course: %d, speed: %d, hit time: %d, damage: %d\n",
	 d_crs, d_speed, hittime, damage);
      printf("CURRENT course %d, speed %d\n", me->p_dir, me->p_speed);
      printf("CURRENT descourse %d, desspeed %d\n", _state.p_desdir,
	 _state.p_desspeed);
   }
   hits = update_torps(&avdir, _state.p_desdir, _state.p_desspeed, &hittime, 
      &damage, lvorbit);
   if(!hits){
      *crs_r 		= _state.p_desdir;
      *speed_r 		= _state.p_desspeed;
      *hittime_r	= -1;
      return 0;
   }
   else if(damage < mindamage){
      cavdir	= avdir;
      /* override caller's desired course and speed */
      d_crs	= _state.p_desdir;
      d_speed	= _state.p_desspeed;
      *hittime_r= hittime;
      mindamage	= damage;
   }

   return
   _try_others(d_crs, crs_r, d_speed, speed_r, hittime_r, cavdir, mindamage,
      lvorbit);
}

_try_others(d_crs, crs_r, d_speed, speed_r, hittime_r, cavdir, damage, lvorbit)

   unsigned char	d_crs;		/* desired course */
   unsigned char	*crs_r;		/* returned course */
   int			d_speed;	/* desired speed */
   int			*speed_r;	/* returned speed */
   int			*hittime_r;	/* hittime returned (init now) */
   int			cavdir;		/* av direction from hits (init now) */
   int			damage;		/* current damage */
   int			lvorbit;
{
   register			i, j;
   register unsigned char	n_crs;
   register int			dif, dchange, n_speed = d_speed;
   register int			mindamage = damage, hits;
   int				avdir,hittime = *hittime_r;

   /* based on cavdir, d_crs, d_speed, and damage find a new course. */

   for(i=0, n_crs=d_crs-64; i< 32; i++, n_crs += 4){
      
      hits = update_torps(&avdir, n_crs, n_speed, &hittime, &damage, lvorbit);
      if(!hits){
	 *crs_r 		= n_crs;
	 *speed_r 		= n_speed;
	 *hittime_r		= -1;
	 if(DEBUG & DEBUG_HITS){
	    printf("RETURN 0\n");
	    printf("iteration %d: course %d, speed %d, found damage %d\n",
	       i, n_crs, n_speed, damage);
	    printf("hittime %d, avdir %d\n", hittime, cavdir);
	 }
	 return 0;
      }
      else if(damage < mindamage){
	 cavdir	= avdir;
	 /* override caller's desired course and speed */
	 *crs_r 	= n_crs;
	 *speed_r 	= n_speed;
	 *hittime_r	= hittime;
	 mindamage	= damage;
      }
   }
   if(DEBUG & DEBUG_HITS){
      printf("RETURN 1(failed)\n");
      printf("iteration %d: course %d, speed %d, found damage %d\n",
	 i, n_crs, n_speed, damage);
      printf("hittime %d, avdir %d\n", hittime, cavdir);
   }
   return 1;
}

/* NOT USED */
_try_others1(d_crs, crs_r, d_speed, speed_r, hittime_r, cavdir, damage, lvorbit)

   unsigned char	d_crs;		/* desired course */
   unsigned char	*crs_r;		/* returned course */
   int			d_speed;	/* desired speed */
   int			*speed_r;	/* returned speed */
   int			*hittime_r;	/* hittime returned (init now) */
   int			cavdir;		/* av direction from hits (init now) */
   int			damage;		/* current damage */
   int			lvorbit;
{
   register			i, j;
   register unsigned char	n_crs;
   register int			dif, dchange, n_speed = d_speed;
   register int			mindamage = damage, hits;
   int				avdir,hittime = *hittime_r;

   /* based on cavdir, d_crs, d_speed, and damage find a new course. */

   for(i=0,j=1; i< 4; i++){
      if(damage > 40)
	 dchange = 32;
      else if(damage > 30)
	 dchange = 16;
      else if(damage > 20)
	 dchange = 8;
      else
	 dchange = 4;

      dif = angdist(cavdir, d_crs);
      n_crs = d_crs + dif;
      if(n_crs == cavdir)
	 /* ccw */
	 n_crs = d_crs + j * dchange;
      else
	 n_crs = d_crs - j * dchange;
      j++;

      if(DEBUG & DEBUG_HITS){
	 printf("\n");
	 printf("iteration %d: course %d, speed %d, found damage %d\n",
	    i, n_crs, n_speed, damage);
	 printf("hittime %d, avdir %d\n", hittime, cavdir);
      }


      hits = update_torps(&avdir, n_crs, n_speed, &hittime, &damage, lvorbit);
      if(!hits){
	 *crs_r 		= n_crs;
	 *speed_r 		= n_speed;
	 *hittime_r		= -1;
	 if(DEBUG & DEBUG_HITS){
	    printf("RETURN 0\n");
	    printf("iteration %d: course %d, speed %d, found damage %d\n",
	       i, n_crs, n_speed, damage);
	    printf("hittime %d, avdir %d\n", hittime, cavdir);
	 }
	 return 0;
      }
      else if(damage < mindamage){
	 j = 1;
	 cavdir	= avdir;
	 /* override caller's desired course and speed */
	 *crs_r 	= n_crs;
	 *speed_r 	= n_speed;
	 *hittime_r	= hittime;
	 mindamage	= damage;
      }

   }
   if(DEBUG & DEBUG_HITS){
      printf("RETURN 1(failed)\n");
      printf("iteration %d: course %d, speed %d, found damage %d\n",
	 i, n_crs, n_speed, damage);
      printf("hittime %d, avdir %d\n", hittime, cavdir);
   }
   return 1;
}

update_torps(avdir_r, dir, speed, mintime_r, damage_r, lvorbit)
   
   int			*avdir_r;
   unsigned char	dir;		/* desired direction */
   int			speed;		/* desired speed */
   int			*mintime_r;	/* team to hit */
   int			*damage_r;	/* amount of damage */
   int			lvorbit;
{
   register			i, dx,dy;
   register struct tpos		*tp;
   register struct pos		*fp;
   register			cx = me->p_x, cy = me->p_y;
   int				mx,my, damage;
   int				ddist, minhittime, hits,
				tdamage, expdist;
   int				avdir = -1;
   char				first = 1;

   hits		= 0;
   damage 	= 0;
   minhittime	= INT_MAX;
   _plhit	= 0;

   if(!_state.torp_wobble)
      expdist = EXPDIST;
   else
      expdist = EXPDIST + 50;

   initppos();

#ifdef nodef
   /* reinitialize plasmas & seeking torps */
   for(tp=_etorps;tp->tp_x != END; tp++){
      if(tp->tp_info & TPPLASMA){
	 tp->tp_fusepos[0].po_x = tp->tp_x;
	 tp->tp_fusepos[0].po_y = tp->tp_y;
	 tp->tp_ndir = tp->tp_dir;
      }
   }
#endif

   for(i=0; i< _state.maxfuse; i++){
      
      getppos(cx,cy, &mx, &my, dir, speed, lvorbit);
      cx = mx; cy = my;

      for(tp=_etorps; tp->tp_x != END; tp++){
	 
	 if(i==0) { 
	    tp->tp_info &= ~TPHIT;
	    if(tp->tp_info & TPSEEK){
	       tp->tp_fusepos[0].po_x = tp->tp_x;
	       tp->tp_fusepos[0].po_y = tp->tp_y;
	       tp->tp_ndir = tp->tp_dir;
	    }
	 }
	 else 
	    if(tp->tp_info & TPHIT) continue;

	 if(tp->tp_info & TPSEEK){
	    if(i >= tp->tp_fuse)
	       continue;	/* plasma ran out */
	    fp = &tp->tp_fusepos[0];	/* first element only */
	    if(tp->tp_info & TPPLASMA)
	       predict_pltorp(tp->tp_j, &tp->tp_ndir, 
		  &fp->po_x, &fp->po_y, cx,cy);
	    else
	       predict_storp(tp->tp_j, &tp->tp_ndir,
		  &fp->po_x, &fp->po_y, cx,cy);
	 }
	 else {
	    if(i < tp->tp_fuse)
	       fp = &tp->tp_fusepos[i];
	    else
	       continue;		/* torp ran out */
	 }
	 
	 dx = fp->po_x - cx;
	 dy = fp->po_y - cy;

	 if(tp->tp_j->p_flags & PFROBOT)
	    expdist = EXPDIST;
	 if(tp->tp_info & TPPLASMA)
	    expdist = EXPDIST+100;
#ifdef nodef
	 else
	    expdist = EXPDIST + (tp->tp_dist*EXPDIST)/10000;
	 if(tp->tp_info & TPSEEK){
	    if(tp->tp_info & TPPLASMA){
	       expdist = _state.seek_const;
	    }
	    else
	       expdist = EXPDIST + (tp->tp_dist*EXPDIST)/_state.seek_const 
		  + 500;
	 }
#endif

	 if(ABS(dx) < expdist && ABS(dy) < expdist) {
	    int	torp_d;
	    
	    if(tp->tp_info & TPPLASMA) {
	       torp_d = tp->tp_j->p_ship.s_plasmadamage;
	    }
	    else
	       torp_d = tp->tp_j->p_ship.s_torpdamage;
	    
	    hits ++;
	    ddist = dx*dx + dy*dy;

	    if(ddist > EXPDIST * EXPDIST){
	       tdamage = torp_d * (DAMDIST -
		  sqrt((double) ddist)) / (DAMDIST - EXPDIST);
	       if(tdamage < 0) tdamage = 0;
	    }
	    else
	       tdamage = torp_d;

	    if(tp->tp_info & TPPLASMA){
	       _plhit = tdamage;
	    }

	    tdamage = (30*640*tdamage)/(i+1);	/* xx 5 configurable */
	    damage += tdamage;

	    if(i<minhittime) minhittime = i;

	    if(first){
	       avdir = (int)tp->tp_dir;
	       first = 0;
	    }
	    else
	       avdir = accavdir(avdir, damage, tp->tp_dir, tdamage);
	    tp->tp_info |= TPHIT;
	 }
	    
      }
   }
   damage /= 640;

   *mintime_r 	= minhittime;
   *damage_r 	= damage;
   *avdir_r 	= avdir;

   return hits;
}

torp_seek_ship(s)

   int	s;
{
   if(!_state.torp_seek) return 0;
   switch(s){
      case BATTLESHIP:
      case STARBASE:
	 return 1;
      
      default:
	 return 0;
   }
}

accavdir(avdir, damage, tdir, tdamage)

   int	avdir;		/* original direction average */
   int	damage;		/* damage to be associated with avdir */
   unsigned char tdir;	/* torp direction */
   int	tdamage;	/* torp damage */
{
   register int	dif = angdist((unsigned char) avdir, tdir), av;
   register int	a1 = tdir, n;

   if(damage + tdamage == 0)
      return avdir;

   n = a1+dif; 
   if(NORMALIZE(n) == avdir){
      /* a1 ccw of avdir */
      av = (a1 + (damage * dif)/(damage + tdamage));
   }
   else {
      av = (avdir + (tdamage * dif)/(damage + tdamage));
   }
   return NORMALIZE(av);
}

initppos()
{
   _myspeed = me->p_speed;
   _mydir = me->p_dir;
   _subdir = _state.p_subdir;
   _subspeed = _state.p_subspeed;
   _cticks = 1;
}

getppos(cx,cy, rx, ry, dir, speed, lvorbit)

   register		cx,cy;
   int			*rx, *ry;
   unsigned char	dir;
   int			speed;
   int			lvorbit;
{
   int				accint = SH_ACCINT(me),
				decint = SH_DECINT(me),
				turns  = SH_TURNS(me);
   register int			myspeed, subspeed, subdir, dticks,
				min, max;
   register unsigned char	mydir;

   if((me->p_flags & PFORBIT) && !lvorbit){
      getorbit_ppos(cx,cy,rx,ry);
      return;
   }

   myspeed 	= _myspeed;
   subspeed	= _subspeed;
   subdir	= _subdir;
   mydir	= _mydir;

   if(mydir != dir){
      if(myspeed == 0){
	 mydir = dir;
	 subdir = 0;
      }
      else {
	 subdir += turns/(1<<myspeed);
	 dticks = subdir / 1000;
	 if(dticks){
	    if(mydir > dir){
	       min = dir;
	       max = mydir;
	    } else {
	       min = mydir;
	       max = dir;
	    }
	    if((dticks > max-min) || (dticks > 256-max+min))
	       mydir = dir;
	    else if((unsigned char) ((int)mydir - (int)dir) > 127)
	       mydir += dticks;
	    else
	       mydir -= dticks;
	    subdir %= 1000;
	 }
      }
   }

   if(myspeed != speed){
      if(speed > myspeed)
	 subspeed += accint;
      if(speed < myspeed)
	 subspeed -= decint;
      if(subspeed / 1000){
	 myspeed += subspeed / 1000;
	 subspeed /= 1000;
/* suggested by Leonard:  subspeed %= 1000 */
	 if(myspeed < 0)
	    myspeed = 0;
	 if(myspeed > _state.maxspeed)
	    myspeed = _state.maxspeed;
      }
   }
   *rx = cx + (double)myspeed * Cos[mydir] * WARP1;
   *ry = cy + (double)myspeed * Sin[mydir] * WARP1;

   /* todo: add all tractors -- how do we detect if we are target? */

   if(_state.closest_e && isAlive(_state.closest_e->p)){
      if(_state.closest_e->pp_flags & PFTRACT)
	 add_tractor(_state.closest_e, _state.closest_e->p, rx, ry, _cticks);
      /* not implemented yet */
      else if(_state.closest_e->pp_flags & PFPRESS)
	 add_pressor(_state.closest_e, _state.closest_e->p, rx, ry);
   }
   
   if(me->p_flags & PFTRACT)
      add_mytractor(rx, ry);
   else if(me->p_flags & PFPRESS)
      add_mypressor(rx, ry);

   _myspeed 	= myspeed;
   _subspeed	= subspeed;
   _subdir	= subdir;
   _mydir	= mydir;
   _cticks++;
}

getorbit_ppos(cx,cy, rx, ry)

   register		cx,cy;
   int			*rx, *ry;
{
   register		px,py;

#ifdef nodef
   if(_state.planet && (&planets[me->p_planet] != _state.planet)){
      printf("error in getorbit_ppos: wrong planet.\n");
   }
#endif

   px = planets[me->p_planet].pl_x;
   py = planets[me->p_planet].pl_y;

   _mydir += 2;

   *rx = px+ORBDIST * Cos[(unsigned char)(_mydir - (unsigned char) 64)];
   *ry = py+ORBDIST * Sin[(unsigned char)(_mydir - (unsigned char) 64)];
}

update_hplanets()
{
   register                     i;
   register struct planet       *pl;
   double                       dx,dy;
   register int                 pdist, mindist = INT_MAX;

   _state.hplanet = NULL;
   _state.hpldist = GWIDTH;

   pl = me_p->closest_pl;
   if(pl && (((me->p_swar | me->p_hostile) & pl->pl_owner) ||
		unknownpl(pl))){
      _state.hplanet = pl;
      _state.hpldist = me_p->closest_pl_dist;
   }

#ifdef nodef
   for(i=0, pl=planets; i<MAXPLANETS; i++,pl++){
      if(!((me->p_swar | me->p_hostile) & pl->pl_owner) && !unknownpl(pl))
         /* not hostile */
         continue;
      dx = pl->pl_x - me->p_x;
      dy = pl->pl_y - me->p_y;
      pdist = (int)hypot(dx,dy);
      /* won't even get close */
      if(pdist >  SCALE*WINSIDE/2)
         continue;
      
      if(pdist < mindist){
         _state.hplanet = pl;
         mindist = pdist;
         _state.hpldist = mindist;
      }
   }
#endif 
}


fixfuse(tx,ty, tf, j, tdx, tdy)
   int			tx,ty;
   int                  *tf;
   struct player        *j;
   double               tdx,tdy;
{
   double       dx = j->p_x - tx;
   double       dy = j->p_y - ty;
   int          d = ihypot(dx,dy);
   int		v = ihypot(tdx,tdy);

   if(v == 0)
      return;

   d /= ihypot(tdx,tdy);
   *tf -= d;
}
 
/* xx */
c_lookahead(v)

   int	v;
{
   static int	prevf;

   if(v & _state.lookahead != MAXFUSE){
      prevf = _state.lookahead;
      _state.lookahead = MAXFUSE;
   }
   else
      _state.lookahead = prevf;
}

set_lookahead(v, b)
   int	v;
{
   static int	prevf;
   if(b){
      prevf = _state.lookahead;
      _state.lookahead = v;
   }
   else
      _state.lookahead = prevf;
}

/*
 *    256/0
 * 192  +   64
 *     128
 */
 
 
#define MINDIST		3700
check_gboundary(mycrs, range)
 
   unsigned char        *mycrs;
   int			range;
{
   int  			l = 0, r = 0;
   register			x = me->p_x, y = me->p_y;
   register unsigned char	crs = *mycrs;
 
   if(x < MINDIST && crs > 128){
      if(crs > 192)
         /* *mycrs += 64; */    *mycrs = 0 + range;
      else
         /* *mycrs -= 64; */    *mycrs = 128 - range;
      l = 1;
   }
   else if(x > GWIDTH-MINDIST && crs < 128){
      if(crs > 64)
         /* *mycrs += 64;*/     *mycrs = 128 + range;
      else
         /* *mycrs -= 64;*/     *mycrs = 256 - range;
      r = 1;
   }
 
   if(y < MINDIST && (crs > 192 || crs < 64)){
      if(crs > 192 && !l)
         /* *mycrs -= 64; */    *mycrs = 192 - range;
      else
         /* *mycrs += 64; */    *mycrs = 64 + range;
   }
   else if(y > GWIDTH-MINDIST && (crs < 192 && crs > 64)){
      if(crs < 128 && !r)
         /* *mycrs -= 64; */    *mycrs = 64 - range;
      else
         /* *mycrs += 64; */    *mycrs = 192 + range;
   }
}
 
init_ftorps(i, j)

   int                  i;
   struct player        *j;
{
   register             k;
   register struct torp *t;

   for(k=0, t= &torps[i*MAXTORP] ; k < MAXTORP; k++, t++){
      if(t->t_status == TFREE) 
         continue;
      else if (t->t_status == TEXPLODE) {
         /* check for damage to enemies */
         check_damage(0, t, SH_TORPDAMAGE(j));

         t->t_status = TFREE;   /* this might be wrong */
         j->p_ntorp --;
         continue;
      }
      /* detonate torps that are past target */
      if(j == me && _state.current_target){
         check_dettorps(t,k);
      }
   }
}

init_phasers(p, j, ph)
 
   Player               *p;
   struct player        *j;
   struct phaser        *ph;
{
   p->fuel -= SH_PHASERCOST(j);
   if(p->fuel < 0) p->fuel = 0;
   if(ph->ph_status == PHHIT)
      do_phaserdamage(p, j, ph);
   p->wpntemp += (SH_PHASERCOST(j)/10);
}

do_phaserdamage(p, j, ph)

   Player               *p;
   struct player        *j;
   struct phaser        *ph;
{
   int  phrange, range, damage;
   struct player        *target;

   target = &players[ph->ph_target];

   if(ph->ph_status == PHHIT){
      if(target == me){
	 if(!_state.human)
	    p->phit_me ++;
	 p->phittime_me = _udcounter;
      }
   }

   phrange = PHASEDIST * j->p_ship.s_phaserdamage/ 100;
   range = pdist_to_p(j, target);
   damage = (j->p_ship.s_phaserdamage) * 
      (1.0 - (range / (float) phrange));
   if(damage < 0) damage = -damage;

#ifdef nodef
   if(DEBUG & DEBUG_ENEMY){
      printf("phaser hit from \"%s\" to \"%s\"\n",
	 j->p_name, target->p_name);
      printf("phaser points: %d\n", damage);
   }
#endif

   dodamage(&_state.players[target->p_no], target, damage);
   if((target->p_flags & PFCLOAK) && _state.players[target->p_no].enemy){
      _state.players[target->p_no].phit = 1;
      _state.players[target->p_no].phittime = _udcounter;
      _state.players[target->p_no].cx = target->p_x;
      _state.players[target->p_no].cy = target->p_y;
   }
}

do_plasmadamage(po, jo, pt)

   Player               *po;
   struct player        *jo;
   struct plasmatorp	*pt;
{
   register 			i;
   register 			dx,dy,dist;
   int			  	damage, pldamage;
   register struct player 	*j;
   register Player		*p;

   /* fuel penalty handled in socket.c */

   for(i=0, p=_state.players; i < MAXPLAYER; i++,p++){
      if(!p->p || (p->p->p_status != PALIVE) || (p->invisible))
	 continue;
      j = p->p;

      if(j == me) continue;

      dx = pt->pt_x - j->p_x;
      dy = pt->pt_y - j->p_y;
      if (ABS(dx) > PLASDAMDIST || ABS(dy) > PLASDAMDIST)
	 continue;
      dist = dx*dx + dy*dy;
      if(dist > PLASDAMDIST * PLASDAMDIST)
	 continue;
      pldamage = SH_PLASMADAMAGE(jo);
      if(dist > (EXPDIST) * (EXPDIST)){
	 /* FIX */
	 damage = pldamage * (PLASDAMDIST - sqrt((double)dist))/
	    (PLASDAMDIST - (EXPDIST));
      }
      else {
	 damage = pldamage;
      }
      if(DEBUG & DEBUG_ENEMY)
         printf("plasma dam to %s(%d): %d\n", j->p_name, j->p_no, damage);

      dodamage(p, j, damage);
   }
}

check_damage(enemy, t, td)

   int		enemy;
   struct torp  *t;
   int          td;
{
   register             i;
   register Player     *p;
   for(i=0, p=_state.players; i< MAXPLAYER; i++, p++){
      if(p->p && !p->invisible){
	 if(!enemy && !p->enemy || (enemy && p->enemy))
	    continue;
         add_damage(t, td, p);
      }
   }
}
 
check_dettorps(t,k)

   struct torp  *t;
   int          k;
{
   Player		*ct = _state.current_target;
   register             x = 0, y = 0;
   unsigned char        tdir, hcrs = 0;
   double		dx,dy;

   if(ct && ct->p && isAlive(ct->p)){
      x = ct->p->p_x;
      y = ct->p->p_y;
      hcrs = ct->p->p_dir;
   }
   else 
      return;

   dx = x - t->t_x, dy = y - t->t_y;

   if(ct->dist < TRRANGE(me) && !(ct->p->p_flags & PFCLOAK)){
      if(_state.player_type == PT_DOGFIGHTER){
	 /* exact check here */
	 get_torp_course(ct->p, &tdir, t->t_x, t->t_y);
	 /*
	 printf("tdir: %d, t->t_dir: %d, angdist: %d\n",
	    tdir, t->t_dir, angdist(tdir, t->t_dir));
	 */
	 if(angdist(tdir, t->t_dir) > 64){
	    if(_state.human || detall)
	       t->t_shoulddet = 1;
	    else
	       req_det_mytorp(k);
	 }
	 else if(angdist(tdir, t->t_dir) < tractt_angle 
	    && (ihypot(dx,dy) < tractt_dist
	    || _do_tract))
	    _do_tract ++;
      }
      else {
	 /* inaxact check */
	 tdir = get_acourse(x,y, t->t_x, t->t_y);
	 if(angdist(tdir, t->t_dir) > 80){
	    if(_state.human || detall)
	       t->t_shoulddet = 1;
	    else
	       req_det_mytorp(k);
	 }
	 else if(angdist(tdir, t->t_dir) < tractt_angle 
	    && (ihypot(dx,dy) < tractt_dist
	    || _do_tract))
	    _do_tract ++;
      }
   }
   else if(ct->p->p_flags & PFCLOAK){
      tdir = get_acourse(x,y, t->t_x, t->t_y);
      if(ihypot(dx,dy) > 3000 && angdist(tdir, t->t_dir) > 128){
	 if(_state.human || detall)
	    t->t_shoulddet = 1;
	 else
	    req_det_mytorp(k);
      }
   }

   /* for now */
   if(me->p_ship.s_type == STARBASE)
      _do_tract = 0;
   /*
   printf("_do_tract: %d\n", _do_tract);
   */

   if(_do_tract && (_tr_timer<0)){
      /*
      printf("tractor on\n");
      */

      handle_tractors(ct, ct->p, ct->dist, 1, "tractor into torps", TR_TORP);
      _tr_timer = (int)(20./updates) + RANDOM()%((tractt_dist+100)/100);
   }

   if(last_tract_req == TR_TORP && !_do_tract && (_tr_timer==0)){
      /*
      printf("tractor off\n");
      */
      req_tractor_off("turn off TR_TORP");
      _tr_timer = -1;
   }

   if(_tr_timer > 0){
      /*
      printf("tr_timer: %d\n", _tr_timer);
      */
      _tr_timer --;
   }

   if(_state.human || detall){
      register	h;
      register struct torp	*l;
      for(h=0,l= &torps[MAXTORP*me->p_no]; h< me->p_ntorp; h++,l++){
	 if(!l->t_shoulddet)
	    return;
      }
      for(h=0,l= &torps[MAXTORP*me->p_no]; h< me->p_ntorp; h++,l++){
	 req_det_mytorp(h);
      }
   }
}

cloaked(p)

   Player	*p;
{
   if(!p || !p->p || !isAlive(p->p))
      return 0;
   return (p->pp_flags & PFCLOAK);
}

unsigned char get_new_course1();
unsigned char get_new_course2();
unsigned char get_new_course3();
_old_compute_course(d_crs, crs_r, d_speed, speed_r, hittime_r, lvorbit)

   unsigned char        d_crs, *crs_r;
   int                  d_speed, *speed_r, *hittime_r;
   int			*lvorbit;
{
   register             i, nh;
   int                  hittime, minhits = INT_MAX, minhittime = INT_MAX,
                        mindamage = INT_MAX,
#ifdef nodef
                        begin_time = mtime(0), 
#endif
			speed, crs, chklower=1, damage;
   int                  avdir;
   int			plhit = 0;

   *crs_r       = d_crs;
   *speed_r     = d_speed;
   *hittime_r   = INT_MAX;

   speed = d_speed;

   /* calculate torp paths */

   *lvorbit = 0;

   /* first check with don't leave orbit */
   nh = update_torps(&avdir, d_crs, d_speed, &hittime, &damage, 0);
   /* max hittime will be MAXFUSE */

   minhits = nh;
   minhittime = hittime;
   mindamage = damage;
   plhit = _plhit;

   if(DEBUG & DEBUG_HITS){
      if(nh > 0){
	 printf("INITIALLY found %d hits from %d with course %d, speed %d\n",
	    nh, avdir, d_crs, d_speed);
	 printf("hit time: %d, damage: %d\n", hittime, damage);
	 fflush(stdout);
      }
   }

   if( !nh /* || hittime > distfunc()*/){

#ifdef nodef
      if(DEBUG & DEBUG_HITS){
         printf("returning no hits at %d\n", hittime);
      }
#endif
      Xplhit = 0;
      return 0;
   }

#ifdef nodef
   /* experiment */
   if(me->p_speed == 0)
      return nh;
#endif
   
   if(d_speed < 2){
      speed = 2;
      d_speed = 2;
   }

   crs = d_crs;

   /* try some other courses/speeds */
   while (1) {

      for(i=0; i< 4; i++){

	 d_crs = get_new_course2(avdir, hittime, d_crs, i);

         nh = update_torps(&avdir, d_crs, d_speed, &hittime, &damage, 1);

         if(DEBUG & DEBUG_HITS){
	    if(nh == 0){
	       printf("NEW: found %d hits from %d with course %d, speed %d\n",
		  nh, avdir, d_crs, d_speed);
	       printf("hit time: %d, damage: %d\n", hittime, damage);
	       fflush(stdout);
	    }
         }

         if( !nh /* || hittime > distfunc()*/){

	    *lvorbit = 1;
            *crs_r = d_crs;
            *speed_r = d_speed;
#ifdef nodef
            if(DEBUG & DEBUG_HITS){
               printf("returning no hits at %d\n", hittime);
            }
#endif
	    Xplhit = 0;
            return 0;
         }
         else if((nh < minhits && hittime >= minhittime) ||
         hittime > minhittime ||
         (nh <= minhits && hittime >= minhittime && damage < mindamage)){

	    *lvorbit = 1;
            *crs_r = d_crs;
            *speed_r = d_speed;
            minhits = nh;
            minhittime = hittime;
            mindamage = damage;
	    plhit = _plhit;
         }
      }

      d_crs = crs;

      /* could try chkupper first */
      if(chklower && d_speed > 2)
         d_speed --;
      else if(d_speed <= 2){    /* should be tuned to ship */
         chklower = 0;
         d_speed = speed+1;
      }
      else if(!chklower && d_speed < _state.maxspeed) 
         d_speed ++;
      else{
         /* exhausted all tries */
         /* try one more thing before giving up .. */

         /* DEBUG */
         nh = update_torps(&avdir, _state.p_desdir, _state.p_desspeed,
            &hittime, &damage, 0);
         if((nh < minhits && hittime >= minhittime )
            || hittime > minhittime || damage < mindamage - 20){
	    *lvorbit = 0;
            *crs_r = _state.p_desdir;
            *speed_r = _state.p_desspeed;
            minhits = nh;
            minhittime = hittime;
            mindamage = damage;
	    plhit = _plhit;
         }

         if(DEBUG & DEBUG_HITS){
            printf("TE: returning course %d, speed %d -- hits %d ",
               *crs_r, *speed_r, minhits);
            printf("in %d cycles, damage: %d --\n", minhittime, mindamage);
         }

#ifdef nodef    /* need more variables */
         if(minhittime < 3 && _state.det_torps < 3)     /* experiment */
            req_detonate("");
#endif

	 Xplhit = plhit;
         return minhits;
      }

#ifdef nodef
      if(mtime(0) - begin_time > /*LOOKAHEAD_I*/30){
         if(DEBUG & DEBUG_HITS){
            printf("***TIMEOUT***: %d\n", minhits);
            printf("returning course %d, speed %d -- hits %d ",
               *crs_r, *speed_r, minhits);
            printf("in %d cycles, damage %d --\n", minhittime, mindamage);
         }
         return minhits;
      }
#endif
   }
   /* NOTREACHED */
}

unsigned char get_new_course1(avdir, hittime, d_crs, it)

   int                  avdir, hittime, it;
   unsigned char        d_crs;
{
   int  t_dir;

   t_dir = NORMALIZE(avdir - me->p_dir);
   if(t_dir / 64 > 1)
      d_crs -= it*20 + 10;
   else
      d_crs += it*20 + 10;
   return d_crs;
}

unsigned char get_new_course2(avdir, hittime, dc, i)

   int                  avdir;
   int			hittime;
   unsigned char        dc;
   int                  i;
{
   int d = RANDOM()%2;
   switch(i){
      case 0:   return d?dc+10:dc-10;
      case 1:   return d?avdir+64:avdir-64;
      case 2:   return d?dc-128:dc+128;
      case 3:   return RANDOM()%256;
   }
}

/* for _state.human */
unsigned char get_new_course3(avdir, hittime, dc, i)

   int                  avdir;
   int			hittime;
   unsigned char        dc;
   int                  i;
{
   int d = RANDOM()%2;

   switch(_state.human){
      case 1:
	 switch(i){
	    case 0:   return d?dc+10:dc-10;
	    case 1:   return d?avdir+64:avdir-64;
	    case 2:   return d?dc-128:dc+128;
	    case 3:   return RANDOM()%256;
	 }
	 break;
      case 2:
	 switch(i){
	    case 1:   return d?avdir+64:avdir-64;
	    case 2:   return d?dc-128:dc+128;
	    default:   return RANDOM()%256;
	 }
	 break;

      default:
	 return RANDOM()%256;
	 break;
   }
}

mod_torp_speed(pdir, pspeed, tdir, tspeed)
   
   unsigned char	pdir,tdir;
   int			pspeed, tspeed;
{
   double	vsx,vsy, vtx,vty, ntx,nty;
   int		newspeed;

   /* ship vector */
   vsx = (double)pspeed * Cos[pdir] * WARP1;
   vsy = (double)pspeed * Sin[pdir] * WARP1;
   /* torp vector */
   vtx = (double)tspeed * Cos[tdir] * WARP1;
   vty = (double)tspeed * Sin[tdir] * WARP1;

   ntx = vsx + vtx;
   nty = vsy + vty;

   newspeed = (int)sqrt(ntx*ntx + nty*nty);
   newspeed = newspeed/WARP1;
   if(newspeed > 20)
      newspeed = 20;
   
   return newspeed;
}

check_server_response(x, y, cycles, e, j)

   int                  *x,*y;
   int			cycles;
   Player		*e;
   struct player	*j;
{
   register             i;
   int                  accint = SH_ACCINT(me),
                        decint = SH_DECINT(me),
                        turns = SH_TURNS(me),
                        myspeed = _state.last_speed;
   unsigned char        mydir = _state.last_dir;
   unsigned int         dticks, min, max;
   unsigned char        dir = _state.last_desdir;
   int                  speed = _state.last_desspeed;

   register int		subspeed = _state.last_subspeed, 
			subdir = _state.last_subdir;

   for(i=0; i< cycles; i++){
      if(mydir != dir){
         if(myspeed == 0){
            mydir = dir;
            subdir = 0;
         }
         else {
            subdir += turns/(1<<myspeed);
            dticks = subdir/1000;
            if(dticks){
               if(mydir > dir){
                  min = dir;
                  max = mydir;
               } else {
                  min = mydir;
                  max = dir;
               }
               if((dticks > max-min) || (dticks > 256-max+min))
                  mydir = dir;
               else if((unsigned char) ((int)mydir - (int)dir) > 127)
                  mydir += dticks;
               else
                  mydir -= dticks;
               subdir %= 1000;
            }
         }
      }

      if(myspeed != speed){
         if(speed > myspeed)
            subspeed += accint;
         if(speed < myspeed)
            subspeed -= decint;
         if(subspeed / 1000){
            myspeed += subspeed / 1000;
            subspeed /= 1000;
/* suggested by Leonard:  subspeed %= 1000 */
            if(myspeed < 0)
               myspeed = 0;
            if(myspeed > _state.maxspeed)
               myspeed = _state.maxspeed;
         }
      }

      if(i==0){
         *x = _state.last_x + (double)myspeed * Cos[mydir] * WARP1;
         *y = _state.last_y + (double)myspeed * Sin[mydir] * WARP1;
      }
      else{
         *x += (double) myspeed * Cos[mydir] * WARP1;
         *y += (double) myspeed * Sin[mydir] * WARP1;
      }
      if(me->p_flags & PFTRACT)
	 add_mytractor(x, y);
      else if(me->p_flags & PFPRESS)
	 add_mypressor(x, y);
      if(e)
	 add_tractor(e, j, x,y, 0);
   }
   if(!e){
      _state.p_subspeed = subspeed;
      _state.p_subdir = subdir;
   }
}

add_tractor(e, j, x, y, cycles)

   Player		*e;
   struct player	*j;
   int			*x,*y;
   int			cycles;
{
   int	dm, dist;

   /* since this is tractor as time goes on we may get closer
      to e -- try reducing distance */

   dist = e->dist - (cycles*1000)/20;
   if(dist <= 0) dist = 1;

   tp_change(me, e->p, dist, 1, x, y, &dm, &dm);
}

add_pressor(e, j, x, y)

   Player		*e;
   struct player	*j;
   int			*x,*y;
{
   int	dm;

   tp_change(me, e->p, e->dist, -1, x, y, &dm, &dm);
}

add_mytractor(x, y)

   int	*x,*y;
{
   struct player	*j = &players[me->p_tractor];
   int			dist, dm;

   if(!j){
      mprintf("major error in addmytractor.\n");
      return;
   }
   dist = _state.players[j->p_no].dist;

   tp_change(j, me, dist, 1, x, y, &dm, &dm);
}

add_mypressor(x, y)

   int	*x,*y;
{
   struct player	*j = &players[me->p_tractor];
   int			dist, dm;

   if(!j){
      mprintf("major error in addmypressor.\n");
      return;
   }
   dist = _state.players[j->p_no].dist;

   tp_change(j, me, dist, -1, x, y, &dm, &dm);
}


/* j2 -- tractor, j1 -- tractee */
tp_change(j1, j2, dist, dir, x, y, hx, hy)

   struct player 	*j1, *j2;
   int			dist, dir;
   int			*x,*y;		/* change to j1 */
   int			*hx,*hy;	/* change to j2 */
{

   float	cosTheta, sinTheta;
   int		halfforce;
   int		myx, myy, hisx, hisy;
   int		odist = dist;

   cosTheta = j1->p_x - j2->p_x;
   sinTheta = j1->p_y - j2->p_y;

   dist = ihypot((double)cosTheta, (double)sinTheta);

   if(dist == 0) dist = 1;

   cosTheta /= dist;
   sinTheta /= dist;
   halfforce = (WARP1*SH_TRACTSTR(j2));

   *hx += dir * cosTheta * halfforce/(SH_MASS(j1));
   *hy += dir * sinTheta * halfforce/(SH_MASS(j1));
   *x -= dir * cosTheta * halfforce/(SH_MASS(j2));
   *y -= dir * sinTheta * halfforce/(SH_MASS(j2));
}

predict_pltorp(j, pt_dir, pt_x, pt_y, mx,my)
   
   struct player	*j;		/* ptorp owner */
   unsigned char	*pt_dir;	/* torp direction */
   int			*pt_x, *pt_y;	/* ptorp position */
   int			mx,my;		/* my position */
{
   unsigned char	get_bearing();
   int			heading;
   int			pt;

/* XXX */
   if(_state.torp_seek)
      pt = 3;
   else
      pt = j->p_ship.s_plasmaturns;

   if(get_bearing(*pt_x, *pt_y, mx, my, *pt_dir) > 128){
      heading = ((int) *pt_dir) - pt;
      *pt_dir = ((heading < 0)? ((unsigned char) (256+heading)):
	 ((unsigned char) heading));
   } 
   else{
      heading = ((int) *pt_dir) + pt;
      *pt_dir = ((heading > 255)? ((unsigned char) (heading-256)):
	 ((unsigned char) heading));
   }

   *pt_x += (double) j->p_ship.s_plasmaspeed * Cos[*pt_dir] * WARP1;
   *pt_y += (double) j->p_ship.s_plasmaspeed * Sin[*pt_dir] * WARP1;
}

predict_storp(j, pt_dir, pt_x, pt_y, mx,my)
   
   struct player	*j;		/* ptorp owner */
   unsigned char	*pt_dir;	/* torp direction */
   int			*pt_x, *pt_y;	/* ptorp position */
   int			mx,my;		/* my position */
{
   unsigned char	get_bearing();
   int			heading;

   if(get_bearing(*pt_x, *pt_y, mx, my, *pt_dir) > 128){
      heading = ((int) *pt_dir) - 1;	/* torp turns */
      *pt_dir = ((heading < 0)? ((unsigned char) (256+heading)):
	 ((unsigned char) heading));
   } 
   else{
      heading = ((int) *pt_dir) + 1;	/* torp turns */
      *pt_dir = ((heading > 255)? ((unsigned char) (heading-256)):
	 ((unsigned char) heading));
   }

   *pt_x += (double) j->p_ship.s_plasmaspeed * Cos[*pt_dir] * WARP1;
   *pt_y += (double) j->p_ship.s_plasmaspeed * Sin[*pt_dir] * WARP1;
}

unsigned char
get_bearing(xme, yme, x, y, dir)
int x, y;
{
  int phi=0;

  phi = ((int) (atan2((double) (x-xme), (double) (yme-y)) / 3.14159 * 128.));
  if (phi < 0)
    phi = 256 + phi;
  if (phi >= dir)
    return((unsigned char) (phi - dir));
  else
    return((unsigned char) (256 + phi - dir));
}

