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

#define MAX_SCOUT_BOMB		30

/* Decide next course of action */

decide()
{
   static int	_donedead;
   static int	 needswardecs=1; /* track t-mode start and war declarations */

   if(_master){
      master();
      return;
   }

   if(_state.override) return;

   if(_state.player_type == PT_OGGER){
      if(decide_ogg() && !_state.ogg_req){
	 ogg_c(_state.current_target, "PT_OGGER");
      }
      return;
   }

   if( status->tourn == 0 ) { /* No t-mode, or t-mode ended */
      needswardecs=1;
   }

   /* New Game, or t-mode just started */
   /* Added check, so that it keeps trying until success on
      war decs */
   if ( status->tourn && needswardecs ) {
      if ( declare_intents(0) ) /* declare war and peace */
         needswardecs=0;
      return;
   }

   if(!inl){
      if((_state.warteam < 0 || _state.total_wenemies == 0) &&
	 _state.total_enemies > 0){
	 /* beginning of game, no enemies */
	 declare_intents(0); /* declare war and peace */
	 return;
      }
   }
   else if(status->tourn){
      if((_state.warteam < 0 || _state.total_wenemies == 0) &&
	 _state.total_enemies > 0){
	 /* beginning of game, no enemies */
	 declare_intents(0); /* declare war and peace */
	 return;
      }
   }

   if(_state.player_type == PT_DEFENDER){
      if(!_state.defend){
	 if(_state.last_defend && _state.last_defend->p 
	    /* && isAlive(_state.last_defend->p) */){
	    defend_c(_state.last_defend, "PT_DEFENDER");
	    return;
	 }
	 else
	    defend_c(_state.closest_f, "PT_DEFENDER");
      }
      return;
   }

   /* no automatic decisions unless t-mode */
   if(!status->tourn && !_state.itourn && !ignoreTMode) {
      return;
   }

   /* decide ship and course of action */
   if(_state.dead && !_donedead){
      _donedead = 1;	/* only once */
      decide_default();
      return;
   }
   else if(_state.dead && _donedead)
      return;

   _donedead = 0;

   if(_state.refit_req){
#ifdef nodef
      if(me->p_flags & PFREFITTING)
	 /* wait til refit completed */
	 return;
      else
	 check_refit();
#endif
	 return;
   }
   
   /* wait til recharge completed */
   if(_state.recharge)
      return;
   
   /* protect planet */
   if(_state.protect){
      decide_protect();
      return;
   }

   if(_state.disengage){
      decide_disengage();
      return;
   }

   /* bug fix */
   if(me->p_armies > 0 && !_state.assault_req){
      check_take(NULL);
      return;
   }

   if(_state.defend){
      decide_defend();
      return;
   }
   
   if(_state.escort_req){
      decide_escort();
      return;
   }

   if(_state.assault_req == ASSAULT_TAKE){
      decide_take();
      return;
   }
   else if(_state.assault_req == ASSAULT_BOMB){
      decide_bomb();
      return;
   }

   if(_state.ogg_req){
      decide_ogg();
      return;
   }

   /* otherwise normal */
   decide_default();
}

/* currently protecting.  Do we continue to protect or do something else? */
decide_protect()
{
   struct planet	*pl = _state.protect_planet;

   if(check_protect(3, NULL, 1)) return;
   if(inl){
      /* if we're protecting during an inl game and there's more then 3
      planets then we must be protecting armies. */
      if(pl->pl_armies <= 4)
	 unprotectp_c("no more armies on planet");
      if(!check_protect(3, NULL, 0)) 
	 unprotectp_c("don't protect armies");
   }
   else
      unprotectp_c("no protect");
}

decide_defend()
{
   Player	*p = _state.protect_player;
   if(!p || !p->p || !isAlive(p->p)){
      rdefend_c("player dead.");
      return;
   }

   if(!inl){
      /* quit if player fully recharged  -- for now */
      if(PL_DAMAGE(p) < 5 && PL_SHIELD(p) > 50 && PL_FUEL(p) > 50)
	 rdefend_c("player ok now");
   }
}

decide_take()
{
   PlanetRec		*pls = _state.planets;
   struct planet *tpl = _state.assault_planet;
   struct planet *cpl = me_p->closest_pl;

   if(!cpl) return;

   if(_state.assault == 0){
/*
      if(me->p_armies == 0)
	 untake_c("no armies");
*/
      /* not there yet -- probably getting armies */
      /* TEST */
      if(me->p_armies > 0)	/* XX: now look for a planet */
	 check_take(NULL);
      return;
   }

   if(tpl && !unknownpl(tpl) && tpl->pl_owner == me->p_team && tpl->pl_mydist > 10000){
      untake_c("planet taken.");
      check_take(NULL);
      return;
   }
   if(MYFUEL() < 30 && tpl && tpl->pl_mydist > 10000 && pl_defended(tpl,1)){
      untake_c("low fuel");
      disengage_c(ELOWFUEL, NULL, "low fuel");
      return;
   }
   if(MYFUEL() < 10 && tpl && tpl->pl_mydist > 6000 && pl_defended(tpl,2)){
      untake_c("low fuel");
      disengage_c(ELOWFUEL, NULL, "low fuel");
      return;
   }

   /*
   if(tpl->pl_mydist < 20000 && pl_defended(tpl,3)){
      untake_c("planet defended.");
      return;
   }
   */

   if(cpl == tpl) return;

   if( (unknownpl(cpl) && guess_team(cpl) == _state.warteam) ||
       (cpl->pl_owner == _state.warteam)){

       if(((cpl->pl_flags & PLAGRI) && me->p_armies < cpl->pl_armies &&
	cpl->pl_armies < 5) || ((cpl->pl_flags & PLAGRI) && me->p_armies < 3))
	 return;

       take_c(cpl, "there's");
   }
}

decide_bomb()
{
   struct planet	*pl = me_p->closest_pl;
   int			dist = pl?pl->pl_mydist:INT_MAX;

   if(!pl) return;

   if(me->p_flags & PFBOMB){
      check_bombdamage(_state.assault_planet);
      return;
   }
   else if(!unknownpl(_state.assault_planet) && 
      _state.assault_planet->pl_armies < 5){
      unassault_c("already bombed.");
      if (ogg_happy)
         check_ogg(NULL,20000); /* test JKH */
      check_bomb(NULL);
      return;
   }

   if(myteam_bombing(_state.assault_planet, _state.assault_planet->pl_mydist)){
      unassault_c("somebody else bombing");
      if (ogg_happy)
         check_ogg(NULL,20000); /* test JKH */
      check_bomb(NULL);
      return;
   }
   if(_state.assault_planet->pl_mydist < 20000 && 
      pl_defended(_state.assault_planet, 2)){
      unassault_c("planet defended");
      if (ogg_happy)
         check_ogg(NULL,20000); /* test JKH */
      check_bomb(NULL);
      return;
   }
#ifdef nodef /* TEST */
   else if(MYFUEL() < 30 && _state.assault_planet && 
      _state.assault_planet->pl_mydist > 10000){
      unassault_c("low fuel");
      disengage_c(ELOWFUEL, NULL, "low fuel");
      return;
   }
#endif

   if(pl == _state.assault_planet) return;

   if( (unknownpl(pl) && guess_team(pl) == _state.warteam) ||
       (pl->pl_owner == _state.warteam)){

      if((unknownpl(pl) || pl->pl_armies > 4) && 
	 me_closest_to_planet(pl, dist)){

	 if(me->p_ship.s_type == SCOUT && pl->pl_armies > MAX_SCOUT_BOMB)
	    return;
	 if(myteam_bombing(_state.assault_planet, _state.assault_planet->pl_mydist))
	    return;
	 if(pl->pl_mydist < 20000 && pl_defended(pl, 2))
	    return;

	 bomb_c(pl, "needs bombed");
      }
   }
}
check_bombdamage(pl)
   
   struct planet	*pl;
{
   if(me->p_ship.s_type == SCOUT &&
      pl->pl_armies > MAX_SCOUT_BOMB){
      unassault_c("too many armies");
   }

#ifdef nodef
   /* experiment */
   if(pl_attacking()){
      if(MYDAMAGE() > 25 || MYFUEL() < 60)
	 disengage_c(EDAMAGE, NULL);
   }
#endif

   if(MYDAMAGE() > 75)
      disengage_c(EDAMAGE, NULL, "dam > 75");
}

decide_disengage()
{
}

decide_ogg()
{
   Player *p = _state.current_target;

   if(_state.player_type != PT_OGGER){
      if(!check_ogg(NULL, 20000))
	 return 0;
   }

   if(!p || !p->p || !isAlive(p->p) || (_state.player_type != PT_OGGER && 
   p->p->p_kills == 0.0)){
      unogg_c("player died.");
      return 0;
   }

   if(!WAR(p->p)){
      unogg_c("player peaceful.");
      return 0;
   }

   if(MYFUEL() < 25 && PL_DAMAGE(p) < 20){
      unogg_c("low fuel");
      disengage_c(ELOWFUEL, NULL, "low fuel");
      return 0;
   }
   if(MYFUEL() < 5){
      unogg_c("no fuel");
      disengage_c(ELOWFUEL, NULL, "damage");
      return 0;
   }
   if(MYDAMAGE() > 50 && p->dist > 10000){
      unogg_c("damage");
      disengage_c(ELOWFUEL, NULL, "damage");
      return 0;
   }
   return 1;
}

decide_escort()
{
   return;
}

/* decide to do something */
decide_default()
{
   int	ship = _state.ship;
   struct planet *pl = _state.protect_planet;

   check_alldistress();

   if(check_escort()){
      return;
   }

   if(check_protect(3, &ship, 1)) {
      _state.ship = ship;
#ifdef nodef
      if(_state.ship != ship){
	 if(!_state.dead && home_dist() < 6000){
	    refit_c("protect refit ship");
	 }
      }
#endif
      return;
   }
   else if(inl && _state.protect && pl){
      /* if we're protecting during an inl game and there's more then 3
      planets then we must be protecting armies. */
      if(pl->pl_armies <= 4)
	 unprotectp_c("no more armies on planet");
      else
	 return;
   }
   else
      unprotectp_c("no protect");

   if(check_take(&ship)) {
      struct planet	*cp = me_p?me_p->closest_pl:NULL;
      if(_state.ship != ship && !(cp && (me->p_team == cp->pl_owner) &&
	 cp->pl_armies > 4)){
	 if(!_state.dead){
	    _state.ship = ship;
	    refit_c("take refit ship");
	    untake_c("wait til refit");
	 }
      }
      return;
   }
   else
      unassault_c("nothing to take");

   if(check_ogg(&ship, 20000)){
      _state.ship = ship;
      if(_state.ship != ship && time_to_refit(_state.current_target)){
	 if(!_state.dead){
	    refit_c("ogg refit ship");
	 }
      }
      return;
   }
   else
      unogg_c("no ogg.");


   if(check_bomb(&ship)) {
      if(_state.ship != ship){
	 if((!_state.dead && home_dist() < 8000)){
	    _state.ship = ship;
	    refit_c("bomb refit ship");
	    unassault_c("wait til refit");
	 }
      }
      return;
   }
   else
      unassault_c("nothing to bomb");

   /* look for armies to protect on planets */
   if(check_protect(3, &ship, 0)) {
      _state.ship = ship;
      return;
   }
   else
      unprotectp_c("no armies to protect");

   /* dogfight */
   if(_state.ship == STARBASE || _state.ship == ASSAULT|| ((RANDOM() %2)==1 && (_state.ship==SCOUT))){
      ship = pick_df_ship();
      _state.ship = ship;
      if(home_dist() < 10000){
	 refit_c("dogfight refit ship");
      }
   }
}

pick_df_ship()
{
   int	r = RANDOM()%10;
   if(_state.galaxy && _state.chaos){
      if(r < 2) return DESTROYER;
      if(r < 7) return GALAXY;
      return BATTLESHIP;
   }
   
   if(r < 5) return CRUISER;
   if(r < 7) return BATTLESHIP;
   if(r < 9
       && strcmp(me->p_login, PRE_T_ROBOT_LOGIN) /* if we're ignoring T we should keep the ships large */
       ) return DESTROYER;
   return CRUISER;
}

/* Examine our planets -- determine if we should protect planet */
check_protect(min_pl, ship, value)
   
   int	min_pl;
   int	*ship;
   int	value;
{
   PlanetRec		*pls = _state.planets;
   register		i,j;
   register Player	*p;
   int			plc[MAXPLANETS];
   int			min_p = INT_MAX-1;
   struct planet	*defp = NULL, *cp;
   int			armycount = 1;

   if(!strcmp(me->p_login, PRE_T_ROBOT_LOGIN)) 
      return 0; /* we can't be protecting if we're just maintaining 4 on 4 */

   /* would have gotten here if take failed with too few armies */
   if(me->p_armies > 0 && (pls->total_tarmies + me->p_armies < 3)){
      protectp_c(home_planet(), "protect home -- drop armies");
      return 1;
   }

   if(_server == SERVER_GRIT) return 0;

   if(!value && inl && me->p_kills < 1.0 && me->p_ship.s_type != STARBASE
      && pls->num_teamsp < 13){
      PlanetRec	*pls = _state.planets;

      if(pls->num_teamsp > 12)
	 armycount = 10;
      else if(pls->total_textra_armies > 20)
	 armycount = 7;
      else
	 armycount = 6;

      for(i=0; i< pls->num_teamsp; i++){
	 if(pls->team_planets[i]->pl_armies >= armycount){
	    cp = pls->team_planets[i];
	    if(cp->pl_mydist < min_p){
	       min_p = cp->pl_mydist;
	       defp = cp;
	    }
	 }
      }
      if(defp){
	 j=0;
	 for(i=0, p=_state.players; i < MAXPLAYER; i++,p++){
	    if(p->enemy || !p->p || !isAlive(p->p))
	       continue;
	    if(p->closest_pl && p->closest_pl == defp && p->p->p_kills == 0.0)
	       j++;
	 }
	 if(j <= 2){
	    protectp_c(defp, "protect armies");
	    return 1;
	 }
      }
   }

   if(pls->num_teamsp >= min_pl) return 0;

   for(i=0; i< pls->num_teamsp; i++){
      plc[pls->team_planets[i]->pl_no] = 0;
      if(pls->team_planets[i]->pl_flags & PLAGRI)
	 plc[pls->team_planets[i]->pl_no] --;
   }

   /* find least protected planet */
   for(i=0, p=_state.players; i < MAXPLAYER; i++,p++){
      if(p->enemy || !p->p || !isAlive(p->p))
	 continue;
      for(j=0; j< pls->num_teamsp; j++){
	 if(p->closest_pl && p->closest_pl == pls->team_planets[j])
	    plc[pls->team_planets[j]->pl_no] ++;
      }
   }

   for(i=0; i< pls->num_teamsp; i++){
      if(plc[pls->team_planets[i]->pl_no] < min_p){
	 min_p = plc[pls->team_planets[i]->pl_no];
	 defp = pls->team_planets[i];
      }
   }
   if(!defp)
      return 0;

   if(_state.protect){
      /* should we leave current planet? */
      if(min_p >= plc[_state.protect_planet->pl_no] && (RANDOM()%20)==0)
	 return 1;
   }

   protectp_c(defp, "protect");
   if(ship)
      *ship = BATTLESHIP;
   return 1;
}

check_bomb(ship)
   
   int	*ship;
{
   PlanetRec			*pls = _state.planets;
   register			k;
   register struct planet	*pl;
   register int			ac,mac=0;
   int				cof;
   int				hd = home_dist();
   int				maxa = 0, nu = 0;

#ifdef nodef
   /* tmp */
   return 0;
#endif

   if(pls->total_wextra_armies == 0) return 0;

   /* nobody bombs on grit */
   if(_server == SERVER_GRIT && pls->total_wextra_armies < 50 &&
      pls->total_textra_armies > 5)
      return 0;

   cof = pls->total_warmies / pls->num_warteamsp;

   if(pls->num_warteamsp > 3 && cof < 6){	/* xx */
      if(_state.num_tbombers > 1 && (RANDOM()%20==1))
	 return 0;
   }

   pl = NULL;
   for(k=0; k< pls->num_warteamsp; k++){
      pl = pls->warteam_planets[k];

      pl->pl_needsbombed = 0;

      if(unknownpl(pl)){
	 /* CONFIGURE */
	 pl->pl_armies = 15;
	 nu ++;
      }
      ac = pl->pl_armies;

      if(ac < 5) continue;

      if(ac > maxa) maxa = ac;
      
      if(me->p_ship.s_type == SCOUT && hd >= 20000 &&
	 pl->pl_armies > MAX_SCOUT_BOMB)
	 continue;
      
      if(pl->pl_mydist > 20000 && hd >= 20000 &&
	 (me->p_ship.s_type == CRUISER ||
	  me->p_ship.s_type == BATTLESHIP))
	 continue;
      
      if(myteam_bombing(pl, pl->pl_mydist))
	 continue;

      if(pl->pl_mydist < 20000 && pl_defended(pl, 2))
	 continue;
      
      pl->pl_needsbombed = 1;
   }
   for(k=0; k< pls->num_warteamsp; k++){
      pl = pls->warteam_planets[k];

      if(pl->pl_needsbombed){
	 if(pl->pl_armies > maxa-10 || (nu && unknownpl(pl))
	    || pl->pl_mydist < 10000){
	    if(ship){
	       if(mac > 15 || cof > 15 || nu > 3)
		  *ship = ASSAULT;
	       else{
		  if(_state.galaxy && _server != SERVER_GRIT) /* XXX */
		     *ship = GALAXY;
		  else
		     *ship = SCOUT;
	       }
	       if(maxa >= MAX_SCOUT_BOMB)
		  *ship = ASSAULT;

	       if(pl->pl_mydist < 10000 && pl->pl_armies < 10)
		  *ship = _state.ship;
	       
	       if(pls->num_warteamsp < 3)
		  *ship = ASSAULT;
	    }

	    bomb_c(pl, NULL);
	    return 1;
	 }
      }
   }
   return 0;
}

check_take(ship)
   
   int	*ship;
{
   PlanetRec			*pls = _state.planets;
   register			k;
   register struct planet	*pl, *tpl = NULL;
   int				hd = home_dist();
   int				min_dist = GWIDTH;

   if(!inl && _server != SERVER_GRIT){
      if(pls->num_warteamsp > 5 || pls->total_wextra_armies > 30){
	 if(me->p_kills < 1.5)
	    return 0;
      }
      if(me->p_kills < 1.0)
	 return 0;
   }
   else if(me->p_kills < 1.0)
      return 0;

   if(pls->total_textra_armies == 0 && me->p_armies == 0) return 0;

   /* Look for closest takeable enemy planet */
   for(k=0; k< pls->num_warteamsp; k++){
      pl = pls->warteam_planets[k];

      /*
      if(pl->pl_mydist < 20000 && pl_defended(pl,3)) continue;
      */

      if(pl->pl_mydist < min_dist){
	 if((pl->pl_flags & PLAGRI) && me->p_kills < 1.7)
	    continue;
	 tpl = pl;
	 min_dist = pl->pl_mydist;
      }
   }

   /* take indep planet first over regular planets */
   /* but take agris first if you have the armies */
   if (!tpl || !(tpl->pl_flags&PLAGRI && me->p_armies >= 5) ) {
       min_dist = GWIDTH;	/* ind overrides non-ind */
       for(k=0; k< pls->num_indsp; k++){
	   pl = pls->ind_planets[k];
	   
	   /*
	     if(pl->pl_mydist < 20000 && pl_defended(pl,3)) continue;
	   */

	   if(pl->pl_mydist < min_dist){
	       tpl = pl;
	       min_dist = pl->pl_mydist;
	   }
       }
   }
   if(tpl){
      if(pls->total_textra_armies + me->p_armies < tpl->pl_armies)
	 return 0;

      /*
      printf("decide to take %s\n", tpl->pl_name);
      */
      if(_server == SERVER_GRIT && ship)
	 *ship = ASSAULT;

      if(ship){
            if((home_dist() < 15000 && me->p_kills < 2 && pls->total_textra_armies > 10) || tpl->pl_armies > 20)
                  *ship = ASSAULT;
            else if((tpl->pl_flags & PLAGRI) && me->p_kills < 2)
                  *ship = ASSAULT;
            else if(!strcmp(me->p_login, PRE_T_ROBOT_LOGIN)) /* if we're ignoring T we probably should keep ships larger */
                  *ship = CRUISER;
            else if(me->p_kills >= 2.0 && tpl->pl_armies < 10)
                  *ship = DESTROYER;
            else if(tpl->pl_armies < 10)
                  *ship = SCOUT;
            else
                  *ship = SCOUT;
      }

      take_c(tpl, NULL);
      return 1;
   }
   return 0;
}

check_ogg(ship, dist)
   
   int	*ship;
   int	dist;
{
   register Player	*p, *op = NULL;
   register		i;
   int			mindist = INT_MAX;
   Player 		*co = _state.current_target;

   for(i=0, p =_state.players; i< MAXPLAYER; i++,p++){
      if(!p->enemy || !p->p || !isAlive(p->p) || p->p->p_kills == 0.0) continue;
      if(!p->armies) continue;

      if(p->dist < mindist){
	 mindist = p->dist;
	 op = p;
      }
   }

   if(!inl && mindist > dist)
      return 0;

   if(co == op && _state.ogg_req == 1)
      return 1;

   if(op && op->p->p_ship.s_type != STARBASE){
      ogg_c(op, "carrying");
      if(ship){
	 switch(op->p->p_ship.s_type){
	    case ASSAULT:
	       *ship = CRUISER;
	       break;
	    case DESTROYER:
	       *ship = CRUISER;
	       break;
	    case SCOUT:
             if(!strcmp(me->p_login, PRE_T_ROBOT_LOGIN)) { /* if we're ignoring T we should keep the ships large */
               *ship = CRUISER;
               break;
             }
	       *ship = DESTROYER;
	       break;
	    case CRUISER:
	       *ship = BATTLESHIP;
	       break;
	    case BATTLESHIP:
	       *ship = BATTLESHIP;
	       break;
	    case GALAXY:
	       *ship = BATTLESHIP;
	       break;
	    default:
	       return 0;	/* for now */
	 }
      }
      return 1;
   }
   return 0;
}

check_escort()
{
   return 0;
}

protectp_c(p, s)

   struct planet	*p;
   char			*s;
{
   if(_state.protect && _state.protect_planet == p) return;

   decideNotify("PROTECT", p->pl_name, s);
   _state.protect = 1;
   _state.protect_planet = p;
   _state.planet = _state.protect_planet;
   _state.arrived_at_planet = 0;
}

unprotectp_c(s)
   
   char	*s;
{
   if(!_state.protect) return;

   decideNotify("UNPROTECT", "", s);
   _state.protect = 0;
   _state.protect_planet = NULL;
}

bomb_c(p, s)

   struct planet	*p;
   char			*s;
{
   decideNotify("BOMB", p->pl_name, s);
   _state.assault_req = ASSAULT_BOMB;
   _state.assault = 1;
   _state.assault_planet = p;
   _state.army_planet = NULL;
   _state.newdir = me->p_dir;
   _state.arrived_at_planet = 0;
   _state.lock = 0;
}

unassault_c(s)
   
   char	*s;
{
   if(_state.assault_req == 0) return;

   decideNotify("ASSAULTRESET", s, NULL);
   _state.assault_req = 0;
   _state.assault = 0;
   _state.assault_planet = NULL;
   _state.army_planet = NULL;
   _state.arrived_at_planet = 0;
}

ogg_c(p, s)
   Player	*p;
   char		*s;
{
   if(!p) return;

   decideNotify("OGG", p->p?p->p->p_name:"(NULL)", s);
   _state.ogg_req = 1;
   set_ogg_vars();
   _state.current_target = p;
   _state.newdir = me->p_dir;
   _state.ogg_type = oggtype("x");
   _state.ogg = 0;
}

unogg_c(s)
   char	*s;
{
   if(_state.ogg_req == 0) return;
   decideNotify("OGGRESET", s, NULL);
   _state.ogg_req = 0;
   _state.ogg = 0;
}

take_c(p, s)

   struct planet	*p;
   char			*s;
{
   decideNotify("TAKE", p->pl_name, s);
   _state.assault_req = ASSAULT_TAKE;
   /* let update figure out whether we need to get armies or not */
   _state.assault_planet = p;
   _state.army_planet = NULL;
   _state.newdir = me->p_dir;
   _state.arrived_at_planet = 0;
}

untake_c(s)
   
   char	*s;
{
   if(_state.assault_req == 0) return;

   decideNotify("TAKERESET", s, NULL);
   _state.assault_req = 0;
   _state.assault = 0;
   _state.assault_planet = NULL;
   _state.army_planet = NULL;
   _state.arrived_at_planet = 0;
}

refit_c(s)
   
   char	*s;
{
   if(_server == SERVER_GRIT && MYDAMAGE() < 50){
      sendRefitReq(_state.ship);
      return;
   }

   if(me->p_ship.s_type != _state.ship && isAlive(me) && 
      !hostilepl(home_planet())){
      decideNotify("REFIT", s, NULL);
      _state.disengage = 1;
      _state.diswhy = EREFIT;
      _state.planet = home_planet();
      _state.refit_req = 1;
   }
}

unrefit_c(s)

   char	*s;
{
   if(!_state.refit_req) return;

   decideNotify("RESETREFIT", s, NULL);
   _state.refit_req = 0;
   _state.planet = NULL;
}

disengage_c(w, p, s)
   ediswhy		w;
   struct planet	*p;
   char			*s;
{
   if(_state.disengage && _state.planet == p && !_state.ogg_req) return;

   decideNotify("DISENGAGE", p?p->pl_name:"(no planet)", s);
   _state.disengage	= 1;
   _state.diswhy	= w;
   _state.planet	= p;

   _state.ogg_req	= 0;
}

rdisengage_c(s)
   
   char	*s;
{
   if(!_state.disengage) return;

   decideNotify("RESETDISENGAGE", s?s:"(none)", NULL);
   _state.disengage	 = 0;
   _timers.disengage_time = 0;
   reset_planet();
   _state.lock = 0;
}

escort_c(p, pl, s)

   Player		*p;
   struct planet	*pl;
   char			*s;
{
   decideNotify("ESCORT", p->p->p_mapchars, pl->pl_name, s);
   _state.escort_req = 1;
   _state.escort_player = p;
   _state.escort_planet = pl;
   _state.escort = 0;
}

rescort_c(s)
   char	*s;
{
   if(!_state.escort_req) return;
   decideNotify("RESETESCORT", s, NULL);
   _state.escort_req = 0;
   _state.escort = 0;
   _state.escort_player = NULL;
   _state.escort_planet = NULL;
}

defend_c(p, s)

   Player	*p;
   char	*s;
{
   if(!p) return;
   if(p->p == me) { 
      /*
      fprintf(stderr, "tried to defend self\n");
      */
      return;
   }

   rall_c("defending");
   decideNotify("DEFEND", p->p->p_name, s);
   _state.defend = 1;
   _state.protect_player = p;
}

rall_c(s)
   
   char	*s;
{
   /*	XX
   unprotectp_c(s);
   */
   unassault_c(s);
   if(_state.player_type != PT_OGGER)
      rdefend_c(s);
   unogg_c(s);
   untake_c(s);
   unrefit_c(s);
   rdisengage_c(s);
   rrecharge_c(s);
   rescort_c(s);
   if(_state.player_type != PT_DEFENDER)
      rdefend_c(s);
}

rrecharge_c(s)

   char	*s;
{
   if(!_state.recharge) return;
   decideNotify("RESETRECHARGE", s, NULL);
   _state.recharge = 0;
}

rdefend_c(s)

   char	*s;
{
   if(!_state.defend) return;
   decideNotify("RESETDEFEND", s, NULL);
   _state.defend = 0;
   _state.protect_player = NULL;
}


decideNotify(com, s, s2)

   char	*com,*s, *s2;
{
   static char	buf[80];

   if(read_stdin){
      sprintf(buf, "%s (%s) \"%s\"", com,s?s:"",s2?s2:"");
      warning(buf, 1);
   }
}

me_closest_to_planet(pl, dist)
   
   struct planet	*pl;
   int			dist;
{
   register		i;
   register Player	*p;

   for(i=0, p=_state.players; i < MAXPLAYER; i++,p++){
      if(!p->p || !isAlive(p->p) || p == me_p) continue;

      if(p->closest_pl && p->closest_pl == pl && p->closest_pl_dist < dist){
	 if(p->enemy && dist > 6000) 
	    continue;
	 return 0;
      }
   }
   return 1;
}

myteam_bombing(pl, dist)

   struct planet	*pl;
   int			dist;
{
   register		i;
   register Player	*p;

   for(i=0,p=_state.players; i < MAXPLAYER; i++,p++){
      if(!p->p || !isAlive(p->p) || p->enemy || p == me_p) continue;

      /* add p->bombing or p->taking in here */

      if(p->closest_pl && p->closest_pl == pl && p->closest_pl_dist < dist &&
	 (inl || (_udcounter - p->bombing < 2*60*10)))
	 return 1;
   }
   return 0;
}

pl_defended(pl, c)

   struct planet	*pl;
   int			c;
{
   PlanetRec		*pls = _state.planets;
   register		i, fc=0, ec=0;
   register	Player	*p;

   /* kludge to force bombing if half planets there -- much more important */
   if(pls->num_warteamsp < 5)
      return 0;

   for(i=0,p=_state.players;i < MAXPLAYER;i++,p++){
      if(!p->p || !isAlive(p->p) || p == me_p) continue;

      if(p->closest_pl && p->closest_pl == pl && p->closest_pl_dist < 8000){

	 if(p->robot) return 1;	/* definitely */

	 if(p->enemy) ec++;
	 else
	    fc++;
      }
   }

   if(me->p_flags & PFBOMB)
      ec += pl_attacking();

   if(ec - fc >= c)
      return 1;
   return 0;
}

pl_attacking()
{
   register		i, ec=0;
   register	Player	*p;
   int			m = me->p_no;

   for(i=0,p=_state.players; i< MAXPLAYER; i++,p++){
      if(!p->p || !isAlive(p->p) || p == me_p || !p->enemy) continue;

      if(p->attacking[m] && p->dist < p->hispr+2000){
	 ec++;
      }
   }
   return ec;
}

time_to_refit(p)
   
   Player	*p;
{
   if(p->dist < 25000 && MYFUEL() > 100)
      return 0;
   if(p->dist < 15000 && MYFUEL() > 50)
      return 0;
   if(p->dist < 6000 && MYFUEL() > 20)
      return 0;
   
   return 1;
}

check_alldistress()
{
   register		i;
   register Player	*p;

   if(_state.escort_req || _state.defend)
      return;

   for(i=0,p=_state.players; i< MAXPLAYER; i++){
      if(!p || !p->p || !isAlive(p->p) || p->enemy) continue;

      if(SECONDS(p->sent_distress, 15)){
	 if(check_distress(p))
	    return;
      }
   }
}

/* player sent a distress call in the last 15 seconds. Decide to defend or 
   not */

check_distress(p)

   Player	*p;
{
   register		i;
   register Player	*e;

   /* don't get distracted */
   if(_state.player_type == PT_DEFENDER)
      return;

   /* can't help if we're carrying */
   if(me->p_armies > 0)
      return 0;

   if(SHIP(p) == STARBASE && (PL_SHIELD(p) < 20 || PL_DAMAGE(p) > 30)){
      defend_c(p, "starbase");
      return 1;
   }
   
   /* can't help */
   if(MYFUEL() < 10 || MYDAMAGE() > 60)
      return 0;
   
   /* is he hurting? */
   if(PL_DAMAGE(p) > 20 && p->dist < 20000){
      defend_c(p, "damage");
      return 1;
   }

   /* is he carrying? */
   if(p->armies > 0){
      /* assume undamaged -- check for potential oggers */

      for(i=0,e=_state.players; i< MAXPLAYER; i++,e++){
	 if(!e || !e->p || !isAlive(e->p) || !e->enemy) continue;
	 if(e->attacking[p->p->p_no] && p->distances[i] <= 20000){

	    mprintf("%s attacked by %s\n",
	       p->p->p_mapchars, e->p->p_mapchars);

	    defend_c(p, "carrying, enemy attackers");
	    return 1;
	 }
      }
   }
}
