/* newbie.c
*/

/*
   Newbie Server robot by Jeff Nowakowski
   Based on Kurt Siegl's base practice server.
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
#include "proto.h"
#include "alarm.h"
#include "roboshar.h"
#include "newbiedefs.h"

int debug=0;

char *roboname = "Merlin";

#define NUMNAMES        20

static char    *names[NUMNAMES] =
{"Annihilator", "Banisher", "Blaster",
 "Demolisher", "Destroyer", "Eliminator",
 "Eradicator", "Exiler", "Obliterator",
 "Razer", "Demoralizer", "Smasher",
 "Shredder", "Vanquisher", "Wrecker",
 "Ravager", "Despoiler", "Abolisher",
 "Emasculator", "Decimator"};

static  char    hostname[64];

int target;  /* Terminator's target 7/27/91 TC */
int phrange; /* phaser range 7/31/91 TC */
int trrange; /* tractor range 8/2/91 TC */
int ticks = 0;
int oldmctl;
int team1 = 0;
int team2 = 0;
static int debugTarget = -1;
static int debugLevel = 0;

static void cleanup(int);
void checkmess();
static void obliterate(int wflag, char kreason);
static void start_a_robot(char *team);
static void stop_a_robot(void);
static int is_robots_only(void);
static int totalRobots(int team);
static void exitRobot(void);
static char * namearg(void);
static int num_players(int *next_team);
static int rprog(char *login, char *monitor);
static void stop_this_bot(struct player * p);
static void save_armies(struct player *p);
static int checkpos(void);
static int num_humans(int team);
static int totalPlayers();

static void
reaper(int sig)
{
    int stat=0;
    static int pid;

    while ((pid = WAIT3(&stat, WNOHANG, 0)) > 0) ;
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
    alarm_init();
    if (!debug)
        SIGNAL(SIGINT, cleanup);

    class = ATT;
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

#define POSITIONX 55000
#define POSITIONY 50000

    me->p_x = POSITIONX;                 /* displace to on overlooking position */
    me->p_y = POSITIONY;                 /* maybe we should just make it fight? */
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

#ifdef nodef
    /* Could make Merlin hostile to everybody, pretty fun */
    me->p_hostile = (FED | ROM | KLI | ORI); /* WAR! */
    me->p_swar = (FED | ROM | KLI | ORI); /* WAR! */
    me->p_war = (FED | ROM | KLI | ORI); /* WAR! */
#endif

#ifdef nodef
    /* Could make Merlin friendly and allow docking JKH */
    /* since he is in the middle of the galaxy */
    me->p_ship.s_type = STARBASE;    /* kludge to allow docking */
    me->p_flags |= PFDOCKOK;         /* allow docking */
    /* Merlin Cloaks when t-mode starts, so this is useless for now */
    /* It's an interesting bug as all the bots like hunterkiller are */
    /* also invisible */
#endif

    status->gameup |= GU_NEWBIE;
    queues[QU_NEWBIE_PLR].q_flags |= (QU_REPORT | QU_OPEN);
    queues[QU_NEWBIE_OBS].q_flags |= (QU_REPORT | QU_OPEN);
    queues[QU_PICKUP].q_flags &= ~(QU_REPORT | QU_OPEN);
    queues[QU_PICKUP_OBS].q_flags &= ~(QU_REPORT | QU_OPEN);

    me->p_process = getpid();
    me->p_timerdelay = HOWOFTEN; 

    /* allows robots to be forked by the daemon -- Evil ultrix bullshit */
    SIGSETMASK(0);

    me->p_status = PALIVE;              /* Put robot in game */

    while ((status->gameup) & GU_GAMEOK) {
        alarm_wait_for();
        checkmess();
    }
    cleanup(0);
}

void checkmess()
{
    int         shmemKey = PKEY;
    static int no_humans = 0;

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


    /* Stop a robot. */
    if ((ticks % ROBOEXITWAIT) == 0) {
        if(debugTarget != -1) {
            messOne(255, roboname, debugTarget, "Total Players: %d  Current bots: %d  Current human players: %d",
                    totalPlayers(), totalRobots(0), num_humans(0));
        }
        if (((QUPLAY(QU_NEWBIE_PLR) + QUPLAY(QU_NEWBIE_BOT)) > queues[QU_PICKUP].max_slots)
            && (QUPLAY(QU_NEWBIE_PLR) <= max_newbie_players)) {
            if(debugTarget != -1) {
                messOne(255, roboname, debugTarget, "Stopping a robot");
                messOne(255, roboname, debugTarget, "Current bots: %d  Current human players: %d",
                        totalRobots(0), num_humans(0));
            }
	    stop_a_robot();
	}
    }

    /* Start a robot */
    if ((ticks % ROBOCHECK) == 0) {
        int next_team = 0;
        num_players(&next_team);

        if (((QUPLAY(QU_NEWBIE_PLR) + QUPLAY(QU_NEWBIE_BOT)) < (queues[QU_PICKUP].max_slots))
            && (totalRobots(0) < max_newbie_robots))  {
            if(debugTarget != -1) {
                messOne(255, roboname, debugTarget, "Starting a robot");
                messOne(255, roboname, debugTarget, "Current bots: %d  Current human players: %d",
                        totalRobots(0), num_humans(0));
            }
            if (next_team == FED)
                start_a_robot("-Tf");
            else if (next_team == ROM)
                start_a_robot("-Tr");
            else if (next_team == ORI)
                start_a_robot("-To");
            else if (next_team == KLI)
                start_a_robot("-Tk");
            else
                fprintf(stderr, "Start_a_robot team select (%d) failed.", next_team);
        }
    }

    /* Check Merlin's x and y position */
    if ( (ticks % ROBOCHECK) == 0 ) {
	checkpos();
    }

    if ((ticks % SENDINFO) == 0) {
        static int alternate = 0;

        alternate++;
        if (1) {
            messAll(255,roboname,"Welcome to the Newbie Server.");
            messAll(255,roboname,"See http://genocide.netrek.org/beginner/newbie.php");
        }
        else {
            messAll(255,roboname,"Think you have what it takes?  Sign up for the draft league!");
            messAll(255,roboname, "See http://draft.lagparty.org/");
        }
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

/* assuming this gets called once a second... */
static int checkpos(void)
{
    static int oldx=POSITIONX;
    static int oldy=POSITIONY;
    static int moving=0;
    static int stopped=0;

    /* are we moving? */
    if ( (me->p_x != oldx) || (me->p_y != oldy) ) {
	moving=1;
	stopped=0;
	oldx=me->p_x;
	oldy=me->p_y;
    }

    /* if we stopped moving */
    /* count how long */
    if ( (me->p_x == oldx) && (me->p_y == oldy) ) {
	moving=0;
	stopped=stopped + 1;
    }

    /* stopped for sometime now */
    if ( moving==0 && stopped > 15 ) {
	/* move us back to overlooking position */
	if ( me->p_x != POSITIONX )
	    me->p_x = POSITIONX;
	if ( me->p_y != POSITIONY )
	    me->p_y = POSITIONY;
	stopped=0; /*do we need to reset this? */
    }

    return 1;
}

static int is_robots_only(void)
{
   return !num_humans(0);
}

static int totalRobots(int team)
{
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
      if(team != 0 && j->p_team != team)
         continue;

      if (rprog(j->p_login, j->p_monitor))
          /* Found a robot. */
         count++;
   }

   return count;
}

static int totalPlayers()
{
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
        count++;
   }
   return count;
}

static int num_humans(int team) {
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
      if(team != 0 && j->p_team != team)
         continue;
      if (!rprog(j->p_login, j->p_monitor)) {
         /* Found a human. */
         count++;
         if(debugTarget != -1 && debugLevel == 2) {
             messOne(255, roboname, debugTarget, "%d: Counting %s (%s %s) as a human",
                     i, j->p_mapchars, j->p_login, j->p_monitor);
         }
      }
      else {
         if(debugTarget != -1 && debugLevel == 2) {
             messOne(255, roboname, debugTarget, "%d: NOT Counting %s (%s %s) as a human",
                     i, j->p_mapchars, j->p_login, j->p_monitor);
          }
      }
   }
   return count;
}

static void stop_a_robot(void)
{
    int i;
    struct player *j;
    int teamToStop, rt;

    if(debugTarget != -1 && debugLevel == 3) {
        messOne(255, roboname, debugTarget, "#1(%d): %d  #2(%d): %d",
                team1, num_humans(team1), team2, num_humans(team2));
    }
    /* Either nuke robot from the team with the fewest humans, or stack humans on
       1 side, depending on balance setting.  If even, stop robot on team with
       the most robots.  If still even, stop a robot on a random team. */
    if (num_humans(team1) < num_humans(team2)) {
        if (!newbie_balance_humans && totalRobots(team2) != 0)
            teamToStop = team2;
        else
            teamToStop = team1;
    }
    else if (num_humans(team1) > num_humans(team2)) {
        if (!newbie_balance_humans && totalRobots(team1) != 0)
            teamToStop = team1;
        else
            teamToStop = team2;
    }
    else {
        if (totalRobots(team1) < totalRobots(team2))
            teamToStop = team2;
        else if (totalRobots(team1) > totalRobots(team2))
            teamToStop = team1;
        else {
            rt = random() % 2;
            if (rt == 0)
                teamToStop = team1;
            else
                teamToStop = team2;
       }
    }
    if(debugTarget != -1 && debugLevel == 3) {
        messOne(255, roboname, debugTarget, "Stopping from %d", teamToStop);
    }
    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE)
            continue;
        if (j->p_flags & PFROBOT)
            continue;

        /* If he's at the MOTD we'll get him next time. */
        if (j->p_team == teamToStop && j->p_status == PALIVE && rprog(j->p_login, j->p_monitor)) {
            stop_this_bot(j);
            return;
        }
    }
}

/* this is by no means foolproof */

static int
rprog(char *login, char *monitor)
{
    if (strcmp(login, "robot!") == 0)
        return 1;

    return 0;
}

int killrobot(pp_team)
{
    struct player *j;
    int i, keep, kill;

    keep = 0;
    kill = 0;
    for (i = 0, j = players; i < MAXPLAYER; i++) {
        if (j[i].p_status == PFREE)
            continue;
        if (j[i].p_status == POBSERV)
            continue;

        if (strcmp(j[i].p_login,"robot!") == 0) {
            if (j[i].p_status == PALIVE ) {
                if (j[i].p_team & pp_team) {
                    keep = i;
                    kill = 1;
                }
            }
        }
    }
    if (kill == 1)
        stop_this_bot(&j[keep]);
    return kill;
}

static void stop_this_bot(struct player *p)
{
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
    int tc, team_count[MAXTEAM+1];
    long int rt;
    int c = 0;
    int feds = 0, roms = 0, klis = 0, oris = 0;

    team_count[ROM] = 0;
    team_count[FED] = 0;
    team_count[ORI] = 0;
    team_count[KLI] = 0;
    tc = 0;

    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PFREE && j->p_status != POBSERV &&
                !(j->p_flags & PFROBOT)) {
            team_count[j->p_team]++;
            c++;
        }
    }

    /* Assign which team gets the next robot. */

    /* Count number of teams, and flag team as having at least 1 player */
    if (team_count[ROM] > 0) {
        tc++;
        roms=1;
    }
    if (team_count[FED] > 0) {
        tc++;
        feds=1;
    }
    if (team_count[KLI] > 0) {
        tc++;
        klis=1;
    }
    if (team_count[ORI] > 0) {
        tc++;
        oris=1;
    }

    if (tc == 0) { /* no teams yet, join anybody */
        rt = random() % 4;

        if (rt==0)
            *next_team = FED;
        if (rt==1)
            *next_team = ROM;
        if (rt==2)
            *next_team = KLI;
        if (rt==3)
            *next_team = ORI;
        /* Assign the first team */
        team1 = *next_team;
    }

    if (tc == 1) { /* 1 team, join 1 of 2 possible opposing teams */
        rt = random() % 2;

        if (team_count[FED] > 0) {
            if (rt == 1) {
                *next_team = ROM; 
            }
            else {
                *next_team = ORI; 
            }
        }

        if (team_count[ROM] > 0) {
            if (rt == 1) {
                *next_team = FED; 
            }
            else {
                *next_team = KLI; 
            }
        }

        if (team_count[KLI] > 0) {
            if (rt == 1) {
                *next_team = ROM; 
            }
            else {
                *next_team = ORI; 
            }
        }

        if (team_count[ORI] > 0) {
            if (rt == 1) {
                *next_team = FED; 
            }
            else {
                *next_team = KLI; 
            }
        }

        /* Assign the second team */
        team2 = *next_team;
    }

    if (tc >= 2) { /* 2 or more teams, join opposing team with less members */
                   /* And let's reassign teams just to be safe */

        if (team_count[FED]>0 && team_count[ROM]>0) {
            if (team_count[ROM]>team_count[FED])
                *next_team=FED;
            else
                *next_team=ROM;
            team1 = FED;
            team2 = ROM;
        }

        if (team_count[ORI]>0 && team_count[KLI]>0) {
            if (team_count[KLI]>team_count[ORI])
                *next_team=ORI;
            else
                *next_team=KLI;
            team1 = ORI;
            team2 = KLI;
        }

        if (team_count[FED]>0 && team_count[ORI]>0) {
            if (team_count[ORI]>team_count[FED])
                *next_team=FED;
            else
                *next_team=ORI;
            team1 = FED;
            team2 = ORI;
        }

        if (team_count[ROM]>0 && team_count[KLI]>0) {
            if (team_count[KLI]>team_count[ROM])
                *next_team=ROM;
            else
                *next_team=KLI;
            team1 = ROM;
            team2 = KLI;
        }

    }

    /* 3 or more tourn teams.... */
    /* kill off bots in teams with less than 4 players */
    /* re-align *next_team so we don't be polish about it */
    if (tc >= 3) { /* 3 or more teams */
        if (team_count[ROM]>=4 && team_count[FED]>=4) {
            killrobot(KLI);
            killrobot(ORI);
            if (team_count[ROM]>team_count[FED])
                *next_team=FED;
            else
                *next_team=ROM;
        }
        if (team_count[FED]>=4 && team_count[ORI]>=4) {
            killrobot(ROM);
            killrobot(KLI);
            if (team_count[ORI]>team_count[FED])
                *next_team=FED;
            else
                *next_team=ORI;
        }
        if (team_count[ROM]>=4 && team_count[KLI]>=4) {
            killrobot(FED);
            killrobot(ORI);
            if (team_count[KLI]>team_count[ROM])
                *next_team=ROM;
            else
                *next_team=KLI;
        }
        if (team_count[KLI]>=4 && team_count[ORI]>=4) {
            killrobot(FED);
            killrobot(ROM);
            if (team_count[KLI]>team_count[ORI])
                *next_team=ORI;
            else
                *next_team=KLI;
        }
    }


    return c;
}

static char           *
namearg(void)
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


static void
start_a_robot(char *team)
{
    char            command[256];
    int pid;

    /* How Merlin forks a robot
     * 
     * RCMD -> remote command, usually "" 
     * robot_host -> hostname of robot server, usually ""
     * OROBOT -> robot executable, usually "robot"
     * team -> whichever team, usually "-Tr" for Roms. 
     * hostname -> target newbie netrek server
     * PORT -> usually 3592
     * namearg() -> Name of the bot, circulates from the namelist above
     *
     * So robot command usually looks like:
     * robot -Tr -h localhost -p 3592 -n Obliterator -X robot! -g -b -0 -i
     * -Tr join Romulans
     * -h hostname
     * -p portname
     * -n player name
     * -X login name
     * -g send the OggV byte to ID self as a robot, the bots use this
     * -b blind mode, do not listen to anybody
     * -O no passwd during login sequence
     * -i INL mode, sets robot updates to 5 updates per second
     * -C read commands file, usually ROBOTDIR/og 
     */
    sprintf(command, "%s %s %s %s -h %s -p %d -n '%s' -X robot! -g -b -O -i",
            RCMD, robot_host, OROBOT, team, hostname, PORT, namearg() );

    sprintf(command, "%s -C %s",command, COMFILE); 

    pid = fork();
    if (pid == -1)
     return;
    if (pid == 0) {
        alarm_prevent_inheritance();
        execl("/bin/sh", "sh", "-c", command, (char *) NULL);
        perror("newbie'execl");
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
    queues[QU_NEWBIE_PLR].q_flags &= ~(QU_REPORT | QU_OPEN);
    queues[QU_NEWBIE_OBS].q_flags &= ~(QU_REPORT | QU_OPEN);
    queues[QU_PICKUP].q_flags |= (QU_REPORT | QU_OPEN);
    queues[QU_PICKUP_OBS].q_flags |= (QU_REPORT | QU_OPEN);
    exitRobot();
}

static void exitRobot(void)
{
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
