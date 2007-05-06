/* pret.c
*/

/*
 * Pre T entertainment by Nick Slager
 * Based on Newbie Server robot by Jeff Nowakowski
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
#include "pretdefs.h"
#include "util.h"
#include "planets.h"
#include "slotmaint.h"

int debug=0;

char *roboname = "Kathy";

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
static int realT = 0;
int team1 = 0;
int team2 = 0;
static int debugTarget = -1;
static int debugLevel = 0;

static void cleanup(int);
void checkmess();
static void obliterate(int wflag, char kreason, int killRobots);
static void start_a_robot(char *team);
static void stop_a_robot(void);
static int is_robots_only(void);
static int totalRobots(int team);
static void exitRobot(void);
static char * namearg(void);
static int num_players(int *next_team);
static void stop_this_bot(struct player * p, char *why);
static void save_armies(struct player *p);
static void resetPlanets(void);
static void checkPreTVictory();
static int num_humans(int team);
static int num_humans_alive();
static int totalPlayers();
static void doResources(void);
static void terminate(void);
 
static void
reaper(int sig)
{
    int stat=0;
    static int pid;

    while ((pid = WAIT3(&stat, WNOHANG, 0)) > 0) ;
    HANDLE_SIG(SIGCHLD,reaper);
}

#ifdef PRETSERVER
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
        SIGNAL(SIGINT, terminate);

    class = STARBASE;
    target = -1;                /* no target 7/27/91 TC */
    if ( (pno = pickslot(QU_PRET_DMN)) < 0) {
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
    enter(team, 0, pno, class, "Kathy"); /* was BATTLESHIP 8/9/91 TC */

    me->p_pos = -1;                              /* So robot stats don't get saved */
    me->p_flags |= (PFROBOT | PFCLOAK);          /* Mark as a robot and hide it */
    
    /* don't appear in the galaxy */
    p_x_y_set(me, -100000, -100000);
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

    status->gameup |= GU_PRET;

    me->p_process = getpid();
    p_ups_set(me, 10 / HOWOFTEN);

    /* set up team1 and team2 so we're not always playing rom/fed */
    team1 = (random()%2) ? FED : KLI;
    team2 = (random()%2) ? ROM : ORI;

    me->p_status = POBSERV;              /* Put robot in game */
    resetPlanets();

    while ((status->gameup) & GU_GAMEOK) {
        alarm_wait_for();
        checkmess();
    }
    cleanup(0);
    return 0;
}

void checkmess()
{ 
    int         shmemKey = PKEY;
    static int no_humans = 0;
    static int no_bots = 0;
    static int time_in_T = 0;

    me->p_ghostbuster = 0;         /* keep ghostbuster away */
    if (me->p_status != POBSERV){  /*So I'm not alive now...*/
        ERROR(2,("ERROR: Kathy died??\n"));
        cleanup(0);   /*Kathy is dead for some unpredicted reason like xsg */
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

    /* Check to see if we should start adding bots again */
    if ((ticks % ROBOCHECK) == 0) {
        if ((no_bots > time_in_T || no_bots >= 300) && realT) {
            messAll(255,roboname,"**** Pre-T Entertainment starting back up. ***");
            realT = 0;
        }

        if (num_humans(0) < 8 && realT) {
            if((no_bots % 60) == 0) {
                messAll(255,roboname,"Pre-T Entertainment will start in %d minutes if T-mode doesn't return...",
                        (((300<time_in_T)?300:time_in_T)-no_bots)/60);
            }
            no_bots += ROBOCHECK / PERSEC;
        }
        else
            no_bots = 0;
    }

    /* check if either side has won */
    if ((ticks % ROBOCHECK) == 0) {
        checkPreTVictory();
    }

    /* Stop a robot. */
    if ((ticks % ROBOEXITWAIT) == 0) {
        if(debugTarget != -1) {
            messOne(255, roboname, debugTarget, "Total Players: %d  Current bots: %d  Current human players: %d",
                    totalPlayers(), totalRobots(0), num_humans(0));
        }
        if(totalPlayers() > PT_MAX_WITH_ROBOTS) {
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

        if (num_humans_alive() > 0 &&
            totalRobots(0) < PT_ROBOTS &&
            totalPlayers() < PT_MAX_WITH_ROBOTS &&
            realT == 0)
        {
            int next_team = 0;
            num_players(&next_team);

            if (debugTarget != -1) {
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

   /* Reset for real T mode ? */
   if ((ticks % ROBOCHECK) == 0) {
        if(totalRobots(0) == 0 && totalPlayers() >= 8) {
            time_in_T += ROBOCHECK / PERSEC;
            if(realT == 0) {
                time_in_T = 0;
                realT = 1;
                status->gameup &= ~GU_BOT_IN_GAME;
                messAll(255,roboname,"Resetting for real T-mode!");
                obliterate(0, KPROVIDENCE, 0);
                resetPlanets();
		status->gameup &= ~GU_PRET;
            }
        }
    }

    if ((ticks % SENDINFO) == 0) {
        if (totalRobots(0) > 0) {
            messAll(255,roboname,"Welcome to the Pre-T Entertainment.");
            messAll(255,roboname,"Your team wins if you're up by at least 3 planets.");
        }
    }
    while (oldmctl!=mctl->mc_current) {
        oldmctl++;
        if (oldmctl==MAXMESSAGE) oldmctl=0;
        robohelp(me, oldmctl, roboname);
    }
}

static int is_robots_only(void)
{
   return !num_humans(0);
}

static int totalPlayers()
{
   int i;
   struct player *j;
   int count = 0;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j == me) continue;
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

static int num_humans(int team)
{
   int i;
   struct player *j;
   int count = 0;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j == me) continue;
        if (j->p_status == PFREE)
            continue;
        if (j->p_flags & PFROBOT)
            continue;
        if (j->p_status == POBSERV)
            continue;
        if(team != 0 && j->p_team != team)
            continue;
        if (!is_robot(j)) {
            /* Found a human. */
            count++;
            if(debugTarget != -1 && debugLevel == 2) {
                messOne(255, roboname, debugTarget, "%d: Counting %s (%s %s) as a human",
                        i, j->p_mapchars, j->p_login, j->p_full_hostname);
            }
        }
        else {
            if(debugTarget != -1 && debugLevel == 2) {
                messOne(255, roboname, debugTarget, "%d: NOT Counting %s (%s %s) as a human",
                        i, j->p_mapchars, j->p_login, j->p_full_hostname);
            }
        }
   }
   return count;
}

static int num_humans_alive()
{
   int i;
   struct player *j;
   int count = 0;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j == me) continue;
        if (j->p_status != PALIVE)
            continue;
        if (j->p_flags & PFROBOT)
            continue;
        if (!is_robot(j)) {
            count++;
        }
   }
   return count;
}

static void stop_a_robot(void)
{
    int i;
    struct player *j;
    int teamToStop;
    char *why = " to make room for a human player.";

    if(debugTarget != -1 && debugLevel == 3) {
        messOne(255, roboname, debugTarget, "#1(%d): %d  #2(%d): %d",
                team1, num_humans(team1), team2, num_humans(team2));
    }
    if(num_humans(team1) < num_humans(team2))
        teamToStop = team1;
    else
        teamToStop = team2;

    if(debugTarget != -1 && debugLevel == 3) {
        messOne(255, roboname, debugTarget, "Stopping from %d", teamToStop);
    }

    /* remove a robot, first check for robots not carrying */
    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PALIVE) continue;
        if (j->p_team != teamToStop) continue;
        if (j->p_armies) continue;
        if (j == me) continue;
        if (is_robot(j)) {
            stop_this_bot(j, why);
            return;
        }
    }

    /* then ignore the risk of them carrying */
    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PALIVE) continue;
        if (j->p_team != teamToStop) continue;
        if (j == me) continue;
        if (is_robot(j)) {
            stop_this_bot(j, why);
            return;
        }
    }
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
        if (j == me) continue;
        if (team != 0 && j->p_team != team)
            continue;

        if (is_robot(j))
            /* Found a robot. */
            count++;
   }
   return count;
}

static void stop_this_bot(struct player *p, char *why)
{
    p->p_ship.s_type = STARBASE;
    p->p_whydead=KQUIT;
    p->p_explode=10;
    p->p_status=PEXPLODE;
    p->p_whodead=0;

    pmessage(0, MALL, "Kathy->ALL", 
        "Robot %s (%2s) was ejected%s",
        p->p_name, p->p_mapchars, why);
    if ((p->p_status != POBSERV) && (p->p_armies>0)) save_armies(p);
}

static void save_armies(struct player *p)
{
  int i, k;

  k=10*(remap[p->p_team]-1);
  if (k>=0 && k<=30) for (i=0; i<10; i++) {
    if (planets[i+k].pl_owner==p->p_team) {
      planets[i+k].pl_armies += p->p_armies;
      pmessage(0, MALL, "Kathy->ALL", "%s's %d armies placed on %s",
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

    team_count[FED] = 0;
    team_count[KLI] = 0;
    team_count[ROM] = 0;
    team_count[ORI] = 0;

    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PFREE && j->p_status != POBSERV &&
            !(j->p_flags & PFROBOT) && j != me)
            {
                team_count[j->p_team]++;
                c++;
            }
    }

    /* team sanity check */
    if(team_count[FED] != team_count[KLI]) {
      int t = (team_count[FED]>team_count[KLI])?FED:KLI;
      if(team1 != t) {
        team1 = t;
        resetPlanets();
      }
    }
    if(team_count[ROM] != team_count[ORI]) {
      int t = (team_count[ROM]>team_count[ORI])?ROM:ORI;
      if(team2 != t) {
        team2 = t;
        resetPlanets();
      }
    }

    /* Assign which team gets the next robot. */
    if (team_count[team1] > team_count[team2])
        *next_team = team2;
    else
        *next_team = team1;

    return c;
}

static char           *
namearg(void)
{
    int i, k = 0;
    struct player *j;
    char           *name;
    int             namef = 1;

    if (pret_guest) return "Guest";

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
    int pid;

    pid = fork();
    if (pid == -1) return;
    if (pid == 0) {
        alarm_prevent_inheritance();
        execl(OROBOT, "pretbot",
              team,
              "-h", (strlen(robot_host))?robot_host:hostname,
              "-p", PORT,
              "-n", namearg(),
              "-X", PRE_T_ROBOT_LOGIN,
              "-b", "-O", "-I", "-g",
              "-C", COMFILE, (char *) NULL);
        perror("pretbot'execl");
        _exit(1);
    }
    status->gameup |= GU_BOT_IN_GAME;
}

static void cleanup(int terminate)
{
    struct player *j;
    int i, retry;

    do {
        /* terminate all robots */
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if ((j->p_status == PALIVE) && j != me && is_robot(j))
                stop_this_bot(j, " because Kathy is cleaning up.");
        }

        USLEEP(2000000);
        retry=0;
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if ((j->p_status != PFREE) && j != me && is_robot(j))
                retry++;
        }
    } while (retry);            /* Some robots havn't terminated yet */

    obliterate(0, KPROVIDENCE, 1);
    status->gameup &= ~GU_PRET;
    if (terminate)
        exitRobot();
}

/* terminate is called as a signal handler and is a wrapper for cleanup() */
static void terminate () {
	cleanup (1);
}

/* a pre-t victory is when one team is up by 3 planets */
static void checkPreTVictory() {
    int f, r, k, o, i;
    int winner = -1;

    /* don't interfere with a real game */
    if(totalRobots(0) == 0) return;

    f = r = k = o = 0;
    for(i=0;i<40;++i) {
       if(planets[i].pl_owner == FED) f++; 
       if(planets[i].pl_owner == ROM) r++; 
       if(planets[i].pl_owner == KLI) k++; 
       if(planets[i].pl_owner == ORI) o++; 
    }
    if(f>=13) winner = FED;
    if(r>=13) winner = ROM;
    if(k>=13) winner = KLI;
    if(o>=13) winner = ORI;

    if(winner > 0) {
        messAll(255,roboname,"The %s have won this round of pre-T entertainment!", team_name(winner));
        obliterate(0, KPROVIDENCE, 0);
        resetPlanets();
    }
}

static void resetPlanets(void) {
    int i;
    int owner;
    for(i=0;i<40;i++) {
        switch(i/10) {
            case 0:
                owner = FED;
                break;
            case 1:
                owner = ROM;
                break;
            case 2:
                owner = KLI;
                break;
            default:
                owner = ORI;
                break;
        }
        if(planets[i].pl_armies < 3) 
            planets[i].pl_armies += (random() % 3) + 2;
        if(planets[i].pl_armies > 7) 
            planets[i].pl_armies = 8 - (random() % 3);
        if(owner != team1 && owner != team2)
            planets[i].pl_armies = 30;
        planets[i].pl_owner = owner;
    }

/* set planet resources */
    doResources ();
}

/* maximum number of agris that can exist in one quadrant */
#define AGRI_LIMIT 3

/* the four close planets to the home planet */
static int core_planets[4][4] =
{{ 7, 9, 5, 8,},
 { 12, 19, 15, 16,},
 { 24, 29, 25, 26,},
 { 34, 39, 38, 37,},
};
/* the outside edge, going around in order */
static int front_planets[4][5] =
{{ 1, 2, 4, 6, 3,},
 { 14, 18, 13, 17, 11,},
 { 22, 28, 23, 21, 27,},
 { 31, 32, 33, 35, 36,},
};

void doResources(void)
{
  int i, j, k, which;
  MCOPY(pdata, planets, sizeof(pdata));
  for (i = 0; i< 40; i++) {
        planets[i].pl_armies = top_armies;
  }
  for (i = 0; i < 4; i++){
    /* one core AGRI */
    planets[core_planets[i][random() % 4]].pl_flags |= PLAGRI;

    /* one front AGRI */
    which = random() % 2;
    if (which){
      planets[front_planets[i][random() % 2]].pl_flags |= PLAGRI;

      /* place one repair on the other front */
      planets[front_planets[i][(random() % 3) + 2]].pl_flags |= PLREPAIR;

      /* place 2 FUEL on the other front */
      for (j = 0; j < 2; j++){
      do {
        k = random() % 3;
      } while (planets[front_planets[i][k + 2]].pl_flags & PLFUEL) ;
      planets[front_planets[i][k + 2]].pl_flags |= PLFUEL;
      }
    } else {
      planets[front_planets[i][(random() % 2) + 3]].pl_flags |= PLAGRI;

      /* place one repair on the other front */
      planets[front_planets[i][random() % 3]].pl_flags |= PLREPAIR;

      /* place 2 FUEL on the other front */
      for (j = 0; j < 2; j++){
      do {
        k = random() % 3;
      } while (planets[front_planets[i][k]].pl_flags & PLFUEL);
      planets[front_planets[i][k]].pl_flags |= PLFUEL;
      }
    }

    /* drop one more repair in the core (home + 1 front + 1 core = 3 Repair)*/
    planets[core_planets[i][random() % 4]].pl_flags |= PLREPAIR;

    /* now we need to put down 2 fuel (home + 2 front + 2 = 5 fuel) */
    for (j = 0; j < 2; j++){
      do {
      k = random() % 4;
      } while (planets[core_planets[i][k]].pl_flags & PLFUEL);
      planets[core_planets[i][k]].pl_flags |= PLFUEL;
    }
  }
}

static void exitRobot(void)
{
    if (me != NULL && me->p_team != ALLTEAM) {
        if (target >= 0) {
            messAll(255,roboname, "I'll be back.");
        }
        else {
            messAll(255,roboname,"#");
            messAll(255,roboname,"#  Kathy is tired.  Pre-T Entertainment is over "
                    "for now");
            messAll(255,roboname,"#");
        }
    }

    freeslot(me);
    exit(0);
}


static void obliterate(int wflag, char kreason, int killRobots)
{
    /* 0 = do nothing to war status, 1= make war with all, 2= make peace with all */
    struct player *j;

    /* clear torps and plasmas out */
    MZERO(torps, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
    for (j = firstPlayer; j<=lastPlayer; j++) {
        if (j->p_status == PFREE)
            continue;
        if (j->p_status == POBSERV)
            continue;
        if ((j->p_flags & PFROBOT) && killRobots == 0)
            continue;
        if (j == me) continue;
        j->p_armies = 0;
        j->p_kills = 0;
        j->p_ntorp = 0;
        j->p_nplasmatorp = 0;
        if (wflag == 1)
            j->p_hostile = (FED | ROM | ORI | KLI);         /* angry */
        else if (wflag == 2)
            j->p_hostile = 0;       /* otherwise make all peaceful */
        j->p_war = (j->p_swar | j->p_hostile);
    }
}
#endif  /* PRETSERVER */


#ifndef PRETSERVER
int
main(argc, argv)
     int             argc;
     char           *argv[];
{
    printf("You don't have PRETSERVER option on.\n");
}

#endif /* PRETSERVER */
