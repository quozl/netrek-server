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
#include "proto.h"
#include "alarm.h"
#include "roboshar.h"
#include "basepdefs.h"
#include "util.h"
#include "slotmaint.h"
#include "shmatomic.h"

int debug=0;

#ifdef BASEPRACTICE

int nb_robots=0;

/* define the name of the moderation bot - please note that due to the way */
/* messages are handled and the formatting of those messages care must be  */
/* taken to ensure that the bot name does not exceed 5 characters.  If the */
/* desired name is larger than 5 chars the message routines will need to   */
/* have their formatting and contents corrected */
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

static	char	hostname[64];
struct	planet	*oldplanets;	/* for restoring galaxy */

int target;			/* Terminator's target 7/27/91 TC */
int phrange;			/* phaser range 7/31/91 TC */
int trrange;			/* tractor range 8/2/91 TC */
int robocheck = 0;
int oldmctl;


void cleanup(int);
void checkmess();
int rprog(char *login, char *monitor);
void start_internal(char *type);
void exitRobot();
void obliterate(int wflag, char kreason);
void check_robots_only();
void startrobot(int num, char *s, int dg, int base, int def);
void fix_planets();

static void reaper(int sig)
{
   int stat=0;
   int pid;

   while ((pid = wait3(&stat, WNOHANG, 0)) > 0)
       nb_robots--;
   signal(SIGCHLD,reaper);
}

int
main(int argc, char *argv[])
{
   int team = 4;
   int pno;
   int class;                  /* ship class 8/9/91 TC */

   if (gethostname(hostname, 64) != 0) {
      perror("gethostname");
      exit(1);
   }
   srandom(time(NULL));

   getpath();
   (void) SIGNAL(SIGCHLD, reaper);
   openmem(0);
   do_message_post_set(check_command);
   strcpy(robot_host,REMOTEHOST);
   readsysdefaults();
   alarm_init();
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);
   if (!debug)
       SIGNAL(SIGINT, cleanup);

    class = ATT;
    target = -1;                /* no target 7/27/91 TC */
    if ( (pno = pickslot(QU_ROBOT)) < 0) exit(0);
    me = &players[pno];
    myship = &me->p_ship;
    mystats = &me->p_stats;
    /* At this point we have memory set up.  If we aren't a fleet, we don't
       want to replace any other robots on this team, so we'll check the
       other players and get out if there are any on our team.
    */

   robonameset(me); /* set the robot@nowhere fields */
   enter(team, 0, pno, class, "Smack"); /* was BATTLESHIP 8/9/91 TC */

    me->p_pos = -1;                  /* So robot stats don't get saved */
    atomic_or(&me->p_flags, PFROBOT);          /* Mark as a robot */
    /* displace to on overlooking position */
    /* maybe we should just make it fight? */
    p_x_y_set(me, 50000, 5000);
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

   me->p_process = getpid();
   p_ups_set(me, 10 / HOWOFTEN);

    me->p_status = PALIVE;              /* Put robot in game */

    while ((status->gameup) & GU_GAMEOK) {
        alarm_wait_for();
        checkmess();
        update_sys_defaults();
    }
    cleanup(0);
    return 0;
}

void checkmess()
{
  me->p_ghostbuster = 0;         /* keep ghostbuster away */
  if (me->p_status != PALIVE){  /*So I'm not alive now...*/
    ERROR(2,("ERROR: Smack died??\n"));
    cleanup(0);
  }

  robocheck++;
  if ((robocheck % ROBOCHECK) == 0)
    check_robots_only();

  /* exit if daemon terminates use of shared memory */
  if (forgotten()) exit(1);

  if ((robocheck % SENDINFO) == 0) {
    messAll(255, roboname, "Welcome to the Basepractice Server.");
    messAll(255, roboname, "Send me the message \"help\" for rules.");
    messAll(255, roboname, "Read the MOTD for detailed info.");
  }

  while (oldmctl != load_acquire(&mctl->mc_current)) {
    oldmctl++;
    if (oldmctl == MAXMESSAGE) oldmctl=0;
    robohelp(me, oldmctl, roboname);
  }
}

/* check to see if all robots in the game. If so tell them to exit */

void check_robots_only()
{
   int i;
   struct player *j;

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

int rprog(char *login, char *monitor)
{
   int             v;

   if (strlen(login) < 6 && sscanf(login, "r1h%d", &v) == 1)
/*      if (strstr(monitor, "uci")) */
	 return 1;

   return 0;
}


/* here we want to make sure everything is fuel and repair */

void fix_planets()
{
   int i;
   struct planet *j;
   char command[256];

   oldplanets = (struct planet *) malloc(sizeof(struct planet) * MAXPLANETS);
   memcpy(oldplanets, planets, sizeof(struct planet) * MAXPLANETS);

   /* Standardize planet locations */
   sprintf(command, "%s/tools/setgalaxy r", LIBDIR);
   system(command);

   for (i = 0, j = planets; i < MAXPLANETS; i++, j++) {
      j->pl_flags |= (PLREPAIR | PLFUEL);
      j->pl_info |= ALLTEAM;
   }
}

int
num_players()
{
   int i;
   struct player *j;
   int             c = 0;
   for (i = 0, j = players; i < MAXPLAYER; i++, j++)
      if (j->p_status != PFREE)
	 c++;
   return c;
}

int do_start_robot(char *comm, struct message *mess)
{
   int i;
   int             sv;
   char            buf[80], team[10], query[20], desc[32];
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

   sv = sscanf(buf, "%19s %d %9s", query, &num, team);
   if (sv < 3) {
	    sv = sscanf(buf, "%19s %9s %31s", query, team, desc);
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
		  return 0;
	    } else {  		/* Start iggy, hoser, ... */
	   	 if (sv == 2) start_internal(team);
	         return 0;
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
      return 0;
   }

   if (num_players() + num > MAXPLAYER) {
      messOne(255,roboname,from,"Too many players, sorry.");
      return 0;
   }
#ifdef nodef
   if (teami == players[from].p_team) {
      messOne(255,roboname,from,"Wrong team, pal.");
      return 0;
   }
#endif

   if (dg)
      sprintf(buf, "%d %s %s dogfighter%s started.", num,
	      adj_s[random() % NUMADJ],
	      team_s[teamv], num == 1 ? "" : "s");
   else if (base)
      sprintf(buf, "one moderately angry %s base started.",
	      team_s[teamv]);
   else if (def)
      sprintf(buf, "one barely competent %s defender started.",
	      team_s[teamv]);
   else
      sprintf(buf, "%d %s %s ogger%s started.", num,
	      adj_s[random() % NUMADJ],
	      team_s[teamv], num == 1 ? "" : "s");


   messAll(255,roboname,buf);

   if (nb_robots < NB_ROBOTS)
       startrobot(num, team, dg, base, def);
   return 0;
}

char           *
namearg()
{
   int i, k = 0;
   struct player *j;
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

void startrobot(int num, char *s, int dg, int base, int def)
{
   char           *serverhost = hostname;
   char           *name, *login, *file, *path;
   char           *argv[20];
   int i, pid, argc;

   /* robots run here connect as ROBOTHOST, others connect to this host */
   if (!strlen(RCMD) && strlen(robot_host))
      serverhost = robot_host;

   for (i = 0; i < num; i++) {
      if (dg) {
	 name = namearg();
	 login = "r1h1f";
	 file = DOGFILE;
      } else if (base) {
	 name = "RoboBase";
	 login = "r1h1";
	 file = BASEFILE;
      } else if (def) {
	 name = "Defender";
	 login = "r1h1d";
	 file = DEFFILE;
      } else {
	 name = namearg();
	 login = "r1h0o";
	 file = COMFILE;
      }

      pid = fork();
      if (pid == -1)
	 return;
      if (pid == 0) {
	 alarm_prevent_inheritance();
	 argc = 0;
	 argv[argc++] = "robot";
	 if (strlen(RCMD)) {
	    path = RCMD;
	    argv[argc++] = robot_host;
	    argv[argc++] = OROBOT;
	 } else {
	    path = OROBOT;
	 }
	 argv[argc++] = s;
	 argv[argc++] = "-h";
	 argv[argc++] = serverhost;
	 argv[argc++] = "-p";
	 argv[argc++] = PORT;
	 argv[argc++] = "-n";
	 argv[argc++] = name;
	 argv[argc++] = "-X";
	 argv[argc++] = login;
	 if (base) {
	    argv[argc++] = "-co";
	 } else if (!dg && !def) {
	    argv[argc++] = "-S";
	    argv[argc++] = "-g";
	    errno = 0;
	    if (nice(NICE) == -1 && errno != 0)
	       perror("basep'nice");
	 }
	 argv[argc++] = "-b";
	 argv[argc++] = "-O";
	 argv[argc++] = "-C";
	 argv[argc++] = file;
	 argv[argc] = NULL;
	 execv(path, argv);
	 perror("basep'execv");
	 _exit(1);
      }
      nb_robots++;
   }
   return;
}

void start_internal(char *type)
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
	alarm_prevent_inheritance();
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
        memcpy(planets, oldplanets, sizeof(struct planet) * MAXPLANETS);
        for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
            if ((j->p_status != PALIVE) || (j == me)) continue;
            getship(&(j->p_ship), j->p_ship.s_type);
        }

    obliterate(1,KPROVIDENCE);
    status->gameup &= ~GU_CHAOS;
    status->gameup &= ~GU_PRACTICE;
    exitRobot();
}

void exitRobot()
{
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


void obliterate(int wflag, char kreason)
{
  /* 0 = do nothing to war status, 1= make war with all, 2= make peace with all */
  struct player *j;

  /* clear torps and plasmas out */
  memset(torps, 0, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
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
main(int argc, char *argv[])
{
    printf("You don't have BASEPRACTICE option on.\n");
    return 1;
}

#endif /* BASEPRACTICE */
