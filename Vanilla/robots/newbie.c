/* newbie.c
*/

/*
   Newbie Server robot by Jeff Nowakowski
   Based on Kurt Siegl's base practice server.
*/


#include <stdio.h>
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
#include "newbiedefs.h"

int debug=0;
int nb_robots=0;

char *roboname = "Merlin";

static char    *team_s[4] = {"federation", "romulan", "klingon", "orion"};

#define NUMADJ 12
static char    *adj_s[NUMADJ] = {
    "VICIOUS", "RUTHLESS", "IRONFISTED", "RELENTLESS",
    "MERCILESS", "UNFLINCHING", "FEARLESS", "BLOODTHIRSTY",
    "FURIOUS", "DESPERATE", "FRENZIED", "RABID"};

#define NUMNAMES        20

static char    *names[NUMNAMES] =
{"Annihilator", "Banisher", "Blaster",
 "Demolisher", "Destroyer", "Eliminator",
 "Eradicator", "Exiler", "Obliterator",
 "Razer", "Demoralizer", "Smasher",
 "Shredder", "Vanquisher", "Wrecker",
 "Ravager", "Despoiler", "Abolisher",
 "Emasculator", "Decimator"};

/*
 * #define NUMNAMES     3 static char *names[NUMNAMES] = { "guest", "guest",
 * "guest" };
 */

static  char    hostname[64];
struct  planet  *oldplanets;    /* for restoring galaxy */

int target;  /* Terminator's target 7/27/91 TC */
int phrange; /* phaser range 7/31/91 TC */
int trrange; /* tractor range 8/2/91 TC */
int ticks = 0;
int oldmctl;


static void cleanup(int);
void checkmess(int);
static void start_internal(char *type);
static void obliterate(int wflag, char kreason);
static void start_a_robot(char *team);
static void stop_a_robot(void);
static int is_robots_only(void);
static int num_humans(void);
static void exitRobot(void);
static char * namearg(void);
static int num_players(int *next_team);
static int rprog(char *login, char *monitor);
static void stop_this_bot(struct player * p);
static void save_armies(struct player *p);

static void
reaper(int sig)
{
    int stat=0;
    static int pid;

    while ((pid = WAIT3(&stat, WNOHANG, 0)) > 0)
        nb_robots--;
    HANDLE_SIG(SIGCHLD,reaper);
}

#ifdef NEWBIESERVER
int
main(argc, argv)
     int             argc;
     char           *argv[];
{
    int team = 4;
    int pno;
    int class;                  /* ship class 8/9/91 TC */
    int i;

#ifndef TREKSERVER
    if (gethostname(hostname, 64) != 0) {
        perror("gethostname");
        exit(1);
    }
#else
    strcpy(hostname, TREKSERVER);
#endif

    srandom(time(NULL));

    getpath();
    (void) SIGNAL(SIGCHLD, reaper);
    openmem(1);
    strcpy(robot_host,REMOTEHOST);
    readsysdefaults();
    SIGNAL(SIGALRM, checkmess);             /*the def signal is needed - MK */
    if (!debug)
        SIGNAL(SIGINT, cleanup);

    class = STARBASE;
    target = -1;                /* no target 7/27/91 TC */
    if ( (pno = pickslot(QU_NEWBIE_DMN)) < 0) {
       printf("exiting! %d\n", pno);
       exit(0);
    }
    me = &players[pno];
    myship = &me->p_ship;
    mystats = &me->p_stats;
    lastm = mctl->mc_current;
    /* At this point we have memory set up.  If we aren't a fleet, we don't
       want to replace any other robots on this team, so we'll check the
       other players and get out if there are any on our team.
       */

    robonameset(me); /* set the robot@nowhere fields */
    enter(team, 0, pno, class, "Merlin"); /* was BATTLESHIP 8/9/91 TC */

    me->p_pos = -1;                  /* So robot stats don't get saved */
    me->p_flags |= PFROBOT;          /* Mark as a robot */
    me->p_x = 75000;                 /* displace to on overlooking position */
    me->p_y = 100;                  /* maybe we should just make it fight? */
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

    status->gameup |= GU_NEWBIE;
    queues[QU_NEWBIE_PLR].q_flags |= QU_REPORT;
    queues[QU_NEWBIE_OBS].q_flags |= QU_REPORT;
    queues[QU_PICKUP].q_flags ^= QU_REPORT;
    queues[QU_PICKUP_OBS].q_flags ^= QU_REPORT;

    /* Robot is signalled by the Daemon */
    ERROR(3,("\nRobot Using Daemon Synchronization Timing\n"));
   
    me->p_process = getpid();
    me->p_timerdelay = HOWOFTEN; 

    /* allows robots to be forked by the daemon -- Evil ultrix bullshit */
    SIGSETMASK(0);

    me->p_status = PALIVE;              /* Put robot in game */

    /* Only allow Rom/Fed game to make robot team selection easier. */
    /* Disable this because it breaks on timercide.  The other team
       needs to come in as a 3rd race after being timercided.
    queues[QU_PICKUP].tournmask = FED|ROM;
    */

    while (1) {
        PAUSE(SIGALRM);
    }
}

void checkmess(int unused)
{
    int         shmemKey = PKEY;
    int i;
    static int no_humans = 0;
   
    HANDLE_SIG(SIGALRM,checkmess);
    me->p_ghostbuster = 0;         /* keep ghostbuster away */
    if (me->p_status != PALIVE){  /*So I'm not alive now...*/
        ERROR(2,("ERROR: Merlin died??\n"));
        cleanup(0);   /*Merlin is dead for some unpredicted reason like xsg */
    }

    /* make sure shared memory is still valid */
    if (shmget(shmemKey, SHMFLAG, 0) < 0) {
        exit(1);
        ERROR(2,("ERROR: Invalid shared memory\n"));
    }

    ticks++;

    /* End the current game if no humans for 60 seconds. */
    if ((ticks % ROBOCHECK) == 0) {
        if (no_humans >= 60)
            cleanup(0); /* Doesn't return. */

        if (is_robots_only())
            no_humans += ROBOCHECK / PERSEC;
        else
            no_humans = 0;
    }

    /* Stop or start a robot. */
    if ((ticks % ROBOCHECK) == 0) {
        int next_team;
        int np = num_players(&next_team);

         if (!(ticks % ROBOEXITWAIT))
		{
             if ((QUPLAY(QU_NEWBIE_PLR) + QUPLAY(QU_NEWBIE_BOT)) >= queues[QU_PICKUP].max_slots)
                 stop_a_robot();
		}
        else if (((QUPLAY(QU_NEWBIE_PLR) + QUPLAY(QU_NEWBIE_BOT)) < (queues[QU_PICKUP].max_slots - 1)) && (nb_robots < NB_ROBOTS))
        {
            if (next_team == FED)
                start_a_robot("-Tf");
            else
                start_a_robot("-Tr");
        }
    }

    if ((ticks % SENDINFO) == 0) {
        static int alternate = 0;

        alternate++;
        if (1) {
            messAll(255,roboname,"Welcome to the Newbie Server.");
            messAll(255,roboname,"See http://cow.netrek.org/current/newbie.html");
        }
        else {
            messAll(255,roboname,"Think you have what it takes?  Sign up for the draft league!");
            messAll(255,roboname, "See http://draft.lagparty.org/");
        }
    }

}


static int is_robots_only(void)
{
   return !num_humans();
}

static int num_humans(void) {
   int i;
   struct player *j;
   int count = 0;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
      if (j->p_status == PFREE)
	 continue;
      if (j->p_flags & PFROBOT)
	 continue;
      if (j->p_status == POBSERV)
         continue;

      if (!rprog(j->p_login, j->p_monitor))
          /* Found a human. */
	 count++;
   }

   return count;
}

static void stop_a_robot(void)
{
    int i;
    struct player *j;

    /* Nuke robot from the team with the fewest humans. */
    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE)
            continue;
        if (j->p_flags & PFROBOT)
            continue;

        /* If he's at the MOTD we'll get him next time. */
        if (j->p_status == PALIVE && rprog(j->p_login, j->p_monitor)) {
            stop_this_bot(j);
            return;
        }
    }
}

/* this is by no means foolproof */

static int
rprog(char *login, char *monitor)
{
    int             v;

    if (strcmp(login, "robot!") == 0)
        /*      if (strstr(monitor, "uci")) */
        return 1;

    return 0;
}

static void stop_this_bot(struct player *p) {
    p->p_ship.s_type = STARBASE;
    p->p_whydead=KQUIT;
    p->p_explode=10;
    p->p_status=PEXPLODE;
    p->p_whodead=0;

    pmessage(0, MALL, "Merlin->ALL", 
        "Robot %s (%2s) was ejected to make room for a human player.",
        p->p_name, p->p_mapchars);
    if ((p->p_status != POBSERV) && (p->p_armies>0)) save_armies(p);
}

static void save_armies(struct player *p)
{
  int i, k;

  k=10*(remap[p->p_team]-1);
  if (k>=0 && k<=30) for (i=0; i<10; i++) {
    if (planets[i+k].pl_owner==p->p_team) {
      planets[i+k].pl_armies += p->p_armies;
      pmessage(0, MALL, "Merlin->ALL", "%s's %d armies placed on %s",
                     p->p_name, p->p_armies, planets[k+i].pl_name);
      break;
    }
  }
}

static int
num_players(int *next_team)
{
    int i;
    struct player *j;
    int team_count[MAXTEAM+1];

    int c = 0;

    team_count[ROM] = 0;
    team_count[FED] = 0;

    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PFREE && j->p_status != POBSERV &&
            !(j->p_flags & PFROBOT))
            {
                team_count[j->p_team]++;
                c++;
            }
    }

    /* Assign which team gets the next robot. */
    if (team_count[ROM] > team_count[FED])
        *next_team = FED;
    else
        *next_team = ROM;

    return c;
}

static char           *
namearg(void)
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


static void
start_a_robot(char *team)
{
    char            command[256];
    int pid;

    sprintf(command, "%s %s %s %s -h %s -p %d -n '%s' -X robot! -b -O -i",
            RCMD, robot_host, OROBOT, team, hostname, PORT, namearg() );
   
    pid = fork();
    if (pid == -1)
     return;
    if (pid == 0) {
        SIGNAL(SIGALRM, SIG_DFL);
        execl("/bin/sh", "sh", "-c", command, 0);
        perror("newbie'execl");
        _exit(1);
    }
    nb_robots++;
}

static void start_internal(char *type)
{
    char *argv[6];
    u_int argc = 0;

    argv[argc++] = "robot";
    if ((strncmp(type, "iggy", 2) == 0) || 
        (strncmp(type, "hunterkiller", 2) == 0)) {
        argv[argc++] = "-Ti";
        argv[argc++] = "-P";
        argv[argc++] = "-f";    /* Allow more than one */
    } else if (strncmp (type, "cloaker", 2) == 0) {
        argv[argc++] = "-Ti";
        argv[argc++] = "-C";    /* Never uncloak */
        argv[argc++] = "-F";    /* Needs no fuel */
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

static void cleanup(int unused)
{
    register struct player *j;
    register int i, retry;

    do {
        /* terminate all robots */
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if ((j->p_status == PALIVE) && rprog(j->p_login, j->p_monitor))
                stop_this_bot(j);
        }

        USLEEP(2000000); 
        retry=0;
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if ((j->p_status != PFREE) && rprog(j->p_login, j->p_monitor))
                retry++;
        }
    } while (retry);            /* Some robots havn't terminated yet */

    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        if ((j->p_status != PALIVE) || (j == me)) continue;
        getship(&(j->p_ship), j->p_ship.s_type);
    }

    obliterate(1,KPROVIDENCE);
    status->gameup &= ~GU_NEWBIE;
    queues[QU_NEWBIE_PLR].q_flags ^= QU_REPORT;
    queues[QU_NEWBIE_OBS].q_flags ^= QU_REPORT;
    queues[QU_PICKUP].q_flags |= QU_REPORT;
    queues[QU_PICKUP_OBS].q_flags |= QU_REPORT;
    exitRobot();
}

static void exitRobot(void)
{
    SIGNAL(SIGALRM, SIG_IGN);
    if (me != NULL && me->p_team != ALLTEAM) {
        if (target >= 0) {
            messAll(255,roboname, "I'll be back.");
        }
        else {
            messAll(255,roboname,"#");
            messAll(255,roboname,"#  Merlin is tired.  Newbie Server is over "
                    "for now");
            messAll(255,roboname,"#");
        }
    }

    freeslot(me);
    exit(0);
}


static void obliterate(int wflag, char kreason)
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
#endif  /* NEWBIESERVER */


#ifndef NEWBIESERVER
int
main(argc, argv)
     int             argc;
     char           *argv[];
{
    printf("You don't have NEWBIESERVER option on.\n");
}

#endif /* NEWBIESERVER */
