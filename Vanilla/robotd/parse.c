#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

static struct planet *_find_planet_by_name();

parse_message(m, p, fl)

   char		*m;
   Player	*p;
   int		fl;
{
   if(me->p_ship.s_type == STARBASE) return;		/* not implemented */

   /* handle distress call */
   if(strstr(m, "Distress")){
      if(starbase(p))
	 proc_distress_call(p, m);
      else
	 proc_distress_call(p, m);
      return;
   }
   proc_other(p, m);
}

proc_distress_call(p, m)

   Player	*p;
   char		*m;
{
   int	i, dam, shield, arm, 
	wpn, woe, eoe;		/* TODO */

   if(!p->p || !isAlive(p->p))
      return;

   p->sent_distress = _udcounter;
   
   get_distress(m, &dam, &shield, &arm, &wpn, &woe, &eoe);

   /* update fields */

   if(dam != -1){
      p->damage = (p->p->p_ship.s_maxdamage * dam)/100;
      mprintf("got damage: %d from dc\n", dam);
   }
   if(shield != -1){
      p->shield = (p->p->p_ship.s_maxshield * shield)/100;
      mprintf("got shield: %d from dc\n", shield);
   }
   if(arm != -1){
      p->armies = p->p->p_armies = arm;
      mprintf("got armies: %d from dc\n", arm);
   }
   if(wpn != -1){
      p->wpntemp = (p->p->p_ship.s_maxwpntemp * wpn)/100;
      mprintf("got wpn: %d from dc\n", wpn);
   }
   /* TODO: weapon temp */

   if(arm <= 0){
      p->sent_distress = 0;
      return;
   }

#ifdef nodef
   printf("new info gives dam %d, shield %d, armies %d\n",
      PL_DAMAGE(p), PL_SHIELD(p), p->armies);
#endif
   
   /* decide on escort */

   if(!_state.escort_req)
      check_distress(p);
}

proc_other(p, m)
   
   Player	*p;
   char		*m;
{
   if(SECONDS(p->sent_distress, 30) && p->armies > 0){
      /* hopefully to take */
      proc_hostile_planet_mesg(p, m);
   }
   /* ... */
}

proc_hostile_planet_mesg(p, m)
   
   Player	*p;
   char		*m;
{
   int	w=0;
   char	word[80];

   while(*m){
      if(isupper(*m))
	 *m = tolower(*m);
      if(isspace(*m)){
	 if(w){
	    word[w] = '\0';
	    if(check_hostile_planet_word(p, word))
	       return;
	    w = 0;
	 }
	 m++;
	 while(isspace(*m)) m++;
	 continue;
      }
      else {
	 word[w] = *m;
	 w++;
      }
      m++;
   }
   if(w){
      word[w] = '\0';
      if(check_hostile_planet_word(p, word))
	 return;
   }
}

check_hostile_planet_word(p, w)

   Player	*p;
   char		*w;
{
   struct planet	*pl;

   /*
   printf("word %s\n", w);
   */

   pl = find_planet_by_longname(w);
   if(!pl)
      pl = find_planet_by_shortname(w, /* hostile */0);
   
   if(me->p_armies > 0 || (inl && me->p_kills >= 2.0))
      return;
   
   
   if(pl){

      if(esc_too_far(p, pl))
	 return;

      if(!_state.escort_req || (_state.escort_player == p && 
	 _state.escort_planet != pl)){
	 if((me->p_armies == 0 && !_state.escort_req && 
	    !inl_prot() && rndesc(p) && _state.player_type != PT_DEFENDER) ||
	    (_state.player_type == PT_DEFENDER && 
	     _state.last_defend == _state.escort_player)){
	    if((RANDOM() % 4) == 1 && ship_match(p)){
	       rall_c("to escort");
	       defend_c(p, "from message");
	    }
	    else{
	       rall_c("to escort");
	       escort_c(p, pl, "from message");
	    }
	 }
      }
      return 1;
   }
   else
      return 0;
}

inl_prot()
{
   if(inl && _state.planets->num_teamsp > 3 && _state.protect)
      return 1;
   return 0;
}

rndesc(p)
   
   Player	*p;
{
   int		armies;

   if(!p){
      mprintf("rndesc: no player (0)\n");
      return 0;
   }
   armies = p->armies;
   if(armies > 3) { 
      mprintf("rndesc: armies > 3, (1)\n");
      return 1;
   }
   else{
      mprintf("rndesc: armies: %d\n", armies);
      switch(armies){
	 case 1: return ((RANDOM() % 4) == 1);
	 case 2: return ((RANDOM() % 3) == 1);
	 case 3: return ((RANDOM() % 2) == 1);
      }
   }
   mprintf("rndesc: unknown\n");
   return 0;
}
      

ship_match(p)
   
   Player	*p;
{
   if(!p || !p->p || !isAlive(p->p))
      return 0;
   switch(p->p->p_ship.s_type){
      case BATTLESHIP:
	 if(me->p_ship.s_type != BATTLESHIP)
	    return 1;
	 break;
      case CRUISER:
	 if(me->p_ship.s_type != BATTLESHIP &&
	    me->p_ship.s_type != ASSAULT &&
	    me->p_ship.s_type != CRUISER)
	    return 1;
	 break;
      case DESTROYER:
	 if(me->p_ship.s_type == SCOUT)
	    return 1;
	 break;
      case ASSAULT:
	 if(me->p_ship.s_type != BATTLESHIP &&
	    me->p_ship.s_type != ASSAULT)
	    return 1;
	 break;
      default:
	 return 0;
   }
   return 0;
}

init_plarray()
{
   register	i;
   char 	*short_name();
   int		plcmp_long();

   mprintf("initializing planets\n");

   for(i=0; i< MAXPLANETS; i++){
      plnamelist[i].pm_name   = planets[i].pl_name;
      plnamelist[i].pm_short  = strdup(short_name(&planets[i]));
      plnamelist[i].pm_planet = &planets[i];
   }

   qsort(plnamelist, MAXPLANETS, sizeof(struct planetmatch), plcmp_long);
}

#define S_SHORT		0
#define S_LONG		1

/*
 * Find planet by 2 or 3 character name. 
 * f indicates whether we're looking for a friendly planet or not.
 */

struct planet *find_planet_by_shortname(n, f)

   char	*n;
   int	f;	/* for short names -- look for friendly or hostile */
{
   struct planet		*amb;
   int				hostile, l;

       /* uh oh.  disambiguate */
   if(strcmp(n, "pol") == 0){
      amb = _find_planet_by_name("pola", 4, S_SHORT);
      hostile = unknownpl(amb) || hostilepl(amb);
      if((!f && hostile) || (f && !hostile))
	 return amb;
      else
	 return _find_planet_by_name("poll", 4, S_SHORT);
   }
   else if(strcmp(n, "cas") == 0){
      amb = _find_planet_by_name("cast", 4, S_SHORT);
      hostile = unknownpl(amb) || hostilepl(amb);
      if(!f && hostile || (f && !hostile))
	 return amb;
      else
	 return _find_planet_by_name("cass", 4, S_SHORT);
   }
   l = strlen(n);
   if(l > 3) l = 3;
   return _find_planet_by_name(n, l, S_SHORT);
}

/*
 * find planet by full name either capitalized or not.
 */

struct planet *find_planet_by_longname(n)

   char	*n;
{
   struct planet	*p;
   char			buf[80];

   strcpy(buf, n);

   p = _find_planet_by_name(buf, strlen(n), S_LONG);
   if(!p && islower(buf[0])){
      buf[0] = toupper(buf[0]);
      return _find_planet_by_name(buf, strlen(n), S_LONG);
   }
   return p;
}

static struct planet *_find_planet_by_name(n, l, d)

   char	*n;
   int	l;
   int	d;
{
   char				buf[80];
   static   struct planetmatch	key;
   struct planetmatch		*r;
   int				plcmp_short(), plcmp_long(), (*proc)();

   strncpy(buf, n, l);
   buf[l] = '\0';

   if(d == S_SHORT){
      proc = plcmp_short;
      key.pm_short = buf;
   }
   else {
      proc = plcmp_long;
      key.pm_name = buf;
   }

   r = (struct planetmatch *) bsearch(&key, plnamelist, MAXPLANETS,
      sizeof(struct planetmatch), proc);
   if(!r)
      return NULL;
   return r->pm_planet;
}

plcmp_short(p1, p2)

   struct planetmatch	*p1, *p2;
{
   return strcmp(p1->pm_short, p2->pm_short);
}

plcmp_long(p1, p2)

   struct planetmatch	*p1, *p2;
{
   return strcmp(p1->pm_name, p2->pm_name);
}

char *short_name(pl)

   struct planet        *pl;
{
   char         *n = pl->pl_name;
   static char  sname[5];

   if(isupper(n[0]))
      sname[0] = tolower(n[0]);

   sname[1] = n[1];
   sname[2] = n[2];
   sname[3] = '\0';

   if(sname[2] == 'N')	/* El Nath */
      sname[2] = '\0';
   
   if( (strncmp(sname, "pol", 3) == 0) ||
       (strncmp(sname, "cas", 3) == 0)){
      sname[3] = n[3];
      sname[4] = '\0';
   }

   return sname;
}

char *short_name_print(pl)

   struct planet        *pl;
{
   char         *n = pl->pl_name;
   static char  sname[5];

   if(isupper(n[0]))
      sname[0] = tolower(n[0]);
   sname[1] = n[1];
   sname[2] = n[2];
   sname[3] = '\0';

   if(sname[2] == 'N')	/* El Nath */
      sname[2] = '\0';

   return sname;
}

esc_too_far(p, pl)
   
   Player		*p;
   struct planet	*pl;
{
   int	sp;
   sp = his_ship_faster(p);
   if(sp > 0)
      return pl->pl_mydist > dist_to_planet(p, pl) - 10000;
   else if(sp < 0)
      return pl->pl_mydist > dist_to_planet(p, pl) + 10000;
   else
      return pl->pl_mydist > dist_to_planet(p, pl) - 3000;
}

his_ship_faster(p)

   Player	*p;
{
   int	sh, sm;

   if(!p || !p->p) return 0;	/* xx */
   sh = p->p->p_ship.s_type;
   sm = me->p_ship.s_type;

   switch(sh){
      case SCOUT:
	 switch(sm){		/* my ship */
	    case SCOUT:
	       return 0;
	    case BATTLESHIP:
	       return 5;
	    case CRUISER:
	       return 3;
	    case GALAXY:
	       return 3;
	    case DESTROYER:
	       return 2;
	    case ASSAULT:
	       return 5;
	 }
	 break;
      case BATTLESHIP:
	 switch(sm){		/* myship */
	    case SCOUT:
	       return -5;
	    case BATTLESHIP:
	       return 0;
	    case CRUISER:
	       return -1;
	    case GALAXY:
	       return -1;
	    case DESTROYER:
	       return -4;
	    case ASSAULT:
	       return 0;
	 }
	 break;
      case CRUISER:
	 switch(sm){		/* myship */
	    case SCOUT:
	       return -4;
	    case BATTLESHIP:
	       return 1;
	    case CRUISER:
	       return 0;
	    case GALAXY:
	       return 0;
	    case DESTROYER:
	       return -3;
	    case ASSAULT:
	       return 1;
	 }
	 break;
      case GALAXY:
	 switch(sm){		/* myship */
	    case SCOUT:
	       return -4;
	    case BATTLESHIP:
	       return 1;
	    case CRUISER:
	       return -1;
	    case GALAXY:
	       return 0;
	    case DESTROYER:
	       return -3;
	    case ASSAULT:
	       return 1;
	 }
	 break;
      case DESTROYER:
	 switch(sm){		/* myship */
	    case SCOUT:
	       return -1;
	    case BATTLESHIP:
	       return 4;
	    case CRUISER:
	       return 3;
	    case GALAXY:
	       return 3;
	    case DESTROYER:
	       return 0;
	    case ASSAULT:
	       return 4;
	 }
	 break;
      case ASSAULT:
	 switch(sm){		/* myship */
	    case SCOUT:
	       return -5;
	    case BATTLESHIP:
	       return 0;
	    case CRUISER:
	       return -1;
	    case GALAXY:
	       return -1;
	    case DESTROYER:
	       return -4;
	    case ASSAULT:
	       return 0;
	 }
   }
   return 0; /* xx */
}

get_distress(m, dam, shield, arm, wpn, woe, eoe)
   
   char	*m;
   int	*dam,*shield,*arm,*wpn, *woe, *eoe;
{
   char	*s;

   if((s=strstr(m, "damage"))||(s=strstr(m, "dmg"))){
      s -= 4;
      if(!sscanf(s, "%d", dam))
	 *dam = -1;
   }
   if((s=strstr(m, "shields"))||(s=strstr(m, "shld"))){
      s -= 5;
      if(!sscanf(s, "%d", shield))
	 *shield = -1;
   }
   if((s=strstr(m, "armies"))||(s=strstr(m, "arms"))){
      s -= 3;
      if(*s == ',') s++;		/* xx */
      if(!sscanf(s, "%d", arm))
	 *arm = -1;
   }
   if((s=strstr(m, "Wtmp")) || (s=strstr(m, "wtmp"))){
      s -= 4;
      if(*s == ',') s++;
      if(!sscanf(s, "%d", wpn))
	 *wpn = -1;
   }
}
