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
#include "planet.h"
#include "util.h"
#include "slotmaint.h"

#ifdef PRETSERVER

int debug=0;

/* define the name of the moderation bot - please note that due to the way */
/* messages are handled and the formatting of those messages care must be  */
/* taken to ensure that the bot name does not exceed 5 characters.  If the */
/* desired name is larger than 5 chars the message routines will need to   */
/* have their formatting and contents corrected */
#ifdef CHRISTMAS
char *roboname = "Santa";
#else
char *roboname = "Kathy";
#endif

#ifdef CHRISTMAS
#define NUMNAMES        9
#else
#define NUMNAMES        20
#endif

/* borrowed from daemon.c until we find a better place */
#define PLAYERFUSE      1

#ifdef CHRISTMAS
static char    *names[NUMNAMES] =
{"Rudolph",     "Dasher",      "Dancer",
 "Prancer",     "Vixen",       "Comet",
 "Cupid",       "Donner",      "Blitzen"};
#else
static char    *names[NUMNAMES] =
{"Annihilator", "Banisher",    "Blaster",
 "Demolisher",  "Destroyer",   "Eliminator",
 "Eradicator",  "Exiler",      "Obliterator",
 "Razer",       "Demoralizer", "Smasher",
 "Shredder",    "Vanquisher",  "Wrecker",
 "Ravager",     "Despoiler",   "Abolisher",
 "Emasculator", "Decimator"};
#endif

static  char    hostname[64];

int target;  /* Terminator's target 7/27/91 TC */
int phrange; /* phaser range 7/31/91 TC */
int trrange; /* tractor range 8/2/91 TC */
int ticks = 0;
int oldmctl;
static int realT = 0;
int team1 = 0;
int team2 = 0;
static struct planet savedplanets[MAXPLANETS];
static int galaxysaved = 0;
time_t savedtime;
time_t now;

static void cleanup(int);
void checkmess();
static void obliterate(int wflag, char kreason, int killRobots, int resetShip);
static void start_a_robot(char *team);
static void stop_a_robot(void);
#ifndef ROBOTS_STAY_IF_PLAYERS_LEAVE
static int is_robots_only(void);
#endif
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
static int totalPlayers(int noteam);
static void terminate(int);
static void savegalaxy(void);
static void restoregalaxy(void);
static void save_carried_armies(void);

static void
reaper(int sig)
{
    int pid, stat = 0;

    while ((pid = wait3(&stat, WNOHANG, 0)) > 0) ;
    signal(SIGCHLD, reaper);
}

int
main(argc, argv)
     int             argc;
     char           *argv[];
{
    int pno;

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
    (void) signal(SIGCHLD, reaper);
    openmem(1);
    strcpy(robot_host,REMOTEHOST);
    readsysdefaults();
    alarm_init();
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    if (!debug)
        signal(SIGINT, terminate);

    target = -1;                /* no target 7/27/91 TC */
    if ((pno = pickslot(QU_PRET_DMN)) < 0) {
        printf("exiting! due pickslot returning %d\n", pno);
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
    enter(4, 0, pno, STARBASE, roboname); /* was BATTLESHIP 8/9/91 TC */

    me->p_pos = -1;                     /* So robot stats don't get saved */
    me->p_flags |= (PFROBOT | PFCLOAK); /* Mark as a robot and hide it */

    /* don't appear in the galaxy */
    p_x_y_set(me, -100000, -100000);
    me->p_hostile = 0;
    me->p_swar = 0;
    me->p_war = 0;
    me->p_team = 0;     /* indep */

    oldmctl = mctl->mc_current;

    if (status->tourn)
        realT = 1;
    else
        status->gameup |= GU_PRET;

    me->p_process = getpid();
    p_ups_set(me, 10 / HOWOFTEN);

    /* set up team1 and team2 so we're not always playing rom/fed */
    team1 = (random()%2) ? FED : KLI;
    team2 = (random()%2) ? ROM : ORI;

    me->p_status = POBSERV;              /* Put robot in game */
    if (!status->tourn)
        resetPlanets();

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
    static int no_bots = 0;
    static int time_in_T = 0;
#ifndef ROBOTS_STAY_IF_PLAYERS_LEAVE
    static int no_humans = 0;
#endif

    me->p_ghostbuster = 0;         /* keep ghostbuster away */
    if (me->p_status != POBSERV){  /*So I'm not alive now...*/
        ERROR(2,("ERROR: %s died??\n", roboname));
        cleanup(0);   /* dead for some unpredicted reason like xsg */
        exit(1);
    }

    /* exit if daemon terminates use of shared memory */
    if (forgotten()) exit(1);

    ticks++;

#ifndef ROBOTS_STAY_IF_PLAYERS_LEAVE
    /* End the current game if no humans for 60 seconds. */
    if ((ticks % ROBOCHECK) == 0) {
        if (no_humans >= 60) {
            cleanup(0);
            exit(1);
        }

        if (is_robots_only())
            no_humans += ROBOCHECK / PERSEC;
        else
            no_humans = 0;
    }
#endif

    /* Check to see if we should start adding bots again */
    if ((ticks % ROBOCHECK) == 0) {
        if ((no_bots > time_in_T || no_bots >= 300) && realT) {
            if (pret_save_galaxy) {
                messAll(255, roboname, "*** Pre-T Entertainment restarting. T-mode galaxy saved. ***");
                messAll(255, roboname, "*** Galaxy will be restored if T-mode starts again within %d minutes ***", pret_galaxy_lifetime/60);
                if (pret_save_armies)
                    save_carried_armies();
                savegalaxy();
                galaxysaved = 1;
                savedtime = time((time_t *) 0);
            } else {
                messAll(255, roboname, "*** Pre-T Entertainment restarting. ***");
            }
            resetPlanets();
            realT = 0;
            status->gameup |= GU_PRET;
        }

        if (num_humans(0) < 8 && realT) {
            if ((no_bots % 60) == 0) {
                messAll(255, roboname, "Pre-T Entertainment will start in %d minutes if T-mode doesn't return...",
                        (((300 < time_in_T) ? 300 : time_in_T) - no_bots) / 60);
            }
            no_bots += ROBOCHECK / PERSEC;
        } else {
            no_bots = 0;
        }
    }

    /* check if either side has won */
    if ((ticks % ROBOCHECK) == 0) {
        checkPreTVictory();
    }

    /* Stop a robot. */
    if ((ticks % ROBOEXITWAIT) == 0) {
        if (robot_debug_target != -1) {
            messOne(255, roboname, robot_debug_target,
                    "Total Players: %d  Current bots: %d  Current human players: %d",
                    totalPlayers(1), totalRobots(0), num_humans(0));
        }
        if (totalPlayers(1) > PT_MAX_WITH_ROBOTS) {
            if (robot_debug_target != -1) {
                messOne(255, roboname, robot_debug_target, "Stopping a robot");
                messOne(255, roboname, robot_debug_target, "Current bots: %d  Current human players: %d",
                        totalRobots(0), num_humans(0));
            }
            stop_a_robot();
        }
    }

    /* Start a robot */
    if ((ticks % ROBOCHECK) == 0) {

        if (num_humans_alive() > 0 &&
            totalRobots(0) < PT_ROBOTS &&
            totalPlayers(1) < PT_MAX_WITH_ROBOTS &&
            realT == 0)
        {
            int next_team = 0;
            num_players(&next_team);

            if (robot_debug_target != -1) {
                messOne(255, roboname, robot_debug_target, "Starting a robot");
                messOne(255, roboname, robot_debug_target, "Current bots: %d  Current human players: %d",
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
                fprintf(stderr, "Start_a_robot team select (%d) failed.\n", next_team);
            status->gameup |= GU_PRET;
        }
    }

   /* Reset for real T mode ? */
   if ((ticks % ROBOCHECK) == 0) {
        if (totalRobots(0) == 0 && totalPlayers(0) >= 8) {
            time_in_T += ROBOCHECK / PERSEC;
            if (realT == 0) {
                time_in_T = 0;
                realT = 1;
                status->gameup &= ~GU_BOT_IN_GAME;
                messAll(255, roboname, "Resetting for real T-mode!");
                obliterate(0, TOURNSTART, 0, 1);
                if (pret_save_galaxy) {
                    if (galaxysaved) {
                        now = time((time_t *) 0);
                        if ((now - savedtime) < pret_galaxy_lifetime) {
                            messAll(255, roboname, "Restoring previous T-mode galaxy.");
                            restoregalaxy();
                        } else {
                            messAll(255, roboname, "Saved T-mode galaxy expired - creating new galaxy");
                            resetPlanets();
                        }
                        galaxysaved = 0;
                    }
                    else
                        resetPlanets();
                }
                status->gameup &= ~GU_PRET;
            }
        }
    }

    if ((ticks % SENDINFO) == 0) {
        if (totalRobots(0) > 0) {
            messAll(255, roboname, "              Welcome to the Pre-T Entertainment!");
            messAll(255, roboname, " ");
            messAll(255, roboname, "During Pre-T mode you are permitted to bomb and take planets.");
            messAll(255, roboname, "However, rank and your stats will NOT increase during this time.");
            messAll(255, roboname, " ");
            messAll(255, roboname, "Your team wins if you're up by at least %d planets.", pret_planets);
            messAll(255, roboname, " ");
            messAll(255, roboname, "Real t-mode will start as soon as there are a minimum of 4 human");
            messAll(255, roboname, "players per team.  Until then robots will enter and exit to");
            messAll(255, roboname, "maintain a minimum 4 vs 4 game.");
        }
    }
    while (oldmctl != mctl->mc_current) {
        oldmctl++;
        if (oldmctl == MAXMESSAGE) oldmctl = 0;
        robohelp(me, oldmctl, roboname);
    }
}

#ifndef ROBOTS_STAY_IF_PLAYERS_LEAVE
static int is_robots_only(void)
{
   return !num_humans(0);
}
#endif

static int totalPlayers(int noteam)
{
   int i;
   struct player *j;
   int count = 0;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j == me) continue;
        if (j->p_status == PFREE) continue;
        if (j->p_flags & PFROBOT) continue;
        if (j->p_flags & PFOBSERV) continue;
        if (!noteam && (j->p_team == ALLTEAM)) continue;
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
        if (j->p_status == PFREE) continue;
        if (j->p_flags & PFROBOT) continue;
        if (j->p_status == POBSERV) continue;
        if (team != 0 && j->p_team != team) continue;
        if (!is_robot(j)) {
            /* Found a human. */
            count++;
            if (robot_debug_target != -1 && robot_debug_level >= 2) {
                messOne(255, roboname, robot_debug_target,
                        "%d: Counting %s (%s %s) as a human",
                        i, j->p_mapchars, j->p_login, j->p_full_hostname);
            }
        } else {
            if (robot_debug_target != -1 && robot_debug_level >= 2) {
                messOne(255, roboname, robot_debug_target,
                        "%d: NOT Counting %s (%s %s) as a human",
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
        if (j->p_status != PALIVE) continue;
        if (j->p_flags & PFROBOT) continue;
        if (is_robot(j)) continue;
        count++;
   }
   return count;
}

static void stop_a_robot(void)
{
    int i;
    struct player *j;
    int teamToStop;
    char *why = " to make room for a human player.";

    if (robot_debug_target != -1 && robot_debug_level >= 3) {
        messOne(255, roboname, robot_debug_target, "#1(%d): %d  #2(%d): %d",
                team1, num_humans(team1), team2, num_humans(team2));
    }
    if (num_humans(team1) < num_humans(team2))
        teamToStop = team1;
    else
        teamToStop = team2;

    if (robot_debug_target != -1 && robot_debug_level >= 3) {
        messOne(255, roboname, robot_debug_target,
                "Stopping from %d", teamToStop);
    }

    /* remove a robot, first check for robots not carrying */
    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PALIVE) continue;
        if (j->p_team != teamToStop) continue;
        if (j->p_armies) continue;
        if (j == me) continue;
        if (!is_robot(j)) continue;
        stop_this_bot(j, why);
        return;
    }

    /* then ignore the risk of them carrying */
    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PALIVE) continue;
        if (j->p_team != teamToStop) continue;
        if (j == me) continue;
        if (!is_robot(j)) continue;
        stop_this_bot(j, why);
        return;
    }
}

static int totalRobots(int team)
{
   int i;
   struct player *j;
   int count = 0;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE) continue;
        if (j->p_flags & PFROBOT) continue;
        if (j->p_status == POBSERV) continue;
        if (j == me) continue;
        if (team != 0 && j->p_team != team) continue;
        if (!is_robot(j)) continue;
        /* Found a robot. */
        count++;
   }
   return count;
}

static void stop_this_bot(struct player *p, char *why)
{
    char msg[16];

    p->p_whydead = KQUIT;
    p->p_status = PDEAD;
    p->p_whodead = 0;
    snprintf(msg, 16, "%s->ALL", roboname);

    pmessage(0, MALL, msg,
#ifdef CHRISTMAS
             "Reindeer %s (%2s) was ejected%s",
#else
             "Robot %s (%2s) was ejected%s",
#endif
             p->p_name, p->p_mapchars, why);
    if ((p->p_status != POBSERV) && (p->p_armies>0)) save_armies(p);
}

static void save_carried_armies(void)
{
    int i;
    struct player *j;
    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE) continue;
        if (j->p_flags & PFROBOT) continue;
        if (j->p_status == POBSERV) continue;
        if (j->p_status == PDEAD) continue;
        if (j == me) continue;
        if (j->p_armies <= 0) continue;
        save_armies(j);
        j->p_armies = 0;
    }
}

static void save_armies(struct player *p)
{
  int i, k;
  char msg[16];
  snprintf(msg, 16, "%s->ALL", roboname);

  k = 10 * (remap[p->p_team] - 1);
  if (k >= 0 && k <= 30) for (i=0; i<10; i++) {
      if (planets[i+k].pl_owner == p->p_team) {
          planets[i+k].pl_armies += p->p_armies;
          pmessage(0, MALL, msg, "%s's %d arm%s placed on %s",
                   p->p_name, p->p_armies, (p->p_armies == 1) ? "y" : "ies",
                   planets[k+i].pl_name);
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
        if (j->p_status != PFREE && j->p_status != POBSERV && j != me) {
          team_count[j->p_team]++;
          c++;
        }
    }

    /* team sanity check */
    if (team_count[FED] != team_count[KLI]) {
        int t = (team_count[FED] > team_count[KLI]) ? FED : KLI;
        if (team1 != t) {
            team1 = t;
            resetPlanets();
        }
    }
    if (team_count[ROM] != team_count[ORI]) {
        int t = (team_count[ROM] > team_count[ORI]) ? ROM : ORI;
        if (team2 != t) {
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
            if (j->p_status != PFREE &&
                strncmp(name, j->p_name, strlen(name) - 1) == 0) {
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
    char why[80];

    snprintf(why, 80, " because %s is cleaning up.", roboname);

    do {
        /* terminate all robots */
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if ((j->p_status == PALIVE) && j != me && is_robot(j))
                stop_this_bot(j, why);
        }

        for (i=0; i<100; i++) {
            usleep(20000);
            me->p_ghostbuster = 0;
        }

        retry = 0;
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if ((j->p_status != PFREE) && j != me && is_robot(j))
                retry++;
        }
    } while (retry);            /* Some robots havn't terminated yet */

    obliterate(0, KOVER, 1, 1);
    status->gameup &= ~GU_PRET;
    if (terminate)
        exitRobot();
}

/* terminate is called as a signal handler and is a wrapper for cleanup() */
static void terminate (int ignored) {
    cleanup(1);
}

/* a pre-t victory is when one team is up by pret_planets planets */
static void checkPreTVictory() {
    int f, r, k, o, i;
    int winner = -1;

    /* fix a potential issue of invalid resets during a t-mode game */
    if (status->tourn) return;

    /* don't interfere with a real game */
    if (totalRobots(0) == 0) return;

    f = r = k = o = 0;
    for (i=0;i<40;++i) {
       if (planets[i].pl_owner == FED) f++;
       if (planets[i].pl_owner == ROM) r++;
       if (planets[i].pl_owner == KLI) k++;
       if (planets[i].pl_owner == ORI) o++;
    }
    if (f >= 10 + pret_planets) winner = FED;
    if (r >= 10 + pret_planets) winner = ROM;
    if (k >= 10 + pret_planets) winner = KLI;
    if (o >= 10 + pret_planets) winner = ORI;

    if (winner > 0) {
        messAll(255, roboname,
                "The %s have won this round of pre-T entertainment!",
                team_name(winner));
        /* wait for conquer parade to complete */
        while ((status->gameup & GU_CONQUER)) {
            usleep(20000);
            me->p_ghostbuster = 0;
        }
        obliterate(0, KWINNER, 0, 1);
        resetPlanets();
        galaxysaved = 0;
    }
}

static void resetPlanets(void) {
    int i;
    int owner;

    for (i=0;i<40;i++) {
        switch (i/10) {
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
        if (planets[i].pl_armies < 3)
            planets[i].pl_armies += (random() % 3) + 2;
        if (planets[i].pl_armies > 7)
            planets[i].pl_armies = 8 - (random() % 3);
        if (owner != team1 && owner != team2)
            planets[i].pl_armies = 30;
        planets[i].pl_owner = owner;
    }
    pl_reset();
}

static void exitRobot(void)
{
    if (me != NULL && me->p_team != ALLTEAM) {
        if (target >= 0) {
            messAll(255, roboname, "I'll be back.");
        } else {
            messAll(255, roboname, "#");
            messAll(255, roboname, "#  %s is tired.  "
                    "Pre-T Entertainment is over for now", roboname);
            messAll(255, roboname, "#");
        }
    }

    freeslot(me);
    exit(0);
}


static void obliterate(int wflag, char kreason, int killRobots, int resetShip)
{
    /* 0 = do nothing to war status, 1= make war with all, 2= make
    peace with all */
    struct player *j;

    /* clear torps and plasmas out */
    memset(torps, 0, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
    for (j = firstPlayer; j<=lastPlayer; j++) {
        if (j->p_status == PFREE) continue;
        if (j->p_status == POBSERV) continue;
        if ((j->p_flags & PFROBOT) && killRobots == 0) continue;
        if (j == me) continue;
        if ((kreason == TOURNSTART) && (j->p_ship.s_type == STARBASE)) {
#ifdef LTD_STATS
            if (ltd_offense_rating(j) < sb_minimal_offense)
#else
            if (offenseRating(j) < sb_minimal_offense)
#endif
            {
                j->p_status = PEXPLODE;
                j->p_whydead = TOURNSTART;
                j->p_whodead = me->p_no;
                j->p_explode = 2 * SBEXPVIEWS / PLAYERFUSE;
            }
        }
        if (resetShip) {
            j->p_kills = 0;
            if (j->p_ship.s_type != STARBASE)
                j->p_ship.s_plasmacost = -1;
        }
        j->p_ntorp = 0;
        j->p_nplasmatorp = 0;
        j->p_armies = 0;
        p_heal(j);
        if (wflag == 1)
            j->p_hostile = (FED | ROM | ORI | KLI);         /* angry */
        else if (wflag == 2)
            j->p_hostile = 0;       /* otherwise make all peaceful */
        j->p_war = (j->p_swar | j->p_hostile);
    }
}

static void savegalaxy(void)
{
    memcpy(savedplanets, planets, sizeof(struct planet) * MAXPLANETS);
}

static void restoregalaxy(void)
{
    memcpy(planets, savedplanets, sizeof(struct planet) * MAXPLANETS);
}

#endif  /* PRETSERVER */


#ifndef PRETSERVER
int
main(argc, argv)
     int             argc;
     char           *argv[];
{
    printf("You don't have PRETSERVER option on.\n");
    return 0;
}

#endif /* PRETSERVER */
