/* basep.c
*/

/*
   Basepractice Sever robot by Kurt Siegl
   Based on Ted Hadleys practice server and Vanilla 2.2 puck
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include <time.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "basepdefs.h"

int debug=0;

char *roboname = "Smack";

static char    *team_s[4] = {"federation", "romulan", "klingon", "orion"};

#define NUMADJ 12
static char    *adj_s[NUMADJ] = {
   "VICIOUS", "RUTHLESS", "IRONFISTED", "RELENTLESS",
   "MERCILESS", "UNFLINCHING", "FEARLESS", "BLOODTHIRSTY",
"FURIOUS", "DESPERATE", "FRENZIED", "RABID"};

#define NUMNAMES	20

static char    *names[NUMNAMES] =
{"Annihilator", "Banisher", "Blaster",
   "Demolisher", "Destroyer", "Eliminator",
   "Eradicator", "Exiler", "Obliterator",
   "Razer", "Demoralizer", "Smasher",
   "Shredder", "Vanquisher", "Wrecker",
   "Ravager", "Despoiler", "Abolisher",
"Emasculator", "Decimator"};

/*
 * #define NUMNAMES	3 static char *names[NUMNAMES] = { "guest", "guest",
 * "guest" };
 */

#define MAX_NUM_PLAYERS	20


static	char	hostname[64];
struct	planet	*oldplanets;	/* for restoring galaxy */

int target;			/* Terminator's target 7/27/91 TC */
int phrange;			/* phaser range 7/31/91 TC */
int trrange;			/* tractor range 8/2/91 TC */
int robocheck = 0;
int oldmctl;


void cleanup(int);
void checkmess(int);
int start_internal();
int obliterate();

void
reaper(sig)
   int	sig;
{
   int stat=0;
   static int pid;

   while ((pid = WAIT3(&stat, WNOHANG, 0)) > 0) ;
   HANDLE_SIG(SIGCHLD,reaper);
}

#ifdef BASEPRACTICE
int
main(argc, argv)
   int             argc;
   char           *argv[];
{
   int team = 4;
   int pno;
   int class;                  /* ship class 8/9/91 TC */
   int i;

   if (gethostname(hostname, 64) != 0) {
      perror("gethostname");
      exit(1);
   }
   srandom(time(NULL));

   getpath();
   (void) SIGNAL(SIGCHLD, reaper);
   openmem(1);
   strcpy(robot_host,REMOTEHOST);
   readsysdefaults();
   SIGNAL(SIGALRM, checkmess);             /*the def signal is needed - MK */
   if (!debug)
       SIGNAL(SIGINT, cleanup);

    class = ATT;
    target = -1;                /* no target 7/27/91 TC */
    if ( (pno = pickslot(QU_ROBOT)) < 0) exit(0);
    me = &players[pno];
    myship = &me->p_ship;
    mystats = &me->p_stats;
    lastm = mctl->mc_current;
    /* At this point we have memory set up.  If we aren't a fleet, we don't
       want to replace any other robots on this team, so we'll check the
       other players and get out if there are any on our team.
    */

   robonameset(me); /* set the robot@nowhere fields */
   enter(team, 0, pno, class, "Smack"); /* was BATTLESHIP 8/9/91 TC */

    me->p_pos = -1;                  /* So robot stats don't get saved */
    me->p_flags |= PFROBOT;          /* Mark as a robot */
    me->p_x = 50000;                 /* displace to on overlooking position */
    me->p_y = 5000;                  /* maybe we should just make it fight? */
    me->p_hostile = 0;
    me->p_swar = 0;
   me->p_war = 0;
    me->p_team = 0;     /* indep */

   oldmctl = mctl->mc_current;
#ifdef nodef
   for (i = 0; i <= oldmctl; i++) {
      check_command(&messages[i]);
   }
#endif
   fix_planets();
   status->gameup |= GU_CHAOS;
   status->gameup |= GU_PRACTICE;

   /* Robot is signalled by the Daemon */
   ERROR(3,("\nRobot Using Daemon Synchronization Timing\n"));
   
   me->p_process = getpid();
   me->p_timerdelay = HOWOFTEN; 

    /* allows robots to be forked by the daemon -- Evil ultrix bullshit */
    SIGSETMASK(0);

    me->p_status = PALIVE;              /* Put robot in game */

    while (1) {
        PAUSE(SIGALRM);
    }
}

void checkmess(int unused)
{
   int 	shmemKey = PKEY;
   int i;

    HANDLE_SIG(SIGALRM,checkmess);
    me->p_ghostbuster = 0;         /* keep ghostbuster away */
    if (me->p_status != PALIVE){  /*So I'm not alive now...*/
      ERROR(2,("ERROR: Smack died??\n"));
      cleanup(0);    /*Smack is dead for some unpredicted reason like xsg */
    }

     robocheck++;
      if ((robocheck % ROBOCHECK) == 0)
	 check_robots_only();
      /* make sure shared memory is still valid */
      if (shmget(shmemKey, SHMFLAG, 0) < 0) {
	 exit(1);
	 ERROR(2,("ERROR: Invalid shared memory\n"));

      }

      if ((robocheck % SENDINFO) == 0) {
	messAll(255,roboname,"Welcome to the Basepractice Server.");
	messAll(255,roboname,"Send me the message \"help\" for rules.");
	messAll(255,roboname,"Read the MOTD for detailed info.");
	}

    while (oldmctl!=mctl->mc_current) {
        oldmctl++;
        if (oldmctl==MAXMESSAGE) oldmctl=0;
        if (messages[oldmctl].m_flags & MINDIV) {
            if (messages[oldmctl].m_recpt == me->p_no)
                check_command(&messages[oldmctl]);

        } else if ((messages[oldmctl].m_flags & MALL) &&
		!(messages[oldmctl].m_from & MGOD)) {
            if (strstr(messages[oldmctl].m_data, "help") != NULL)
                messOne(255,roboname,messages[oldmctl].m_from,
                   "If you want help from me, send ME the message 'help'.");
        }
    }
}

/* check to see if all robots in the game. If so tell them to exit */

check_robots_only()
{
   register        i;
   register struct player *j;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
      if (j->p_status == PFREE)
	 continue;
      if (j->p_flags & PFROBOT)
	 continue;

      if (!rprog(j->p_login, j->p_monitor))
	 return;
   }

   /*
    * printf("basep: found %d robots, no humans.  Killing..\n", k);
    * fflush(stdout);
    */

   cleanup(0);
}

/* this is by no means foolproof */

int
rprog(login, monitor)
   char           *login, *monitor;
{
   int             v;

   if (strlen(login) < 6 && sscanf(login, "r1h%d", &v) == 1)
/*      if (strstr(monitor, "uci")) */
	 return 1;

   return 0;
}


/* here we want to make sure everything is fuel and repair */

fix_planets()
{
   register        i;
   register struct planet *j;

   oldplanets = (struct planet *) malloc(sizeof(struct planet) * MAXPLANETS);
   MCOPY(planets, oldplanets, sizeof(struct planet) * MAXPLANETS);

   for (i = 0, j = planets; i < MAXPLANETS; i++, j++) {
      j->pl_flags |= (PLREPAIR | PLFUEL);
      j->pl_info |= ALLTEAM;
   }
}

int
num_players()
{
   register        i;
   register struct player *j;
   int             c = 0;
   for (i = 0, j = players; i < MAXPLAYER; i++, j++)
      if (j->p_status != PFREE)
	 c++;
   return c;
}

int 
do_start_robot(comm, mess)
   char           *comm;
   struct message *mess;
{
   register        i;
   int             sv;
   char            buf[80], team[10], query[20],  host[60], log[4], extra[80], desc[32];
   int             num;
   int             teamv = 0, teami = -1;
   int             dg = 0, base = 0, def = 0;
   int		   from;

   from=mess->m_from;
   while (isspace(*comm))
      comm++;
   strcpy(buf, comm);

   for (i = 0; i < strlen(comm); i++)
      if (isupper(buf[i]))
	 buf[i] = tolower(buf[i]);

   sv = sscanf(buf, "%s %d %s %s %s", query, &num, team, host, log);
   if (sv < 3) {
	    sv = sscanf(buf, "%s %s %s %s %s", query, team, desc, host, log);
	    if (sv > 2) {
	       if (strcmp(desc, "df") == 0) {
		  num = 1;
		  dg = 1;
	       } else if (strcmp(desc, "base") == 0) {
		  num = 1;
		  base = 1;
	       } else if (strcmp(desc, "def") == 0) {
		  num = 1;
		  def = 1;
	       } else
		  return;
	    } else {  		/* Start iggy, hoser, ... */
	   	 if (sv == 2) start_internal(team);
	         return;
	      }
   }
   if (strncmp(team, "fed", 3) == 0) {
      teamv = 0;
      teami = FED;
      strcpy(team, "-Tf");
   } else if (strncmp(team, "rom", 3) == 0) {
      teamv = 1;
      teami = ROM;
      strcpy(team, "-Tr");
   } else if (strncmp(team, "kli", 3) == 0) {
      teamv = 2;
      teami = KLI;
      strcpy(team, "-Tk");
   } else if (strncmp(team, "ori", 3) == 0) {
      teamv = 3;
      teami = ORI;
      strcpy(team, "-To");
   } else {
      messOne(255,roboname,from,"Unknown team name \"%s\"", team);
      return;
   }

   if (num_players() + num > MAXPLAYER) {
      messOne(255,roboname,from,"Too many players, sorry.");
      return;
   }
#ifdef nodef
   if (teami == players[from].p_team) {
      messOne(255,roboname,from,"Wrong team, pal.");
      return;
   }
#endif

   extra[0] = '\0';

   if (sv >= 4) {
      strcpy(extra, " (");
      if (sv > 4) {
	 strcat(extra, host);
	 strcat(extra, " log");
      } else{
	 strcat(extra, "on remote host\"");
	 strcat(extra, host);
	 strcat(extra, "\"");
      }
      strcat(extra, ")");
   }
   if (dg)
      sprintf(buf, "%d %s %s dogfighter%s started.%s", num,
	      adj_s[random() % NUMADJ],
	      team_s[teamv], num == 1 ? "" : "s", extra);
   else if (base)
      sprintf(buf, "one moderately angry %s base started.%s",
	      team_s[teamv], extra);
   else if (def)
      sprintf(buf, "one barely competent %s defender started.%s",
	      team_s[teamv], extra);
   else
      sprintf(buf, "%d %s %s ogger%s started.%s", num,
	      adj_s[random() % NUMADJ],
	      team_s[teamv], num == 1 ? "" : "s", extra);


   messAll(255,roboname,buf);

   startrobot(num, team, sv >= 4 ? host : NULL, sv > 4 ? log : NULL, dg, base, def);
}

char           *
namearg()
{
   register        i, k = 0;
   register struct player *j;
   char           *name;
   int             namef = 1;

   while (1) {

      name = names[random() % NUMNAMES];
      k++;

      namef = 0;
      for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
	 if (j->p_status != PFREE && strncmp(name, j->p_name, strlen(name) - 1)
	     == 0) {
	    namef = 1;
	    break;
	 }
      }
      if (!namef)
	 return name;

      if (k == 50)
	 return "guest";
   }
}

int
startrobot(num, s, h, log, dg, base, def)
   int             num;
   char           *s, *h, *log;
   int             dg, base, def;
{
   char           *remotehost;
   char            command[256];
   char            logc[256];
   register        i;

   if (h)
      remotehost = h;
   else
      remotehost = robot_host;
   if (log)
      sprintf(logc, "-l %s", LOGFILE);
   else
      sprintf(logc, " ");

   for (i = 0; i < num; i++) {
      if (dg)
	 sprintf(command, "%s %s %s %s -h %s -p %d -n '%s' -X r1h1f -b -O %s -C %s",
	       RCMD, remotehost, OROBOT, s, hostname, PORT, namearg(), logc,
		 DOGFILE);
      else if (base)
	 sprintf(command, "%s %s %s %s -h %s -p %d -n 'RoboBase' -X r1h1 -co -b -O %s -C %s",
		 RCMD, remotehost, OROBOT, s, hostname, PORT, logc,
		 BASEFILE);
      else if (def)
	 sprintf(command, "%s %s %s %s -h %s -p %d -n 'Defender' -X r1h1d -b -O %s -C %s",
		 RCMD, remotehost, OROBOT, s, hostname, PORT, logc,
		 DEFFILE);
      else
	 sprintf(command, "%s %s %s %s %s -h %s -p %d -n '%s' -X r1h0o -S -g -b -O %s -C %s",
		 RCMD, remotehost, NICE, OROBOT, s, hostname, PORT,
		 namearg(), logc, COMFILE);

      /*
       * printf("%s\n", command);
       */

      if (fork() == 0) {
	 SIGNAL(SIGALRM, SIG_DFL);
	 execl("/bin/sh", "sh", "-c", command, 0);
	 perror("basep'execl");
	 _exit(1);
      }
      sleep(5);
   }
   return 1;
}

start_internal(type)
    char *type;
{
    char *argv[6];
    u_int argc = 0;

    argv[argc++] = "robot";
    if ((strncmp(type, "iggy", 2) == 0) || 
	(strncmp(type, "hunterkiller", 2) == 0)) {
	argv[argc++] = "-Ti";
	argv[argc++] = "-P";
	argv[argc++] = "-f"; 	/* Allow more than one */
    } else if (strncmp (type, "cloaker", 2) == 0) {
	argv[argc++] = "-Ti";
	argv[argc++] = "-C";	/* Never uncloak */
	argv[argc++] = "-F";	/* Needs no fuel */
	argv[argc++] = "-f";
    } else if (strncmp (type, "hoser", 2) == 0) {
	argv[argc++] = "-p";
	argv[argc++] = "-f";
    } else return;

    argv[argc] = NULL;
    if (fork() == 0) {
	SIGNAL(SIGALRM, SIG_DFL);
	execv(Robot,argv);
	perror(Robot);
	_exit(1);
	}
}

void cleanup(int unused)
{
    register struct player *j;
    register int i, retry;

  do {
   /* terminate all robots */
   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
      if ((j->p_status == PALIVE) && rprog(j->p_login, j->p_monitor))
         messOne(255,roboname,j->p_no, "exit");
   }

   USLEEP(2000000); 
   retry=0;
   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
      if ((j->p_status != PFREE) && rprog(j->p_login, j->p_monitor))
	 retry++;
   }
  } while (retry);		/* Some robots havn't terminated yet */

	/* restore galaxy */
        MCOPY(oldplanets, planets, sizeof(struct planet) * MAXPLANETS);
        for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
            if ((j->p_status != PALIVE) || (j == me)) continue;
            getship(&(j->p_ship), j->p_ship.s_type);
        }

    obliterate(1,KPROVIDENCE);
    status->gameup &= ~GU_CHAOS;
    status->gameup &= ~GU_PRACTICE;
    exitRobot();
}

exitRobot()
{
    SIGNAL(SIGALRM, SIG_IGN);
    if (me != NULL && me->p_team != ALLTEAM) {
        if (target >= 0) {
            messAll(255,roboname, "I'll be back.");
        }
        else {
            messAll(255,roboname,"#");
            messAll(255,roboname,"#  Smack is tired.  Basepractice is over for now");
            messAll(255,roboname,"#");
        }
    }

    freeslot(me);
    exit(0);
}


obliterate(wflag, kreason)
     int wflag;
     char kreason;
{
  /* 0 = do nothing to war status, 1= make war with all, 2= make peace with all */
  struct player *j;
  int i, k;

  /* clear torps and plasmas out */
  MZERO(torps, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
  for (j = firstPlayer; j<=lastPlayer; j++) {
    if (j->p_status == PFREE)
      continue;
    j->p_status = PEXPLODE;
    j->p_whydead = kreason;
    if (j->p_ship.s_type == STARBASE)
      j->p_explode = 2 * SBEXPVIEWS ;
    else
      j->p_explode = 10 ;
    j->p_ntorp = 0;
    j->p_nplasmatorp = 0;
    if (wflag == 1)
      j->p_hostile = (FED | ROM | ORI | KLI);         /* angry */
    else if (wflag == 2)
      j->p_hostile = 0;       /* otherwise make all peaceful */
    j->p_war = (j->p_swar | j->p_hostile);
  }
}
#endif  /* BASEPRACTICE */


#ifndef BASEPRACTICE
int
main(argc, argv)
   int             argc;
   char           *argv[];
{
    printf("You don't have BASEPRACTICE option on.\n");
}

#endif /* BASEPRACTICE */
