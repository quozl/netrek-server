/*
 * dmessage.c 
 *
 * for the client of a socket based protocol.
 */
#include "copyright.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

char * randbye();
char * ogg_targ();

static char *_commands[] = {

   "",
   "DEBUGGING",
   "d?               - show state of debug variables",
   "dassault         - show assault information",
   "dcourse          - show course information",
   "ddis             - show disengage information",
   "denemy           - show enemy information",
   "dengage          - show engage information",
   "descort          - show escort information",
   "dogg             - show ogg information",
   "dhits            - show hits information",
   "dresp            - show response time",
   "dserver          - show server information",
   "dstate           - show state execution",
   "dtime            - show time information",
   "dwar             - show war information",
   "showstate        - show all state variables",

   "",
   "GENERAL",
   "exit             - force immediate exit",
   "flush            - flush stdin/stderr",
   "log              - log stdout/stderr",
   "help [categ]     - this message",
   "hoser            - send in practice robot",
   "reset            - reset state",
   "rnowait          - set rwatch to non-blocking mode",
   "rwatch <host>    - set rwatch on host (run rwatch first)",
   "qrwatch          - terminate rwatch",
   "quit             - quit",

   "",
   "TOGGLES",
   "borgd            - toggle borg detect",
   "tn               - take notify -- use 'E' before taking planet",
   "chaos            - set KSU chaos mode",
   "detall           - toggle det all (as opposed to det single torp)",
   "galaxy           - allow galaxy class ship",
   "galaxymode       - set galaxy mode",
   "havepl           - toggle have-plasmas",
   "human            - toggle human reactions",
   "ignoresudden     - ignore players just entering game",
   "itourn           - toggle ignore-t-mode",
   "killer           - toggle killer (ignore fuel reserve)",
   "master           - set master mode (other robots on team)",
   "normalmode       - set non-galaxy mode",
   "nospeed          - toggle no-ship-speed-from-server",
   "notspeed         - toggle no-torp-speed from server",
   "override         - toggle automatic t-mode decisions",
   "ptorpbounce	     - toggle plasma torp 'ping-pong' affect",
   "sb <player>      - toggle docking on specified sb",
   "tn               - notify team when taking planet",
   "torpseek         - toggle torpseek (BB,SB)",
   "torpwobble       - toggle torp wobble",
   "randtorp         - toggle non-perfect torps",
   "serverst         - ?",
   "vector           - toggle vector-torps",
   "vquiet           - don't given version in response to '     '",
   "udp	             - toggle UDP",
   "udprecv          - set UDP receive mode",
   "wrap             - toggle wrap-around galaxy",
   "woff             - toggle no weapon",
   "coff             - toggle no cloak",
   "hcr              - toggle logic that assumes humans carry",
   "ogh (ogg happy)  - switch logic to ogg carriers while bombing",
   "robdc            - logic says robots don't carry",

   "",
   "QUERY",
   "astats           - show all army stats",
   "est [<player>]   - given enemy stats",
   "fst [<player>]   - give friend stats",
   "stats            - show robot stats",
   "orating          - show ogg-difficulty rating",
   "w?               - show war/peace with all teams",

   "",
   "VARIABLES",
   "adist <num>      - set assault evasion distance",
   "arange <num>     - set assault course range avoidance",
   "cdist <num>      - set when to cloak for ogg",
   "detconst <value> - set det damage constant",
   "lookahead <tf>   - set max torpfuse look ahead for dodge",
   "eradius <value>  - set evasion radius for starbase oggs",
   "hm <val>         - set level of difficulty (> = easier)",
   "odist <num>      - set ogg evasion distance",
   "ospeed <num>     - set value to add to speed during ogg",
   "ovars            - show all variables related to starbase ogging",
   "peace <team>     - make peace with team",
   "serverd <float>  - set server delay",
   "ship <ship>      - set ship",
   "team <team>      - set team",
   "ttracta <angle>  - set tractor-into-torp angle",
   "ttractd <dist>   - set tractor-into-torp distance",
   "upd <num>        - set update time in tenths of seconds",
   "war <team>       - declare war with team",

   "",
   "COMMANDS",
   "?<player|FKOR|A> - send a message",
   "bomb <planet>    - bomb planet",
   "goto <pl>        - come to player",
   "defend <player>  - defend player",
   "escort <pl> <pln>- escort player to planet",
   "ign <player>     - ignore player",
   "ogg <player>[d]  - ogg player (d can be one of 'lrt')",
   "osync <player>   - synchronize ogg with specified player",
   "ospeed <val>     - speed to attach during ogg",
   "protect <planet> - protect planet",
   "refit <ship>     - refit",
   "req <req>        - send low level server request code",
   "take <planet>    - take planet",
   "undefend         - reset defend",
   "unign <player>   - no longer ignore player",
   "unprotect        - reset protect",

   "",
   NULL,
};

#define FRIEND	1
#define HOSTILE	2

#define VERSION		"robot, [AI %s]" 

Player 	*id_to_player();
eoggtype oggtype();
char 	*oggtype_to_string();

char *version()
{
   static char	buf[80];
   extern char	revision[];

   sprintf(buf, VERSION, revision);
   return buf;
}

dmessage(message,flags,from,to)
char *message;
unsigned char flags, from, to;
{
    /* Message from someone.
      Pass it on to robot for processing */
    R_ProcMessage(message, flags, from, to, 0, 0);
}

init_comm()
{
   extern	char *commfile;		/* main.c */
   FILE		*fi;
   char		buf[80];
   if(!commfile)
      return;

   fi = fopen(commfile, "r");
   if(!fi){
      perror(commfile);
      return;
   }
   while(fgets(buf, 78, fi)){
      R_ProcMessage(buf, 0, -1, (unsigned char)-1, (unsigned char)1, 1);
   }
   fclose(fi);
}

instr(string1, string2)
char *string1, *string2;
{
    char *s;
    int length;

    length=strlen(string2);
    for (s=string1; *s!=0; s++) {
	if (*s == *string2 && strncmp(s, string2, length)==0) return(1);
    }
    return(0);
}

R_ProcMessage(message, flags, from, to, std, config)

   char			*message;
   unsigned char	flags, from, to;
   int			std;
   int                  config;	/* if 1 commands are coming from config file */
{
   char			*m, buf[256];

   if(from == me->p_no) return;

   if((!std /*&& (rsock == -1)*/) || (!std && (flags & MINDIV)))
      warning(message, 1);
   
   if(!std && !(flags & MINDIV)){
     process_general_message(message, flags, from, to);
   }
   if(std) 
      m = message;
   else
      /* everything after 11 characters is message */
      m = &message[10];
   
   /* pig client */
   if(!std && (strcmp(m, "     ")==0)){
      if(!_state.vquiet){
	 sendMessage(version(), MINDIV, from);
	 sprintf(buf, "version request from %d", from);
	 response(buf);
      }
      return;
   }

   /* don't allow players to message bots. 
      But allow local admin to send directives through terminal
      also allow bots to read commands file. 
   */
   if (inl && !std) return;

   /* prevent players from giving bots directives to prevent abuse */
#ifdef BOTS_IGNORE_COMMANDS
   if (!config)
      return;
#endif

   if((flags & MINDIV) || std){
     if(!strcmp(me->p_login, PRE_T_ROBOT_LOGIN)
         && players[from].p_team != players[to].p_team) {
       sendMessage("Try pushing around your own team punk.", MINDIV, from);
       return;
     }

      /* nopwd means accept commands from anyone */
      if(!std && (!nopwd || (nopwd && locked))){
	 if(!_state.controller && (from != me->p_no) && from != 255){
	    if(strcmp(m, PASS) == 0){
	       _state.controller = from+1;
	       response("check");
	       return;
	    }
	 }

	 if(from != 255 && (!_state.controller || _state.controller-1 != from)){
	    if(strcmp(m, PASS2) == 0){
	       _state.controller = from+1;
	       response("check");
	       return;
	    }
	    else
	       return;
	 }
      }
      if(nopwd && !locked && !std) _state.controller = from+1;

      if(_state.controller == 20)
	 _state.controller = me->p_no+1;	/* XX */
      
      if(strncmp(m, "lock", 4)==0){
	 if(/* from >= 0 &&*/ from < MAXPLAYER){
	    locked = 1;
	    _state.controller = from+1;
	    sprintf(buf, "Locked to %c%c", players[from].p_mapchars[0],
					   players[from].p_mapchars[1]);
	    response(buf);
	 }
      }
      else if(strncmp(m, "unlock", 6)==0){
	 locked = 0;
	 sprintf(buf, "No longer locked to %c%c", players[from].p_mapchars[0],
					   players[from].p_mapchars[1]);
	 response(buf);
      }
      else if(strncmp(m, "msock", 5)==0){
	 if(connect_master(m))
	    response("yes master.\n");
	 else
	    response("no master.\n");
      }
      else if(strncmp(m, "quit", 4)==0){
	 response(randbye());
	 if(_state.player_type == PT_OGGER)
	    exitRobot(0);
	 else
	    _state.command = C_QUIT;
      }
      else if(strncmp(m, "vquiet", 4)==0){
	 _state.vquiet = !_state.vquiet;
	 sprintf(buf, "vquiet: %s", _state.vquiet?"true":"false");
	 response(buf);
      }
      else if(strncmp(m, "reset", 5)==0){
	 _state.command = C_RESET;
	 response("resetting.");
      }
      else if(strncmp(m, "override", 5)==0){
	 _state.override = !_state.override;
	 sprintf(buf, "override: %s", _state.override?"true":"false");
	 if(_state.override){
	    _state.command = C_RESET;
	    unassault_c("override");
	    unprotectp_c("override");
	 }
	 response(buf);
      }
      else if(strncmp(m, "d?", 2)==0){			/* debug query */
	 char	buf[80];
	 response("dstate, ddisengage, dcourse, dhits");
	 sprintf(buf, "debug status: ");
	 if(DEBUG & DEBUG_STATE)
	    strcat(buf, "dstate ");
	 if(DEBUG & DEBUG_DISENGAGE)
	    strcat(buf, "ddisengage ");
	 if(DEBUG & DEBUG_COURSE)
	    strcat(buf, "dcourse");
	 if(DEBUG & DEBUG_HITS)
	    strcat(buf, "dhits");
	 if(DEBUG & DEBUG_TIME)
	    strcat(buf, "dtime");
	 if(DEBUG & DEBUG_SERVER)
	    strcat(buf, "dserver");
	 if(DEBUG & DEBUG_ENEMY)
	    strcat(buf, "denemy");
	 if(DEBUG & DEBUG_PROTECT)
	    strcat(buf, "dprotect");
	 if(DEBUG & DEBUG_ENGAGE)
	    strcat(buf, "dengage");
	 if(DEBUG & DEBUG_WARFARE)
	    strcat(buf, "dwarfare");
	 if(DEBUG & DEBUG_ASSAULT)
	    strcat(buf, "dassault");
	 if(DEBUG & DEBUG_RESPONSETIME)
	    strcat(buf, "dresp");
	 if(DEBUG & DEBUG_OGG)
	    strcat(buf, "dogg");
	 if(DEBUG & DEBUG_ESCORT)
	    strcat(buf, "descort");
	 if(DEBUG & DEBUG_SHMEM)
	    strcat(buf, "dshmem");
	 response(buf);
      }
      else if(strncmp(m, "dstate", 3) == 0){
	 if(DEBUG & DEBUG_STATE){
	    DEBUG &= ~DEBUG_STATE;
	    response("dstate off.");
	 }
	 else{
	    DEBUG |= DEBUG_STATE;
	    response("dstate on.");
	 }
      }
      else if(strncmp(m, "ddis", 2) == 0){
	 if(DEBUG & DEBUG_DISENGAGE){
	    DEBUG &= ~DEBUG_DISENGAGE;
	    response("ddisengage off.");
	 }
	 else{
	    DEBUG |= DEBUG_DISENGAGE;
	    response("ddisengage on.");
	 }
      }
      else if(strncmp(m, "dcou", 2) == 0){
	 if(DEBUG & DEBUG_COURSE){
	    DEBUG &= ~DEBUG_COURSE;
	    response("dcourse off.");
	 }
	 else {
	    DEBUG |= DEBUG_COURSE;
	    response("dcourse on.");
	 }
      }
      else if(strncmp(m, "dhits", 2) == 0){
	 if(DEBUG & DEBUG_HITS){
	    DEBUG &= ~DEBUG_HITS;
	    response("dhits off.");
	 }
	 else {
	    DEBUG |= DEBUG_HITS;
	    response("dhits on.");
	 }
      }
      else if(strncmp(m, "dtime", 3)==0){
	 if(DEBUG & DEBUG_TIME){
	    DEBUG &= ~DEBUG_TIME;
	    response("dtime off.");
	 }
	 else {
	    DEBUG |= DEBUG_TIME;
	    response("dtime on.");
	 }
      }
      else if(strncmp(m, "dserver", 4)==0){
	 if(DEBUG & DEBUG_SERVER){
	    DEBUG &= ~DEBUG_SERVER;
	    response("dserver off.");
	 }
	 else {
	    DEBUG |= DEBUG_SERVER;
	    response("dserver on.");
	 }
      }
      else if(strncmp(m, "denemy", 4)==0){
	 if(DEBUG & DEBUG_ENEMY){
	    DEBUG &= ~DEBUG_ENEMY;
	    response("denemy off.");
	 }
	 else {
	    DEBUG |= DEBUG_ENEMY;
	    response("denemy on.");
	 }
      }
      else if(strncmp(m, "dpro", 4)==0){
	 if(DEBUG & DEBUG_PROTECT){
	    DEBUG &= ~DEBUG_PROTECT;
	    response("dpro off.");
	 }
	 else {
	    DEBUG |= DEBUG_PROTECT;
	    response("dpro on.");
	 }
      }
      else if(strncmp(m, "dassault", 4)==0){
	 if(DEBUG & DEBUG_ASSAULT){
	    DEBUG &= ~DEBUG_ASSAULT;
	    response("dassault off.");
	 }
	 else {
	    DEBUG |= DEBUG_ASSAULT;
	    response("dassault on.");
	 }
      }
      else if(strncmp(m, "astats", 6)==0){
	 output_astats();
      }
      else if(strncmp(m, "dresp", 4)==0){
	 if(DEBUG & DEBUG_RESPONSETIME){
	    DEBUG &= ~DEBUG_RESPONSETIME;
	    response("dresp off.");
	 }
	 else {
	    DEBUG |= DEBUG_RESPONSETIME;
	    response("dresp on.");
	 }
      }
      else if(strncmp(m, "dogg", 4)==0){
	 if(DEBUG & DEBUG_OGG){
	    DEBUG &= ~DEBUG_OGG;
	    response("dogg off.");
	 }
	 else {
	    DEBUG |= DEBUG_OGG;
	    response("dogg on.");
	 }
      }
      else if(strncmp(m, "dengag", 4)==0){
	 if(DEBUG & DEBUG_ENGAGE){
	    DEBUG &= ~DEBUG_ENGAGE;
	    response("dengage off.");
	 }
	 else {
	    DEBUG |= DEBUG_ENGAGE;
	    response("dengage on.");
	 }
      }
      else if(strncmp(m, "dwar", 4) == 0){
	 if(DEBUG & DEBUG_WARFARE){
	    DEBUG &= ~DEBUG_WARFARE;
	    response("dwar off.");
	 }
	 else{
	    DEBUG |= DEBUG_WARFARE;
	    response("dwar on.");
	 }
      }
      else if(strncmp(m, "desc", 4) == 0){
	 if(DEBUG & DEBUG_ESCORT){
	    DEBUG &= ~DEBUG_ESCORT;
	    response("descort off.");
	 }
	 else{
	    DEBUG |= DEBUG_ESCORT;
	    response("descort on.");
	 }
      }
      else if(strncmp(m, "dshmem", 4) == 0){
	 if(DEBUG & DEBUG_SHMEM){
	    DEBUG &= ~DEBUG_SHMEM;
	    response("dshmem off.");
	 }
	 else{
	    DEBUG |= DEBUG_SHMEM;
	    response("dshmem on.");
	 }
      }
      else if(strncmp(m, "stats", 2)==0)
	 output_stats();
      else if(strncmp(m, "ship", 4)==0){
	 char	*name;
	 int ship = ship_int(&m[5], &name);
	 if(ship == -1){
	    response("unknown ship type.");
	 }
	 else {
	    sprintf(buf, "ship set to %s.", name);
	    _state.ship = ship;
	    response(buf);
	 }
      }
      else if(strncmp(m, "orating", 3)==0){
	 if(oggv_packet){
	    sprintf(buf, "ogg rating: %d", def);
	    response(buf);
	 }
      }
      else if(strncmp(m, "team", 4)==0){
	 int t = team_int(&m[5]);

	 if(t != -1){
	    _state.team = t;
	    sprintf(buf, "team set to %s", &m[5]);
	    response(buf);
	 }
	 else{
	    _state.team = -1;
	    response("Team none.");
	 }
      }
      else if(strncmp(m, "peace", 5)==0){
	 char t = team_bit(&m[6]);
	 if(t != -1){
	    if(t & me->p_swar){
	       response("at war with team.");
	    }
	    else if(!(t & me->p_hostile)){
	       response("already peaceful");
	    }
	    else {
	       response("declaring peace");
	       me->p_hostile &= ~t;
	       sendWarReq(me->p_hostile);
	    }
	 }
	 else 
	    response("unknown team.");
      }
      else if(strncmp(m, "war", 3)==0){
	 char t = team_bit(&m[4]);
	 if(t != -1){
	    if(t & me->p_swar || (t & me->p_hostile)){
	       response("already at war.");
	    }
	    else {
	       response("declaring war");
	       sendWarReq(me->p_hostile^t);
	    }
	 }
	 else 
	    response("unknown team.");

      }
      else if(strncmp(m, "detconst", 8)==0){
	 int	val = atoi(&m[9]);
	 _state.det_const = val;

	 sprintf(buf, "detconst set to %d", val);
	 response(buf);
      }
      else if(strncmp(m, "w?", 2)==0){
	 strcpy(buf, "At war with: \"");
	 if(me->p_swar & FED) 
	    strcat(buf, "fed ");
	 if(me->p_swar & ROM) 
	    strcat(buf, "rom ");
	 if(me->p_swar & KLI) 
	    strcat(buf, "kli ");
	 if(me->p_swar & ORI) 
	    strcat(buf, "ori ");

	 strcat(buf, "\". Hostile to: \"");
	 if(me->p_hostile & FED) 
	    strcat(buf, "fed ");
	 if(me->p_hostile & ROM) 
	    strcat(buf, "rom ");
	 if(me->p_hostile & KLI) 
	    strcat(buf, "kli ");
	 if(me->p_hostile & ORI) 
	    strcat(buf, "ori ");
	 strcat(buf, "\".");
	 response(buf);
      }
      else if(strncmp(m, "goto", 3)==0){
	 /* xx - needs work */
	 _state.command = C_COMEHERE;
	 response("coming..");
      }
      else if(strncmp(m, "protect", 7)==0){
	 _state.protect = 1;
	 _state.protect_planet = name_to_planet(&m[8], 0);
	 _state.planet = _state.protect_planet; /* in case disengaging */
	 _state.arrived_at_planet = 0;
	 if(!_state.protect_planet){
	    response("unknown or hostile planet");
	    _state.protect = 0;
	 }
	 else{
	    response("got it.");
	 }
      }
      else if(strncmp(m, "defend", 6) == 0){
	 _state.defend = 1;
	 _state.protect_player = id_to_player(&m[7], FRIEND);
	 if(!_state.protect_player){
	    response("unknown or hostile player");
	    _state.defend = 0;
	 }
	 else{
	    response("got it.");
	    _state.last_defend = _state.protect_player;
	 }
      }
      else if(strncmp(m, "escort", 6) == 0){
	 _state.escort_req = 1;
	 _state.escort_player = id_to_player(&m[7], FRIEND);
	 _state.escort_planet = name_to_planet(&m[9], 1);
	 _state.escort = 0;
	 if(!_state.escort_player){
	    response("unknown or hostile player");
	    _state.escort_req = 0;
	    _state.escort_planet = NULL;
	 }
	 else if(!_state.escort_planet){
	    response("unknown or team planet");
	    _state.escort_req = 0;
	    _state.escort_player = NULL;
	 }
	 else
	    response("got it.");
      }
      else if(strncmp(m, "undefend", 4)==0){
	 _state.defend = 0;
	 _state.protect_player = NULL;
	 response("undefending.");
      }
      else if(strncmp(m, "ogg", 3) == 0){
	 Player	*p;
	 eoggtype	ot;
         char defaultOgg = 'x';
	 _state.ogg_req = 1;
	 set_ogg_vars();
	 p = id_to_player(&m[4], HOSTILE);
         if(strlen(m) == 5) {
           ot = oggtype(&defaultOgg);
         } else {
	   ot = oggtype(&m[5]);
         }
	 if(!p){
	    response("unknown or friendly player");
	    _state.ogg_req = 0;
	 }
	 else {
	    ogg_c(p, "From dmessage");
	    sprintf(buf, "ogging %c%c", p->p->p_mapchars[0],
		p->p->p_mapchars[1]);
	    response(buf);
	 }
      }
      else if(strncmp(m, "osync?", 6)==0){
	 sprintf(buf, "syncing with: %d", _state.ogg_pno - 1);
	 response(buf);
      }
      else if(strncmp(m, "osync", 5) == 0){
	 Player	*p;
	 p = id_to_player(&m[6], FRIEND);
	 if(!p || !p->p || p->p == me){
	    response("syncing with no one.");
	    _state.ogg_pno = 0;
	    shmem_updmysync(-1);
	 }
	 else {
	    _state.ogg_pno = p->p->p_no + 1;
	    sprintf(buf, "ogg syncing with %c%c", 
	       p->p->p_mapchars[0], p->p->p_mapchars[1]);
	    response(buf);
	    shmem_updmysync(_state.ogg_pno-1);
	 }
	 sendOggVPacket();
      }
      else if(strncmp(m, "sb", 2)==0){
	 Player	*p;
	 p = id_to_player(&m[3], FRIEND);
	 if(!p){
	    response("unknown or hostile player");
	 }
	 else{
	    p->sb_dontuse = !p->sb_dontuse;
	    sprintf(buf, "sb_check %s: %s", p->p->p_name, 
	       p->sb_dontuse?"no":"yes");
	    response(buf);
	 }
      }
      else if(strncmp(m, "unprotect", 5)==0){
	 unprotectp_c("from dmessage");
	 response("no longer protecting.");
      }
      else if(strncmp(m, "showstate", 5)==0){
	 show_state();
      }
      else if(strncmp(m, "looka", 5) == 0){
	 int_var(&_state.lookahead, "torp dodge look-ahead", 
	    "<set distance ahead to look for torps>", m, MAXFUSE);
	 if(_state.lookahead == MAXFUSE)
	    _state.lookahead--;	/* special case */
	 sendOggVPacket();
      }
      else if(strncmp(m, "seekc", 5)==0){
	 int	d = (int)strtol(&m[6], NULL, 10);
	 if(d == 0)
	    response("bad number.");
	 else {
	    sprintf(buf, "seek const set to %d", d);
	    response(buf);
	    _state.seek_const = d;
	 }
      }
      else if(strncmp(m, "adist", 5)==0){
	 int_var(&_state.assault_dist, "assault distance", 
	    "<set degree avoidance distance for planet defenders>", m, 20000);
      }
      else if(strncmp(m, "arange", 5)==0){
	 int_var(&_state.assault_range, "assault range", 
	    "<set degree avoidance range for planet defenders>", m, 50);
      }
      else if(strncmp(m, "eradius", 5)==0){
	 if(int_var(&evade_radius, "evasion radius", 
	    "<distance to start cloak evasion>", m, 4000+rrnd(2000)))
	    set_ogg_vars();
      }
      else if(strncmp(m, "cdist", 5)==0){
	 int_var(&cloak_odist, "cloak distance", 
	    "<distance when to cloak>", m, 8000+rrnd(4000));
	 sendOggVPacket();
      }
      else if(strncmp(m, "odist", 5)==0){
	 int_var(&_state.ogg_dist, "ogg uncloak distance", 
	    "<distance at which to uncloak to ogg>", m,
	    4500 + rrnd(1500));
      }
      else if(strncmp(m, "ospeed", 6)==0){
	 int_var(&_state.ogg_speed, "ogg speed increment",
	    "<value to add to speed during ogg>", m,
	    2 + rrnd(4));
      }
      else if(strncmp(m, "serverd", 7) == 0){
	 double	sd = (double)atoi(&m[8]); /* strtod(&m[8], NULL); Linux libc bug */
	 sprintf(buf, "_serverdelay set to %.3g", sd);
	 response(buf);
	 _serverdelay = sd;
      }
      else if(strncmp(m, "ovars", 5)==0){
	 extern unsigned char ogg_incoming_dir;
	 sprintf(buf, 
	    "odist:%d,cdist:%d,eradius:%d,ospeed:%d,dir:%d,target:%s",
	    _state.ogg_dist, cloak_odist, evade_radius, _state.ogg_speed,
	    ogg_incoming_dir, ogg_targ());
	 response(buf);
      }
      else if(strncmp(m, "upd?", 4)==0){
	 sprintf(buf, "updates at %g.2 (%d per second) (cycletime: %d)",
	    _state.timer_delay_ms, (int)(1000000./(_state.timer_delay_ms * 
	       100000.)), _cycletime);
	 response(buf);
	 sendOggVPacket();
      }
      else if(strncmp(m, "upd", 3) == 0){
	 double df = (double)atoi(&m[4]); /* strtod(&m[4], NULL, 10); Linux libc bug */
	 if(df == 0.0)
	    response("bad value.");
	 else{
	    sprintf(buf, "updates set to %d per second.", 
	       (int)(1000000./(df*100000.)));
	    response(buf);
	    _state.timer_delay_ms = (float)df;
	    updates = (float)df;
	    /*sendOptionsPacket();*/
	    sendUpdatePacket((int)(df * 100000.));
	 }
      }
      /* debug */
      else if(strncmp(m, "rv", 2) == 0){
	 extern int dntorps, dtint, dgo;
	 int		tcrs;
	 extern unsigned char dtcrs;

	 if(sscanf(&m[3], "%d %d %d", &dntorps, &dtint, &tcrs) != 3){
	    response("use: rv <ntorps> <interval> <course>.");
	    return;
	 }
	 dtcrs = tcrs;

	 sprintf(buf, "dntorps: %d, dtint: %d, dtcrs: %d.",
	    dntorps, dtint, dtcrs);
	 dgo = 1;
	 response(buf);
      }
      else if(strncmp(m, "est", 3)==0){
	 Player	*p;
	 p = id_to_player(&m[strlen(m)-1], HOSTILE);
	 if(p)
	    output_pstats(p, NULL);
	 else
	    output_pstats(_state.closest_e, "closest:");
      }
      else if(strncmp(m, "fstat", 3)==0){
	 Player	*p;
	 p = id_to_player(&m[strlen(m)-1], FRIEND);
	 if(p)
	    output_pstats(p, NULL);
	 else
	    output_pstats(_state.closest_f, "closest:");
      }
      else if(strncmp(m, "ign", 3)==0){
	 char	c = m[4];
	 int	pno = player_no(c);
	 if(pno < 0 || pno > MAXPLAYER)
	    response("player out of bounds.");
	 else if(!WAR(&players[pno]))
	    response("player already peaceful.");
	 else{
	    set_ignore(pno);
	    sprintf(buf, "ignoring %s.", players[pno].p_name);
	    response(buf);
	 }
      }
      else if(strncmp(m, "unign", 5)==0){
	 char	c = m[6];
	 int	pno = player_no(c);
	 if(pno < 0 || pno > MAXPLAYER)
	    response("player out of bounds.");
	 else if(_state.ignore_e[pno] == '0')
	    response("already ignoring");
	 else{
	    set_unignore(pno);
	    sprintf(buf, "no longer ignoring %s.", players[pno].p_name);
	    response(buf);
	 }
      }
      else if(strncmp(m, "?", 1) == 0){
	 char	c = m[1];
	 if(m[2] == '\0') { m[3] = '\0'; }
	 pmessage(c, &m[3]);
      }
      else if(strncmp(m, " ?", 2) == 0){
	 char	c = m[2];
	 if(m[3] == '\0') { m[4] = '\0'; }
	 pmessage(c, &m[4]);
      }
      else if(strncmp(m, "woff",4)==0){
	 _state.no_weapon = !_state.no_weapon;
	 sprintf(buf, "No weapons: %s", _state.no_weapon?"true":"false");
	 response(buf);
	 sendOggVPacket();
      }
      else if(strncmp(m, "coff",4)==0){
	 no_cloak = ! no_cloak;
	 sprintf(buf, "No cloak: %s", no_cloak?"true":"false");
	 response(buf);
      }
      else if(strncmp(m, "killer", 6) == 0){
	 _state.killer = !_state.killer;
	 sprintf(buf, "killer %s", _state.killer?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "refit", 5)==0){
	 char	*name;
	 int ship = ship_int(&m[6], &name);
	 if(ship == -1){
	    sprintf(buf, "unknown ship: %s", &m[6]);
	    response(buf);
	 }
	 else {
	    sprintf(buf, "refitting to %s.", name);
	    response(buf);
	    _state.ship = ship;
	    _state.disengage = 1;
	    _timers.disengage_time = mtime(0);
	    _state.diswhy = EREFIT;
	    _state.planet = home_planet();
	    _state.refit_req = 1;
	 }
	 
      }
      else if(strncmp(m, "req", 3) == 0){
	 handle_generic_req(m);
      }
      else if(strncmp(m, "take", 4)==0){
	 if(troop_capacity(me) == 0){
	    response("not enough kills.");
	 }
	 else {
	    _state.assault_req = ASSAULT_TAKE;
	    _state.assault = 0;
	    _state.assault_planet = name_to_planet(&m[5], 1);
	    _state.army_planet = NULL;
	    _state.newdir = me->p_dir;
	    _state.arrived_at_planet = 0;
	    if(!_state.assault_planet){
	       response("friendly planet.");
	       _state.assault_req = 0;
	    }
	    else{
	       response("got it.");
	    }
	 }
      }
      else if(strncmp(m, "bomb", 4)==0){
	    _state.assault_req = ASSAULT_BOMB;
	    _state.assault = 1;
	    _state.assault_planet = name_to_planet(&m[5], 1);
	    _state.army_planet = NULL;
	    _state.newdir = me->p_dir;
	    _state.arrived_at_planet = 0;
	    if(!_state.assault_planet){
	       response("friendly planet.");
	       _state.assault_req = 0;
	       _state.assault = 0;
	    }
	    else{
	       response("got it.");
	    }
      }
      else if(strncmp(m, "nospeed", 7)==0){
	 _state.no_speed_given = !_state.no_speed_given;
	 sprintf(buf, "no speed given %s", _state.no_speed_given?"true":
	    "false");
	 response(buf);
      }
      else if(strncmp(m, "notspeed", 7)==0){
	 no_tspeed = !no_tspeed;
	 sprintf(buf, "no torp speed given %s", no_tspeed?"true":
	    "false");
	 response(buf);
      }
      else if(strncmp(m, "torpseek", 7)==0){
	 _state.torp_seek = !_state.torp_seek;
	 sprintf(buf, "torp seek: %s", _state.torp_seek?"true":
	    "false");
	 response(buf);
      }
      else if(strncmp(m, "ptorpbou", 7)==0){
	 _state.torp_bounce = !_state.torp_bounce;
	 sprintf(buf, "torp bounce: %s", _state.torp_bounce?"true":
	    "false");
	 response(buf);
      }
      else if(strncmp(m, "itourn", 6)==0){
	 _state.itourn = !_state.itourn;
	 sprintf(buf, "ignore tmode: %s", _state.itourn?"true":
	    "false");
	 response(buf);
      }
      else if(strncmp(m, "hm", 2)==0){
	 int	d = (int)strtol(&m[3], NULL, 10);
	 if(m[2] == '?'){
	    sprintf(buf, "human level at %d", _state.human);
	    response(buf);
	 }
	 else {
	    sprintf(buf, "human level set to %d", d);
	    response(buf);
	    _state.human = d;
	    /*
	    _state.lookahead = 15 - _state.human;
	    */
	    sendOggVPacket();
	 }
      }
      else if(strncmp(m, "human", 5)==0){
	 _state.human = !_state.human;
	 sprintf(buf, "emulate human: %s", _state.human?"true":
	    "false");
	 response(buf);
      }
      else if(strncmp(m, "hoser", 4) ==0){
	 sendPractrReq();
      }
      else if(strncmp(m, "vector", 4)==0){
	 _state.vector_torps = !_state.vector_torps;
	 sprintf(buf, "vector torps %s", _state.vector_torps?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "wrap", 4)==0){
	 _state.wrap_around = !_state.wrap_around;
	 sprintf(buf, "wrap around %s", _state.wrap_around?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "torpwobble", 5)==0){
	 _state.torp_wobble = !_state.torp_wobble;
	 sprintf(buf, "torp wobble %s", _state.torp_wobble?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "galaxymode", 7)== 0){
	 _state.chaos = 1;
	 _state.galaxy = 1;
	 _state.torp_wobble = 0;
	 _state.wrap_around = 1;
	 response("galaxy mode");
      }
      else if(strncmp(m, "galaxy", 6)==0){
	 _state.galaxy = !_state.galaxy;
	 sprintf(buf, "galaxy ship %s", _state.galaxy?"true":"false");
	 response(buf);
      }
      else if(strncmp(m, "normalmode", 7)==0){
	 _state.chaos = 0;
	 _state.galaxy = 0;
	 _state.torp_wobble = 1;
	 _state.wrap_around = 0;
	 response("normal mode");
      }
      else if(strncmp(m, "serverst", 8)==0){
	 show_serverst();
      }
      else if(strncmp(m, "chaos", 4)==0){
	 _state.chaos = !_state.chaos;
	 sprintf(buf, "ksuchaos %s", _state.chaos?"on":"off");
	 init_ships();
	 response(buf);
      }
      else if(strncmp(m, "ignoresudden", 7)==0){
	 _state.ignore_sudden = !_state.ignore_sudden;
	 sprintf(buf, "ignore_sudden %s", _state.ignore_sudden?"true":"false");
	 response(buf);
      }
      else if(strncmp(m, "rwatch", 6)==0){
	 char	*h = &m[6];
	 register	i;
	 while(isspace(*h)) h++;
	 if(*h){
	    strcpy(rw_host, h);
	    for(i=0; i< 10; i++)
	       if(connectToRWatch(rw_host, 2692) != 0)
		  break;
	    if(rsock > -1){
	       /* force reupdate of client data */
	       shutdown(sock, 2);
	       connectToServer(nextSocket);
	    }
	 }
      }
      else if(strncmp(m, "qrwatch", 7)==0){
	  if (rsock!=-1) {
	      shutdown(rsock,2);
	      rsock= -1;
	  }
      }
      else if(strncmp(m, "rnowait", 5)==0){
	 if(rsock != -1){
	    set_rsock_nowait();
	    response("rwatch set to non-blocking.");
	 }
      }
      else if(strncmp(m, "havepl", 6)==0){
	 _state.have_plasma = !_state.have_plasma;
	 sprintf(buf, "have plasma %s", _state.have_plasma?"true":"false");
	 response(buf);
      }
      else if(strncmp(m, "borgd", 5)==0){
	 _state.borg_detect = !_state.borg_detect;
	 sprintf(buf, "borg detect %s", _state.borg_detect?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "tn", 2)==0){
	 _state.take_notify = !_state.take_notify;
	 sprintf(buf, "take notify %s", _state.take_notify?"true":"false");
	 response(buf);
      }
      else if(strncmp(m, "help", 4)==0){
	 list_all(m);
      }
      else if(strncmp(m, "master", 6)==0){
	 _master = !_master;
	 sprintf(buf, "master %s", _master?"true":"false");
	 response(buf);
      }
#ifdef ATM
      else if(strncmp(m, "udprecv", 7)==0){
	 udpaction(UDP_RECV);
      }
      else if(strncmp(m, "udp", 3)==0){
	 /* toggle */
         if(commMode == COMM_TCP)
	    response("trying UDP");
	 else
	    response("switch to TCP");
	 udpaction(UDP_CURRENT);
      }
      else if(strncmp(m, "force", 5)==0){
	 udpaction(UDP_FORCE_RESET);
      }
      else if(strncmp(m, "=", 1)==0){
	 udpaction(UDP_UPDATE_ALL);
	 response("Sending udp update-all req");
      }
#endif
      else if(strncmp(m, "shortp", 6)==0){
	 if(recv_short)
	    sendShortReq(0);
	 else
	    sendShortReq(1);
      }
      else if(strncmp(m, "log", 3)==0){
	 setlog("./");
	 response("logging on");
      }
      else if(strncmp(m, "flush", 5)==0){
	 fflush(stderr);
	 fflush(stdout);
	 response("descriptors flushed");
      }
      else if(strncmp(m, "exit", 4)==0){
	 response(randbye());
	 exitRobot(0);
      }
      else if(strncmp(m, "poll", 4)==0){
	 pollmode = !pollmode;
	 sprintf(buf, "poll %s", pollmode?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "expltest", 8)==0){
	 expltest = !expltest;
	 sprintf(buf, "fire after explode test: %s", expltest?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "randtorp", 8)==0){
	 randtorp = !randtorp;
	 sprintf(buf, "random torps %s", randtorp?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "hcr", 3)==0){
	 hm_cr = !hm_cr;
	 sprintf(buf, "humans carry %s", hm_cr?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "ogh", 3)==0){
	 ogg_happy = !ogg_happy;
	 sprintf(buf, "ogg while bombing %s", ogg_happy?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "robdc", 5)==0){
	 robdc = !robdc;
	 sprintf(buf, "robots don't carry: %s", robdc?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "detall", 6)==0){
	 detall = !detall;
	 sprintf(buf, "detall %s", detall?"on":"off");
	 response(buf);
      }
      else if(strncmp(m, "ttracta", 7)==0){
	 int	d = (int)strtol(&m[8], NULL, 10);
	 sprintf(buf, "tractor angle set to %d", d);
	 response(buf);
	 tractt_angle = d;
      }
      else if(strncmp(m, "ttractd", 7)==0){
	 int	d = (int)strtol(&m[8], NULL, 10);
	 sprintf(buf, "tractor distance set to %d", d);
	 response(buf);
	 tractt_dist = d;
      }
      else if(strncmp(m, "nowpntemp", 6)==0){
	 me->p_ship.s_maxwpntemp = 99999999;
	 response("weapon temp ignored.");
	 return;
      }
      else if(strncmp(m, "unknown", 7)==0){
	 return;
      }
      else
	 if(!inl)
	    response("unknown command.");
   }
}

set_ignore(pno)

   int	pno;
{
   _state.ignore_e[pno] = '1';
}

set_unignore(pno)

   int	pno;
{
   _state.ignore_e[pno] = '0';
}

team_int(m)

   char	*m;
{
   if(strncmp(m, "fed", 1)==0){
      return 0;
   }
   else if(strncmp(m, "rom", 1)==0){
      return 1;
   }
   else if(strncmp(m, "kli", 1)==0){
      return 2;
   }
   else if(strncmp(m, "ori", 1)==0){
      return 3;
   }
   else
      return -1;
}

char team_bit(m)

   char	*m;
{
   if(strncmp(m, "fed", 1)==0){
      return FED;
   }
   else if(strncmp(m, "rom", 1)==0){
      return ROM;
   }
   else if(strncmp(m, "kli", 1)==0){
      return KLI;
   }
   else if(strncmp(m, "ori", 1)==0){
      return ORI;
   }
   else
      return 0;
}

team_inttobit(t)

   int	t;
{
   switch(t){
      case 0 : return FED;
      case 1 : return ROM;
      case 2 : return KLI;
      case 3 : return ORI;
      default : return 0;
   }
}

ship_int(s, name)

   char *s, **name;
{
   if(strncasecmp(s, "scout", 2)==0){
      *name = "SCOUT";
      return SCOUT;
   }
   else if(strncasecmp(s, "destroy", 3)==0 || strncasecmp(s, "dd", 2)==0){
      *name = "DESTROYER";
      return DESTROYER;
   }
   else if(strncasecmp(s, "cruiser", 3)==0 || strncasecmp(s, "ca", 2)==0){
      *name = "CRUISER";
      return CRUISER;
   }
   else if(strncasecmp(s, "battleship", 3)==0 || strncasecmp(s, "bb", 2)==0){
      *name = "BATTLESHIP";
      return BATTLESHIP;
   }
   else if(strncasecmp(s, "assault", 2)==0){
      *name = "ASSAULT";
      return ASSAULT;
   }
   else if(strncasecmp(s, "starbase", 8)==0 || strncasecmp(s, "sb", 2)==0){
      *name = "STARBASE";
      return STARBASE;
   }
   else if(strncmp(s, "galaxy", 6)== 0){
      *name = "GALAXY";
      return GALAXY;
   }
   *name = "UNKNOWN: ";
   return -1;
}

response(buf)

   char	*buf;
{
   if(_state.controller){
      sendMessage(buf, MINDIV, _state.controller-1);
   }
   warning(buf, 1);
}

pmessage(c, buf)

   char	c, *buf;
{
   int	pno;

   switch(c){
      case 'A':
	 sendMessage(buf, MALL, 0);
	 break;
      case 'F':
	 sendMessage(buf, MTEAM, FED);
	 break;
      case 'R':
	 sendMessage(buf, MTEAM, ROM);
	 break;
      case 'K':
	 sendMessage(buf, MTEAM, KLI);
	 break;
      case 'O':
	 sendMessage(buf, MTEAM, ORI);
	 break;

      default:
	 pno = player_no(c);
	 if(pno < 0 || pno > MAXPLAYER){
	    response("player number out of bounds.");
	    return;
	 }
	 if(!isAlive((&players[pno]))){
	    response("player is not in game.");
	    return;
	 }
	 sendMessage(buf, MINDIV, pno);
	 break;
   }
}

player_no(c)

   char	c;
{
   if(isdigit(c))
      return c - '0';
   else 
      return c - 'a' + 10;
}
      
struct planet *name_to_planet(s, h)

   char	*s;
   int	h;
{
   register	i;
   register	struct planet	*pl;
   char		buf[32];
   int		l = 3, not_hostile;

   if(strncmp(s, "home", 4)==0)
      return home_planet();

   if(isupper(s[0]))
      s[0] = tolower(s[0]);

   if(strncmp(s, "pol", 3) == 0)	/* Polaris/Pollux */
      l = 4;
   if(strncmp(s, "el", 2) == 0)		/* El Nath */
      l = 2;
   if(strncmp(s, "cas", 3) == 0)	/* Castor/Cassiopia */
      l = 4;

   for(i=0, pl=planets; i< MAXPLANETS; i++,pl++){
      strcpy(buf, pl->pl_name);
      buf[0] = tolower(buf[0]);
      if(strncmp(s, buf, l) == 0){
	 if(h == 3) return pl;

	 not_hostile = (me->p_team == pl->pl_owner ||
	    !((me->p_swar | me->p_hostile)& pl->pl_owner));
	 
	 if((not_hostile || unknownpl(pl)) && !h)
	    return pl;
	 else if(((!not_hostile||unknownpl(pl)) && h) || 
	    (h && pl->pl_armies == 0))
	    return pl;
      }
   }
   /* not found */
   return NULL;
}

Player *id_to_player(s, t)

   char	*s;
   int	t;
{
   int pn = player_no(s[0]);
   Player	*p;
   

   if(!*s || pn > MAXPLAYER || pn < 0) return NULL;

   p = &_state.players[pn];

   if(t == FRIEND){
      if(p->enemy || !p->p || !isAlive(p->p))
	 return NULL;
      return p;
   }
   else{
      if(!p->enemy || !p->p || !isAlive(p->p))
	 return NULL;
      return p;
   }
}

eoggtype oggtype(s)

   char	*s;
{
   switch(*s){
      case 'r': return EOGGRIGHT;
      case 'l': return EOGGLEFT;
      case 't': return EOGGTOP;
      default:
	 switch(RANDOM()%3){
	    case 0: return EOGGRIGHT;
	    case 1: return EOGGLEFT;
	    case 2: return EOGGTOP;
	    break;
	 }
   }
}

char *oggtype_to_string(ot)
   
   eoggtype	ot;
{
   switch(ot){
      case EOGGRIGHT : return "right";
      case EOGGLEFT  : return "left";
      case EOGGTOP   : return "top";
      default	     : return "unknown";
   }
}

char *team_to_string(t)

   int	t;
{
   switch(t){
      case FED : return "FED";
      case ROM : return "ROM";
      case ORI : return "ORI";
      case KLI : return "KLI";
      default:
	 return "(none)";
   }
}

char *diswhy_string(e)

   ediswhy	e;
{
   switch(e){
      case ENONE: return "ENONE";
      case ERUNNING: return "ERUNNING";
      case EDAMAGE: return "EDAMAGE";
      case EKILLED: return "EKILLED";
      case ELOWFUEL: return "ELOWFUEL";
      case EPROTECT: return "EPROTECT";
      case EREFIT: return "EREFIT";
      case EOVERHEAT: return "EOVERHEAT";
      default: return "(unknown)";
   }
}

show_serverst()
{
   mprintf("server: ");
   switch(_server){
      case SERVER_AUK:    	mprintf("rwd4.mach.cs.cmu.edu\n"); break;
      case SERVER_BEZIER: 	mprintf("bezier.berkeley.edu\n"); break;
      case SERVER_FOGHORN: 	mprintf("foghorn\n"); break;
      case SERVER_NEEDMORE: 	mprintf("needmore\n"); break;
      case SERVER_GRIT: 	mprintf("grit\n"); break;
      case SERVER_PITT:   	mprintf("pitt\n"); break;
      default:		  	mprintf("local\n");break;
   }

   mprintf("chaos: %s\n", _state.chaos?"on":"off");
   mprintf("vector torps: %s\n", _state.vector_torps?"on":"off");
   mprintf("wrap around: %s\n", _state.wrap_around?"on":"off");
   mprintf("torp wobble: %s\n", _state.torp_wobble?"on":"off");
   mprintf("galaxy ship: %s\n", _state.galaxy?"true":"false");
   mprintf("torp seek: %s\n", _state.torp_seek?"true":"false");
   mprintf("plasma bounce: %s\n", _state.torp_bounce?"true":"false");
}

handle_generic_req(m)
   
   char	*m;
{
   char	*arg;
   int	req_no, arg_no = 0;
   char	buf[64];

   while(!isspace(*m)) m++;
   if(!*m) return;
   while(isspace(*m)) m++;

   arg = m; 
   while(!isspace(*arg)) arg++;
   if(!*arg) return;
   *arg = '\0';
   arg++;

   if(strncmp(m, "list", 2)==0){
      list_reqs();
      return;
   }
   if(!*arg){ 
      response("no argument."); 
      return; 
   }
   req_no = atoi(m);
   arg_no = atoi(arg);
   sprintf(buf, "sendShortPacket(%d, %d)", req_no, arg_no);
   response(buf);
   if(req_no < 1 || req_no > 33){
      response("request out of range");
      return;
   }
   sendShortPacket(req_no, arg_no);
}

list_reqs()
{
   mprintf("CP_TORP (dir)     : %10d\n", CP_TORP);
   mprintf("CP_PHASER (dir)   : %10d\n", CP_PHASER);
   mprintf("CP_SPEED (speed)  : %10d\n", CP_SPEED);
   mprintf("CP_DIRECTION (dir): %10d\n", CP_DIRECTION);
   mprintf("CP_SHIELD (1/0)   : %10d\n", CP_SHIELD);
   mprintf("CP_ORBIT          : %10d\n", CP_ORBIT);
   mprintf("CP_REPAIR (1/0)   : %10d\n", CP_REPAIR);
   mprintf("CP_BEAM (1/0)     : %10d\n", CP_BEAM);
   mprintf("CP_DET_TORPS      : %10d\n", CP_DET_TORPS);
   mprintf("CP_CLOAK (1/0)    : %10d\n", CP_CLOAK);
   mprintf("CP_BOMB           : %10d\n", CP_BOMB);
   mprintf("CP_PLASMA (dir)   : %10d\n", CP_PLASMA);
   mprintf("CP_COUP           : %10d\n", CP_COUP);
   mprintf("CP_BYE            : %10d\n", CP_BYE);
   mprintf("CP_DOCKPERM (1/0) : %10d\n", CP_DOCKPERM);
   mprintf("CP_RESETSTATS 89  : %10d\n", CP_RESETSTATS);
}

show_state()
{
   mprintf("STATE:\n");
   mprintf("%-30s%d\n", "status:", _state.status);
   mprintf("%-30s%s\n", "state:", state_name(_state.state));
   mprintf("%-30s%d\n", "controller:", _state.controller);
   mprintf("%-30s%d\n", "disengage:", _state.disengage);
   mprintf("%-30s%d\n", "closing:", _state.closing);
   mprintf("%-30s%d\n", "do_lock:", _state.do_lock);
   mprintf("%-30s%d\n", "refit:", _state.refit);
   mprintf("%-30s%d\n", "refit_req:", _state.refit_req);
   mprintf("%-30s%s\n", "diswhy:", diswhy_string(_state.diswhy));
   mprintf("%-30s%d\n", "recharge:", _state.recharge);
   mprintf("%-30s%d\n", "protect:", _state.protect);
   mprintf("%-30s%d\n", "arrived_at_planet:", _state.arrived_at_planet);
   mprintf("%-30s%s\n", "protect_planet:",
      _state.protect_planet?_state.protect_planet->pl_name : "(none)");
   mprintf("%-30s%s\n", "planet:",
      _state.planet?_state.planet->pl_name : "(none)");
   mprintf("%-30s%s\n", "clostest_e:",
      _state.closest_e?_state.closest_e->p->p_name : "(none)");
   mprintf("%-30s%d\n", "defend:", _state.defend);
   mprintf("%-30s%d\n", "arrived_at_player:", _state.arrived_at_player);
   mprintf("%-30s%s\n", "protect_player:", 
      _state.protect_player?_state.protect_player->p->p_name:"(none)");
   mprintf("%-30s%s\n", "current_target:",
      _state.current_target?_state.current_target->p->p_name : "(none)");
   mprintf("%-30s%d\n", "ogg:", _state.ogg);
   /*
   printf("%-30s%d\n", "ogg_req:", _state.ogg_req);
   */
   mprintf("%-30s%s\n", "closest_f:",
      _state.closest_f?_state.closest_f->p->p_name : "(none)");
   mprintf("%-30s%d\n", "assault:", _state.assault);
   mprintf("%-30s%d\n", "assault_req:", _state.assault_req);
   mprintf("%-30s%s\n", "assault_planet:", 
      _state.assault_planet?_state.assault_planet->pl_name:"(none)");
   mprintf("%-30s%s\n", "army_planet:", 
      _state.army_planet?_state.army_planet->pl_name:"(none)");

   mprintf("%-30s%d\n", "escort_req:", _state.escort_req);
   mprintf("%-30s%d\n", "escort:", _state.escort);
   mprintf("%-30s%s\n", "escort_player:", _state.escort_player?
      _state.escort_player->p->p_name:"(none)");
   mprintf("%-30s%s\n", "escort_planet:", _state.escort_planet?
      _state.escort_planet->pl_name:"(none)");

   mprintf("%-30s%d\n", "command:", _state.command);
   mprintf("%-30s%d\n", "lasttorpreq:", _state.lasttorpreq);
   mprintf("%-30s%d\n", "lastphaserreq:", _state.lastphaserreq);
   mprintf("%-30s%d\n", "lastdetreq:", _state.lastdetreq);
   mprintf("%-30s%d\n", "torp_i:", _state.torp_i);
   mprintf("%-30s%d\n", "torp_attack:", _state.torp_attack);
   mprintf("%-30s%d\n", "torp_attack_timer:", 
      _state.torp_attack_timer);
   mprintf("%-30s%d\n", "total_enemies:", _state.total_enemies);
   mprintf("%-30s%d\n", "total_friends:", _state.total_friends);
   mprintf("%-30s%d\n", "debug:", _state.debug);
   mprintf("%-30s%d\n", "p_desdir:", (int)_state.p_desdir);
   mprintf("%-30s%d\n", "p_desspeed:", (int)_state.p_desspeed);
   mprintf("%-30s%d\n", "team:", _state.team);
   mprintf("%-30s%d\n", "ship:", _state.ship);
   mprintf("%-30s%d\n", "dead:", _state.dead);
   mprintf("%-30s%d\n", "torpq-n:", _state.torpq->ntorps);
   mprintf("%-30s%d\n", "torp_danger:", _state.torp_danger);
   mprintf("%-30s%d\n", "tdanger_dist:", _state.tdanger_dist);
   mprintf("%-30s%d\n", "assault_dist:", _state.assault_dist);
   mprintf("%-30s%d\n", "ignore_sudden:", _state.ignore_sudden);
   mprintf("%-30s%d\n", "ogg_dist:", _state.ogg_dist);
   mprintf("%-30s%d\n", "pl_danger:", _state.pl_danger);
   mprintf("%-30s%d\n", "det_torps:", _state.det_torps);
   mprintf("%-30s%d\n", "lookahead:", _state.lookahead);
   mprintf("%-30s%d\n", "chase:", _state.chase);
   mprintf("%-30s%g\n", "timer_delay_ms:", _state.timer_delay_ms);
   mprintf("%-30s%s\n", "hplanet:", _state.hplanet?_state.hplanet->pl_name:
      "(none)");
   mprintf("%-30s%d\n", "hpldist:", _state.hpldist);
   mprintf("%-30s%s\n", "ignore_e:", _state.ignore_e);
   mprintf("%-30s%d\n", "maxspeed:", _state.maxspeed);
   mprintf("%-30s%d\n", "killer:", _state.killer);
   mprintf("%-30s%s\n", "warteam:", team_to_string(_state.warteam));
   mprintf("%-30s%s\n", "lock:", _state.lock?"on":"off");
   mprintf("%-30s%s\n", "no_assault:", _state.no_assault?"yes":"no");
   mprintf("%-30s%s\n", "vector_torps:", _state.vector_torps?"yes":"no");
   mprintf("%-30s%s\n", "chaos:", _state.chaos?"yes":"no");
   mprintf("%-30s%s\n", "no_weapon:", _state.no_weapon?"true":"false");
   mprintf("%-30s%s\n", "have_plasma:", _state.have_plasma?"true":"false");
   mprintf("%-30s%d\n", "disengage_time:", 
      _state.disengage?(mtime(0)-_timers.disengage_time):-1);
   mprintf("%-30s%d\n", "lifetime:", _state.lifetime);
   mprintf("%-30s%s\n", "override:", _state.override?"true":"false");
   mprintf("%-30s%s\n", "vquiet:", _state.vquiet?"true":"false");
   mprintf("%-30s%d\n", "human:", _state.human);
   mprintf("%-30s%d\n", "player_type:", _state.player_type);

   /* globals */
   mprintf("\n");
   mprintf("%-30s%d\n", "_master:", _master);
   mprintf("%-30s%d\n", "_udcounter:", _udcounter);
   mprintf("%-30s%d\n", "_cycletime:", _cycletime);
   mprintf("%-30s%g\n", "_serverdelay:", _serverdelay);
   mprintf("%-30s%g\n", "_avsdelay:", _avsdelay);
}

list_all(m)
   
   char	*m;
{
   char	**s = _commands;

   while(!isspace(*m) && *m != '\0') m++;
   if(!*m){
      mprintf("%s\n", version());
      while(*s){
	 mprintf("%s\n", *s);
	 s++;
      }
   }
   else{
      while(isspace(*m)) m++;
      while(*s){
	 if(strncmp(m, *s, 3)==0){
	    do{
	       mprintf("%s\n", *s);
	       s++;
	    } while(**s != '\0');
	    break;
	 }
	 s++;
      }
   }
}

#ifdef ATM

udpaction(com)
   int	com;
{
   char		buf[80];
   switch (com) {
   case UDP_CURRENT:
      if (commMode == COMM_TCP)
	 sendUdpReq(COMM_UDP);
      else
	 sendUdpReq(COMM_TCP);
      break;
   case UDP_STATUS:
   case UDP_DROPPED:
      break;
   case UDP_SEQUENCE:
      udpSequenceChk = !udpSequenceChk;
      break;
   case UDP_SEND:
      udpClientSend++;
      if(udpClientSend > 3) udpClientSend = 0;
      break;
   case UDP_RECV:
      udpClientRecv++;
#ifdef DOUBLE_UDP
      if(udpClientRecv > MODE_DOUBLE) udpClientRecv = 0;
#else
      if(udpClientRecv >= MODE_DOUBLE) udpClientRecv = 0;
#endif 
      sendUdpReq(COMM_MODE + udpClientRecv);

      sprintf(buf, "Receiving with ");
      switch (udpClientRecv) {
      case MODE_TCP:
          strcat(buf, "TCP only"); break;
      case MODE_SIMPLE:
          strcat(buf, "simple UDP"); break;
      case MODE_FAT:
          strcat(buf, "fat UDP"); break;
#ifdef DOUBLE_UDP
      case MODE_DOUBLE:
          strcat(buf, "double UDP"); break;
#endif /*DOUBLE_UDP*/
      }
      response(buf);

      break;
   case UDP_FORCE_RESET:
        /* clobber UDP */
        UDPDIAG(("*** FORCE RESET REQUESTED\n"));
        sendUdpReq(COMM_TCP);
        commMode = commModeReq = COMM_TCP;
        commStatus = STAT_CONNECTED;
        commSwitchTimeout = 0;
        udpClientSend = udpClientRecv = udpSequenceChk = udpTotal = 1;
        udpDebug = udpDropped = udpRecentDropped = 0;
        if (udpSock >= 0)
            closeUdpConn(udpSock);
        break;
      
    case UDP_UPDATE_ALL:
	sendUdpReq(COMM_UPDATE);
	break;

    default:
        fprintf(stderr, "unknown UDP command\n");
    }
}
#endif

setlog(dir)
   
   char	*dir;
{
   char	buf[128];
   FILE		*test;

   sprintf(buf, "%s/robot%d.out.log", dir, getpid());
   test = freopen(buf, "w", stdout);
   if(!test)
      perror("freopen");
   sprintf(buf, "%s/robot%d.err.log", dir, getpid());
   test = freopen(buf, "w", stderr);
   if(!test)
      perror("freopen");
   sprintf(buf, "logging (%d)", getpid());
}

int_var(x, name, desc, in, rnd)

   int	*x, rnd;
   char	*desc, *in;
{
   char	buf[80];
   int	f = 1;

   if(index(in, '?')){
      sprintf(buf, "%s: %d", name, *x);
      response(buf);
      return 0;
   }
   if(index(in, '*')){
      *x = rnd;
      sprintf(buf, "%s set to %d (random)", name, *x);
      response(buf);
      return 1;
   }

   f = ((in = index(in, ' ')) != NULL);

   if(f){
      f = (sscanf(in, "%d", x)==1);
      sprintf(buf, "%s set to %d", name, *x);
      response(buf);
      return 1;
   }
   else{
      sprintf(buf, "usage: %s %s", name, desc);
      response(buf);
      return 0;
   }
}

char *
randbye()
{
#define NUM_R	10
   static char	*randr[NUM_R] = {
      "later",
      "so long",
      "take it easy",
      "hasta",
      "have a nice day",
      "nice oggin ya",
      "it's been real",
      "i'll be back",
      "better luck next time",
      "bye",
   };

   return randr[random()%NUM_R];
}

char *
ogg_targ()
{
   static char		buf[5];
   struct player	*j;
   if(_state.current_target && _state.current_target->p){
      j = _state.current_target->p;
      sprintf(buf, "%c%c", j->p_mapchars[0], j->p_mapchars[1]);
      return buf;
   }
   else
      return "none";
}

struct distress *loaddistress(enum dist_type i)
{
    static struct distress dist;

    dist.sender = me->p_no;
    dist.dam = (100 * me->p_damage) / me->p_ship.s_maxdamage;
    dist.shld = (100 * me->p_shield) / me->p_ship.s_maxshield;
    dist.arms = me->p_armies;
    dist.fuelp = (100 * me->p_fuel) / me->p_ship.s_maxfuel;
    dist.wtmp = (100 * me->p_wtemp) / me->p_ship.s_maxwpntemp;
    dist.etmp = (100 * me->p_etemp) / me->p_ship.s_maxegntemp;
    
    /* What is this for??? */
    dist.sts = (me->p_flags & 0xff) | 0x80;

    dist.wtmpflag =  me->p_flags&PFWEP   ? 1 : 0;
    dist.etempflag = me->p_flags&PFENG   ? 1 : 0;
    dist.cloakflag = me->p_flags&PFCLOAK ? 1 : 0;

    dist.distype = i;

    dist.close_pl = me_p->closest_pl->pl_no;
    dist.close_en = _state.closest_e ? _state.closest_e->p->p_no : me->p_no;
    dist.close_fr = _state.closest_f ? _state.closest_f->p->p_no : me->p_no;
    if(!_state.closest_f) {
       dist.close_j = dist.close_en;
    } else if(!_state.closest_e) {
       dist.close_j = dist.close_fr;
    } else {
       dist.close_j = (_state.closest_e->dist < _state.closest_f->dist) ?
		       dist.close_en : dist.close_fr;
    }
                    
    /* These are just guesses.... */
    dist.tclose_pl = 0;
    dist.tclose_en = 0;
    dist.tclose_fr = 0;
    dist.tclose_j = 0;

    dist.cclist[0] = 0x80;
    dist.preappend[0] = '\0';
    dist.macroflag = 0;

    return &dist;
}

/* this converts a dist struct to the appropriate text
   (excludes F1->FED text bit).. sorry if this is not what we said 
   earlier jeff.. but I lost the paper towel I wrote it all down on */
static void Dist2Mesg(struct distress *dist, char *buf)
{
    int len, i;

    sprintf(buf, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
	   (dist->macroflag << 5) + (dist->distype),
	   dist->fuelp | 0x80,
	   dist->dam | 0x80,
	   dist->shld | 0x80,
	   dist->etmp | 0x80,
	   dist->wtmp | 0x80,
	   dist->arms | 0x80,
	   dist->sts | 0x80,
	   dist->close_pl | 0x80,
	   dist->close_en | 0x80,
	   dist->tclose_pl | 0x80,
	   dist->tclose_en | 0x80,
	   dist->tclose_j | 0x80,
	   dist->close_j | 0x80,
	   dist->tclose_fr | 0x80,
           dist->close_fr | 0x80);
	   
    /* cclist better be terminated properly otherwise we hose here */
    i=0;
    while (((dist->cclist[i] & 0xc0) == 0xc0)) {
	buf[16+i]=dist->cclist[i];
	i++;
    }
    /* get the pre/append cclist terminator in there */
    buf[16+i]=dist->cclist[i];
    buf[16+i+1]='\0';

    len = 16+i+1;
    if (dist->preappend[0] != '\0') {
	strncat(buf, dist->preappend, MSG_LEN - len);/* false sense of security? */
	buf[MSG_LEN - 1] = '\0';
    }
}

void send_RCD(struct distress *dist)
{
    char buf[MSG_LEN];

    Dist2Mesg(dist, buf);
    sendMessage(buf, MTEAM|MDISTR, me->p_team);
}
