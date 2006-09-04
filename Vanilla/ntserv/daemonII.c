/* 
 * daemonII.c 
 */
#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "planets.h"
#include "proto.h"
#include "conquer.h"
#include "daemon.h"
#include "alarm.h"

#include INC_UNISTD
#include INC_SYS_FCNTL
#include INC_SYS_TIME

#ifdef AUTOMOTD
#include <sys/stat.h>
#endif

#ifdef PUCK_FIRST 
#include <sys/sem.h> 
/* this is from a semctl man page */
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED) 
/* union semun is defined by including <sys/sem.h> */ 
#else /*__GNU_LIBRARY__ && !__SEM_SEMUN_UNDEFINED */
/* according to X/OPEN we have to define it ourselves */ 
union semun {
   int val;                                   /* value for SETVAL */
   struct semid_ds *buf;              /* buffer for IPC_STAT, IPC_SET */
   unsigned short int *array;         /* array for GETALL, SETALL */
   struct seminfo *__buf;             /* buffer for IPC_INFO */ 
   }; 
#endif /*__GNU_LIBRARY__ && !__SEM_SEMUN_UNDEFINED */
#endif /*PUCK_FIRST*/

#define fuse(X) ((ticks % (X)) == 0)
#define TOURNEXTENSION 15       /* Tmode gone for 15 seconds 8/26/91 TC */
#define NotTmode(X) (!(status->tourn) && ((X - tourntimestamp)/10 > TOURNEXTENSION))

#undef PMOVEMENT

#ifdef SBFUEL_FIX
#ifndef MIN
#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#endif
#endif

/* file scope prototypes */
static void check_load(void);
static int tournamentMode(void);
static int check_scummers(int);
static void move();
static void udplayersight(void);
static void udplayers(void);
static void udplayerpause(void);
static void changedir(struct player *sp);
static void udcloak(void);
static void torp_track_target(struct torp *t);
static void udtorps(void);
static int near(struct torp *t);
static void explode(struct torp *torp);
#ifndef LTD_STATS
static void loserstats(int pl);
#ifdef CHAIN_REACTION
static void killerstats(int cl, int pl, struct player *victim);
#else
static void killerstats(int pl, struct player *victim);
#endif /* CHAIN_REACTION */
static void checkmaxkills(int pl);
#endif /* LTD_STATS */
static void army_track(int type, void *who, void *what, int num);
static void udplanets(void);
static void PopPlanet(int plnum);
static void RandomizePopOrder(void);
static void udsurrend(void);
static void udphaser(void);
static void teamtimers(void);
static void pldamageplayer(struct player *j);
static void plfight(void);
static void beam(void);
static void blowup(struct player *sh);
static void exitDaemon(int sig);
static void save_planets(void);
static void checkgen(int loser, struct player *winner);
static int checkwin(struct player *winner);
static void ghostmess(struct player *victim, char *reason);
static void saveplayer(struct player *victim);
static void rescue(int team, int target);
#ifdef CHAIN_REACTION
static void killmess(struct player *victim, struct player *credit_killer,
                     struct player *k, int whydead);
#else
static void killmess(struct player *victim, struct player *k);
#endif
static void reaper(int sig);
static void addTroops(int loser, int winner);
static void surrenderMessage(int loser);
static void genocideMessage(int loser, int winner);
static void conquerMessage(int winner);
static void displayBest(FILE *conqfile, int team, int type);
static void fork_robot(int robot);
/* static void doRotateGalaxy(void); */
static void signal_servers(void);

/* external scope prototypes */
extern void pinit(void);
extern void solicit(int force);
extern void pmove(void);

#ifdef PUCK_FIRST
static void signal_puck(void);
static int pucksem_id;
static union semun pucksem_arg;
static struct sembuf pucksem_op[1];
#endif /*PUCK_FIRST*/

static int debug = 0;
static int ticks = 0;
static int tourntimestamp = 0; /* ticks since last Tmode 8/2/91 TC */

static int tcount[MAXTEAM + 1];
u_char getbearing();

int arg[8];

int main(int argc, char **argv)
{
    register int i;
    int x = 0;
    static struct itimerval udt;
    int glfd, plfd;

#ifdef AUTOMOTD
  time_t        mnow;
  struct stat   mstat;
#endif

    if (argc > 1)
        debug = 1;

    getpath();          /* added 11/6/92 DRG */
    srandom(getpid());
    if (!debug) {
        for (i = 1; i < NSIG; i++) {
            (void) SIGNAL(i, exitDaemon);
        }
        SIGNAL(SIGSTOP, SIG_DFL); /* accept SIGSTOP? 3/6/92 TC */
        SIGNAL(SIGTSTP, SIG_DFL); /* accept SIGTSTP? 3/6/92 TC */
        SIGNAL(SIGCONT, SIG_DFL); /* accept SIGCONT? 3/6/92 TC */
      }

    /* Set up the shared memory segment and attach to it*/
    if (!(setupmem())){
#ifdef ONCHECK
        execl("/bin/rm", "rm", "-f", On_File, 0);
#endif
        exit(1);
    }

    readsysdefaults();
    
    for (i = 0; i < MAXPLAYER; i++) {
        players[i].p_status = PFREE;
#ifdef LTD_STATS
        ltd_reset(&(players[i]));
#else
        players[i].p_stats.st_tticks=1;
#endif
        players[i].p_no=i;
        players[i].p_timerdelay = defskip;
        MZERO(players[i].voting, sizeof(time_t) * PV_TOTAL);
    }

#ifdef NEUTRAL
    plfd = open(NeutFile, O_RDWR, 0744);
#else
    plfd = open(PlFile, O_RDWR, 0744);
#endif

    if (resetgalaxy) {          /* added 2/6/93 NBT */
        doResources(); 
    }
    else {
      if (plfd < 0) {
        ERROR(1,( "No planet file.  Restarting galaxy\n"));
        doResources();
      }
      else {
        if (read(plfd, (char *) planets, sizeof(pdata)) != sizeof(pdata)) {
            ERROR(1,("Planet file wrong size.  Restarting galaxy\n"));
            doResources();
        }
        (void) close(plfd);
      }
    }

    /* Initialize the planet movement capabilities */
    pinit();

    glfd = open(Global, O_RDWR, 0744);
    if (glfd < 0) {
        ERROR(1,( "No global file.  Resetting all stats\n"));
        MZERO((char *) status, sizeof(struct status));
    } else {
        if (read(glfd, (char *) status, sizeof(struct status)) != 
            sizeof(struct status)) {
            ERROR(1,( "Global file wrong size.  Resetting all stats\n"));
            MZERO((char *) status, sizeof(struct status));
        }
        (void) close(glfd);
    }
    if (status->time==0) {
        /* Start all stats at 1 to prevent overflow */
        /* 10, now because of division by 10 in socket.c */
        status->time=10;
        status->timeprod=10;
        status->planets=10;
        status->armsbomb=10;
        status->kills=10;
        status->losses=10;
    }
    context->daemon = getpid();

#undef wait

    queues_init();  /* Initialize the queues for pickup */

    status->active = 0;
    status->gameup = (GU_GAMEOK | (chaosmode ? GU_CHAOS : 0));

    (void) SIGNAL(SIGCHLD, reaper);

#ifdef AUTOMOTD
   if(stat(Motd, &mstat) == 0 && (time(NULL) - mstat.st_mtime) > 60*60*12){
      if(fork() == 0){
         execl("/bin/sh", "sh", "-c", MakeMotd, 0);
         perror(MakeMotd);
         _exit(1);
      }
   }
#endif

   if (start_robot) fork_robot(start_robot);

#ifdef PUCK_FIRST
    if ((pucksem_id = semget(PUCK_FIRST, 1, 0600 | IPC_CREAT)) != -1 ||
        (pucksem_id = semget(PUCK_FIRST, 1, 0600)) != -1) 
    {
        pucksem_arg.val = 0;
        pucksem_op[0].sem_num = 0;
        pucksem_op[0].sem_op = -1;
        pucksem_op[0].sem_flg = 0;
    }
    else 
    {
        ERROR(1,("Unable to get puck semaphore."));
    }
#endif /*PUCK_FIRST*/
    
    alarm_init();
    udt.it_interval.tv_sec = 0;
    udt.it_interval.tv_usec = reality;
    udt.it_value.tv_sec = 0;
    udt.it_value.tv_usec = reality;
    if (setitimer(ITIMER_REAL, &udt, (struct itimerval *) 0) < 0){
        ERROR(1,( "daemon:  Error setting itimer.  Exiting.\n"));        
        exitDaemon(0);
    }

    check_load();

    /* signal parent ntserv that daemon is ready */
    kill(getppid(), SIGUSR1);

    x = 0;
    for (;;) {
        alarm_wait_for();
        move();
        if (debug) {
            if (!(++x % 50))
                ERROR(1,("Mark %d\n", x));
            if (x > 10000) x = 0;                       /* safety measure */
        }
    }
}

/* These specify how often special actions will take place in
   UPDATE units (0.10 seconds, currently.)
*/

#define PLAYERFUSE      1
#define TORPFUSE        1
#define PLASMAFUSE      1
#define PHASERFUSE      1
#define CLOAKFUSE       2
#define TEAMFUSE        5
#define PLFIGHTFUSE     5
#define SIGHTFUSE       5
#define BEAMFUSE        8       /* scott 8/25/90 -- was 10 */
#define PMOVEFUSE       300      /* planet movement fuse 07/26/95 JRP */
#define QUEUEFUSE       600     /* cleanup every 60 seconds */
#ifdef INL_POP
#define PLANETFUSE      400/MAXPLANETS  /* INL 3.9 POP */
#else
#define PLANETFUSE      400     /* scott 8/25/90 -- was 600 */
#endif
#define MINUTEFUSE      600     /* 1 minute, surrender funct etc. 4/15/92 TC */
#define SYNCFUSE        3000
#define CHECKLOADFUSE   6000    /* 10 min. */
#define HOSEFUSE        18000   /* 30 min., was 20 minutes 6/7/95 JRP */
#define HOSEFUSE2       3000    /*  5 min., was  3 minutes 6/29/92 TC */

#ifndef INL_POP
#define PLANETSTAGGER   4       /* how many udplanets calls we want
                                   to replace the original call.  #ifdef 
                                   this out for just 1 call.  4/15/92 TC */

#endif

#ifdef PLANETSTAGGER            /* (shorten planetfuse here) */
#undef PLANETFUSE
#define PLANETFUSE      400/PLANETSTAGGER
#endif

#define GHOSTTIME       (30 * 1000000 / UPDATE) /* 30 secs */
#define KGHOSTTIME      (32 * 1000000 / UPDATE) /* 32 secs */
#define OUTFITTIME      (3 * AUTOQUIT * 1000000 / UPDATE) /* three minutes */
#define LOGINTIME       (6 * AUTOQUIT * 1000000 / UPDATE) /* six minutes */

#define HUNTERKILLER    (-1)
#define TERMINATOR      (-2)    /* Terminator */
#define STERMINATOR     (-3)    /* sticky Terminator */

int dietime = -1;  /* isae - was missing int */

static int tournamentMode(void)
/* Return 1 if tournament mode, 0 otherwise.
 * This function only used to determine if we should be recording stats.
 */
{
    int               i, teams[MAXTEAM + 1]; /* not the global var */
    struct player       *p;

    if (practice_mode) return(0);     /* No t-mode in practice mode 
                                         06/19/94 [007] */
#ifdef PRETSERVER    
    if(bot_in_game) return(0);
#endif
    
    MZERO((int *) teams, sizeof(int) * (MAXTEAM + 1));
    for (i=0, p=players; i<MAXPLAYER; i++, p++) {
        if (((p->p_status != PFREE) && 
#ifdef OBSERVERS
             (p->p_status != POBSERV)) && 
#endif
           !(p->p_flags & PFROBOT)) {
            teams[p->p_team]++; /* don't count robots for Tmode 10/31/91 TC */
        }               /* don't count observers for Tmode 06/09/95 JRP */
    }
    i=0;
    if (teams[FED]>=tournplayers) i++;
    if (teams[ROM]>=tournplayers) i++;
    if (teams[KLI]>=tournplayers) i++;
    if (teams[ORI]>=tournplayers) i++;
    if (i>1) return(1);
    return(0);
}


/* Check the player list for possible t-mode scummers.
   Nick Trown   12/19/92
*/

static int check_scummers(int verbose)
{
    int i, j;
    int num;
    int who=0;
    FILE *fp;
    struct tm *today;
    time_t gmtsecs;

    for (i=0; i<MAXPLAYER; i++) {
      struct player *me = &players[i];
      num=0;
      if (me->p_status == PFREE)
           continue;
      if (me->p_flags & PFROBOT)
           continue;
#ifdef OBSERVERS
      if (me->p_status == POBSERV)
           continue;
#endif
#ifdef LTD_STATS
      if (ltd_ticks(me, LTD_TOTAL) != 0)
#else
      if (me->p_stats.st_tticks != 0)
#endif
      {
        for (j=i+1; j<MAXPLAYER; j++) {
          struct player *them = &players[j];
          if (them->p_status == PFREE)
                  continue;
          if (them->p_flags & PFROBOT)
                  continue;
#ifdef OBSERVERS
          if (them->p_status == POBSERV)
                  continue;
#endif
#ifdef LTD_STATS
          if (ltd_ticks(them, LTD_TOTAL) != 0)
#else
          if (them->p_stats.st_tticks != 0)
#endif
          {
            if (strcmp(me->p_ip, them->p_ip) == 0) {
                who=j;
                num++;
            }
          }
        }
        if (num>(check_scum-1)){
            if (!verbose) return 1;
            pmessage(0,MALL,"GOD->ALL", "*****************************************");
            pmessage(0,MALL,"GOD->ALL","Possible t-mode scummers have been found.");
            pmessage(0,MALL,"GOD->ALL","They have been noted for god to review.");
            pmessage(0,MALL,"GOD->ALL", "*****************************************");
            if ((fp = fopen (Scum_File,"a"))==NULL) {
                ERROR(1,("Unable to open scum file.\n"));
                return 1;
            }
            fprintf(fp,"POSSIBLE T-MODE SCUMMERS FOUND!!! (slots %d and %d) ",i,who);
            gmtsecs = time(NULL);
            today = localtime(&gmtsecs); 
            fprintf(fp,"on %d-%02d-%02d at %d:%d.%d\n",
                today->tm_year+1900,(today->tm_mon)+1,today->tm_mday,
                today->tm_hour,today->tm_min,today->tm_sec);
            for (j=0; j<MAXPLAYER; j++) {
#ifdef LTD_STATS
                if (!(players[j].p_status == PFREE ||
                      ltd_ticks(&(players[j]), LTD_TOTAL) == 0)) {
#else
                if (!(players[j].p_status == PFREE ||
                      players[j].p_stats.st_tticks == 0)) {
#endif

                  fprintf(fp,"  %2s: %-16s  <%s@%s> %6.2f\n",
                    players[j].p_mapchars, players[j].p_name,
                    players[j].p_login,
#ifndef FULL_HOSTNAMES
                    players[j].p_monitor,
#else
                    players[j].p_full_hostname,
#endif

                    players[j].p_kills);
                }
            }
            fclose(fp);
            return 1;
        }
      }
    }
    return 0;
}


static void political_begin(int message)
{
        switch (message) {
        case 0:
                pmessage(0, MALL, "GOD->ALL","A dark mood settles upon the galaxy");
                break;
        case 1:
                pmessage(0, MALL, "GOD->ALL","Political pressure to expand is rising");
                break;
        case 2:
                pmessage(0, MALL, "GOD->ALL","Border disputes break out as political tensions increase!");
                break;
        case 3:
                pmessage(0, MALL, "GOD->ALL","Galactic superpowers seek to colonize new territory!");
                break;
        case 4:
                pmessage(0, MALL, "GOD->ALL","'Manifest Destiny' becomes motive in galactic superpower conflict!");
                break;
        case 5:
                pmessage(0, MALL, "GOD->ALL","Diplomat insults foriegn emperor's mother and fighting breaks out!");
                break;
        case 6:
                pmessage(0, MALL, "GOD->ALL","%s declares self as galactic emperor and chaos breaks out!",
                         WARMONGER);
                break;
        default:
                pmessage(0, MALL, "GOD->ALL","Peace parties have been demobilized, and fighting ensues.");
                break;
        }
}

static void political_end(int message)
{
        switch (message) {
        case 0:
                pmessage(0, MALL, "GOD->ALL","A new day dawns as the oppressive mood lifts");
                break;
        case 1:
                pmessage(0, MALL, "GOD->ALL","The expansionist pressure subsides");
                break;
        case 2:
                pmessage(0, MALL, "GOD->ALL","Temporary agreement is reached in local border disputes.");
                break;
        case 3:
                pmessage(0, MALL, "GOD->ALL","Galactic governments reduce colonization efforts.");
                break;
        case 4:
                pmessage(0, MALL, "GOD->ALL","'Manifest Destiny is no longer a fad.' says influential philosopher.");
                break;
        case 5:
                pmessage(0, MALL, "GOD->ALL","Diplomat apologizes to foreign emperor's mother and invasion is stopped!");
                break;
        case 6:
                pmessage(0, MALL, "GOD->ALL","%s is locked up and order returns to the galaxy!",
                         WARMONGER);
                break;
        default:
                pmessage(0, MALL, "GOD->ALL","The peace party has reformed, and is rallying for peace");
                break;
        }
}

static void move()
{
    static int oldmessage;
    int old_robot;
    static enum ts {
            TS_PICKUP, 
            TS_SCUMMERS,
            TS_BEGIN,
            TS_TOURNAMENT,
            TS_END
    } ts = TS_PICKUP;

    if (fuse(QUEUEFUSE)){
        queues_purge();
        solicit(0);
        bans_age_temporary(QUEUEFUSE/reality);
    }

    if (ispaused){ /* Game is paused */
      if (fuse(PLAYERFUSE))
        udplayerpause();
      if (status->gameup & GU_CONQUER) conquer_update();
      signal_servers();
      return;
    }

    if (++ticks == dietime) {/* no player for 1 minute. kill self */
        if (debug) {
            ERROR(1,("Ho hum.  1 minute, no activity...\n"));
        }
        else {
            ERROR(3,("Self-destructing the daemon!\n"));
#ifdef SELF_RESET /* isae -- reset the galaxy when the daemon dies */
            ERROR(3,( "Resetting the galaxy!\n"));
            doResources();
#endif
        }
        exitDaemon(0);
    }
    old_robot = start_robot;
    if (update_sys_defaults()) {
        static struct itimerval udt;

        if (chaosmode)
            status->gameup |= GU_CHAOS;
        else
            status->gameup &= ~GU_CHAOS;
        if (old_robot != start_robot) {
            if (robot_pid) {             /* Terminate old Robot */
                kill(robot_pid,SIGINT); 
                robot_pid=0;
            }
            if (start_robot) fork_robot(start_robot);
        }

        /* allow for change to reality timer */
        udt.it_interval.tv_sec = 0;
        udt.it_interval.tv_usec = reality;
        udt.it_value.tv_sec = 0;
        udt.it_value.tv_usec = reality;
        if (setitimer(ITIMER_REAL, &udt, (struct itimerval *) 0) < 0){
            ERROR(1,( "daemon:  Error resetting itimer.  Exiting.\n"));        
            exitDaemon(0);
        }

        /* This message does 2 things:
         * First, it tells the players that the system defaults have in fact
         * changed.  Secondly, it triggers the ntserv processes to check the
         * new defaults.
         */
        pmessage(0, MALL, "GOD->ALL","Loading new server configuration.");
    }

    switch (ts) {
    case TS_PICKUP:
            status->tourn = 0;
            if (tournamentMode()) {
                    ts = TS_BEGIN;
                    if (check_scum && check_scummers(1))
                            ts = TS_SCUMMERS;
            }
            break;

    case TS_SCUMMERS:
            status->tourn = 0;
            if (!tournamentMode()) {
                    ts = TS_PICKUP;
            } else {
                    if (!check_scum) {
                            ts = TS_BEGIN;
                            break;
                    }
                    if (!check_scummers(0))
                            ts = TS_BEGIN;
            }
            break;

    case TS_BEGIN:
            oldmessage = (random() % 8);
            political_begin(oldmessage);
            ts = TS_TOURNAMENT;
            /* break; */

    case TS_TOURNAMENT:
            status->tourn = 1;
            status->time++;
            tourntimestamp = ticks;
            if (tournamentMode()) break;
            ts = TS_END;
            /* break; */

    case TS_END:
            tourntimestamp = ticks; /* record end of Tmode 8/2/91 TC */
            political_end(oldmessage);
            ts = TS_PICKUP;
            break;
    }

#ifdef PUCK_FIRST
    /* The placement of signal_puck before udplayers() is key.  If udplayers()
       happens first the puck can be pushed out of range of a valid shot,
       among other things.  */
    signal_puck();
#endif /*PUCK_FIRST */

    if (fuse(PLAYERFUSE)) {
        udplayers();
    }
    if (fuse(TORPFUSE)) {
        udtorps();
    }
    if (fuse(PHASERFUSE)) {
        udphaser();
    }
    if (fuse(CLOAKFUSE)) {
        udcloak();
    }

  /* not sure where's the best place to put this (or even if it matters).
     If here, we tell the ntserv processes to update now while the daemon
     putters through the remaining non-essential updates */
    signal_servers ();

    if (fuse(TEAMFUSE)) {
        teamtimers();
    }
    if (fuse(SIGHTFUSE)) {
        udplayersight();
    }
    if (fuse(PLFIGHTFUSE)) {
        plfight();
    }
    if (fuse(BEAMFUSE)) {
        beam();
    }

    if (planet_move && fuse(PMOVEFUSE)) {
        pmove();
    }

    if (fuse(SYNCFUSE)) {
        save_planets();
    }
    if ((killer && !topgun && fuse(HOSEFUSE)) ||        /* NBT added: sysdef */
        (killer &&  topgun && fuse(HOSEFUSE2))) {
                                /* change 5/20/91 TC: add the HK bot */
        rescue(HUNTERKILLER, 0); /* no team, no target */
    }
    if (fuse(PLANETFUSE)) {
        udplanets();
    }
    if (fuse(MINUTEFUSE)) {     /* was SURRENDFUSE 4/15/92 TC */
        udsurrend();
    }
    if (fuse(CHECKLOADFUSE)) {
        check_load(); 
    }
}



static void udplayersight(void)
{
  struct player *j, *k;
  LONG dx, dy, dist, max_dist;
  U_LONG max_dist_sq;

  for (j = firstPlayer; j <= lastPlayer; j++) {
    if ((j->p_status != PALIVE) ||
        /*
         * Orbitting any non-owned planet gets you seen */
        ((j->p_flags & PFORBIT) &&
         (planet_(j->p_planet)->pl_owner != j->p_team))) {
      j->p_flags |= PFSEEN;
      continue;
    }
    max_dist    = (j->p_flags & PFCLOAK) ? (GWIDTH/7) : (GWIDTH/3);
    max_dist_sq = (j->p_flags & PFCLOAK) ? (GWIDTH/7)*(GWIDTH/7) :
      (GWIDTH/3)*(GWIDTH/3);
    
    if (j->p_lastseenby != VACANT) {
      k = player_(j->p_lastseenby);
      /*
       * If existent, check the player K who last saw player J.  
       * There is a very good change that K can still see J.
       * 
       * The only way p_lastseenby gets set to non-VACANT is in the for-loop
       * below; hence, we don't have to check whether K is a robot or whether
       * K is on the same team as J; we know neither of these was true when
       * K got set, hence they remain false now.  Also, we don't bother with
       * the square range check, because it is likely that the players will
       * still be in range of each other.  */
      if ((k->p_status == PALIVE) &&
          ((k->p_x - j->p_x)*(k->p_x - j->p_x) +
           (k->p_y - j->p_y)*(k->p_y - j->p_y) < max_dist_sq)) {
        /* Still seen. */
        continue;
      }
    }
        
    j->p_flags &= ~PFSEEN;
    j->p_lastseenby = VACANT;
    
    /*
     * Search enemy team to see if they see this player.  */
    for (k = firstPlayer; k <= lastPlayer; k++) {
      /*
       * Player must be alive, not a robot, and an enemy  */
      if (k->p_status != PALIVE) 
        continue;
      if (k->p_flags & PFROBOT) 
        continue;
      if (k->p_team == j->p_team)
        continue;
      /*
       * As usual, check square distance first (for speed): */
      dx = k->p_x - j->p_x;
      if ((dx > max_dist) || (dx < -max_dist))
        continue;
      dy = k->p_y - j->p_y;
      if ((dy > max_dist) || (dy < -max_dist))
        continue;

      dist = dx*dx + dy*dy;
      if (dist <= max_dist_sq) {
        j->p_flags |= PFSEEN;
        j->p_lastseenby = k->p_no;
        break;
      }
    }
  }
}


/* update players during pause */
static void udplayerpause(void) {
  int i;
  struct player *j;

  for (i=0; i<MAXPLAYER; i++) {
    int kill_player = 0;        /* 1 = QUIT during pause */
                                /* 2 = ghostbust during pause */
    j = &players[i];

    switch(j->p_status) {
      case PFREE:       /* reset ghostbuster and continue */
        j->p_ghostbuster = 0;
        continue;
        break;
      case PEXPLODE:
        if (j->p_whydead == KQUIT)
          j->p_status = PDEAD;
        break;
      case PDEAD:
        if (j->p_whydead == KQUIT)
          kill_player = 1;
        break;
    }

    if (++(j->p_ghostbuster) > GHOSTTIME) {
      ERROR(4,("daemonII/udplayerpause: %s: ship ghostbusted (wd=%d)\n", 
                   j->p_mapchars, j->p_whydead));
      ghostmess(j, "no ping in pause");
      j->p_status = PDEAD;
      j->p_whydead = KGHOST;
      j->p_whodead = i;
      kill_player = 2;
    }

    if (kill_player && (j->p_process > 1)) {
      j->p_ghostbuster = 0;

      saveplayer(j);
      ERROR(8,("daemonII/udplayerpause: %s: sending SIGTERM to %d\n", 
               j->p_mapchars, j->p_process));

      if (kill (j->p_process, SIGTERM) < 0)
        ERROR(1,("daemonII/udplayerpause:  kill failed!\n"));

      /* let's be safe */
      freeslot(j);
    }
  }
}


static void udplayers(void)
{
    register int i, k;
    register struct player *j;
    float dist; /* used by tractor beams */
    int maxspeed;

    int nfree = 0;
    tcount[FED] = tcount[ROM] = tcount[KLI] = tcount[ORI] = 0;
    for (i = status->active = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        int outfitdelay;

        switch (j->p_status) {
            case POUTFIT:

                switch (j->p_whydead) {
                case KLOGIN:
                    outfitdelay = LOGINTIME;
                    break;
                case KQUIT:
                case KPROVIDENCE:
                case KBADBIN:
                    outfitdelay = GHOSTTIME;
                    break;
                case KGHOST:
                    outfitdelay = KGHOSTTIME;
                    break;
                default:
                    outfitdelay = OUTFITTIME;
                    break;
                }

                if (++(j->p_ghostbuster) > outfitdelay) {
                    ERROR(4,("%s: ship in POUTFIT too long (gb=%d/%d,wd=%d)\n", 
                             j->p_mapchars, j->p_ghostbuster,
                             outfitdelay, j->p_whydead));
                    if (j->p_whydead == KGHOST) {
                        ghostmess(j, "no ping alive");
                    } else {
                        ghostmess(j, "outfit timeout");
                    }
                    saveplayer(j);
                    if (j->p_process > 1) {
                        ERROR(8,("%s: sending SIGTERM to %d\n", 
                                 j->p_mapchars, j->p_process));
                        if (kill (j->p_process, SIGTERM) < 0)
                            ERROR(1,("daemonII/udplayers:  kill failed!\n"));
                    } else {
                        ERROR(1,("daemonII/udplayers:  bad p_process!\n"));
                        freeslot(j);
                    }
                }
                continue;
            case PFREE:
                nfree++;
                j->p_ghostbuster = 0;   /* stop from hosing new players */
                continue;
#ifdef OBSERVERS
            case POBSERV:
                /* observer players, perpetually 'dead'-like state */
                /* think of them as ghosts! but they still might get */
                /* ghostbusted - attempt to handle it */
                if (++(j->p_ghostbuster) > GHOSTTIME) {
                    ghostmess(j, "no ping observ");
                    saveplayer(j);
                    j->p_status = PDEAD;
                    j->p_whydead = KGHOST;
                    j->p_whodead = i;
                    break;
                }
                continue;
#endif  /* OBSERVERS */

            case PDEAD:
                if (--j->p_explode <= 0) {      /* Ghost Buster */
                    saveplayer(j);
                    j->p_status = POUTFIT; /* change 5/24/91 TC, was PFREE */
                }
                continue;
            case PEXPLODE:
                j->p_updates++;
                j->p_flags &= ~PFCLOAK;
                if ((j->p_ship.s_type == STARBASE) && 
                   (j->p_explode == ((2*SBEXPVIEWS-3)/PLAYERFUSE)))
                        blowup(j);  /* damdiffe everyone else around */
                else if (j->p_explode == ((10-3)/PLAYERFUSE))
                        blowup(j);      /* damdiffe everyone else around */

                if (--j->p_explode <= 0) {
                    j->p_status = PDEAD;
                    j->p_explode = 600/PLAYERFUSE; /* set ghost buster */
                }

                bay_release(j);
                if (j->p_ship.s_type == STARBASE) {
                    bay_release_all(j);
                    if (((j->p_whydead == KSHIP) || 
                            (j->p_whydead == KTORP) ||
                            (j->p_whydead == KPHASER) || 
                            (j->p_whydead == KPLASMA) ||
                            (j->p_whydead == KPLANET) ||
                            (j->p_whydead == KGENOCIDE)) && (status->tourn))
                    teams[j->p_team].s_turns=BUILD_SB_TIME; /* in defs.h */
                        /* 30 minute reconstruction period for new starbase */
                }
                /* And he is ejected from orbit. */
                j->p_flags &= ~PFORBIT;

                /* Fall through to alive so explosions move */
                        
            case PALIVE:
                /* Move Player in orbit */
                if ((j->p_flags & PFORBIT) && !(j->p_flags & PFDOCK)) {
                    j->p_dir += 2;
                    j->p_desdir = j->p_dir;
                    j->p_x = planets[j->p_planet].pl_x + ORBDIST
                        * Cos[(u_char) (j->p_dir - (u_char) 64)];
                    j->p_y = planets[j->p_planet].pl_y + ORBDIST
                        * Sin[(u_char) (j->p_dir - (u_char) 64)];
                }

                /* Move player through space */
                else if (!(j->p_flags & PFDOCK)) {

                    if ((j->p_dir != j->p_desdir) && (j->p_status != PEXPLODE))
                        changedir(j);

                    /* Alter speed */
#ifdef SB_TRANSWARP
                  if (j->p_flags & PFTWARP){
                      if ((j->p_speed *= 2)>(j->p_desspeed)) 
                          j->p_speed = j->p_desspeed;
                      j->p_fuel -= (int)((j->p_ship.s_warpcost * j->p_speed) * 0.8);
                      j->p_etemp += j->p_ship.s_maxspeed;
                  } else { /* not transwarping */
#endif
#ifdef NEW_ETEMP
                    if (j->p_flags & PFENG)
                        maxspeed = 1;
                    else
#endif
                    maxspeed = (j->p_ship.s_maxspeed+2) -
                            (j->p_ship.s_maxspeed+1)*
                           ((float)j->p_damage/ (float)(j->p_ship.s_maxdamage));
                    if (j->p_desspeed > maxspeed)
                        j->p_desspeed = maxspeed;
                    if (j->p_flags & PFENG)
                        j->p_desspeed = 1;

                    /* Charge for speed */
                    if (j->p_fuel < (j->p_ship.s_warpcost * j->p_speed)) {
                      /* 
                       * Out of fuel ... set speed to one that uses less fuel
                       *  than the ship's reactor generates (if necessary).
                       */
                      if (j->p_ship.s_recharge/j->p_ship.s_warpcost < 
                                j->p_speed) {
                        j->p_desspeed = j->p_ship.s_recharge/j->p_ship.s_warpcost + 2;
                        /* Above incantation is a magic formula ... (hack). */
                        if (j->p_desspeed > j->p_ship.s_maxspeed)
                          j->p_desspeed = j->p_ship.s_maxspeed - 1;
                      } else {
                        j->p_desspeed = j->p_speed;
                      }
                      j->p_fuel = 0;
                    } else {
                        j->p_fuel -= (j->p_ship.s_warpcost * j->p_speed);
                        j->p_etemp += j->p_speed;

                        /* Charge SB's for mass of docked vessels ... */
                        if (j->p_ship.s_type == STARBASE) {
                            int bays = 0;
                            for (k=0; k<NUMBAYS; k++)
                                if(j->p_bays[k] != VACANT) {
                                    j->p_fuel -= players[j->p_bays[k]].p_ship.s_warpcost * j->p_speed;
                                    bays++;
                                }
                            j->p_etemp += .7*(j->p_speed * bays);
                        }
                    }
#ifdef SB_TRANSWARP
                }
#endif                  

                    if (j->p_desspeed > j->p_speed) {
                        j->p_subspeed += j->p_ship.s_accint;
                    }
                    if (j->p_desspeed < j->p_speed)
                        j->p_subspeed -= j->p_ship.s_decint;

                    if (j->p_subspeed / 1000) {
                        j->p_speed += j->p_subspeed / 1000;
                        j->p_subspeed %= 1000;
                        if (j->p_speed < 0)
                            j->p_speed = 0;
                        if (j->p_speed > j->p_ship.s_maxspeed)
                            j->p_speed = j->p_ship.s_maxspeed;
                    }

                    j->p_x += (double) (j->p_speed * WARP1) * Cos[j->p_dir];
                    j->p_y += (double) (j->p_speed * WARP1) * Sin[j->p_dir];

                    /* Bounce off the side of the galaxy */
                    if (j->p_x < 0) {
             if (!wrap_galaxy) {
                         j->p_x = -j->p_x;
                         if (j->p_dir == 192)
                            j->p_dir = j->p_desdir = 64;
                         else
                            j->p_dir = j->p_desdir = 64 - (j->p_dir - 192);
             } else 
                     j->p_x = GWIDTH;
                    } else if (j->p_x > GWIDTH) {
             if (!wrap_galaxy) {
                          j->p_x = GWIDTH - (j->p_x - GWIDTH);
                          if (j->p_dir == 64)
                            j->p_dir = j->p_desdir = 192;
                          else
                            j->p_dir = j->p_desdir = 192 - (j->p_dir - 64);
             } else 
                     j->p_x = 0;
                    } if (j->p_y < 0) {
             if (!wrap_galaxy) {
                          j->p_y = -j->p_y;
                          if (j->p_dir == 0)
                            j->p_dir = j->p_desdir = 128;
                          else
                            j->p_dir = j->p_desdir = 128 - j->p_dir;
             } else 
                     j->p_y = GWIDTH;
                    } else if (j->p_y > GWIDTH) {
             if (!wrap_galaxy) {
                          j->p_y = GWIDTH - (j->p_y - GWIDTH);
                          if (j->p_dir == 128)
                            j->p_dir = j->p_desdir = 0;
                          else
                            j->p_dir = j->p_desdir = 0 - (j->p_dir - 128);
             } else 
                     j->p_y = 0;

                    }
                }

                /* If player is actually dead, don't do anything below ... */
                if (j->p_status == PEXPLODE || j->p_status == PDEAD
#ifdef OBSERVERS
                      ||  j->p_status == POBSERV
#endif
                   )
                  break;

            if (status->tourn
#ifdef BASEPRACTICE
                || practice_mode
#endif
                ) {

#ifdef LTD_STATS
                    if (status->tourn) ltd_update_ticks(j);
                    if (j->p_ship.s_type != STARBASE)
                        status->timeprod++;
#else
                    if (j->p_ship.s_type==STARBASE) {
                        j->p_stats.st_sbticks++;
                    } else if (status->tourn) {
                        j->p_stats.st_tticks++;
                        status->timeprod++;
#ifdef ROLLING_STATS
                        /*
                        **  Rolling statistics modifications.  This is
                        **  temporary code and has not been approved by
                        **  the server maintenance team.
                        **
                        **  Every "n" player ticks in t-mode, this code
                        **  is activated.  It keeps track of the
                        **  previous counters and ensures that the
                        **  t-mode statistics represent, on average, the
                        **  equivalent of "m" sample intervals.
                        **
                        **  ROLLING_STATS_MASK must be one less than a
                        **  power of two for this conditional to work 
                        **  properly.  A value of 32767 yields 3276.7
                        **  seconds per slot, which if ROLLING_STATS_SLOTS
                        **  is set to 10, total statistics will be 9.1
                        **  hours of play. 
                        */
                        if ((j->p_stats.st_tticks&ROLLING_STATS_MASK)==0)
                        {
                            int st_skills, st_slosses, st_sarmsbomb,
                                st_splanets, st_sticks,
                                slot = j->p_stats.st_rollingslot;

                            /* save current slot for later deduction */
                            st_skills =
                                j->p_stats.st_rolling[slot].st_rkills;
                            st_slosses =
                                j->p_stats.st_rolling[slot].st_rlosses;
                            st_sarmsbomb =
                                j->p_stats.st_rolling[slot].st_rarmsbomb;
                            st_splanets =
                                j->p_stats.st_rolling[slot].st_rplanets;
                            st_sticks =
                                j->p_stats.st_rolling[slot].st_rticks;

                            /* calculate change and store in current slot */
                            j->p_stats.st_rolling[slot].st_rkills =
                                j->p_stats.st_tkills -
                                j->p_stats.st_okills;
                            j->p_stats.st_rolling[slot].st_rlosses =
                                j->p_stats.st_tlosses -
                                j->p_stats.st_olosses;
                            j->p_stats.st_rolling[slot].st_rarmsbomb =
                                j->p_stats.st_tarmsbomb -
                                j->p_stats.st_oarmsbomb;
                            j->p_stats.st_rolling[slot].st_rplanets =
                                j->p_stats.st_tplanets -
                                j->p_stats.st_oplanets;
                            j->p_stats.st_rolling[slot].st_rticks =
                                j->p_stats.st_tticks -
                                j->p_stats.st_oticks;

                            /* adjust current stats backwards by saved slot */
                            j->p_stats.st_tkills    -= st_skills;
                            j->p_stats.st_tlosses   -= st_slosses;
                            j->p_stats.st_tarmsbomb -= st_sarmsbomb;
                            j->p_stats.st_tplanets  -= st_splanets;
                            j->p_stats.st_tticks    -= st_sticks;

                            /* accumulate current slot to historical */
                            j->p_stats.st_hkills +=
                                j->p_stats.st_rolling[slot].st_rkills;
                            j->p_stats.st_hlosses +=
                                j->p_stats.st_rolling[slot].st_rlosses;
                            j->p_stats.st_harmsbomb +=
                                j->p_stats.st_rolling[slot].st_rarmsbomb;
                            j->p_stats.st_hplanets +=
                                j->p_stats.st_rolling[slot].st_rplanets;
                            j->p_stats.st_hticks +=
                                j->p_stats.st_rolling[slot].st_rticks;

                            /* copy new current to old stats */
                            j->p_stats.st_okills    = j->p_stats.st_tkills;
                            j->p_stats.st_olosses   = j->p_stats.st_tlosses;
                            j->p_stats.st_oarmsbomb = j->p_stats.st_tarmsbomb;
                            j->p_stats.st_oplanets  = j->p_stats.st_tplanets;
                            j->p_stats.st_oticks    = j->p_stats.st_tticks;

                            /* increment slot number */
                            j->p_stats.st_rollingslot++;
                            if (j->p_stats.st_rollingslot >= ROLLING_STATS_SLOTS)
                                j->p_stats.st_rollingslot = 0;
                        }
#endif /* ROLLING_STATS */
                    } else {
                        j->p_stats.st_ticks++;
                    }
#endif /* LTD_STATS */
                }

                if (++(j->p_ghostbuster) > GHOSTTIME) {
                    j->p_status = PEXPLODE;
                    if (j->p_ship.s_type == STARBASE)
                        j->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
                    else
                        j->p_explode = 10/PLAYERFUSE;
                    j->p_whydead = KGHOST;
                    j->p_whodead = i;
                    ERROR(4,("daemonII/udplayers: %s: ship ghostbusted (gb=%d,gt=%d)\n", j->p_mapchars, j->p_ghostbuster, GHOSTTIME));
                }
                        
                status->active += (1<<i);
                tcount[j->p_team]++;
                j->p_updates++;

                /* Charge for shields */
                if (j->p_flags & PFSHIELD) {  /* This is a kludge, I know. */
                    switch (j->p_ship.s_type) {
                    case SCOUT:
                        j->p_fuel -= 2;
                        break;
                    case DESTROYER: 
                    case CRUISER:
                    case BATTLESHIP:
                    case ASSAULT:
                        j->p_fuel -= 3;
                        break;
                    case STARBASE:
                        j->p_fuel -= 6;
                        break;
                    default:
                        break;
                    }
                }

                /* affect tractor beams */
                if (j->p_flags & PFTRACT) {
                  if ((isAlive(&players[j->p_tractor])) &&
                      ((j->p_fuel > TRACTCOST) && !(j->p_flags & PFENG)) &&
                      ((dist=hypot((double)(j->p_x-players[j->p_tractor].p_x),
                                   (double)(j->p_y-players[j->p_tractor].p_y))) 
                                   < ((double) TRACTDIST)*j->p_ship.s_tractrng) &&
                      (!(j->p_flags & (PFORBIT | PFDOCK))) &&
                      (!(players[j->p_tractor].p_flags & PFDOCK))) {
                    float cosTheta, sinTheta;  /* Cos and Sin from me to him */
                    int halfforce; /* Half force of tractor */
                    int dir;       /* -1 for repress, otherwise 1 */

                    if (players[j->p_tractor].p_flags & PFORBIT)
                       players[j->p_tractor].p_flags &= ~PFORBIT;
                    
                    j->p_fuel -= TRACTCOST;
                    j->p_etemp += TRACTEHEAT;
                    
                    cosTheta = players[j->p_tractor].p_x - j->p_x;
                    sinTheta = players[j->p_tractor].p_y - j->p_y;
                    if (dist == 0) dist = 1; /* prevent divide by zero */
                    cosTheta /= dist;   /* normalize sin and cos */
                    sinTheta /= dist;
                    /* force of tractor is WARP2 * tractstr */
                    halfforce = (WARP1*j->p_ship.s_tractstr);

                    dir = 1;
                    if (j->p_flags & PFPRESS) dir = -1;
                    /* change in position is tractor strength over mass */
                    j->p_x += dir * cosTheta * halfforce/(j->p_ship.s_mass);
                    j->p_y += dir * sinTheta * halfforce/(j->p_ship.s_mass);
                    players[j->p_tractor].p_x -= dir * cosTheta *
                       halfforce/(players[j->p_tractor].p_ship.s_mass);
                    players[j->p_tractor].p_y -= dir * sinTheta *
                       halfforce/(players[j->p_tractor].p_ship.s_mass);
                  } else {
                    j->p_flags &= ~(PFTRACT | PFPRESS);
                  }     
                }
                        
                /* cool weapons */
                j->p_wtemp -= j->p_ship.s_wpncoolrate;
                if (j->p_wtemp < 0)
                    j->p_wtemp = 0;
                if (j->p_flags & PFWEP) {
                    if (--j->p_wtime <= 0)
                        j->p_flags &= ~PFWEP;
                }
                else if (j->p_wtemp > j->p_ship.s_maxwpntemp) {
                    if (!(random() % 40)) {
                        j->p_flags |= PFWEP;
                        j->p_wtime = ((short) (random() % 150) + 100) /
                                     PLAYERFUSE;
                    }
                }
                /* cool engine */
                j->p_etemp -= j->p_ship.s_egncoolrate;
                if (j->p_etemp < 0)
                    j->p_etemp = 0;
                if (j->p_flags & PFENG) {
                    if (--j->p_etime <= 0)
                        j->p_flags &= ~PFENG;
                }
                else if (j->p_etemp > j->p_ship.s_maxegntemp) {
                    if (!(random() % 40)) {
                        j->p_flags |= PFENG;
                        j->p_etime = ((short) (random() % 150) + 100) /
                                     PLAYERFUSE;
                        j->p_desspeed = 0;
                    }
                }

                /* Charge for cloaking */
                if (j->p_flags & PFCLOAK) {
                    if (j->p_fuel < j->p_ship.s_cloakcost) {
                        j->p_flags &= ~PFCLOAK;
                    } else {
                        j->p_fuel -= j->p_ship.s_cloakcost;
                    }
                }

#ifdef SBFUEL_FIX
                /* Add fuel */
                j->p_fuel += 2 * j->p_ship.s_recharge;
                if ((j->p_flags & PFORBIT) &&
                    (planets[j->p_planet].pl_flags & PLFUEL) &&
                    (!(planets[j->p_planet].pl_owner & j->p_war))) {
                  j->p_fuel += 6 * j->p_ship.s_recharge;
                } else if ((j->p_flags & PFDOCK) && 
                           (j->p_fuel < j->p_ship.s_maxfuel) &&
                           (players[j->p_dock_with].p_fuel > SBFUELMIN)) {
                  int fc = MIN(10*j->p_ship.s_recharge,
                               j->p_ship.s_maxfuel - j->p_fuel);
                  j->p_fuel += fc;
                  players[j->p_dock_with].p_fuel -= fc;
                }
#else
                /* Add fuel */
                if ((j->p_flags & PFORBIT) &&
                    (planets[j->p_planet].pl_flags & PLFUEL) &&
                    (!(planets[j->p_planet].pl_owner & j->p_war))) {
                  j->p_fuel += 8 * j->p_ship.s_recharge;
                } else if ((j->p_flags & PFDOCK) && (j->p_fuel < j->p_ship.s_maxfuel)) {
                  if (players[j->p_dock_with].p_fuel > SBFUELMIN) {
                    j->p_fuel += 12*j->p_ship.s_recharge;
                    players[j->p_dock_with].p_fuel -= 12*j->p_ship.s_recharge;
                  }
                } else
                  j->p_fuel += 2 * j->p_ship.s_recharge;
#endif /* SBFUEL_FIX */
                
                if (j->p_fuel > j->p_ship.s_maxfuel)
                  j->p_fuel = j->p_ship.s_maxfuel;
                if (j->p_fuel < 0) {
                  j->p_desspeed = 0;
                  j->p_flags &= ~PFCLOAK;
                }
        
        /* repair shields */
                if (j->p_shield < j->p_ship.s_maxshield) {
                    if ((j->p_flags & PFREPAIR) && (j->p_speed == 0)) {
                        j->p_subshield += j->p_ship.s_repair * 4;
                        if ((j->p_flags & PFORBIT) &&
                            (planets[j->p_planet].pl_flags & PLREPAIR) &&
                            (!(planets[j->p_planet].pl_owner & j->p_war))) {
                                    j->p_subshield += j->p_ship.s_repair * 4;
                        } 
                        if (j->p_flags & PFDOCK)  {
                            j->p_subshield += j->p_ship.s_repair * 6;
                        }
                    }
                    else {
                        j->p_subshield += j->p_ship.s_repair * 2;
                    }
                    if (j->p_subshield / 1000) {
#ifdef LTD_STATS
                        if (status->tourn)
                            ltd_update_repaired(j, j->p_subshield / 1000);
#endif
                        j->p_shield += j->p_subshield / 1000;
                        j->p_subshield %= 1000;
                    }
                    if (j->p_shield > j->p_ship.s_maxshield) {
                        j->p_shield = j->p_ship.s_maxshield;
                        j->p_subshield = 0;
                    }
                }

                /* repair damage */
                if (j->p_damage && !(j->p_flags & PFSHIELD)) {
                  if ((j->p_flags & PFREPAIR) && (j->p_speed == 0)) {
                    j->p_subdamage += j->p_ship.s_repair * 2;
                    if ((j->p_flags & PFORBIT) &&
                        (planets[j->p_planet].pl_flags & PLREPAIR) &&
                        (!(planets[j->p_planet].pl_owner & j->p_war))) {
                      j->p_subdamage += j->p_ship.s_repair * 2;
                    }
                    if (j->p_flags & PFDOCK) {
                      j->p_subdamage += j->p_ship.s_repair * 3;
                    }
                  }
                  else {
                    j->p_subdamage += j->p_ship.s_repair;
                  }
                  if (j->p_subdamage / 1000) {
#ifdef LTD_STATS
                    if (status->tourn)
                        ltd_update_repaired(j, j->p_subdamage / 1000);
#endif
                    j->p_damage -= j->p_subdamage / 1000;
                    j->p_subdamage %= 1000;
                  }
                  if (j->p_damage < 0) {
                    j->p_damage = 0;
                    j->p_subdamage = 0;
                  }
                }

                /* Move Player in dock */
                if (j->p_flags & PFDOCK) {
                    j->p_x = players[j->p_dock_with].p_x + DOCKDIST*Cos[(j->p_dock_bay*90+45)*255/360];
                    j->p_y = players[j->p_dock_with].p_y + DOCKDIST*Sin[(j->p_dock_bay*90+45)*255/360];
                }

                /* Set player's alert status */
#define YRANGE ((GWIDTH)/7)
#define RRANGE ((GWIDTH)/10)
                j->p_flags |= PFGREEN;
                j->p_flags &= ~(PFRED|PFYELLOW);
                for (k = 0; k < MAXPLAYER; k++) {
                    int dx, dy, dist;
                    if ((players[k].p_status != PALIVE) ||
                        ((!(j->p_war & players[k].p_team)) &&
                        (!(players[k].p_war & j->p_team)))) {
                            continue;
                    }
                    else if (j == &players[k]) {
                        continue;
                    }
                    else {
                        dx = j->p_x - players[k].p_x;
                        dy = j->p_y - players[k].p_y;
                        if (ABS(dx) > YRANGE || ABS(dy) > YRANGE)
                            continue;
                        dist = dx * dx + dy * dy;
                        if (dist <  RRANGE * RRANGE) {
                            j->p_flags |= PFRED;
                            j->p_flags &= ~(PFGREEN|PFYELLOW);
                            /* jac: can't get any worse, should we break; ? */
                        }
                        else if ((dist <  YRANGE * YRANGE) &&
                            (!(j->p_flags & PFRED))) {
                            j->p_flags |= PFYELLOW;
                            j->p_flags &= ~(PFGREEN|PFRED);
                        }
                    }
                }
            break;
        } /* end switch */
    }
    if (nfree == MAXPLAYER) {
        if (dietime == -1) {
            if (status->gameup & GU_GAMEOK) {
                dietime = ticks + 600 / PLAYERFUSE;
            } else {
                dietime = ticks + 10 / PLAYERFUSE;
            }
        }
    } else {
        dietime = -1;
    }
}

static void changedir(struct player *sp)
{
    u_int ticks;
    u_int min, max;
    int speed;

/* Change 1/20/91 TC new-style turn rates*/

    speed=sp->p_speed;
    if (speed == 0) {
        sp->p_dir = sp->p_desdir;
        sp->p_subdir = 0;
    }
    else {
        if (newturn) 
            sp->p_subdir += sp->p_ship.s_turns/(speed*speed);
        else
            sp->p_subdir += sp->p_ship.s_turns/((sp->p_speed<30)?(1<<sp->p_speed):1000000000);

        ticks = sp->p_subdir / 1000;
        if (ticks) {
            if (sp->p_dir > sp->p_desdir) {
                min=sp->p_desdir;
                max=sp->p_dir;
            } else {
                min=sp->p_dir;
                max=sp->p_desdir;
            }
            if ((ticks > max-min) || (ticks > 256-max+min))
                sp->p_dir = sp->p_desdir;
            else if ((u_char) ((int)sp->p_dir - (int)sp->p_desdir) > 127)
                sp->p_dir += ticks;
            else 
                sp->p_dir -= ticks;
            sp->p_subdir %= 1000;
        }
    }
}


static void udcloak(void)
{
  register int i;

  for (i=0; i<MAXPLAYER; i++) {
    if (isAlive(&players[i])) {
      if ((players[i].p_flags & PFCLOAK) && 
          (players[i].p_cloakphase < (CLOAK_PHASES-1))) {
        players[i].p_cloakphase++;
      } else if (!(players[i].p_flags & PFCLOAK) && 
                 (players[i].p_cloakphase > 0)) {
        players[i].p_cloakphase--;
      }
    }
  }
}
    

#if 0    
u_char 
getcourse(x, y, xme, yme)
     int x, y, xme, yme;
{
  return (u_char) nint((atan2((double) (x - xme),
                              (double) (yme - y)) / 3.14159 * 128.));
}


u_char 
get_bearing(xme, yme, x, y, dir)
     int x, y;
{
  int phi = ((int) nint((atan2((double) (x-xme), 
                               (double) (yme-y)) / 3.14159 * 128.)));
  if (phi < 0)
    phi = 256 + phi;
  if (phi >= dir)
    return (u_char) (phi - dir);
  else
    return (u_char) (256 + phi - dir);
}
#endif

/* 
 * Find nearest hostile vessel and turn toward it
 */
static void torp_track_target(struct torp *t)
{
  int heading, bearing;
  int dx, dy, range;
  U_LONG range_sq, min_range_sq;
  struct player *j, *owner;
  int turn;

  turn = 0;
  owner = player_(t->t_owner);
  range = 65535;
  min_range_sq = 0xffffffff;
  /*
   * Look at every player for the closest one that is "reachable". */
  for (j = firstPlayer; j <= lastPlayer; j++) {
    if (j->p_status != PALIVE)
      continue;
    if (j == owner)
      continue;
    if (j->p_team == owner->p_team)
      continue;
    if (! ((t->t_war & j->p_team) || (t->t_team & j->p_war)))
      continue;

    /*
     * Before doing the fancy computations, don't bother exploring the
     * tracking of any player more distant than our current target.   */
    dx = t->t_x - j->p_x;
    if ((dx >= range) || (dx <= -range))
      continue;
    dy = t->t_y - j->p_y;
    if ((dy >= range) || (dy <= -range))
      continue;
    
    /*
     * Get the direction that the potential target is from the torp:  */
    heading = -64 + nint(atan2((double) dy, (double) dx) / 3.14159 * 128.0);
    if (heading < 0)
      heading += 256;
    /*
     * Now use the torp's direction and the heading to determine the
     * bearing of the potential target from the current direction.  */
    if (heading >= t->t_dir)
      bearing = heading - t->t_dir;
    else
      bearing = heading - t->t_dir + 256;

    /* 
     * To prevent the torpedo from tracking unreachable targets:
     * If target is in an arc that the torp might reach, then it is valid. */
    if ((t->t_turns * t->t_fuse > 127) ||
        (bearing < t->t_turns * t->t_fuse) ||
        (bearing > (256 - t->t_turns * t->t_fuse))) {
      range_sq = dx*dx + dy*dy;
      if (range_sq < min_range_sq) {
        min_range_sq = range_sq;
        range = sqrt(range_sq);
        turn = (bearing > 127) ? -1 : 1;
      }
    }
  }

  /*
   * Found a target, or not.  Adjust torpedo direction.  */
  if (turn < 0) {
    heading = ((int) t->t_dir) - t->t_turns;
    t->t_dir = (heading < 0) ? (256+heading) : heading;
  } else if (turn > 0) {
    heading = ((int) t->t_dir) + t->t_turns;
    t->t_dir = (heading > 255) ? (heading-256) : heading;
  }
}

static void udtorps(void)
{
  struct torp *t;

  /*
   * Update all torps AND all plasmas; they are in the same array: */
  for (t = firstTorp; t <= lastPlasma; t++) {
    switch (t->t_status) {
    case TFREE:
      continue;
    case TMOVE:
      /*
       * See if the torp is out of time. */
      if (t->t_fuse-- <= 0)
        break;

      /*
       * Change direction for tracking torps  */
      if (t->t_turns > 0)
        torp_track_target(t);
                
#define DO_WALL_HIT(torp) (torp)->t_status = TDET; \
                          (torp)->t_whodet = (torp)->t_owner; \
                          explode(torp)

      /*
       * Move the torp, checking for wall hits  */
      t->t_x += (double) t->t_gspeed * Cos[t->t_dir];
      if (t->t_x < 0) {
        if (!wrap_galaxy) {
          t->t_x = 0;
          DO_WALL_HIT(t);
#ifdef LTD_STATS
          /* update torp hit wall stat */
          switch(t->t_type) {
            case TPHOTON:
              if (status->tourn)
                ltd_update_torp_wall(&(players[t->t_owner]));
              break;
            case TPLASMA:
              if (status->tourn)
                ltd_update_plasma_wall(&(players[t->t_owner]));
              break;
          }
#endif
          continue;
        }
        else
          t->t_x = GWIDTH;
      }
      else if (t->t_x > GWIDTH) {
        if (!wrap_galaxy) {
          t->t_x = GWIDTH;
          DO_WALL_HIT(t);
#ifdef LTD_STATS
          /* update torp hit wall stat */
          switch(t->t_type) {
            case TPHOTON:
              if (status->tourn)
                ltd_update_torp_wall(&(players[t->t_owner]));
              break;
            case TPLASMA:
              if (status->tourn)
                ltd_update_plasma_wall(&(players[t->t_owner]));
              break;
          }
#endif
          continue;
        }
        else
          t->t_x = 0;
      }
      t->t_y += (double) t->t_gspeed * Sin[t->t_dir];

      if (t->t_y < 0) {
        if (!wrap_galaxy) {
          t->t_y = 0;
          DO_WALL_HIT(t);
#ifdef LTD_STATS
          /* update torp hit wall stat */
          switch(t->t_type) {
            case TPHOTON:
              if (status->tourn)
                ltd_update_torp_wall(&(players[t->t_owner]));
              break;
            case TPLASMA:
              if (status->tourn)
                ltd_update_plasma_wall(&(players[t->t_owner]));
              break;
          }
#endif
          continue;
        }
        else
          t->t_y = GWIDTH;
      }
      else if (t->t_y > GWIDTH) {
        if (!wrap_galaxy) {
          t->t_y = GWIDTH;
          DO_WALL_HIT(t);
#ifdef LTD_STATS
          /* update torp hit wall stat */
          switch(t->t_type) {
            case TPHOTON:
              if (status->tourn)
                ltd_update_torp_wall(&(players[t->t_owner]));
              break;
            case TPLASMA:
              if (status->tourn)
                ltd_update_plasma_wall(&(players[t->t_owner]));
              break;
          }
#endif
          continue;
        }
        else
          t->t_y = 0;
      }
#undef DO_WALL_HIT

      if (t->t_attribute & TWOBBLE)
        t->t_dir += (random() % 3) - 1;

      if (near(t)) {
#ifdef LTD_STATS
        /* if near(t) returns 1, it hit a player, update stats */
        switch(t->t_type) {
          case TPHOTON:
            if (status->tourn)
              ltd_update_torp_hit(&(players[t->t_owner]));
            break;
          case TPLASMA:
            if (status->tourn)
              ltd_update_plasma_hit(&(players[t->t_owner]));
            break;
        }
#endif
        explode(t);
      }
      continue;
    case TDET:
      explode(t);
      continue;
    case TEXPLODE:
      if (t->t_fuse-- > 0)
        continue;
      break;
    case TOFF:
      break;
#if 1
    default:                    /* Shouldn't happen */
      break;
#endif
    }

    t->t_status = TFREE;
    switch (t->t_type) {
      case TPHOTON: players[t->t_owner].p_ntorp--; break;
      case TPLASMA: players[t->t_owner].p_nplasmatorp--; break;
    }
  }
}


/* See if there is someone close enough to explode for */
static int near(struct torp *t)
{
  int dx, dy;
  struct player *j;

  for (j = firstPlayer; j <= lastPlayer; j++) {
    if (j->p_status != PALIVE)
      continue;
    if (t->t_owner == j->p_no)
      continue;
    if (! ((t->t_war & j->p_team) || (t->t_team & j->p_war)))
      continue;
    dx = t->t_x - j->p_x;
    if ((dx < -EXPDIST) || (dx > EXPDIST))
      continue;
    dy = t->t_y - j->p_y;
    if ((dy < -EXPDIST) || (dy > EXPDIST))
      continue;
    if (dx*dx + dy*dy <= EXPDIST * EXPDIST)
      return 1;
    }
  return 0;
}
    

/* 
 * Dole out damage to all unsafe surrounding players 
 */
static void explode(struct torp *torp)
{
  int dx, dy, dist;
  int damage, damdist;
  struct player *j;
#ifdef CHAIN_REACTION
  struct player *k;
#endif

  damdist = (torp->t_type == TPHOTON) ? DAMDIST : PLASDAMDIST;
  for (j = firstPlayer; j <= lastPlayer; j++) {
    if (j->p_status != PALIVE)
      continue;

    /*
     * Check to see if this player is safe for some reason  */
    if (j->p_no == torp->t_owner) {
      if (torp->t_attribute & TOWNERSAFE)
        continue;
    }
    else {
      if ((torp->t_attribute & TOWNTEAMSAFE) && (torp->t_team == j->p_team))
        continue;
    }
    if (torp->t_status == TDET) {
      if (j->p_no == torp->t_whodet) {
        if (torp->t_attribute & TDETTERSAFE)
          continue;
      }
      else {
        if ((torp->t_attribute & TDETTEAMSAFE) &&
            (j->p_team == players[(int)torp->t_whodet].p_team))
          continue;
      }
    }

    /*
     * This player is not safe, and will be affected if close enough.
     * Check the range.    */
    dx = torp->t_x - j->p_x;
    if ((dx < -damdist) || (dx > damdist))
      continue;
    dy = torp->t_y - j->p_y;
    if ((dy < -damdist) || (dy > damdist))
      continue;
    dist = dx*dx + dy*dy;
    if (dist > damdist * damdist)
      continue;

    /*
     * Ouch!  
     * Damage the player: */
    if (dist <= EXPDIST * EXPDIST)
      damage = torp->t_damage;
    else {
      damage = torp->t_damage * (damdist - sqrt((double) dist))
        / (damdist - EXPDIST);
    }

    if (damage > 0) {

#ifdef LTD_STATS

      /* update torp damage stats */
      switch (torp->t_type) {
        case TPHOTON:
          if (status->tourn)
            ltd_update_torp_damage(&(players[torp->t_owner]), j, damage);
          break;
        case TPLASMA:
          if (status->tourn)
            ltd_update_plasma_damage(&(players[torp->t_owner]), j, damage);
          break;
      }

#endif

      /* 
       * First, check to see if torp owner has started a war.
       * Note that if a player is at peace with the victim, then
       * the torp has caused damage either accidently, or because
       * the victim was at war with, or hostile to, the player.
       * In either case, we don't consider the damage to be
       * an act of war */
      if (player_(torp->t_owner)->p_hostile & j->p_team) {
        player_(torp->t_owner)->p_swar |= j->p_team;
      }

      /*
       * Inflict the damage:  */
      if (j->p_flags & PFSHIELD) {
        j->p_shield -= damage;
        if (j->p_shield < 0) {
          j->p_damage -= j->p_shield;
          j->p_shield = 0;
        }
      }
      else {
        j->p_damage += damage;
      }

      /* 
       * Check for kill:  */
      if (j->p_damage >= j->p_ship.s_maxdamage) {
        j->p_status = PEXPLODE;
        if (j->p_ship.s_type == STARBASE) {
          j->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
        } else {
          j->p_explode = 10/PLAYERFUSE;
        }
#ifdef CHAIN_REACTION
        if ((torp->t_whodet != NODET) 
            && (torp->t_whodet != j->p_no) 
            && (players[(int)torp->t_whodet].p_team != j->p_team))
          k = player_(torp->t_whodet);
        else 
          k = player_(torp->t_owner);
        if ((k->p_team != j->p_team) || 
            ((k->p_team == j->p_team) && (j->p_flags & PFPRACTR)))
        {
          k->p_kills += 1.0 + j->p_armies * 0.1 + j->p_kills * 0.1;

#ifndef LTD_STATS       /* ltd_update_kills automatically checks max kills */

          checkmaxkills(k->p_no);

#endif /* LTD_STATS */

        }

#ifndef LTD_STATS

        killerstats(k->p_no, torp->t_owner, j);
        loserstats(j->p_no);

#endif /* LTD_STATS */

        j->p_whydead = (torp->t_type == TPHOTON) ? KTORP : KPLASMA;
        j->p_whodead = k->p_no;

#ifdef LTD_STATS

        /* k = killer to be credited
           torp->t_owner = killer who did the damage
           j = victim */

        if (status->tourn) {

          status->kills++;
          status->losses++;

          ltd_update_kills(k, &players[torp->t_owner], j);
          ltd_update_deaths(j, k);

        }

#endif /* LTD_STATS */

        killmess(j, k, player_(torp->t_owner), 
                 (k->p_no != torp->t_owner) ? 
                 ((torp->t_type == TPHOTON) ? KTORP2 : KPLASMA2) 
                 : j->p_whydead);

#else /* CHAIN_REACTION */

#ifndef LTD_STATS

        killerstats(torp->t_owner, j);

#endif /* LTD_STATS */
        players[torp->t_owner].p_kills += 1.0 + 
          j->p_armies * 0.1 + j->p_kills * 0.1;

#ifndef LTD_STATS       /* ltd_update_kills checks for max kills */

        checkmaxkills(torp->t_owner);
        loserstats(j->p_no);

#endif /* LTD_STATS */

        j->p_whydead = (torp->t_type == TPHOTON) ? KTORP : KPLASMA;
        j->p_whodead = torp->t_owner;

#ifdef LTD_STATS

        /* torp->t_owner = actual killer = credit killer
           j = victim */

        if (status->tourn) {

          status->kills++;
          status->losses++;

          ltd_update_kills(&(players[torp->t_owner]),
                           &(players[torp->t_owner]), j);
          ltd_update_deaths(j, k);

        }

#endif /* LTD_STATS */

        killmess(j, &players[torp->t_owner]);
#endif /* CHAIN_REACTION */
      } 
    } 
  } 
  torp->t_status = TEXPLODE; 
  torp->t_fuse = 10/TORPFUSE; 
} 

#ifndef LTD_STATS

static void loserstats(int pl)
{
    struct player *dude;

    dude= &players[pl];

    if (dude->p_ship.s_type == STARBASE) {
        if (status->tourn
#ifdef BASEPRACTICE
            || practice_mode
#endif
            )
         dude->p_stats.st_sblosses++;
    } else {
        if (status->tourn) {
            dude->p_stats.st_tlosses++;
            status->losses++;
        } else {
            dude->p_stats.st_losses++;
        }
    }
}

#ifdef CHAIN_REACTION

static void killerstats(int cl, int pl, struct player *victim)
{
   struct player  *dude, *credit_killer;

   credit_killer = &players[cl];
   dude = &players[pl];

   if(credit_killer->p_team == dude->p_team)
      credit_killer = dude;

   if (credit_killer->p_ship.s_type == STARBASE) {
        if (status->tourn
#ifdef BASEPRACTICE
            || practice_mode
#endif
            )
      credit_killer->p_stats.st_sbkills++;
      credit_killer->p_stats.st_armsbomb += 5 * victim->p_armies;
   } else {
      if (status->tourn) {
         credit_killer->p_stats.st_tkills++;
         status->kills++;
#ifdef NO_CHUNG_CREDIT
         if (dude->p_team != victim->p_team) {
#endif
            credit_killer->p_stats.st_tarmsbomb += 5 * victim->p_armies;
            status->armsbomb += 5 * victim->p_armies;
#ifdef NO_CHUNG_CREDIT
         }
#endif
      } else {
         credit_killer->p_stats.st_kills++;
         credit_killer->p_stats.st_armsbomb += 5 * victim->p_armies;
      }
   }
}

#else /* CHAIN_REACTION */

static void killerstats(int pl, struct player *victim)
{
    struct player *dude;

    dude= &players[pl];

    if (dude->p_ship.s_type == STARBASE) {
        if (status->tourn
#ifdef BASEPRACTICE
            || practice_mode
#endif
            )
        dude->p_stats.st_sbkills++;
        dude->p_stats.st_armsbomb += 5 * victim->p_armies;
    } else {
        if (status->tourn) {
            dude->p_stats.st_tkills++;
            status->kills++;
#ifdef NO_CHUNG_CREDIT
            if (dude->p_team != victim->p_team) {
#endif
                dude->p_stats.st_tarmsbomb += 5 * victim->p_armies;
                status->armsbomb += 5 * victim->p_armies;
#ifdef NO_CHUNG_CREDIT
            }
#endif
        } else {
            dude->p_stats.st_kills++;
            dude->p_stats.st_armsbomb += 5 * victim->p_armies;
        }
    }
}
#endif /* CHAIN_REACTION */

#endif /* LTD_STATS */

#ifndef LTD_STATS

static void checkmaxkills(int pl)
{
    struct stats *stats;
    struct player *dude;

    dude= &(players[pl]);
    stats= &(dude->p_stats);

    if (dude->p_ship.s_type == STARBASE) {
        if (stats->st_sbmaxkills < dude->p_kills) {
            stats->st_sbmaxkills = dude->p_kills;
        }
    } else if (stats->st_maxkills < dude->p_kills) {
        stats->st_maxkills = dude->p_kills;
    }
}

#endif /* LTD_STATS */

/* Track armies using messages */
#define AMT_POP 0
#define AMT_PLAGUE 1
#define AMT_BOMB 2
#define AMT_BEAMUP 3
#define AMT_BEAMDOWN 4
#define AMT_TRANSUP 5
#define AMT_TRANSDOWN 6
#define AMT_MAX 7

inline static void army_track(int type, void *who, void *what, int num)
{
#ifdef ARMYTRACK
    static char * amt_list[AMT_MAX] =
    {
        "popped",
        "plagued",
        "bombed",
        "beamed up",
        "beamed down",
        "transferred up",
        "transferred down"
    };
    char addr_str[9] = "WRN->\0\0\0";
    struct planet *pl;
    struct player *p, *sb;

    if (!(inl_mode)) return;
    pl = (struct planet *)what;

    switch (type) {
    case AMT_POP:
    case AMT_PLAGUE:
        strcpy(&addr_str[5], "GOD");
        pmessage(0, 0, addr_str,
                 "ARMYTRACK -- -- -- %s %d armies at %s (%d) [%s]",
                 amt_list[type], num, /* num == 1 ? "y" : "ies",  */
                 pl->pl_name, pl->pl_armies, teamshort[pl->pl_owner]);
        break;
    case AMT_BOMB:
        p = (struct player *)who;
        strncpy(&addr_str[5], p->p_mapchars, 3);
        pmessage(0, 0, addr_str,
                 "ARMYTRACK %s {%s} (%d) %s %d armies at %s (%d) [%s]",
                 p->p_mapchars, shiptypes[p->p_ship.s_type], p->p_armies,
                 amt_list[type], num, /* num == 1 ? "y" : "ies",  */
                 pl->pl_name, pl->pl_armies, teamshort[pl->pl_owner]);
        break;
    case AMT_BEAMUP:
    case AMT_BEAMDOWN:
        p = (struct player *)who;
        strncpy(&addr_str[5], p->p_mapchars, 3);
        pmessage(0, 0, addr_str,
                 "ARMYTRACK %s {%s} (%d) %s %d armies at %s (%d) [%s]",
                 p->p_mapchars, shiptypes[p->p_ship.s_type], p->p_armies,
                 amt_list[type], num, /* num == 1 ? "y" : "ies",  */
                 pl->pl_name, pl->pl_armies, teamshort[pl->pl_owner]);
        break;
    case AMT_TRANSUP:
    case AMT_TRANSDOWN:
        p = (struct player *)who;
        sb = (struct player *)what;
        strncpy(&addr_str[5], p->p_mapchars, 3);
        pmessage(0, 0, addr_str,
                 "ARMYTRACK %s {%s} (%d) %s %d armies at %s {%s} (%d) [%s]",
                 p->p_mapchars, shiptypes[p->p_ship.s_type], p->p_armies,
                 amt_list[type], num, /* num == 1 ? "y" : "ies",  */
                 sb->p_mapchars, shiptypes[sb->p_ship.s_type], sb->p_armies,
                 teamshort[sb->p_team]);
        break;
    }

#endif
}

#ifdef INL_POP
/* updates planets, grows armies on them, etc */
static int pl_poporder[MAXPLANETS]= { -1 } ;   /* planet number to pop next */
static int lastpop;     /* pl_poporder index number of planet last popped */

static void udplanets(void)
{

    if(--lastpop <0)
        RandomizePopOrder();

    PopPlanet(pl_poporder[lastpop]);
}


static void PopPlanet(int plnum) 
{
    register struct planet *l;
    int num;
    int orig_armies;

    if(debug)printf("populating planet %d\n", plnum);

    if( plnum <0 || plnum >= MAXPLANETS) return; /* arg sanity */

    l = &planets[plnum];

    if (l->pl_armies == 0)
        return;

    if ((l->pl_armies >= max_pop) && !(status->tourn))
        return;

#ifndef NO_PLANET_PLAGUE
#ifdef ERIKPLAGUE
    if (l->pl_armies > 5) {
        /* This barely plagues unless you have a HUGE number of armies;
           if you happen to have ~4500, though, you could end up with < 0! */
        num = ((random() % 4500) + (l->pl_armies * l->pl_armies)) / 4500;
        if (num) {
            army_track(AMT_PLAGUE, NULL, l, num);
            l->pl_armies -= num;
/* not needed since armies never < 5
            if (l->pl_armies < 5)
                l->pl_flags |= PLREDRAW ;
*/
        }
    }
#else
    if ((random() % 3000) < l->pl_armies) { /* plague! */
        num = (random() % l->pl_armies);
        l->pl_armies -= num;
        army_track(AMT_PLAGUE, NULL, l, num);
        if (l->pl_armies < 5) l->pl_flags |= PLREDRAW /* XXX put in client! */;
    }
#endif
#endif

    orig_armies = l->pl_armies;


    if (l->pl_armies < 4) {
        if ((random() % 20) == 0) { /* planets< 4 grow faster */
            l->pl_armies++;
        }
        if (l->pl_flags & PLAGRI) { /* Always grow 1 if Agricultural. */
            if (++l->pl_armies > 4) l->pl_flags |= PLREDRAW;
                /* XXX REDRAW in client */
        }
    }

    if ((random() % 10) == 0) {  /* Chance for extra armies. */
        if (l->pl_armies < 5) l->pl_flags |= PLREDRAW;
                /* XXX REDRAW in client */
        num = (random() % 3) + 1;
        l->pl_armies += num;
    }

    /* Argicultural worlds have extra chance for army growth. */
    if (l->pl_flags & PLAGRI) {
        if ((random() % 5) == 0) {
            if (++l->pl_armies > 4) l->pl_flags |= PLREDRAW;
                /* XXX REDRAW in client */
        }
    }

    num = l->pl_armies - orig_armies;
    if (num)
        army_track(AMT_POP, NULL, l, num);
}

/* reorder order in which planets will be populated
** set lastpop to MAXPLANETS */
static void RandomizePopOrder(void)
{
     register int i,tmp,tmp2;

     if(debug) printf("Randomizing planetary pop order!\n");

     if (pl_poporder[0] < 0) {
     for(i=0;i<MAXPLANETS;i++) /* init the pop order table */
        pl_poporder[i]= i;
        }

     /* jmn - optimized based on hadley suggestion */
     for(i=MAXPLANETS-1;i;i--) {
        tmp=(random( ) % (i+1));
        tmp2=pl_poporder[tmp];
        pl_poporder[tmp]=pl_poporder[i];
        pl_poporder[i]=tmp2;
        }

     if(debug){
         printf("Planet pop order:");
         for(i=0;i<MAXPLANETS;i++)printf(" %d", pl_poporder[i]);
         printf("\n");
     }

     lastpop=MAXPLANETS-1;
}

#else /* NORMAL POPPING SCHEME */
static void udplanets(void)
{
  register int i;
  register struct planet *l;
  
  /* moved to udsurrend
     for (i = 0; i <= MAXTEAM; i++) {
     (teams[i].s_turns--);
     }
     */
  
  for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
    
#ifdef PLANETSTAGGER
    /* since we only want to run the pop code on about */
    /* MAXPLANETS/PLANETSTAGGER planets on average, there should be */
    /* a corresponding probability that we do so; otherwise, we just */
    /* skip the planet.  Note that randomly choosing a planet */
    /* MAXPLANETS/PLANETSTAGGER times to pop results in much wilder */
    /* pop distributions.  Also note that we can't just scale down */
    /* the probabilities in each (random() % X), because not each */
    /* statement below involves a probability.  4/15/92 TC */
    
    if ((random() % PLANETSTAGGER) > 0)
      continue;
#endif

    if ((l->pl_armies >= max_pop) && !(status->tourn))
        return;
    
    /* moved to udsurrend
       if (l->pl_couptime)      / Decrement coup counter one minute /
            l->pl_couptime--;
            */
    if (l->pl_armies == 0)
            continue;
    if ((random() % 3000) < l->pl_armies) {
      l->pl_armies -= (random() % l->pl_armies);
      if (l->pl_armies < 5) l->pl_flags |= PLREDRAW;
    }
    if (l->pl_armies < 4) {
      if ((random() % 20) == 0) {
        l->pl_armies++;
      }
      if (l->pl_flags & PLAGRI) { /* Always grow 1 if Agricultural. */
        if (++l->pl_armies > 4) l->pl_flags |= PLREDRAW;
      }
    }
    if ((random() % 10) == 0) {  /* Chance for extra armies. */
      if (l->pl_armies < 5) l->pl_flags |= PLREDRAW;
      l->pl_armies += (random() % 3) + 1;
    }
    /* Argicultural worlds have extra chance for army growth. */
    if (((random() % 5) == 0) && (l->pl_flags & PLAGRI)) {
      if (++l->pl_armies > 4) l->pl_flags |= PLREDRAW;
    }
  }
}
#endif /* INL_POP */

/* new code TC */
static void udsurrend(void)
{
    register int i, t;
    struct planet *l;
    struct player *j;

    /* Decrement SB reconstruction timer code moved here (from udplanets()) */

    for (i = 0; i <= MAXTEAM; i++) {
        (teams[i].s_turns--);
    }

    /* Coup timer moved here (from udplanets()) */

    for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
        if (l->pl_couptime)     /* Decrement coup counter one minute */
            l->pl_couptime--;
    }

    /* All tmode-only code must follow this statement. */

    if (!status->tourn) return;

    /* Terminate non-warring humans 9/1/91 TC */

    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
        if ((j->p_status == PALIVE) &&
            !(j->p_flags & PFROBOT) &&
#ifdef OBSERVERS
            (j->p_status != POBSERV) &&
#endif
            (j->p_team != NOBODY) &&
            (realNumShips(j->p_team) < tournplayers) &&
            (random()%5 == 0))
            rescue(TERMINATOR, j->p_no);
    }


#ifdef OBSERVERS
    /* 
     * Bump observers of non-Tmode races to selection screen to choose
     * T mode teams
     */
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
        if( (j->p_status == POBSERV) && 
            (j->p_team != NOBODY) &&
            (realNumShips(j->p_team) < tournplayers) ) {
            j->p_status = PEXPLODE;
            j->p_whydead = KPROVIDENCE;
            j->p_flags &= ~(PFPLOCK | PFPLLOCK);
            j->p_x = -100000;         /* place me off the tactical */
            j->p_y = -100000;
        }
    }
#endif /* OBSERVERS */

    for (t = 0; t <= MAXTEAM; t++) { /* maint: was "<" 6/22/92 TC */
        /* "suspend" countdown if less than Tmode players */
        if ((teams[t].s_surrender == 0) || (realNumShips(t) < tournplayers))
            continue;

        /* also if team has more than surrenderStart planets */
        if (teams[t].s_plcount > surrenderStart)
            continue;

        teams[t].s_surrender--;
        if (teams[t].s_surrender == 0) {

            /* terminate race t */

            pmessage(0, MALL, " ", " ");
            pmessage(0, MALL, "GOD->ALL", "The %s %s surrendered.",
                team_name(t), team_verb(t));
            pmessage(0, MALL, " ", " ");
            surrenderMessage(t);
            
            /* neutralize race t's planets  */

            for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
                if (((l->pl_flags & ALLTEAM) == t) && (l->pl_flags & PLHOME))
                    l->pl_couptime = MIN_COUP_TIME +
                        (random() % (MAX_COUP_TIME - MIN_COUP_TIME));

                /* removed above dependency on PLANETFUSE and hardcoded */
                /* constants (now in defs.h).  30-60 min for a coup. */
                /* 4/15/92 TC */

                if (l->pl_owner == t) {
                    l->pl_owner = 0;
                    l->pl_armies = 0;
                    l->pl_flags |= PLCHEAP;
                }
            }

            /* kick out all the players on that team */
            /* makes it easy for them to come in as a different team */
            for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
                if (j->p_team == t) {
                    j->p_genoplanets=0;
                    j->p_genoarmsbomb=0;
                    j->p_armsbomb=0;
                    j->p_planets=0;
                }
                if ((j->p_status == PALIVE) && (j->p_team == t)) {
                    j->p_status = PEXPLODE;
                    if (j->p_ship.s_type == STARBASE)
                        j->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
                    else
                        j->p_explode = 10/PLAYERFUSE;
                    j->p_whydead = KPROVIDENCE;
                    j->p_whodead = 0;
                }
            }                   /* end for */
            
        }                       /* end if */
        else {                  /* surrender countdown */
            if ((teams[t].s_surrender % 5) == 0) {
                pmessage(0, MALL, " ", " ");
                pmessage(0, MALL, "GOD->ALL", "The %s %s %d minutes remaining.",
                        team_name(t), team_verb(t),
                        teams[t].s_surrender);

                pmessage(0, MALL, "GOD->ALL", "%d planets will sustain the empire.  %d suspends countdown.",
                        SURREND, surrenderStart+1);
                pmessage(0, MALL, " ", " ");
            }
        }
    }                           /* end for team */
}

static void udphaser(void)
{
    register int i;
    register struct phaser *j;
    register struct player *victim;

    for (i = 0, j = &phasers[i]; i < MAXPLAYER; i++, j++) {
        switch (j->ph_status) {
            case PHFREE:
                continue;
            case PHMISS:
            case PHHIT2:
                if (j->ph_fuse-- == 1)
                    j->ph_status = PHFREE;
                break;
            case PHHIT:
                if (j->ph_fuse-- == players[i].p_ship.s_phaserfuse) {
                    victim = player_(j->ph_target);

                    /* start a war, if necessary */
                    if (player_(i)->p_hostile & victim->p_team) {
                        player_(i)->p_swar |= victim->p_team;
                    }

                    if (victim->p_status == PALIVE) {
                        if (victim->p_flags & PFSHIELD) {
                            victim->p_shield -= j->ph_damage;
                            if (victim->p_shield < 0) {
                                victim->p_damage -= victim->p_shield;
                                victim->p_shield = 0;
                            }
                        }
                        else {
                            victim->p_damage += j->ph_damage;
                        }
                        if (victim->p_damage >= victim->p_ship.s_maxdamage) {
                            victim->p_status = PEXPLODE;
                            if (victim->p_ship.s_type == STARBASE)
                                victim->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
                            else
                                victim->p_explode = 10/PLAYERFUSE;
                            players[i].p_kills += 1.0 + 
                                victim->p_armies * 0.1 +
                                victim->p_kills * 0.1;

#ifndef LTD_STATS
#ifdef CHAIN_REACTION

                            killerstats(i, i, victim);
                            checkmaxkills(i);
                            killmess(victim, &players[i], &players[i],
                                     KPHASER);

#else /* CHAIN_REACTION */

                            killerstats(i, victim);
                            checkmaxkills(i);
                            killmess(victim, &players[i]);

#endif /* CHAIN_REACTION */

                            loserstats(j->ph_target);

#endif /* LTD_STATS */

                            victim->p_whydead = KPHASER;
                            victim->p_whodead = i;

#ifdef LTD_STATS
                            /* i = credit killer = actual killer
                               victim = dead guy */

                            if (status->tourn) {

                              status->kills++;
                              status->losses++;

                              ltd_update_kills(&(players[i]), &(players[i]),
                                               victim);
                              ltd_update_deaths(victim, &players[i]);

                            }

#ifdef CHAIN_REACTION

                            killmess(victim, &players[i], &players[i],
                                     KPHASER);

#else /* CHAIN_REACTION */

                            killmess(victim, &players[i]);

#endif /* CHAIN_REACTION */

#endif /* LTD_STATS */

                        }
                    }
                }
                if (j->ph_fuse == 0)
                    j->ph_status = PHFREE;
                break;
        }
    }
}

int pl_warning[MAXPLANETS];     /* To keep planets shut up for awhile */
int tm_robots[MAXTEAM + 1];             /* To limit the number of robots */
int tm_coup[MAXTEAM + 1];               /* To allow a coup */

static void teamtimers(void)
{
    register int i;
    for (i = 0; i <= MAXTEAM; i++) {
        if (tm_robots[i] > 0)
            tm_robots[i]--;
        if (tm_coup[i] > 0)
            tm_coup[i]--;
    }
}


/* 
 * Have planet(s) do damage to player j.  
 * This is some of the more inefficient code in the game.  If you really
 * need cycles, change the code to have planets only attack ships that are 
 * in orbit.
 */
static void pldamageplayer(struct player *j)
{
  register struct planet *l;
  LONG dx, dy;
  int damage;
  char buf[90];

#if 0
  /*
   * See if this player can have possibly reached a harmful planet yet: */
  if (j->p_planetdistfloor > 0)
    return;
#endif

  for (l = firstPlanet; l <= lastPlanet; l++) {
    if (! (j->p_war & l->pl_owner))
      continue;
    if (l->pl_armies == 0)
      continue;

    dx = ABS(l->pl_x - j->p_x);
    dy = ABS(l->pl_y - j->p_y);
    if (dx < 3 * PFIREDIST && dy < 3 * PFIREDIST)
      l->pl_flags |= PLREDRAW;
    if (dx > PFIREDIST || dy > PFIREDIST)   /*XXX*/
      continue;
    if (dx*dx + dy*dy > PFIREDIST*PFIREDIST)
      continue;
    
    damage = l->pl_armies / 10 + 2;
    
    if (j->p_flags & PFSHIELD) {
      j->p_shield -= damage;
      if (j->p_shield < 0) {
        j->p_damage -= j->p_shield;
        j->p_shield = 0;
      }
    }
    else {
      j->p_damage += damage;
    }
    if (j->p_damage >= j->p_ship.s_maxdamage) {
      if (j->p_ship.s_type == STARBASE)
        j->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
      else
        j->p_explode = 10/PLAYERFUSE;
      j->p_status = PEXPLODE;

#ifndef LTD_STATS

      loserstats(j->p_no);

#endif /* LTD_STATS */

      (void) sprintf(buf, "%s killed by %s (%c)",
                     j->p_longname,
                     l->pl_name,
                     teamlet[l->pl_owner]);
      arg[0] = DMKILLP; /* type */
      arg[1] = j->p_no;
      arg[2] = l->pl_no;
      arg[3] = 0;
      arg[4] = j->p_armies;
#ifdef CHAIN_REACTION
      strcat(buf,"   [planet]");
      arg[5] = KPLANET;
#endif
      pmessage(0, MALL | MKILLP, "GOD->ALL", buf);

      j->p_whydead = KPLANET;
      j->p_whodead = l->pl_no;

#ifdef LTD_STATS

      /* j = dead guy
         NULL = killed by planet */

      if (status->tourn) {

        status->losses++;
        ltd_update_deaths(j, NULL);

      }

#endif /* LTD_STATS */

    }
  }
}


static void plfight(void)
{
  register int h;
  register struct player *j;
  register struct planet *l;
  int rnd;
  int ab;
  char buf[90];
  char addrbuf[10];

  for (h = 0, l = &planets[h]; h < MAXPLANETS; h++, l++) {
    if (l->pl_flags & PLCOUP) {
      l->pl_flags &= ~PLCOUP;
      sprintf(addrbuf, "%-3s->%s ", l->pl_name, teamshort[l->pl_owner]);
      pmessage(l->pl_owner, MTEAM | MCOUP1, addrbuf, 
               "Planet lost in political struggle");
      l->pl_owner = (l->pl_flags & ALLTEAM);
      l->pl_armies = 4;
      sprintf(addrbuf, "%-3s->%s ", l->pl_name, teamshort[l->pl_owner]);
      pmessage(l->pl_owner, MTEAM | MCOUP2, addrbuf,
               "Previous dictatorship overthrown in coup d'etat");
    }
    l->pl_flags &= ~PLREDRAW;
    if (pl_warning[h] > 0)
      pl_warning[h]--;
  }

  for (j = firstPlayer; j <= lastPlayer; j++) {
    if (j->p_status != PALIVE)
      continue;

    pldamageplayer(j);
    
    /* do bombing */
    if ((!(j->p_flags & PFORBIT)) || (!(j->p_flags & PFBOMB)))
      continue;
    l = &planets[j->p_planet];

    if (j->p_team == l->pl_owner)
      continue;
    if (!(j->p_war & l->pl_owner))
      continue;
    if (l->pl_armies < 5)
      continue;

      /* Warn owning team */
    if (pl_warning[j->p_planet] <= 0)
    {
      pl_warning[j->p_planet] = 50/PLFIGHTFUSE; 
      (void) sprintf(buf, "%-3s->%-3s",
                     l->pl_name, teamshort[l->pl_owner]);
      arg[0] = DMBOMB; /* type */
      arg[1] = j->p_no;
      arg[2] =(100*j->p_damage)/(j->p_ship.s_maxdamage)  ;
      arg[3] = l->pl_no;
      pmessage(l->pl_owner, MTEAM | MBOMB, buf, 
               "We are being attacked by %s %s who is %d%% damaged.",
               j->p_name,
               j->p_mapchars,
               (100*j->p_damage)/(j->p_ship.s_maxdamage));
      (void) sprintf(buf, "%-3s->%-3s",
                     l->pl_name, teamshort[l->pl_owner]);
    }
    
    /* start the war (if necessary) */
    j->p_swar |= l->pl_owner;
    
    rnd = random() % 100;
    if (rnd < 50) {
      continue;
    } 
    
    if (rnd < 80)
      ab = 1;
    else if (rnd < 90)
      ab = 2;
    else
      ab = 3;

    if (j->p_ship.s_type == ASSAULT)
      ab++;

    l->pl_armies -= ab;

    if (l->pl_armies<5)
      l->pl_flags |= PLREDRAW;

    j->p_kills += (0.02 * ab);
    j->p_armsbomb += ab;
    j->p_genoarmsbomb += ab;

    army_track(AMT_BOMB, j, l, ab);

#ifdef LTD_STATS

    /* ltd stats are tournament stats only.  j hasn't killed anyone,
       but update max kills because bombing gives kill credit.  if
       we're in tournament mode, we shouldn't have to check if the
       opposing team has >= 3 players.  player gets no additional
       credit for bombing a planet flat. */

    if (status->tourn)
    {

      status->armsbomb += ab;

      /* j hasn't killed anyone, but does get kill credit for bombing */

      ltd_update_kills_max(j);

      /* j = player who has bombed the planet
         l = planet that was just bombed
         ab = number of armies that were just bombed */

      ltd_update_bomb(j, l, ab);

    }

#else /* LTD_STATS */

    checkmaxkills(j->p_no);


    /* Give him bombing stats if he is bombing a team with 3+ */
    if (status->tourn && realNumShips(l->pl_owner) >= 3)
    {
      status->armsbomb += ab;
      j->p_stats.st_tarmsbomb += ab;
    }
    else {
      j->p_stats.st_armsbomb += ab;
    }

#ifdef FLAT_BONUS                         /* Bonus if you flatten a planet */
    if (status->tourn && realNumShips(l->pl_owner) >= 3) {
      if (l->pl_armies < 5) {
        j->p_stats.st_tarmsbomb += 2;
        status->armsbomb +=2;
      }
    }

#endif /* FLAT_BONUS */

#endif /* LTD_STATS */

    /* Send in a robot if there are no other defenders (or not Tmode)
       and the planet is in the team's home space */
    
    if (!(inl_mode)) {
      if (((tcount[l->pl_owner] == 0) || (NotTmode(ticks))) && 
        (l->pl_flags & l->pl_owner) &&
#ifdef PRETSERVER
    !bot_in_game &&
#endif
        tm_robots[l->pl_owner] == 0) {

        rescue(l->pl_owner, NotTmode(ticks));
        tm_robots[l->pl_owner] = (1800 + (random() % 1800)) / TEAMFUSE;
      }
    }
  }
}

static void beam(void)
{
    register int i;
    register struct player *j;
    register struct planet *l = NULL;
    char buf1[MSG_LEN];

    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        if ((j->p_status != PALIVE) || !(j->p_flags & (PFORBIT|PFDOCK)))
            continue;
        if (j->p_flags & PFORBIT)
            l = &planets[j->p_planet];
        if (j->p_flags & PFBEAMUP) {
            if (j->p_flags & PFORBIT)
                if (l->pl_armies < 5)
                    continue;
            if (j->p_flags & PFDOCK)
                if (players[j->p_dock_with].p_armies < 1)
                    continue;
            if (j->p_armies >= j->p_ship.s_maxarmies)
                continue;
            if (j->p_ship.s_type == ASSAULT) {
                if (j->p_armies >= (int)((float)((int)(j->p_kills*100)/100.0) * 3.0))
                    continue;
            } else if (j->p_ship.s_type != STARBASE)
                if (j->p_armies >= (int)((float)((int)(j->p_kills*100)/100.0) * 2.0))
                    continue;
            if (j->p_flags & PFORBIT) {
                if (j->p_team != l->pl_owner)
                    continue;
                if (j->p_war & l->pl_owner)
                    continue;
            } 

            j->p_armies++;
            if (j->p_flags & PFORBIT) {
                l->pl_armies--;
                army_track(AMT_BEAMUP, j, l, 1);

#ifdef LTD_STATS
                /* j = player, NULL = army beamed from planet */
                if (status->tourn) {
                    ltd_update_armies_carried(j, NULL);
                }
#endif
            } else if (j->p_flags & PFDOCK) {
                struct player *base = bay_owner(j);
                base->p_armies--;
                army_track(AMT_TRANSUP, j, base, 1);

#ifdef LTD_STATS
                if (status->tourn) {
                    ltd_update_armies_carried(j, base);
                }
#endif
 
            }

            continue;
        }
        if (j->p_flags & PFBEAMDOWN) {
            if (j->p_armies == 0)
                continue;
            if (j->p_flags & PFORBIT) {
                if ((!(j->p_war & l->pl_owner))
                    && (j->p_team != l->pl_owner) && (l->pl_owner != NOBODY))
                    continue;
                army_track(AMT_BEAMDOWN, j, l, 1);
                if (j->p_team != l->pl_owner) {

#ifdef NO_BEAM_DOWN_OUT_OF_T_MODE
                    /* prevent beaming down to non-team planet out of t */
                    if (!status->tourn) continue;
#endif

                    j->p_armies--;

                    if (l->pl_armies) {
                        int oldowner = l->pl_owner;

                        /* start the war (if necessary) */
                        j->p_swar |= l->pl_owner;

                        l->pl_armies--;
                        j->p_kills += 0.02;
                        j->p_armsbomb += 1;
                        j->p_genoarmsbomb += 1;

#ifdef LTD_STATS

                        /* LTD is only for tournament stats */

                        if (status->tourn) {


                            /* credit the player for beamdown
                               j = player who just beamed down 1 army
                               l = planet that just lost 1 army */

                            ltd_update_armies(j, l);

                        }

                        /* update max kills for bomb kill credit */

                        ltd_update_kills_max(j);

#else /* LTD_STATS */

                        checkmaxkills(i);
                        if (status->tourn && realNumShips(oldowner) >= 3) {
                            j->p_stats.st_tarmsbomb++;
                            status->armsbomb++;
                        } else {
                            j->p_stats.st_armsbomb++;
                        }

#endif /* LTD_STATS */

                        if (l->pl_armies == 0) {
                            /* Give planet to nobody */
                            (void) sprintf(buf1, "%-3s->%-3s",
                                           l->pl_name, teamshort[l->pl_owner]);

                            arg[0] = DMDEST; /* type */
                            arg[1] = l->pl_no;
                            arg[2] = j->p_no;

#ifndef LTD_STATS       /* LTD_STATS should not use NEW_CREDIT */
#ifdef NEW_CREDIT       /* give 1 planet credit for destroying planet */

                            j->p_kills += 0.25;
                            checkmaxkills(i);
                            if (status->tourn && realNumShips(oldowner) >= 3)
                            {
                                j->p_stats.st_tplanets++;
                                status->planets++;
                                if ((l->pl_flags & PLCORE) && 
                                  !(j->p_team & l->pl_flags) &&
                                  (realNumShips(ALLTEAM & l->pl_flags)>2))
                                {
                                    j->p_stats.st_tplanets++;
                                    status->planets++;
                                }
                            }
                            else {
                                j->p_stats.st_planets++;
                            }

#endif /* NEW_CREDIT */
#endif /* LTD_STATS */

                            pmessage(l->pl_owner, MTEAM | MDEST, buf1,
                                "%s destroyed by %s",
                                l->pl_name,
                                j->p_longname);
                            if (status->tourn && 
                                  realNumShips(l->pl_owner) < 2 &&
                                  realNumShips(l->pl_flags & 
                                    (FED|ROM|ORI|KLI)) < 2) {
                                l->pl_flags |= PLCHEAP;
                                /* Next person to take this planet will */
                                /* get limited credit */
                            }
                            l->pl_owner = NOBODY;

#ifdef LTD_STATS
                            /* update planets stat
                               j = player who just destroyed the planet
                               l = planet that is neutral */

                            if (status->tourn)
                            {
                                ltd_update_planets(j, l);
                            }

#endif /* LTD_STATS */

                            /* out of Tmode?  Terminate.  8/2/91 TC */

                            if (NotTmode(ticks)
#ifdef PRETSERVER
                                && !bot_in_game
#endif
                                ) {
                                if ((l->pl_flags & (FED|ROM|KLI|ORI)) !=
                                    j->p_team) /* not in home quad, give them an extra one 8/26/91 TC */
                                    rescue(STERMINATOR, j->p_no);
                                rescue(STERMINATOR, j->p_no);
                            }
                            checkgen(oldowner, j);
                        }
                    }
                    else {      /* planet taken over */
                        l->pl_armies++;
                        j->p_planets++;
                        j->p_genoplanets++;



#ifndef LTD_STATS

                        if (status->tourn && !(l->pl_flags & PLCHEAP)) {
#ifdef NEW_CREDIT
                            j->p_stats.st_tplanets+=2;
                            status->planets+=2;
#else /* NEW_CREDIT */
                            j->p_stats.st_tplanets++;
                            status->planets++;
#endif /* NEW_CREDIT */
                            if ((l->pl_flags & PLCORE) && 
                              !(j->p_team & l->pl_flags) &&
                              (realNumShips(ALLTEAM & l->pl_flags)>2)) {
#ifdef NEW_CREDIT
                                j->p_stats.st_tplanets+=2;
                                status->planets+=2;
#else /* NEW_CREDIT */
                                j->p_stats.st_tplanets++;
                                status->planets++;
#endif /* NEW_CREDIT */
                            }
                        } else {
#ifdef NEW_CREDIT
                            j->p_stats.st_planets+=2;
#else /* NEW_CREDIT */
                            j->p_stats.st_planets++;
#endif /* NEW_CREDIT */
                        }
#endif /* LTD_STATS */

                        l->pl_flags &= ~PLCHEAP;


#ifndef NEW_CREDIT
                        j->p_kills += 0.25;

#ifndef LTD_STATS
                        checkmaxkills(i);
#endif /* LTD_STATS */

#endif /* NEW_CREDIT */

                        l->pl_owner = j->p_team;
                        l->pl_info = j->p_team;
#ifdef LTD_STATS
                        /* LTD is valid only for tourn mode */

                        if (status->tourn)
                        {

                            status->planets++;

                            /* credit the player for beamdown
                               j = player who just beamed down 1 army
                               l = planet that was just taken over,
                                   has 1 friendly army */

                            ltd_update_armies(j, l);


                            /* update planet stats
                               j = planet taker
                               l = planet just taken over (has 1 friendly
                               army) */

                            ltd_update_planets(j, l);

                        }

                        /* update the max kills regardless of tourn mode */

                        ltd_update_kills_max(j);

#endif /* LTD_STATS */

                        if (checkwin(j)) return;

                        /* now tell new team */
                        (void) sprintf(buf1, "%-3s->%-3s",
                                       l->pl_name, teamshort[l->pl_owner]);
                        arg[0] = DMTAKE; /* type */
                        arg[1] = l->pl_no;
                        arg[2] = j->p_no;
                        pmessage(l->pl_owner, MTEAM | MTAKE, buf1,
                                "%s taken over by %s",
                                l->pl_name,
                                j->p_longname);
                    }
                }

                /* planet is friendly, j->pteam == l->pl_owner */
                else {

#ifndef LTD_STATS

                    if (status->tourn && l->pl_armies < 4 && 
                          j->p_ship.s_type!=STARBASE) {
                        j->p_stats.st_tarmsbomb+=(4 - l->pl_armies);
                        status->armsbomb+=(4 - l->pl_armies);
                    }
#endif /* LTD_STATS */

                    j->p_armies--;
                    l->pl_armies++;

#ifdef LTD_STATS
                    /* update ltd stats
                       j = player who just dropped 1 army
                       l = friendly planet that just got army++, army > 1
                           because if army == 1, it was neutral before
                           the drop */

                    if (status->tourn) {
                        ltd_update_armies(j, l);
                    }
#endif /* LTD_STATS */

                }
            } else if (j->p_flags & PFDOCK) {
                struct player *base = bay_owner(j);
                if (base->p_team != j->p_team)
                    continue;
                if (base->p_armies >= base->p_ship.s_maxarmies) {
                    continue;
                } else {
                    army_track(AMT_TRANSDOWN, j, base, 1);
                    j->p_armies--;
                    base->p_armies ++;
#ifdef LTD_STATS
                    if (status->tourn) {
                        ltd_update_armies_ferried(j, base);
                    }
#endif /* LTD_STATS */
                        
                }
            }
        }

    }
}

static void blowup(struct player *sh)
{
    register int i;
    int dx, dy, dist;
    int damage;
    register struct player *j;
#ifdef CHAIN_REACTION /* isae - chain reaction */
    register struct player *k;
#endif
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PALIVE)
            continue;
        if (sh == j)
            continue;
        /* the following keeps people from blowing up on others */
        me = sh;
        if ((me->p_whydead == KQUIT) && (friendlyPlayer(j)))
            continue;
        /* isae -- nuked/ejected players do no damage */ 
        if ((me->p_whydead == KGHOST) || (me->p_whydead == KPROVIDENCE)) 
          /* ghostbusted ships do no damage 7/19/92 TC */
            continue;
        dx = sh->p_x - j->p_x;
        dy = sh->p_y - j->p_y;
        if (ABS(dx) > SHIPDAMDIST || ABS(dy) > SHIPDAMDIST)
            continue;
        dist = dx * dx + dy * dy;
        if (dist > SHIPDAMDIST * SHIPDAMDIST)
            continue;
        if (dist > EXPDIST * EXPDIST) {
            if (sh->p_ship.s_type == STARBASE)
                damage = 200 * (SHIPDAMDIST - sqrt((double) dist)) /
                    (SHIPDAMDIST - EXPDIST);
            else if (sh->p_ship.s_type == SCOUT)
                damage = 75 * (SHIPDAMDIST - sqrt((double) dist)) /
                    (SHIPDAMDIST - EXPDIST);
            else
                damage = 100 * (SHIPDAMDIST - sqrt((double) dist)) /
                    (SHIPDAMDIST - EXPDIST);
        }
        else {
            if (sh->p_ship.s_type == STARBASE)
                damage = 200;
            else if (sh->p_ship.s_type == SCOUT)
                damage = 75;
            else
                damage = 100;
        }
        if (damage > 0) {
            if (j->p_flags & PFSHIELD) {
                j->p_shield -= damage;
                if (j->p_shield < 0) {
                    j->p_damage -= j->p_shield;
                    j->p_shield = 0;
                }
            }
            else {
                j->p_damage += damage;
            }
            if (j->p_damage >= j->p_ship.s_maxdamage) {
                j->p_status = PEXPLODE;
                if (j->p_ship.s_type == STARBASE)
                    j->p_explode = 2*SBEXPVIEWS;
                else
                    j->p_explode = 10;
#ifdef CHAIN_REACTION
                if ((j->p_no != sh->p_whodead) && (j->p_team != players[sh->p_whodead].p_team)
                    && (sh->p_whydead == KSHIP ||  sh->p_whydead == KTORP ||
                        sh->p_whydead == KPHASER || sh->p_whydead == KPLASMA))
                  k= &players[sh->p_whodead]; 
                else
                  k= sh;
                if ((k->p_team != j->p_team) ||
                    ((k->p_team == j->p_team) && !(j->p_flags & PFPRACTR))){
                  k->p_kills += 1.0 + 
                    j->p_armies * 0.1 + j->p_kills * 0.1;
#ifdef LTD_STATS
                  ltd_update_kills_max(k);
#else /* LTD_STATS */
                  checkmaxkills(k->p_no);
#endif /* LTD_STATS */
                }

#ifndef LTD_STATS
                killerstats(k->p_no, sh->p_no, j);
                loserstats(i);
#endif /* LTD_STATS */

                j->p_whydead = KSHIP;
                j->p_whodead = k->p_no;

#ifdef LTD_STATS

                if (status->tourn) {

                  status->kills++;
                  status->losses++;

                  /* k = killer to be credited
                     sh = ship explosion that dealt the killing damage
                     j = victim */

                  ltd_update_kills(k, sh, j);

                  /* j = victim
                     k = killer that was credited */

                  ltd_update_deaths(j, k);

                }

#endif /* LTD_STATS */

                killmess(j, k, sh, (k->p_no == sh->p_no)? KSHIP : KSHIP2);
#else /* CHAIN_REACTION */
                sh->p_kills += 1.0 + 
                  j->p_armies * 0.1 + j->p_kills * 0.1;

#ifndef LTD_STATS

                killerstats(sh->p_no, j);
                checkmaxkills(sh->p_no);
                loserstats(i);

#endif /* LTD_STATS */

                j->p_whydead = KSHIP;
                j->p_whodead = sh->p_no;

#ifdef LTD_STATS

                if (status->tourn) {

                  status->kills++;
                  status->losses++;

                  /* sh = killer to be credited
                     sh = killer that did the damage
                     j = victim */

                  ltd_update_kills(sh, sh, j);

                  /* j = victim
                     sh = killer that was credited */

                  ltd_update_deaths(j, sh);

                }
#endif /* LTD_STATS */

                killmess(j, sh);

#endif /* CHAIN REACTION */
            }
        }
    }
}

static void exitDaemon(int sig)
{
    register int i;
    register struct player *j;

    if (sig) HANDLE_SIG(sig,exitDaemon);
    if (sig!=SIGINT) {
        if(sig) {
            ERROR(2,("exitDaemon: got signal %d\n", sig));
            fflush(stderr);
        }

        if (sig == SIGFPE) return;  /* Too many errors occur ... ignore them. */

        /* some signals can be ignored */
        if (sig == SIGCHLD || sig == SIGCONT)
            return;

        if (sig) {
            ERROR(1,( "exitDaemon: going down on signal %d\n", sig));
            fflush(stderr);
        }
    }

    /* if problem occured before shared memory setup */
    if (!players)
        exit(0);

    /* Blow players out of the game */
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE) continue;

        j->p_status = PEXPLODE;
        j->p_whydead = KDAEMON;
        j->p_ntorp = 0;
        j->p_nplasmatorp = 0;
        j->p_explode = 600/PLAYERFUSE; /* ghost buster was leaving players in */
        if (j->p_process > 1) {
            ERROR(8,("%s: sending SIGTERM\n", j->p_mapchars));
            (void) kill (j->p_process, SIGTERM);
        }
    }

    status->gameup &= ~(GU_GAMEOK); /* say goodbye to xsg et. al. 4/10/92 TC */
    /* Kill waiting players */
    for (i=0; i<MAXQUEUE; i++) queues[i].q_flags=0;
    save_planets();
    solicit(0);
#if !(defined(SCO) || defined(linux))
    sleep(2);
#endif
    if (!removemem())
        ERROR(1,("exitDaemon: cannot removed shared memory segment"));

#ifdef PUCK_FIRST
    if(pucksem_id != -1)
        if (semctl(pucksem_id, 0, IPC_RMID, pucksem_arg) == -1)
          ERROR(1,("exitDaemon: cannot remove puck semaphore"));
#endif /*PUCK_FIRST*/

    switch(sig){
        case SIGSEGV:
        case SIGBUS:
        case SIGILL:
        case SIGFPE:
#ifdef SIGSYS
        case SIGSYS:
#endif
            (void) SIGNAL(SIGABRT, SIG_DFL);
            abort();       /* get the coredump */
        default:
            break;
    }
#ifdef ONCHECK
    execl("/bin/rm", "rm", "-f", On_File, 0);
#endif
    exit(0);
}

static void save_planets(void)
{
    int plfd, glfd;

#ifdef NEUTRAL
    plfd = open(NeutFile, O_RDWR|O_CREAT, 0744);
#else
    plfd = open(PlFile, O_RDWR|O_CREAT, 0744);
#endif

    glfd = open(Global, O_RDWR|O_CREAT, 0744);

    if (plfd >= 0) {
        (void) lseek(plfd, (off_t) 0, 0);
        (void) write(plfd, (char *) planets, sizeof(pdata));
        (void) close(plfd);
    }
    if (glfd >= 0) {
        (void) lseek(glfd, (off_t) 0, 0);
        (void) write(glfd, (char *) status, sizeof(struct status));
        (void) close(glfd);
    }
}

static void check_load(void)
{
    FILE *fp;
    char buf[100];
    char *s;
    float load;

  if (!loadcheck)
        return;

  if (fork() == 0) {
    fp=popen(UPTIME, "r");
    if (fp==NULL) {
        exit(0);
    }
    fgets(buf, 100, fp);
    s=RINDEX(buf, ':');
    if (s==NULL) {
        SIGNAL(SIGCHLD, SIG_DFL);
        pclose(fp);
        SIGNAL(SIGCHLD, reaper);
        exit(0);
    }
    if (sscanf(s+1, " %f", &load) == 1) {
        if (load>=maxload && (status->gameup & GU_GAMEOK)) {
            status->gameup&=~(GU_GAMEOK);
            pmessage(0, MALL, "GOD->ALL",
                "The load is %f, this game is going down", load);
        } else if (load<maxload && !(status->gameup & GU_GAMEOK)) {
            status->gameup|=GU_GAMEOK;
            pmessage(0, MALL, "GOD->ALL",
                "The load is %f, this game is coming up", load);
        }
        else {
            s[strlen(s)-1]='\0';
            pmessage(0, MALL, "GOD->ALL","Load check%s", s);
        }
    } else {
        pmessage(0, MALL, "GOD->ALL","Load check failed :-(");
    }
    SIGNAL(SIGCHLD, SIG_DFL);
    pclose(fp);
    SIGNAL(SIGCHLD, reaper);
    exit(0);
  }
}

/* This function checks to see if a team has been genocided --
   their last planet has been beamed down to zero.  It will set
   a timer on their home planet that will prevent them from
   couping for a random number of minutes.
*/
static void checkgen(int loser, struct player *winner)
{
    register int i;
    register struct planet *l;
    struct planet *homep = NULL;
    struct player *j;

    /* let the inl robot check for geno if in INL mode */
    if (inl_mode) return;

    for (i = 0; i <= MAXTEAM; i++) /* maint: was "<" 6/22/92 TC */
        teams[i].s_plcount = 0;

    for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
        teams[l->pl_owner].s_plcount++; /* for now, recompute here */
/*      if (l->pl_owner == loser)
            return;*/
        if (((l->pl_flags & ALLTEAM) == loser) && (l->pl_flags & PLHOME))
            homep = l;          /* Keep track of his home planet for marking */
    }

    /* check to initiate surrender */

    if ((teams[loser].s_surrender == 0) &&
        (teams[loser].s_plcount == surrenderStart)) {
        pmessage(0, MALL, " ", " ");
        pmessage(0, MALL, "GOD->ALL",
                "The %s %s %d minutes before the empire collapses.",
                team_name(loser), team_verb(loser),
                binconfirm ? SURRLENGTH : SURRLENGTH*2/3);
        pmessage(0, MALL, "GOD->ALL",
                "%d planets are needed to sustain the empire.",
                SURREND);
        pmessage(0, MALL, " ", " ");

        teams[loser].s_surrender = binconfirm ? /* shorter for borgs? */
            SURRLENGTH : SURRLENGTH*2/3; /* start the clock 1/27/92 TC */
    }

    if (teams[loser].s_plcount > 0) return;

    teams[loser].s_surrender = 0;       /* stop the clock */

            /* Give extra credit for neutralizing last planet! */
    winner->p_genoplanets++;
    winner->p_planets++;
#ifndef LTD_STATS       /* NEW_CREDIT is incompatible with LTD_STATS */
                        /* LTD_STATS does not give credit for timercide
                           or geno. */
#ifdef NEW_CREDIT
    if (status->tourn) {
        winner->p_stats.st_tplanets+=2;
        status->planets+=3;
    } else {
        winner->p_stats.st_planets+=2;
    }
#else
    if (status->tourn) {
        winner->p_stats.st_tplanets++;
        status->planets++;
    } else {
        winner->p_stats.st_planets++;
    }
#endif /* NEW_CREDIT */
#endif /* LTD_STATS */

    if (checkwin(winner)) return;  /* conquer is higher priority, but
                                   genocide is a necessary condition
                                   3/25/92 TC */

    /* Tell winning team what wonderful people they are (pat on the back!) */
    genocideMessage(loser, winner->p_team);
    /* Give more troops to winning team? */
    addTroops(loser, winner->p_team);

    /* kick out all the players on that team */
    /* makes it easy for them to come in as a different team */
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
        if (j->p_team == loser) {
            j->p_genoplanets=0;
            j->p_genoarmsbomb=0;
            j->p_armsbomb=0;
            j->p_planets=0;
        }
        if (j->p_status == PALIVE && j->p_team == loser) {
            j->p_status = PEXPLODE;
            if (j->p_ship.s_type == STARBASE)
                j->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
            else
                j->p_explode = 10/PLAYERFUSE;
            j->p_whydead = KGENOCIDE;
            j->p_whodead = winner->p_no;
        }
    }

    /* well, I guess they are losers.  Hose them now */
    homep->pl_couptime = MIN_COUP_TIME +
        (random() % (MAX_COUP_TIME - MIN_COUP_TIME));

    /* removed above dependency on PLANETFUSE and hardcoded */
    /* constants (now in defs.h).  30-60 min for a coup.  4/15/92 TC */
}

/* This function is called when a planet has been taken over.
   It checks all the planets to see if the victory conditions
   are right.  If so, it blows everyone out of the game and
   resets the galaxy
*/

/* Now also called by checkgen after a planet has been neutralized; this
   is somewhat inefficient but it works. 3/25/92 TC */

static int checkwin(struct player *winner)
{
    register int i, h;
    register struct planet *l;
    int team[MAXTEAM + 1];
    int teamtemp[MAXTEAM + 1];

    /* let the inl robot check for geno if in INL mode */
    if (inl_mode) return 0;

    teams[0].s_plcount = 0;
    for (i = 0; i < 4; i++) {
        team[1<<i] = 0;
        teams[1<<i].s_plcount = 0;
    }
    
    /* count the number of home systems owned by one team */
    for (i = 0; i < 4; i++) {
        teamtemp[NOBODY] = teamtemp[FED] = teamtemp[ROM] = teamtemp[KLI] =
            teamtemp[ORI] = 0;

        for (h = 0, l = &planets[i*10]; h < 10; h++, l++) {
            teams[l->pl_owner].s_plcount++;
            teamtemp[l->pl_owner]++;
        }

        /* count neutral planets towards conquer 3/24/92 TC */

        if ((teamtemp[FED]+teamtemp[ROM]+teamtemp[KLI]) == 0) team[ORI]++;
        else if ((teamtemp[FED]+teamtemp[ROM]+teamtemp[ORI]) == 0) team[KLI]++;
        else if ((teamtemp[FED]+teamtemp[KLI]+teamtemp[ORI]) == 0) team[ROM]++;
        else if ((teamtemp[ROM]+teamtemp[KLI]+teamtemp[ORI]) == 0) team[FED]++;
    }

    /* check to end surrender */

    if ((teams[winner->p_team].s_surrender > 0) &&
             (teams[winner->p_team].s_plcount == SURREND)) {
        pmessage(0, MALL, " ", " ");
        pmessage(0, MALL, "GOD->ALL",
                "The %s %s prevented collapse of the empire.",
                team_name(winner->p_team), team_verb(winner->p_team));
        pmessage(0, MALL, " ", " ");
        teams[winner->p_team].s_surrender = 0; /* stop the clock */
    }

    for (i = 0; i < 4; i++) {
        if (team[1<<i] >= VICTORY) {
            conquerMessage(winner->p_team);
            conquer_begin(winner);
            return 1;
        }
    }
    return 0;
}

static void ghostmess(struct player *victim, char *reason)
{
    static float        ghostkills = 0.0;
    int                 i,k;

    ghostkills += 1.0 + victim->p_armies * 0.1 + victim->p_kills * 0.1;
                arg[0] = DGHOSTKILL; /* type */
                arg[1] = victim->p_no;
                arg[2] = 100 * ghostkills;
    pmessage(0, MALL, "GOD->ALL",
        "%s was kill %0.2f for the GhostBusters, %s",
        victim->p_longname, ghostkills, reason);
#ifdef OBSERVERS
    /* if ghostbusting an observer do not attempt carried army rescue */
    if (victim->p_status == POBSERV) return;
#endif
    if (victim->p_armies > 0) {
        k = 10*(remap[victim->p_team]-1);
        if (k >= 0 && k <= 30) for (i=0; i<10; i++) {
            if (planets[i+k].pl_owner == victim->p_team) {
                pmessage(0, MALL | MGHOST, "GOD->ALL",
                    "%s's %d armies placed on %s",
                    victim->p_name, victim->p_armies, planets[k+i].pl_name);
                planets[i+k].pl_armies += victim->p_armies;
                victim->p_armies = 0;
                break;
            }
        }
    }
}

static void saveplayer(struct player *victim)
{
    int fd;

    if (victim->p_pos < 0) return;
    if (victim->p_stats.st_lastlogin == 0) return;
    if (victim->p_flags & PFROBOT) return;

    fd = open(PlayerFile, O_WRONLY, 0644);
    if (fd >= 0) {
        lseek(fd, victim->p_pos * sizeof(struct statentry) + 
              offsetof(struct statentry, stats), SEEK_SET);
        write(fd, (char *) &victim->p_stats, sizeof(struct stats));
        close(fd);
    }
}

/* NBT 2/20/93 */

char *whydeadmess[] = { 
  " ", 
  "[quit]",            /* KQUIT        */
  "[photon]",          /* KTORP        */   
  "[phaser]",          /* KPHASER      */
  "[planet]",          /* KPLANET      */
  "[explosion]",       /* KSHIP        */
  " ",                 /* KDAEMON      */
  " ",                 /* KWINNER      */
  "[ghostbust]",       /* KGHOST       */
  "[genocide]",        /* KGENOCIDE    */
  " ",                 /* KPROVIDENCE  */
  "[plasma]",          /* KPLASMA      */
  "[tournament end]",  /* TOURNEND     */
  "[game over]",       /* KOVER        */
  "[tournament start]", /* TOURNSTART   */
  "[bad binary]",      /* KBADBIN      */
  "[detted photon]",   /* KTORP2       */
  "[chain explosion]", /* KSHIP2       */
#ifdef WINSMACK
  "[zapped plasma]",   /* KPLASMA2     */
#endif
  " "};
#if 0
#define numwhymess (sizeof(whydeadmess)/sizeof(char *))
#endif

/*
 *
 */
#ifdef CHAIN_REACTION
static void killmess(struct player *victim, struct player *credit_killer,
              struct player *k, int whydead)
#else
static void killmess(struct player *victim, struct player *k)
#endif
{
    char *why;
    char kills[20];

#ifndef CHAIN_REACTION
    struct player *credit_killer;
    int whydead;

    whydead = victim->p_whydead;
    credit_killer = k;
#else
    if (credit_killer->p_team == k->p_team)
      credit_killer = k;
#endif

    why = whydeadmess[whydead];

    sprintf(kills, "%0.2f", k->p_kills);

    if ((victim->p_team == k->p_team) && !(victim->p_flags & PFPRACTR)) {
      strcpy(kills, "NO CREDIT");
    }

    if (victim->p_armies == 0) {
        arg[0] = DMKILL; /* type */
        arg[1] = victim->p_no;
        arg[2] = k->p_no;
        arg[3] = 100 * k->p_kills;
        arg[4] = victim->p_armies;
        arg[5] = whydead;
        if (inl_mode) {
            pmessage(0, MALL | MKILL, "GOD->ALL", 
                "%s {%s} was kill %s for %s {%s} %s",
                 victim->p_longname,
                 shiptypes[victim->p_ship.s_type],
                 kills,
                 k->p_longname,
                 shiptypes[k->p_ship.s_type],
                 why);
        } else {
            pmessage(0, MALL | MKILL, "GOD->ALL", 
                "%s was kill %s for %s %s",
                 victim->p_longname,
                 kills,
                 k->p_longname,
                 why);
        }
        if(k != credit_killer){
            pmessage(0, MALL | MKILL, "GOD->ALL",
                "  Credit for %s awarded to %s, kill %0.2f",
                       victim->p_mapchars,
                       credit_killer->p_longname,
                       credit_killer->p_kills);
      }
    } else {
        arg[0] = DMKILL; /* type */
        arg[1] = victim->p_no;
        arg[2] = k->p_no;
        arg[3] = 100 * k->p_kills;
        arg[4] = victim->p_armies;
        arg[5] = whydead;
        if (inl_mode) {
          pmessage(0, MALL | MKILLA, "GOD->ALL",
                   "%s (%s+%d) {%s} was kill %s for %s {%s} %s",
                   victim->p_name,
                   victim->p_mapchars,
                   victim->p_armies,
                   shiptypes[victim->p_ship.s_type],
                   kills,
                   k->p_longname,
                   shiptypes[k->p_ship.s_type],
                   why);
        } else {
          pmessage(0, MALL | MKILLA, "GOD->ALL",
                   "%s (%s+%d armies) was kill %s for %s %s",
                   victim->p_name,
                   victim->p_mapchars,
                   victim->p_armies,
                   kills,
                   k->p_longname,
                   why);
        }
        if(k != credit_killer){
            pmessage(0, MALL | MKILLA, "GOD->ALL", 
                "Credit for %s+%d arm%s awarded to %s, kill %0.2f",
                       victim->p_mapchars,
                       victim->p_armies,
                       victim->p_armies > 1?"ies":"y",
                       credit_killer->p_longname,
                       credit_killer->p_kills);
        }

        /*
         * Auto-doosher: 
         * Will send a message to all when a player is killed with
         * armies. Where did 'doosh' come from?  2/19/93 NBT
         */
        if (doosher) {
            if (victim->p_ship.s_type == STARBASE)
                pmessage2(0,MALL | MKILLA,"GULP!!",DOOSHMSG, " ");
            else if ((victim->p_armies<4) && (victim->p_armies>0))
                pmessage2(0,MALL | MKILLA, "doosh",DOOSHMSG," ");
            else if (victim->p_armies<8)
                pmessage2(0,MALL | MKILLA, "Doosh",DOOSHMSG," ");
            else pmessage2(0,MALL | MKILLA,"DOOSH!",DOOSHMSG," ");
        }
    }
}


/* Send in a robot to avenge the aggrieved team */
/* -1 for HK, -2 for Terminator, -3 for sticky Terminator */

/* if team in { FED, ROM, KLI, ORI }, a nonzero target means "fleet" mode */
static void rescue(int team, int target)
{
    char *argv[6];      /* leave room for worst case #args + NULL */
    u_int argc = 0;
    int pid;

    if ((pid = vfork()) == 0) {
        if (!debug) {
            (void) close(0);
            (void) close(1);
            (void) close(2);
        }
        alarm_prevent_inheritance();
        argv[argc++] = "robot";
        switch (team) {
          case FED:
            argv[argc++] = "-Tf";
            break;
          case ROM:
            argv[argc++] = "-Tr";
            break;
          case KLI:
            argv[argc++] = "-Tk";
            break;
          case ORI:
            argv[argc++] = "-To";
            break;
          case HUNTERKILLER:
            argv[argc++] = "-Ti";       /* -1 means independent robot */
            argv[argc++] = "-P";
            break;
          case TERMINATOR:      /* Terminator */
          case STERMINATOR:     /* sticky Terminator */
            {
            static char targ[5];
            sprintf(targ, "-Tt%c", players[target].p_mapchars[1]);
            argv[argc++] = targ;
            }
            if (team == STERMINATOR)
                argv[argc++] = "-s";
            break;
        }
        if (target > 0)         /* be fleet 8/28/91 TC */
                argv[argc++] = "-f";
        if (debug)
                argv[argc++] =  "-d";
        argv[argc] = (char *) 0;
        execv(Robot, argv);
        perror("execv(Robot)");
        fflush(stderr);
        _exit(1);                /* NBT exit just in case a robot couldn't
                                    be started                              */
    }
    else if (debug) {
            ERROR(1,( "Forking robot: pid is %d\n", pid));
    }
}

#include <sys/resource.h>

/* Don't fear the ... */

static void reaper(int sig)
{
    static int status;
    static int pid;

    while ((pid = WAIT3(&status, WNOHANG, 0)) > 0) {
        if (debug) {
            ERROR(1,( "Reaping: pid is %d (status: %d)\n",
                    pid, status));
        }
    }

    HANDLE_SIG(SIGCHLD, reaper);

    /* get rid of annoying compiler warning */
    if (sig) return;
}



/* Give more troops to winning team in case they need them. */
static void addTroops(int loser, int winner)
{
    int count=0;
    int i;
    char addrbuf[10];

    for (i=0; i<MAXPLAYER; i++) {
        if (players[i].p_status != PFREE && players[i].p_team == loser) count++;
    }
    /* Never give armies if 0-2 defenders */
    if (count<3) return;
    /* Sometimes give armies if 3-5 defenders */
    if (count<6 && (random() % count)==0) return;
    /* Always give armies if 6+ defenders */
    sprintf(addrbuf, "GOD->%s ", teamshort[winner]);
    pmessage(winner, MTEAM, addrbuf, 
        "The recent victory has boosted enrollment!");
    for (i=0; i<MAXPLANETS; i++) { 
        if (planets[i].pl_owner==winner) {
            planets[i].pl_armies+=random() % (2*count);
        }
    }
}

static void surrenderMessage(int loser)
{
    char buf[MSG_LEN];
    FILE *conqfile;
    time_t curtime;

    conqfile=fopen(ConqFile, "a");
    if (conqfile == NULL) conqfile = stderr;

/* TC 10/90 */
    time(&curtime);
    strcpy(buf,"\nSurrender! ");
    strcat(buf, ctime(&curtime));
    fprintf(conqfile,"  %s\n",buf);

    sprintf(buf, "The %s %s surrendered.", team_name(loser),
        team_verb(loser));
        
    fprintf(conqfile, "  %s\n", buf);
    fprintf(conqfile, "\n");
    if (conqfile != stderr) fclose(conqfile);
}

static void genocideMessage(int loser, int winner)
{
    char buf[MSG_LEN];
    FILE *conqfile;
    time_t curtime;

    conqfile=fopen(ConqFile, "a");
    if (conqfile == NULL) conqfile = stderr;

/* TC 10/90 */
    time(&curtime);
    strcpy(buf,"\nGenocide! ");
    strcat(buf, ctime(&curtime));
    fprintf(conqfile,"  %s\n",buf);

    pmessage(0, MALL | MGENO, " ","%s",
        "***********************************************************");
    sprintf(buf, "The %s %s been genocided by the %s.", team_name(loser),
        team_verb(loser), team_name(winner));
    pmessage(0, MALL | MGENO, " ","%s",buf);
        
    fprintf(conqfile, "  %s\n", buf);
    sprintf(buf, "The %s:", team_name(winner));
    pmessage(0, MALL | MGENO, " ","%s",buf);
    fprintf(conqfile, "  %s\n", buf);
    displayBest(conqfile, winner, KGENOCIDE);
    sprintf(buf, "The %s:", team_name(loser));
    pmessage(0, MALL | MGENO, " ","%s",buf);
    fprintf(conqfile, "  %s\n", buf);
    displayBest(conqfile, loser, KGENOCIDE);
    pmessage(0, MALL | MGENO, " ","%s",
        "***********************************************************");
    fprintf(conqfile, "\n");
    if (conqfile != stderr) fclose(conqfile);
}

static void conquerMessage(int winner)
{
    char buf[MSG_LEN];
    FILE *conqfile;
    time_t curtime;

    conqfile=fopen(ConqFile, "a");
    if (conqfile == NULL) conqfile = stderr;

/* TC 10/90 */
    time(&curtime);
    strcpy(buf,"\nConquer! ");
    strcat(buf, ctime(&curtime));
    fprintf(conqfile,"  %s\n",buf);

    pmessage(0, MALL | MCONQ, " ","%s",
        "***********************************************************");
    sprintf(buf, "The galaxy has been conquered by the %s:", team_name(winner));
    pmessage(0, MALL | MCONQ, " ","%s",buf);
    fprintf(conqfile, "  %s\n", buf);
    sprintf(buf, "The %s:", team_name(winner));
    pmessage(0, MALL | MCONQ, " ","%s",buf);
    fprintf(conqfile, "  %s\n", buf);
    displayBest(conqfile, winner, KWINNER);
    pmessage(0, MALL | MCONQ, " ","%s",
        "***********************************************************");
    fprintf(conqfile, "\n");
    if (conqfile != stderr) fclose(conqfile);
}

typedef struct playerlist {
    char name[NAME_LEN];
    char mapchars[2];
    int planets, armies;
} Players;

static void displayBest(FILE *conqfile, int team, int type)
{
    register int i,k,l;
    register struct player *j;
    int planets, armies;
    Players winners[MAXPLAYER+1];
    char buf[MSG_LEN];
    int number;

    number=0;
    MZERO(winners, sizeof(Players) * (MAXPLAYER+1));
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
        if (j->p_team != team || j->p_status == PFREE) continue;
#ifdef GENO_COUNT
        if (type == KWINNER) j->p_stats.st_genos++;
#endif
        if (type == KGENOCIDE) {
            planets=j->p_genoplanets;
            armies=j->p_genoarmsbomb;
            j->p_genoplanets=0;
            j->p_genoarmsbomb=0;
        } else {
            planets=j->p_planets;
            armies=j->p_armsbomb;
        }
        for (k = 0; k < number; k++) {
            if (30 * winners[k].planets + winners[k].armies < 
                30 * planets + armies) {
                /* insert person at location k */
                break;
            }
        }
        for (l = number; l >= k; l--) {
            winners[l+1] = winners[l];
        }
        number++;
        winners[k].planets=planets;
        winners[k].armies=armies;
        STRNCPY(winners[k].mapchars, j->p_mapchars, 2);
        STRNCPY(winners[k].name, j->p_name, NAME_LEN);
        winners[k].name[NAME_LEN-1]=0;  /* `Just in case' paranoia */
    }
    for (k=0; k < number; k++) {
        if (winners[k].planets != 0 || winners[k].armies != 0) {
            sprintf(buf, "  %16s (%2.2s) with %d planets and %d armies.", 
                winners[k].name, winners[k].mapchars, winners[k].planets, winners[k].armies);
            pmessage(0, MALL | MCONQ, " ",buf);
            fprintf(conqfile, "  %s\n", buf);
        }
    }
    return;
}

static void fork_robot(int robot)
{
   if ((robot_pid=vfork()) == 0) {
      alarm_prevent_inheritance();
      switch(robot) {
        case NO_ROBOT: break;
#ifdef BASEPRACTICE
        case BASEP_ROBOT:
                execl(Basep, "basep", (char *) NULL);
                perror(Basep);
                break;
#endif
#ifdef NEWBIESERVER
        case NEWBIE_ROBOT:
                execl(Newbie, "newbie", (char *) NULL);
                perror(Newbie);
                break;
#endif
#ifdef PRETSERVER
        case PRET_ROBOT:
                execl(PreT, "pret", (char *) NULL);
                perror(PreT);
                break;
#endif
#ifdef DOGFIGHT
        case MARS_ROBOT:
                execl(Mars, "mars", (char *) NULL);
                perror(Mars);
                break;
#endif
        case PUCK_ROBOT:
                execl(Puck, "puck", (char *) NULL);
                perror(Puck);
                break;
        case INL_ROBOT:
                execl(Inl, "inl", (char *) NULL);
                perror(Inl);
                break;
        default: ERROR(1,( "Unknown Robot: %d\n", robot ));
      }
      _exit(1);
   }
}

/* maximum number of agris that can exist in one quadrant */

#define AGRI_LIMIT 3

#ifdef INL_RESOURCES /* isae -- INL planet resources */
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
#else
void doResources(void)
{
    int i;
    int agricount[4];
    do {
        MCOPY(pdata, planets, sizeof(pdata)); /* bug, was before do 2/12/92 TMC */
        for (i = 0; i< 40; i++) {
            planets[i].pl_armies = top_armies;
        }
        agricount[0] = 0;
        agricount[1] = 0;
        agricount[2] = 0;
        agricount[3] = 0;

        for (i = 0; i < 40; i++) {
            /*  if (planets[i].pl_flags & PLHOME)
                planets[i].pl_flags |= (PLREPAIR|PLFUEL|PLAGRI);*/
            if (random() % 4 == 0)
                planets[i].pl_flags |= PLREPAIR;
            if (random() % 2 == 0)
                planets[i].pl_flags |= PLFUEL;
            if (random() % 8 == 0) {
                planets[i].pl_flags |= PLAGRI; agricount[i/10]++;
            }
        }
    } while ((agricount[0] > AGRI_LIMIT) || /* bug: used && 1/23/92 TC */
             (agricount[1] > AGRI_LIMIT) ||
             (agricount[2] > AGRI_LIMIT) ||
             (agricount[3] > AGRI_LIMIT));
}
#endif /* INL_RESOURCES */

#ifdef nodef
static void doRotateGalaxy(void)
{
   int i, ox, oy;
   static int degree=1;
   
   if (degree++>3) degree=0;
   for (i=0; i<40; i++){
     ox = planets[i].pl_x;
     oy = planets[i].pl_y;
     switch(degree){
       case 0: 
         break;
       case 1:
        planets[i].pl_x = (GWIDTH/2) - oy + (GWIDTH/2);
        planets[i].pl_y = ox - (GWIDTH/2) + (GWIDTH/2);
        break;
       case 2:
        planets[i].pl_x = (GWIDTH/2) - ox + (GWIDTH/2);
        planets[i].pl_y = (GWIDTH/2) - oy + (GWIDTH/2);
        break;
       case 3:
        planets[i].pl_x = oy - (GWIDTH/2) + (GWIDTH/2);
        planets[i].pl_y = (GWIDTH/2) - ox + (GWIDTH/2);
        break;
       default: 
        break;
       }
   }
}
#endif
#ifdef PUCK_FIRST
void do_nuttin (int sig) { }

/* This has [should have] the daemon wait until the puck has finished by
   waiting on a semaphore.  Error logging is not entirely useful.
*/
static void signal_puck(void)
{
    int i;
    struct player *j;
    int puckwait = 0;
    
    for (i = 0, j = players; i < MAXPLAYER; i++, j++)
        if (j->p_status != PFREE && j->w_queue == QU_ROBOT &&
            strcmp(j->p_name, "Puck") == 0) 
        { 
            /* Set semaphore to 0 in case there are multiple pucks. Yuck. */
            if (pucksem_id != -1 &&
                semctl(pucksem_id, 0, SETVAL, pucksem_arg) == -1) 
            {
                perror("signal_puck semctl");
                pucksem_id = -1;
                /* are there any errors that would 'fix themselves?' */
            }
            if (alarm_send(j->p_process) < 0) 
            {
                if (errno == ESRCH) 
                {
                    ERROR(1,("daemonII/signal_puck: slot %d missing\n", i));
                    freeslot(j);
                }
            }
            else if (pucksem_id != -1) 
            {
                puckwait = 1;
            }
        }
    
    if (puckwait)
    {
        semop(pucksem_id, pucksem_op, 1);
    }
}
#endif /*PUCK_FIRST*/

/*
 * The minor problem here is that the only client update speeds that
 * make sense are 10, 5, 3, 2, 1.
 */

static void signal_servers(void)
{
  register int i, t;
  register struct player *j;
  static int counter;           /* need our own counter */

  counter++;

  for (i = 0, j = players; i < MAXPLAYER; i++, j++)
    {
      if (!j->p_status)
        continue;
      if (j->p_process <= 1)
        continue;

#ifdef PUCK_FIRST
      if (j->p_status != PFREE && j->w_queue == QU_ROBOT &&
          strcmp(j->p_name, "Puck") == 0)
      {
          continue; 
      }
#endif /*PUCK_FIRST*/

      t = j->p_timerdelay;
      if (!t)                   /* paranoia */
        t = 5;

      /* adding 'i' to 'counter' allows us to "interleave" update signals to
         the ntserv processes: at 5 u/s, 1/2 the processes are signalled on
         every update, at 2 u/s, 1/5 are signalled, etc.  (Of course, if
         everyone is at 10 u/s we have to signal them all every time) */

      if (t == 1 /*skip mod */  || (((counter + i) % t) == 0))
        {
          if (alarm_send(j->p_process) < 0)
            {
              /* if the ntserv process died what are we doing here? */
              if (errno == ESRCH) {
                ERROR(1,("daemonII/signal_servers: slot %d ntserv missing\n", i)); 
                freeslot(j);
                }
            }
        }
    }
}

void message_flag(struct message *cur, char *address)
{
            if(arg[0] != DINVALID){
                cur->args[0] = arg[0];
                cur->args[1] = arg[1];
                cur->args[2] = arg[2];
                cur->args[3] = arg[3];
                cur->args[4] = arg[4];
                cur->args[5] = arg[5];
        }
        else {
                if (strstr(address,"GOD->")!=NULL)
                        cur->args[0] = SVALID; /* It has a real header */
                else
                        cur->args[0] = SINVALID;
                        /* Marked as invalid to send over */
                                /* SP_S_WARNING  and SP_S_MESSAGE HW */
        }
        arg[0] = DINVALID;
}

/*  Hey Emacs!
 * Local Variables:
 * c-file-style:"bsd"
 * End:
 */
