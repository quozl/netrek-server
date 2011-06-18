/*
 * main.c
 */
#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <pwd.h>
#ifdef hpux
#include <time.h>
#else				/* hpux */
#include <sys/time.h>
#endif				/* hpux */
#ifndef hpux
#include <sys/wait.h>
#endif				/* hpux */
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

#define TIMEOUT1		50
#define TIMEOUT2		50

char	revision[] = "$Revision: 1.3 $";

static   int    first = 1;
jmp_buf         env;
char	*commfile = NULL;

main(argc, argv)
   int             argc;
   char          **argv;
{
   int             intrupt(), noargs = 0;
   int             team = 1, s_type = CRUISER;
   int             usage = 0;
   int             err = 0;
   char           *name, *ptr, *cp, *rindex();
   int             dcore(), resetRobot();
   void            reaper(int), kill_rwatch(int), exitRobot(int);
   int             passive = 0;
   char           *defaultsFile = NULL;
   register        i;
   int             rwatch=0;
   static int      switchedteams=0;
   char            password[64];
   int             timer2 = 0, tryudp=1;
   char          **_argv = argv;
   int             _argc = argc;

   if(argc == 1)
      printUsage(argv[0]);

   password[0] = '\0';
   login[0] = '\0';

   strncpy(pseudo, "Marvin", sizeof(pseudo));
   pseudo[sizeof(pseudo) - 1] = '\0';

   srand(getpid() * time((long *) 0));
   srandom(getpid() * time((long *) 0));

   name = *argv++;
   argc--;
   if ((ptr = rindex(name, '/')) != NULL)
      name = ptr + 1;
   while (*argv) {
      if (**argv == '-')
	 ++* argv;
      else
	 break;

      argc--;
      ptr = *argv++;
      while (*ptr) {
	 switch (*ptr) {

	 case 'b': /* argv opt, -b : blind, don't read from stdin */
	    read_stdin = 0;
	    break;

	 case 'c': /* argv opt, -c[sdcbago] : ship type */
	    ptr++;
	    switch (*ptr) {
	    case 's':
	       s_type = SCOUT;
	       break;
	    case 'd':
	       s_type = DESTROYER;
	       break;
	    case 'c':
	       s_type = CRUISER;
	       break;
	    case 'b':
	       s_type = BATTLESHIP;
	       break;
	    case 'a':
	       s_type = ASSAULT;
	       break;
	    case 'g':
	       s_type = GALAXY;
	       break;
	    case 'o':
	       s_type = STARBASE;
	       break;
	    default:
	       s_type = CRUISER;
	       break;
	    }
	    break;

	 case 'C': /* argv opt, -C file : initial commands file */
	    commfile = *argv;
	    argv++;
	    argc--;
	    break;

	 case 'g': /* argv opt, -g : tell server we are a robot */
	    oggv_packet = 1;
	    break;

	 case 'h': /* argv opt, -h server : server host name or ip */
	    serverName = *argv;
	    argc--;
	    argv++;
	    break;

	 case 'i': /* argv opt, -i : set inl flag */
	    inl = 1;
	    updates = 2.0;
	    break;

	 case 'I': /* argv opt, -I : ignore lack of t-mode */
	    ignoreTMode = 1;
	    break;

	 case 'l': /* argv opt, -l logfile : log into log file */
	    setlog(*argv);
	    argv++;
	    argc--;
	    break;

	 case 'n': /* argv opt, -n name : set robot player name */
	    strncpy(pseudo, *argv, sizeof(pseudo));
	    pseudo[sizeof(pseudo) - 1] = '\0';
	    argv++;
	    argc--;
	    break;

	 case 'o': /* argv opt, -o : override decision making */
	    override = 1;
	    break;

	 case 'O': /* argv opt, -O : disable robot in-game control password */
	    nopwd = 1;
	    break;

	 case 'p': /* argv opt, -p port : connect to server port */
	    if (*argv) {
	       xtrekPort = atoi(*argv);
	       argv++;
	       argc--;
	    }
	    break;

	 case 'q': /* argv opt, -q : unknown */
	    noargs ++;
	    break;

	 case 'r': /* argv opt, -r file : read from defaults file */
	    defaultsFile = *argv;
	    argv++;
	    argc--;
	    break;

	 case 'R': /* argv opt, -R : doreserved = 1 */
#ifndef PUBLIC
	    doreserved = 1;
#endif
	    break;

	 case 's': /* argv opt, -s port : accept ntserv connection on port */
	    if (*argv) {
	       xtrekPort = atoi(*argv);
	       passive = 1;
	       argv++;
	       argc--;
	    }
	    break;

	 case 'S': /* argv opt, -S : shmem = 1*/
	    shmem = 1;
	    break;

	 case 'T': /* argv opt, -T[frko] : set team to join */
	    ptr++;
	    switch (*ptr) {
	    case 'f':
	       team = FED_N;
	       break;
	    case 'r':
	       team = ROM_N;
	       break;
	    case 'k':
	       team = KLI_N;
	       break;
	    case 'o':
	       team = ORI_N;
	       break;
	    default:
	       team = -1;
	       break;
	    }
	    break;

	 case 'u': /* argv opt, -u updates : set updates per second */
	    ptr++;
	    mprintf("ptr: %s\n", ptr);
	    sscanf(ptr, "%f", &updates);
	    mprintf("updates at %g (%d per second)\n", updates,
	       (int)(1000000./(updates * 100000.)));
	    ptr += strlen(ptr)-1;
	    break;

	 case 'U': /* argv opt, -U[fs] : set UDP mode */
	    if(*(ptr+1)){
	       ptr++;
	       switch(*ptr){
		  case 'f': 
		     udpClientRecv = 2; 
		     break;
		  case 's': 
		     udpClientRecv = 1;
		     break;
	       }
	    }
	    tryudp++;
	    mprintf("tryudp on, udp recv mode %d\n", udpClientRecv);
	    break;

	 case 'v': /* argv opt, -v : emit usage summary */
	    usage++;
	    break;

	 case 'w': /* argv opt, -w : trigger rwatch */
	    rwatch = 1;
	    if(gethostname(rw_host, 80) < 0){
	       perror("gethostname");
	       exit(1);
	    }
	    break;

#ifdef nodef
	 case 'W': /* argv opt, -W host : set rwatch hostname */
	    rwatch = 2;
	    strcpy(rw_host, *argv);
	    argv++;
	    argc--;
	    break;
#endif

	 case 'x': /* argv opt, -x password : use login password */
	    strcpy(password, *argv);
	    argv++;
	    argc--;
	    break;

	 case 'X': /* argv opt, -X login : user login name */
	    strncpy(login, *argv, sizeof(login));
	    argv++;
	    argc--;
	    break;

	 default:
	    fprintf(stderr, "%s: unknown option '%c'\n", name, *ptr);
	    err++;
	    break;
	 }
	 ptr++;
      }
   }
   initDefaults(defaultsFile);
   if (usage || err) {
      printUsage(name);
      exit(err);
   }

   _udcounter = 100;	/* should initialize elsewhere */
   _udnonsync = 100;
   _cycleroundtrip = 0;
   _avsdelay = 0.0;
   reset_r_info(&team, &s_type, 1, login);
   /* this creates the necessary x windows for the game */

   openmem();

   if(rwatch == 1){
      if(fork() == 0){
	 execl("./rwatch", "rwatch", 0);
	 perror("execl");
      }
      for(i=0; i< 10; i++){
	 sleep(3);
	 if(connectToRWatch(rw_host, 2692) != 0)
	    break;
      }
      if(i==10){
	 fprintf(stderr, "robot: Can't connect through port 2692\n");
	 exit(1);
      }
   }

   if (!passive) {
      callServer(xtrekPort, serverName);
   } else {
      connectToServer(xtrekPort);
   }

   if(noargs){
      int	pl = strlen(_argv[0]);
      for(i=0; i< _argc; i++)
	 bzero(_argv[i], strlen(_argv[i]));
      strncpy(_argv[0], "aiupdate", pl);
      _argv[0][pl-1] = '\0';
   }

   findslot();
   lastm = mctl->mc_current;
   signal(SIGHUP, exitRobot);
   signal(SIGTERM, exitRobot);
   /* TMP 
   signal(SIGBUS, exitRobot);
   signal(SIGSEGV, exitRobot);
   */
   signal(SIGFPE, exitRobot);
   (void) signal(SIGCHLD, reaper);

   signal(SIGPIPE, kill_rwatch);

   if(!login[0])
      strncpy(login, "r1h0", sizeof(login));

   if(pseudo[0] == '\0'){
      if ((cp = getdefault("name")) != 0)
	 (void) strncpy(pseudo, cp, sizeof(pseudo));
      pseudo[sizeof(pseudo) - 1] = '\0';
   }

   mprintf("login ..."); fflush(stdout);
   if(password[0])
      renter(pseudo, password, login);
   else
#ifdef ROBOTPASS
      renter(pseudo, ROBOTPASS, login);
#else
      renter(pseudo, "p\001", login);
#endif
   /* FLAW: cannot send an empty password */

   mprintf("\n");

   loggedIn = 1;

   showShields = booleanDefault("showshields", showShields);
   showStats = booleanDefault("showstats", showStats);
   keeppeace = 1;
   reportKills = booleanDefault("reportkills", reportKills);
   sendOptionsPacket();

   if(shmem)
      shmem_open();

   /*
    * Set p_hostile to hostile, so if keeppeace is on, the guy starts off
    * hating everyone (like a good fighter should)
    */
   me->p_hostile = (FED | ROM | KLI | ORI);

   me->p_planets = 0;
   me->p_genoplanets = 0;
   me->p_armsbomb = 0;
   me->p_genoarmsbomb = 0;
   /* Set up a reasonable default */
   me->p_whydead = KQUIT;
   me->p_team = ALLTEAM;

   startPing();

   setjmp(env);			/* Reentry point of game */
   /* give the player the motd and find out which team he wants */

   _state.lifetime = 0;

   if(!reset_r_info(&team, &s_type, first, login)){
      sendByeReq();
      sleep(1);
      exit(1);
   }

   timer2 = 0;

   mprintf("team ...\n");
   if(!teamRequest(team, s_type)){
      switchedteams = 1;
      mprintf("team or ship rejected.\n");

      /* if base destroyed */
      if(s_type == STARBASE){
	 s_type = CRUISER;
	 if(inl){
	    _state.override = 0;
	 }
      }

      showteams();
      do {
	 if(teamRequest(team, s_type))
	    break;
	 if(read_stdin)
	    process_stdin();
#ifdef ATM
	 readFromServer(1);
#else
	 readFromServer();
#endif
	 if(_state.command == C_QUIT || timer2 >= TIMEOUT2){
	    exitRobot(0);
	 }
	 timer2 ++;
	 if(_state.team != -1){
	    team = getteam();
	    if(team == -1)
	       timer2 = TIMEOUT2;	/* must exit */
	 }

	 if(team != _state.team && team != -1){
	    timer2 = 0;
	    _state.team = team;
	    if (!teamRequest(team, s_type)){
	       switchedteams = 1;
	       mfprintf(stderr, "team or ship rejected.\n");
	       showteams();
	    }
	    else 
	       break;
	 }
      } while(1);
   }
   set_team(team);
   me->p_team = team_inttobit(team);
   getship(myship, s_type);
   if(!inl && s_type == STARBASE)
      req_dock_on();
   else if(inl && s_type == STARBASE)
      req_dock_off();	/* til we figure out how to keep it from preempting
			   home planet */

   me->p_status = PALIVE;	/* Put player in game */
   me->p_ghostbuster = 0;

   if(first)
      send_initial();
   first = 0;

   if((_state.try_udp || tryudp) && commMode != COMM_UDP){
      sendUdpReq(COMM_UDP);
   }

   if (switchedteams) {
      switchedteams = 0;
      declare_intents(0); /* war and peace */
   } else {
      declare_intents(1); /* peace only */
   }

   input();
}

printUsage(prog)
   char           *prog;
{
   static char *list[] = {
   "network connection options",
   "  -h server   : server host name or ip (critical)",
   "  -p port     : connect to server port",
   "  -s port     : accept ntserv connection on port",
   "  -U[fs]      : set UDP mode",
   "authentication and identification options",
   "  -n name     : set robot player name",
   "  -X login    : user login name",
   "  -x password : use login password",
   "  -g          : tell server we are a robot",
   "team and ship selection options",
   "  -T[frko]    : set team to join",
   "  -c[sdcbago] : ship type",
   "in-game behaviour options",
   "  -b          : blind, don't read from stdin",
   "  -C file     : initial commands file",
   "  -I          : ignore lack of t-mode",
   "  -O          : disable robot in-game control password",
   "  -o          : override decision making",
   "  -u updates  : set updates per second",
   "other options",
   "  -i          : set inl flag",
   "  -l logfile  : log into log file",
   "  -q          : unknown",
   "  -r file     : read from defaults file",
   "  -R          : doreserved = 1",
   "  -S          : shmem = 1",
   "  -v          : emit usage summary",
   "  -w          : trigger rwatch",
   "  -W host     : set rwatch hostname",
   NULL,
};

   char **s;

   fprintf(stderr, "usage: %s [options]\n", prog);
   for(s = list; *s; s++) fprintf(stderr, "%s\n", *s);
   exit(0);
}

void reaper(int sig)
{
#ifdef hpux
   wait((int *) 0);
#else				/* hpux */
   /* SUPRESS 530 */
   while (wait3((int *) 0, WNOHANG, NULL) > 0);
#endif				/* hpux */
   signal(SIGCHLD, reaper);
}

void exitRobot(int sig)
{
   fprintf(stderr, "robotd: exiting\n");
   fflush(stderr);

   if(shmem)
      shmem_check();

   sendByeReq();
   if(sock)
      shutdown(sock, 2);
   if(udpSock)
      shutdown(udpSock, 2);
   sleep(1);

   switch(sig){
      case SIGBUS:
      case SIGSEGV:
      case SIGFPE:
	 abort();
      default:
	 exit(0);
   }
}

resetRobot()
{
   fprintf(stderr, "BUS or SEGV\n");
   _state.command = C_RESET;
   longjmp(env,0);
}

dcore()
{
   abort();
}

void kill_rwatch(int sig)
{
   if(rsock != -1){
      shutdown(rsock, 2);
      rsock = -1;
   }
   fprintf(stderr, "shutting down rwatch on SIGPIPE\n");
   /* exit otherwise? */
}

getteam()
{
   int	nf= -1, nk= -1, nr= -1, no= -1;

   if(tournMask & FED){
      nf = realNumShips(FED);
   }
   if(tournMask & ROM){
      nr = realNumShips(ROM);
   }
   if(tournMask & KLI){
      nk = realNumShips(KLI);
   }
   if(tournMask & ORI){
      no = realNumShips(ORI);
   }

   if(nf > -1 && nr > -1){
      if(nf < nr) 
	 return FED_N;
      else
	 return ROM_N;
   }
   if(nf > -1 && no > -1){
      if(nf < no)
	 return FED_N;
      else
	 return ORI_N;
   }
   if(nk > -1 && nr > -1){
      if(nk < nr)
	 return KLI_N;
      else
	 return ROM_N;
   }
   if(nk > -1 && no > -1){
      if(nk < no)
	 return KLI_N;
      else
	 return ORI_N;
   }
   if(nf > 0) return FED_N;
   if(nr > 0) return ROM_N;
   if(nk > 0) return KLI_N;
   if(no > 0) return ORI_N;

   if(nf == 0) return FED_N;
   if(nr == 0) return ROM_N;
   if(nk == 0) return KLI_N;
   if(no == 0) return ORI_N;

   return -1;
}
