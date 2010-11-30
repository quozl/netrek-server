/*! @file daemon.c
    @brief Netrek universe simulation.
    @details Runs the simulation of the Netrek universe, in a
    step-wise frame by frame sequence, keeping only the current frame,
    updating object positions, causing object to object effects, and
    signalling the per player server processes each update.  The
    daemon is started by the first per player server process and runs
    until all players have left or the game is terminated.

    The daemon is self-contained; it calls out to libnetrek functions,
    it forks new processes, but nothing calls the daemon.
    Communication from other processes to the daemon is to be via the
    shared memory segment.

    There is only one public function, main().  Everything else is
    to be declared static.
    */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "conquer.h"
#include "alarm.h"
#include "blog.h"
#include "util.h"
#include "slotmaint.h"
#include "advertise.h"
#include "draft.h"
#include "solicit.h"
#include "planet.h"

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
   int val;                           /* value for SETVAL */
   struct semid_ds *buf;              /* buffer for IPC_STAT, IPC_SET */
   unsigned short int *array;         /* array for GETALL, SETALL */
   struct seminfo *__buf;             /* buffer for IPC_INFO */
   };
#endif /*__GNU_LIBRARY__ && !__SEM_SEMUN_UNDEFINED */
#endif /* PUCK_FIRST */

/* fuse macro, true every so often, relative to frames per second and
time distortion value, only valid outside pause (since context->frame
must be incremented), base rate is in ticks and not frames */
#define fuse(X) ((context->frame % (X*fps/distortion)) == 0)
#define seconds_to_frames(X) ((X)*fps)
#define frames_to_seconds(X) ((X)/fps)
#define TOURNEXTENSION 15       /* Tmode gone for 15 seconds 8/26/91 TC */
#define NotTmode (!(status->tourn) && (frames_to_seconds(context->frame - context->frame_tourn_end) > TOURNEXTENSION))

#undef PMOVEMENT

#ifdef SBFUEL_FIX
#ifndef MIN
#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#endif
#endif

/* file scope prototypes */
static int is_tournament_mode(void);
static int check_scummers(int);
static void move();
static void udplayersight(void);
static void udplayers(void);
static void udships(void);
static void udplayers_pause(void);
static void changedir(struct player *sp);
static void udcloak(void);
static void udtwarp(void);
static void udtorps(void);
static void t_explosion(struct torp *torp);
static void p_explosion(struct player *player, int whydead, int whodead);
#ifndef LTD_STATS
static void loserstats(int pl);
static void killerstats(int cl, int pl, struct player *victim);
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
static void plrescue(struct planet *l);
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
static void killmess(struct player *victim, struct player *credit_killer,
                     struct player *k, int whydead);
static void reaper(int sig);
static void reaper_start();
static void addTroops(int loser, int winner);
static void surrenderMessage(int loser);
static void genocideMessage(int loser, int winner);
static void conquerMessage(int winner);
static void displayBest(FILE *conqfile, int team, int type);
static void fork_robot(int robot);
static void signal_servers(void);

#ifdef PUCK_FIRST
static void signal_puck(void);
#endif /* PUCK_FIRST */
static void message_flag(struct message *cur, char *address);

/* external scope prototypes */
extern void pinit(void);
extern void pmove(void);

/* global scope variables */

/* note: consider restart capability before extending these, it may be
   better to place new variables in struct memory */

static int opt_debug   = 0;             /*!< --debug                      */
static int opt_tell    = 0;             /*!< --tell                       */
static int opt_restart = 0;             /*!< --restart                    */

static int sig_restart = 0;             /*!< restart signal received      */

static int tcount[MAXTEAM + 1];         /*!< team count, planet rescue    */
static int arg[8];                      /*!< short message arg list       */

#ifdef PUCK_FIRST
static int pucksem_id;
static union semun pucksem_arg;
static struct sembuf pucksem_op[1];
#endif /* PUCK_FIRST */

int pl_warning[MAXPLANETS];     /*!< fuse, per planet, under attack msg */
int tm_robots[MAXTEAM + 1];     /*!< fuse, per team, non-t rescue robot */

/*! @brief Restart handler.
    @details Handles a SIGHUP signal by incrementing the restart
    signal received counter and restarting the handler for the next
    SIGHUP. */
static void restart_handler(int signum)
{
  sig_restart++;
  signal(SIGHUP, restart_handler);
}

/*! @brief Fork a script.
    @details Executes a specified script in a forked process.
    @param script the name of the script. */
static void fork_script(char *script)
{
  if (fork() == 0) {
    alarm_prevent_inheritance();
    execlp(script, script, NULL);
    perror(script);
    _exit(1);
  }
}

/*! @brief Main function.
    @details Runs the simulation, does not return, calls exit(3) on
    stop, exits via exec(3) on restart. During start, initialises the
    universe, and informs the parent process when initialisation has
    completed and regular simulation has begun. */
int main(int argc, char **argv)
{
    register int i;
    int x = 0;
    int glfd, plfd;

#ifdef AUTOMOTD
    time_t        mnow;
    struct stat   mstat;
#endif

    setlinebuf(stdout);
    ERROR(1,("daemon: begin, pid %d\n", getpid()));
    i = 1;
    for(i=1;argc>i;i++) {
        if (!strcmp(argv[i], "--debug")) { opt_debug = 1; continue; }
        if (!strcmp(argv[i], "--restart")) { opt_restart++; continue; }
        if (!strcmp(argv[i], "--tell")) { opt_tell++; continue; }
    }

    getpath();          /* added 11/6/92 DRG */
    srandom(getpid());
    if (!opt_debug) {
        for (i = 1; i < NSIG; i++) {
            (void) signal(i, exitDaemon);
        }
        signal(SIGSTOP, SIG_DFL); /* accept SIGSTOP? 3/6/92 TC */
        signal(SIGTSTP, SIG_DFL); /* accept SIGTSTP? 3/6/92 TC */
        signal(SIGCONT, SIG_DFL); /* accept SIGCONT? 3/6/92 TC */
        signal(SIGHUP, restart_handler);
    }
    reaper_start();

    /* set up the shared memory segment and attach to it */
    if (opt_restart) {
        if (openmem(-1) == 0) {
            ERROR(1,("daemon: restart failed, no existing memory segment\n"));
            opt_restart = 0;
        } else {
            if (context->daemon != getpid()) {
                if (kill(context->daemon, 0) == 0) {
                    ERROR(1,("daemon: restart failed, %d still exists\n", context->daemon));
                    opt_restart = 0;
                }
            }
        }
    }
    if (!opt_restart)
        if (!setupmem()) {
            exit(1);
        }

    do_message_pre_set(message_flag);
    do_message_force_daemon();
    readsysdefaults();
    if (opt_restart) {
        ERROR(1,("daemon: warm start\n"));
        goto restart;
    }

    ERROR(1,("daemon: cold start\n"));
    context->daemon = getpid();
    context->frame = context->frame_tourn_start = context->frame_tourn_end = 0;
    context->quorum[0] = context->quorum[1] = NOBODY;

    for (i = 0; i < MAXPLAYER; i++) {
        players[i].p_status = PFREE;
#ifdef LTD_STATS
        ltd_reset(&(players[i]));
#else
        players[i].p_stats.st_tticks=1;
#endif
        players[i].p_no=i;
        players[i].p_ups = defups;
        players[i].p_fpu = fps / defups;
        memset(players[i].voting, 0, sizeof(time_t) * PV_TOTAL);
    }

    if (resetgalaxy) {          /* added 2/6/93 NBT */
        pl_reset();
    } else {
        plfd = open(PlFile, O_RDWR, 0744);
        if (plfd < 0) {
            ERROR(1,("daemon: no planet file, restarting galaxy\n"));
            pl_reset();
        } else {
            int psize = sizeof(struct planet) * MAXPLANETS;
            if (read(plfd, (char *) planets, psize) != psize) {
                ERROR(1,("daemon: planet file bad size, restarting galaxy\n"));
                pl_reset();
            }
            close(plfd);
        }
    }

    glfd = open(Global, O_RDWR, 0744);
    if (glfd < 0) {
        ERROR(1,("daemon: no global file, resetting stats\n"));
        memset((char *) status, 0, sizeof(struct status));
    } else {
        if (read(glfd, (char *) status, sizeof(struct status)) !=
            sizeof(struct status)) {
            ERROR(1,("daemon: global file wrong size, resetting stats\n"));
            memset((char *) status, 0, sizeof(struct status));
        }
        close(glfd);
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

    memcpy(&context->start, status, sizeof(struct status));
    context->blog_pickup_game_full = 0;
    context->blog_pickup_queue_full = 0;
    context->conquer_trigger = 0;

#undef wait

    queues_init();  /* Initialize the queues for pickup */

    status->active = 0;
    status->gameup = (GU_GAMEOK | (chaosmode ? GU_CHAOS : 0));

#ifdef AUTOMOTD
   if(stat(Motd, &mstat) == 0 && (time(NULL) - mstat.st_mtime) > 60*60*12){
      fork_script(MakeMotd);
   }
#endif

   if (manager_type) fork_robot(manager_type);

 restart:

#ifdef PUCK_FIRST
    if ((pucksem_id = semget(PUCK_FIRST, 1, 0600 | IPC_CREAT)) != -1 ||
        (pucksem_id = semget(PUCK_FIRST, 1, 0600)) != -1) {
        pucksem_arg.val = 0;
        pucksem_op[0].sem_num = 0;
        pucksem_op[0].sem_op = -1;
        pucksem_op[0].sem_flg = 0;
    } else {
        ERROR(1,("daemon: unable to get puck semaphore."));
    }
#endif /* PUCK_FIRST */

    /* initialize the planet movement capabilities */
    pinit();

    alarm_init();
    alarm_setitimer(distortion, fps);

    /* signal parent ntserv that daemon is ready */
    if (opt_tell) kill(getppid(), SIGUSR1);

    sig_restart = 0;
    x = 0;
    for (;;) {
        alarm_wait_for();
        move();
        if (sig_restart) {
            ERROR(1,("daemon: pid %d received SIGHUP for restart, "
                     "exec'ing\n", getpid()));
            execl(Daemon, "netrek-daemon", "--restart", NULL);
            perror(Daemon);
            ERROR(1,("daemon: pid %d gave up on restart\n", getpid()));
            sig_restart = 0;
        }
        if (opt_debug) {
            if (!(++x % 50))
                ERROR(1,("daemon: mark %d\n", x));
            if (x > 10000) x = 0;                       /* safety measure */
        }
    }
}

/* These specify how often special actions will take place in
   UPDATE units (0.10 seconds, currently.) independent of frame rate
   So 1 means at 10 per second.  Only valid out of pause.
*/

#define PLAYERFUSE      1
#define PLASMAFUSE      1
#define PHASERFUSE      1
#define CLOAKFUSE       1
#define TWARPFUSE       1
#define TEAMFUSE        5
#define PLFIGHTFUSE     5
#define SIGHTFUSE       5
#define BEAMFUSE        8       /* scott 8/25/90 -- was 10 */
#define DRAFTFUSE       1
#define PMOVEFUSE       300      /* planet movement fuse 07/26/95 JRP */
#define QUEUEFUSE       600     /* cleanup every 60 seconds */
#define PLANETFUSE      400/MAXPLANETS  /* INL 3.9 POP */
#define MINUTEFUSE      600     /* 1 minute, surrender funct etc. 4/15/92 TC */
#define SYNCFUSE        3000
#define CHECKLOADFUSE   6000    /* 10 min. */
#define HOSEFUSE        18000   /* 30 min., was 20 minutes 6/7/95 JRP */
#define HOSEFUSE2       3000    /*  5 min., was  3 minutes 6/29/92 TC */

/* default 30 secs */
#define GHOSTTIME       (ghostbust_timer * 1000000 / UPDATE)

/* default 32 secs */
#define KGHOSTTIME      ((ghostbust_timer + 2) * 1000000 / UPDATE)

#define OUTFITTIME      (3 * AUTOQUIT * 1000000 / UPDATE) /* minutes */

#define HUNTERKILLER    (-1)
#define TERMINATOR      (-2)    /* Terminator */
#define STERMINATOR     (-3)    /* sticky Terminator */

int dietime = -1;  /* isae - was missing int */

/*! @brief Check if tournament mode conditions are met.
    @details Used to determine if we should be recording stats.
    @return t boolean, 1 for true, 0 for false. */
static int is_tournament_mode(void)
{
    int i, count[MAXTEAM+1], quorum[MAXTEAM+1];
    struct player *p;

    if (practice_mode) return 0;
#ifdef PRETSERVER
    if (bot_in_game) return 0;
#endif

    memset((int *) count, 0, sizeof(int) * (MAXTEAM+1));
    memset((int *) quorum, 0, sizeof(int) * (MAXTEAM+1));

    for (i=0, p=players; i<MAXPLAYER; i++, p++) {
        if (((p->p_status != PFREE) &&
             /* don't count observers for Tmode 06/09/95 JRP */
             !(p->p_flags & PFOBSERV) &&
             (p->p_status != POBSERV)) &&
             /* don't count robots for Tmode 10/31/91 TC */
            !(p->p_flags & PFROBOT)) {
                count[p->p_team]++;
        }
    }

    i=0;
    if (count[FED] >= tournplayers) quorum[i++] = FED;
    if (count[ROM] >= tournplayers) quorum[i++] = ROM;
    if (count[KLI] >= tournplayers) quorum[i++] = KLI;
    if (count[ORI] >= tournplayers) quorum[i++] = ORI;
    context->quorum[0] = quorum[0];
    context->quorum[1] = quorum[1];
    if (i > 1) return 1;
    return 0;
}


/*! @brief Check player list for possible t-mode scummers.
    @author Nick Trown 12/19/92
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
      if (me->p_status == PFREE) continue;
      if (is_robot(me)) continue;
      if (me->p_status == POBSERV) continue;
#ifdef LTD_STATS
      if (ltd_ticks(me, LTD_TOTAL) != 0)
#else
      if (me->p_stats.st_tticks != 0)
#endif
      {
        for (j=i+1; j<MAXPLAYER; j++) {
          struct player *them = &players[j];
          if (them->p_status == PFREE) continue;
          if (is_robot(them)) continue;
          if (them->p_status == POBSERV) continue;
#ifdef LTD_STATS
          if (ltd_ticks(them, LTD_TOTAL) != 0)
#else
          if (them->p_stats.st_tticks != 0)
#endif
          {
            if (strcmp(me->p_ip, them->p_ip) == 0) {
              if (!them->p_ip_duplicates) {
                who=j;
                num++;
              }
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
                ERROR(1,("daemon: unable to open scum file.\n"));
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
                    players[j].p_full_hostname,
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

/*! @brief Send t-mode commencement message to all players.
    @details Messages that announce t-mode are fictional, and so the
    matching message to announce end of t-mode has to match the
    fiction.
    @bug Messages with the same theme would be better listed next to
    each other, and perhaps be obtained from a default configuration
    file for the server installed by the "make install" target. Having
    them coded here is inflexible. */
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
                pmessage(0, MALL, "GOD->ALL","Diplomat insults foreign emperor's mother and fighting breaks out!");
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

/*! @brief Send matching t-mode end message to all players. */
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
                pmessage(0, MALL, "GOD->ALL","Diplomat apologizes to emperor's mother and invasion is stopped!");
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

/*! @brief Per frame simulation.
    @details Runs the simulation forward by one frame, affecting the
    state of the universe and objects. Called once per frame time as a
    result of a SIGALRM. */
static void move()
{
    static int oldmessage;
    int old_manager_type, old_distortion, old_fps;
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
        bans_age_temporary(QUEUEFUSE/distortion);
    }

    /* during a pause or conquer parade */
    if (status->gameup & GU_PAUSED) {
      static int cycle = 0;
      /* run player and conquer parade at ten times per second */
      if (cycle++ % (fps / 10) == 0) {
        udplayers_pause();
        if (status->gameup & GU_CONQUER) conquer_update();
      }
      if (context->conquer_trigger != 0) {
        conquer_begin_special();
        context->conquer_trigger = 0;
      }
      do_message_requeue_all();
      /* but continue to signal player processes at their chosen rate */
      signal_servers();
      return;
    }

    if (context->frame_test_mode) {
      if (!context->frame_test_counter) {
        status->gameup |= GU_PAUSED;
        context->frame_test_mode = 0;
        return;
      }
      context->frame_test_counter--;
    }

    if (++context->frame == dietime) {/* no player for 1 minute. kill self */
        blog_game_over(&context->start, status);
        if (opt_debug) {
            ERROR(1,("daemon: ho hum, no activity...\n"));
        } else {
            ERROR(3,("daemon: self-destructing\n"));
            if (self_reset) {
                    ERROR(3,("daemon: resetting galaxy!\n"));
                    pl_reset();
            }
        }
        exitDaemon(0);
    }
    old_manager_type = manager_type;
    old_distortion = distortion;
    old_fps = fps;
    if (update_sys_defaults()) {
        if (chaosmode)
            status->gameup |= GU_CHAOS;
        else
            status->gameup &= ~GU_CHAOS;
        if (old_manager_type != manager_type) {
            if (manager_pid) {
                kill(manager_pid, SIGINT);
                ERROR(8,("daemon: kill manager pid %d\n", manager_pid));
                manager_pid = 0;
            }
            if (manager_type) fork_robot(manager_type);
        }

        /* allow for change to distortion timer */
        if (old_distortion != distortion || old_fps != fps)
            alarm_setitimer(distortion, fps);

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
            if (is_tournament_mode()) {
                    ts = TS_BEGIN;
                    if (check_scum && check_scummers(1))
                            if (!inl_mode)
                                    ts = TS_SCUMMERS;
            }
            break;

    case TS_SCUMMERS:
            status->tourn = 0;
            if (!is_tournament_mode()) {
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
            context->frame_tourn_start = context->frame;
            if (strlen(script_tourn_start) > 0) fork_script(script_tourn_start);
            /* break; */

    case TS_TOURNAMENT:
            status->tourn = 1;
            /* BUG: 50 fps change impacts global unduly */
            status->time++;
            context->frame_tourn_end = context->frame;
            if (is_tournament_mode()) break;
            ts = TS_END;
            /* break; */

    case TS_END:
            context->frame_tourn_end = context->frame;
            political_end(oldmessage);
            ts = TS_PICKUP;

            if (glog_open() == 0) {
                int n = frames_to_seconds(context->frame_tourn_end -
                                          context->frame_tourn_start);
                glog_printf("t-mode duration (seconds): %d\n", n);
                glog_flush();
            }

            if (strlen(script_tourn_end) > 0) fork_script(script_tourn_end);
            break;
    }

#ifdef PUCK_FIRST
    signal_puck();
#endif /* PUCK_FIRST */

    udships();
    if (fuse(PLAYERFUSE)) {
        udplayers();
    }
    if (fuse(TWARPFUSE)) {
        udtwarp();
    }
    udtorps();
    if (fuse(PHASERFUSE)) {
        udphaser();
    }
    if (fuse(CLOAKFUSE)) {
        udcloak();
    }

    do_message_requeue_all();
    signal_servers();

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

    if (fuse(DRAFTFUSE)) {
        if (status->gameup & GU_INL_DRAFTING) inl_draft_update();
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
        advertise();
        if (manager_type && manager_pid == 0) {
            ERROR(1,("daemon: manager restart\n"));
            fork_robot(manager_type);
        }
    }
}


static void killer_credit(struct player *killer, struct player *died)
{
        killer->p_kills += 1.0 + died->p_armies * 0.1 + died->p_kills * 0.1;
#ifdef STURGEON
        if (sturgeon && sturgeon_extrakills)
                killer->p_kills += died->p_upgrades * 0.15;
#endif
}

/*! @brief Update player sight.
    @details Updates player visibility state (PFSEEN) based on orbit
    status, and distance to an enemy.
    @warning The decision whether to show position is taken by
    updateShips() in genspkt.c */
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
       * There is a very good chance that K can still see J.
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

/*! @brief Explosion effects on docking bays.
    @details Release docking bays as a result of a ship explosion.  If
    the ship is a base, release all docked ships from bays, set the
    starbase rebuild time, and blog about the loss of the base. */
static void udplayers_pexplode_bays(struct player *j)
{
        bay_release(j);
        if (j->p_ship.s_type == STARBASE) {
                bay_release_all(j);
                /* set reconstruction time for new starbase */
                if (((j->p_whydead == KSHIP) ||
                     (j->p_whydead == KTORP) ||
                     (j->p_whydead == KPHASER) ||
                     (j->p_whydead == KPLASMA) ||
                     (j->p_whydead == KPLANET) ||
                     (j->p_whydead == KGENOCIDE)) && (status->tourn)) {
                        teams[j->p_team].te_turns = starbase_rebuild_time;
                        if (j->p_status == PDEAD) blog_base_loss(j);
                }
        }
}

/*! @brief Explosion effects on ships, during pause.
    @details Only animate explosions in a pause that are due to a TRADE. */
static void udplayerspause_pexplode(struct player *j)
{
        if (j->p_whydead == TOURNSTART) {
                j->p_flags &= ~(PFCLOAK|PFORBIT);
                j->p_status = PDEAD;
                udplayers_pexplode_bays(j);
        }
}

/*! @brief Dead state.
    @details Ship has finished exploding. In PDEAD state the p_explode
    counter is decremented until it reaches zero or below, then the
    player statistics are saved and the ship enters POUTFIT state.
    During a pause, p_explode is ignored and the POUTFIT state happens
    on the next frame.
    @warning Delay for expiry of in-flight weapons before player is
    given team selection window is implemented in intrupt() in
    redraw.c, not in the daemon.
*/
static void udplayers_pdead(struct player *j)
{
        if ((--j->p_explode <= 0) || (status->gameup & GU_PAUSED)) {
                saveplayer(j);
                j->p_status = POUTFIT;
        }
}

/*! @brief Update players, during pause.
    @details Update players structure during a pause.  Similar in
    effect to udplayers(), but only a subset of functions are called,
    which are those necessary to sustain normal operation during a
    pause.  No object animation occurs.

    For each player, adopt any new coordinates requested by tools,
    handle state, detect ghostbuster watchdog expiry, and terminate
    player process if required. */
static void udplayers_pause(void) {
  int i;
  struct player *j;

  for (i=0; i<MAXPLAYER; i++) {
    int kill_player = 0;        /* 1 = QUIT during pause */
                                /* 2 = ghostbust during pause */
    j = &players[i];

    /* adopt new coordinates if requested */
    if (j->p_x_y_set) p_x_y_commit(j);

    switch(j->p_status) {
      case PFREE:       /* reset ghostbuster and continue */
        j->p_ghostbuster = 0;
        continue;
        break;
      case PEXPLODE:
        udplayerspause_pexplode(j);
        break;
      case PDEAD:
        if (j->p_whydead == KQUIT)
          kill_player = 1;
        else
          udplayers_pdead(j);
        break;
    }

    if (++(j->p_ghostbuster) > GHOSTTIME) {
      ERROR(4,("daemon/udplayers_pause: %s: ship ghostbusted (wd=%d)\n",
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
      ERROR(8,("daemon/udplayers_pause: %s: sending SIGTERM to %d\n",
               j->p_mapchars, j->p_process));

      if (kill (j->p_process, SIGTERM) < 0)
        ERROR(1,("daemon/udplayers_pause: kill(%d, SIGTERM) failed!\n",
                 j->p_process));

      /* let's be safe */
      freeslot(j);
    }
  }
}

/*! @brief Update players, during POUTFIT.
    @details During POUTFIT nothing much happens, but if the state lasts
    too long the slot is declared ghostbusted.

    While in POUTFIT state we are waiting for the player to rejoin a team.
    redraw.c sees to it that they cannot rejoin until all in-flight weapons
    have ended, that is n_torps reaches zero. POUTFIT has nothing to do with
    refitting. */
static void udplayers_poutfit(struct player *j)
{
        int outfitdelay;

        switch (j->p_whydead) {
        case KLOGIN:
                outfitdelay = logintime * 1000000 / UPDATE;
                break;
        case KQUIT:
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
                ERROR(4,("daemon: %s: ship in POUTFIT too long (gb=%d/%d,wd=%d)\n",
                         j->p_mapchars, j->p_ghostbuster,
                         outfitdelay, j->p_whydead));
                if (j->p_whydead == KGHOST) {
                        ghostmess(j, "no ping alive");
                } else {
                        ghostmess(j, "outfit timeout");
                }
                saveplayer(j);
                if (j->p_process > 1) {
                        ERROR(8,("daemon: %s: sending SIGTERM to %d\n",
                                 j->p_mapchars, j->p_process));
                        if (kill (j->p_process, SIGTERM) < 0)
                                ERROR(1,("daemon/udplayers: kill failed!\n"));
                } else {
                        ERROR(1,("daemon/udplayers: bad p_process!\n"));
                        freeslot(j);
                }
        }
}

static void udplayers_pobserv(struct player *j)
{
        /* observer players, perpetually 'dead'-like state */
        /* think of them as ghosts! but they still might get */
        /* ghostbusted - attempt to handle it */
        if (++(j->p_ghostbuster) > GHOSTTIME) {
                ghostmess(j, "no ping observ");
                saveplayer(j);
                j->p_status = PDEAD;
                j->p_whydead = KGHOST;
                j->p_whodead = j->p_no;
        }
}

/*! @brief Explosion effects on ships.
    @details Cancel cloak. Damage nearby ships. Set dead
    state. Determine dead time. Cascade explosion into docking
    bays. Eject from orbit. Trim life timers of in-flight weapons. */
static void udplayers_pexplode(struct player *j)
{
        j->p_updates++;
        j->p_flags &= ~PFCLOAK;
        if ((j->p_ship.s_type == STARBASE) &&
            /* todo: fps support, use of fuse for explosion time */
            (j->p_explode == ((2*SBEXPVIEWS-3)/PLAYERFUSE)))
                blowup(j);  /* damdiffe everyone else around */
        else if (j->p_explode == ((10-3)/PLAYERFUSE))
                blowup(j);      /* damdiffe everyone else around */

        if (--j->p_explode <= 0) {
                j->p_status = PDEAD;
                j->p_explode = 600/PLAYERFUSE; /* set PDEAD time limit */
        }

        udplayers_pexplode_bays(j);
        /* And he is ejected from orbit. */
        j->p_flags &= ~PFORBIT;

        /* Reduce any torp or plasma timers longer than 5 seconds,
           mainly for ATT torps/plasmas, and sturgeon weapons */
        struct torp *t;
        for (t = firstTorpOf(j); t <= lastTorpOf(j); t++) {
                if (t->t_status == TMOVE && t->t_fuse > seconds_to_frames(5))
                        t->t_fuse = seconds_to_frames(5);
        }
        for (t = firstPlasmaOf(j); t <= lastPlasmaOf(j); t++) {
                if (t->t_status == TMOVE && t->t_fuse > seconds_to_frames(5))
                        t->t_fuse = seconds_to_frames(5);
        }
}

/*! @brief Orbit simulation.
    @details While in orbit, ships rotate around the primary in a
    circle of fixed diameter, with the ship facing the direction of
    travel. Manage the orbital direction and reset the ship
    coordinates to what they should be for the new position in orbit.

    Orbit simulation occurs on major updates, effectively at a ten
    frames per second rate. */
static void udplayers_palive_move_in_orbit(struct player *j)
{
        j->p_dir += 2;
        j->p_desdir = j->p_dir;
        j->p_x_internal = planets[j->p_planet].pl_x * SPM + SPM * ORBDIST
                * Cos[(u_char) (j->p_dir - (u_char) 64)];
        j->p_y_internal = planets[j->p_planet].pl_y * SPM + SPM * ORBDIST
                * Sin[(u_char) (j->p_dir - (u_char) 64)];
        j->p_x = spo(j->p_x_internal);
        j->p_y = spo(j->p_y_internal);
}

static void udships_palive_move_in_orbit(struct player *j)
{
        /* every minor update, at whatever fps is set, travel at warp
        two along a straight-line segment of the orbital path */
        j->p_x_internal +=
                (double) (SPM * 2 * WARP1) * Cos[j->p_dir] / TPF;
        j->p_y_internal +=
                (double) (SPM * 2 * WARP1) * Sin[j->p_dir] / TPF;
        j->p_x = spo(j->p_x_internal);
        j->p_y = spo(j->p_y_internal);
}

static void udplayers_palive_move_in_dock(struct player *j)
{
        /* nothing needs to be done */
}

static void udships_palive_move_in_dock(struct player *j)
{
        /* keep position synchronised with base docking port */
        j->p_x_internal = players[j->p_dock_with].p_x_internal +
                 SPM*DOCKDIST*Cos[(j->p_dock_bay*90+45)*255/360];
        j->p_y_internal = players[j->p_dock_with].p_y_internal +
                 SPM*DOCKDIST*Sin[(j->p_dock_bay*90+45)*255/360];
        j->p_x = spo(j->p_x_internal);
        j->p_y = spo(j->p_y_internal);
}

static void udplayers_palive_move_in_space(struct player *j)
{
        int maxspeed, k;

        if ((j->p_dir != j->p_desdir) && (j->p_status != PEXPLODE))
                changedir(j);

        if (j->p_flags & PFTWARP) return; /* udtwarp() to handle ship */
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
}

static void udships_palive_move_in_space(struct player *j)
{
        /* adopt new coordinates if requested */
        if (j->p_x_y_set) p_x_y_commit(j);

        /* calculate new coordinates given current speed and direction */
        j->p_x_internal +=
                (double) (SPM * j->p_speed * WARP1) * Cos[j->p_dir] / TPF;
        j->p_y_internal +=
                (double) (SPM * j->p_speed * WARP1) * Sin[j->p_dir] / TPF;

        /* ship hit wall, wrap or bounce */
        if (j->p_x_internal < j->p_x_min) {
                int dx = j->p_x_min - j->p_x_internal;
                if (!wrap_galaxy) {
                        j->p_x_internal += 2 * dx;
                        if (j->p_dir == 192)
                                j->p_dir = j->p_desdir = 64;
                        else
                                j->p_dir = j->p_desdir = 64 - (j->p_dir - 192);
                } else
                        j->p_x_internal = j->p_x_max - dx;
        } else if (j->p_x_internal > j->p_x_max) {
                int dx = j->p_x_internal - j->p_x_max;
                if (!wrap_galaxy) {
                        j->p_x_internal -= 2 * dx;
                        if (j->p_dir == 64)
                                j->p_dir = j->p_desdir = 192;
                        else
                                j->p_dir = j->p_desdir = 192 - (j->p_dir - 64);
                } else
                        j->p_x_internal = j->p_x_min + dx;
        }

        if (j->p_y_internal < j->p_y_min) {
                int dy = j->p_y_min - j->p_y_internal;
                if (!wrap_galaxy) {
                        j->p_y_internal += 2 * dy;
                        if (j->p_dir == 0)
                                j->p_dir = j->p_desdir = 128;
                        else
                                j->p_dir = j->p_desdir = 128 - j->p_dir;
                } else
                        j->p_y_internal = j->p_y_max - dy;
        } else if (j->p_y_internal > j->p_y_max) {
                int dy = j->p_y_internal - j->p_y_max;
                if (!wrap_galaxy) {
                        j->p_y_internal -= 2 * dy;
                        if (j->p_dir == 128)
                                j->p_dir = j->p_desdir = 0;
                        else
                                j->p_dir = j->p_desdir = 0 - (j->p_dir - 128);
                } else
                        j->p_y_internal = j->p_y_min + dy;
        }

        j->p_x = spo(j->p_x_internal);
        j->p_y = spo(j->p_y_internal);
}

static void udships_palive(struct player *j)
{
        if (j->p_flags & PFDOCK) {
                udships_palive_move_in_dock(j);
                return;
        }
        if (j->p_flags & PFORBIT) {
                udships_palive_move_in_orbit(j);
                return;
        }
        udships_palive_move_in_space(j);
}

static void udships()
{
    int i;
    struct player *j;

    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        switch (j->p_status) {
            case PEXPLODE:
            case PALIVE:
                udships_palive(j);
                break;
        } /* end switch */
    }
}

static void udplayers_palive_update_stats(struct player *j)
{
        if (status->tourn
#ifdef BASEPRACTICE
            || practice_mode
#endif
                ) {

#ifdef LTD_STATS
                ltd_update_ticks(j);
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
}

static void udplayers_palive_check_ghostbuster(struct player *j)
{
        if (++(j->p_ghostbuster) > GHOSTTIME) {
                p_explosion(j, KGHOST, j->p_no);
                ERROR(4,("daemon/udplayers: %s: ship ghostbusted (gb=%d,gt=%d)\n", j->p_mapchars, j->p_ghostbuster, GHOSTTIME));
        }
}

static void udplayers_palive_fuel_shields(struct player *j)
{
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
}

static void udplayers_palive_tractor(struct player *j)
{
        float dist;

        if (!(j->p_flags & PFTRACT)) return;
        /* affect tractor beams */
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
                /* todo: fps support, tractor pulses at 10 fps */
                j->p_x += dir * cosTheta * halfforce/(j->p_ship.s_mass);
                j->p_y += dir * sinTheta * halfforce/(j->p_ship.s_mass);
                p_x_y_to_internal(j);
                /* todo: fps support, tractor pulses at 10 fps */
                players[j->p_tractor].p_x -= dir * cosTheta *
                        halfforce/(players[j->p_tractor].p_ship.s_mass);
                players[j->p_tractor].p_y -= dir * sinTheta *
                        halfforce/(players[j->p_tractor].p_ship.s_mass);
                p_x_y_to_internal(&players[j->p_tractor]);
        } else {
                j->p_flags &= ~(PFTRACT | PFPRESS);
        }
}

static void udplayers_palive_cool_weapons(struct player *j)
{
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
                        /* todo: fps support, use of fuse for duration */
                        j->p_wtime = ((short) (random() % 150) + 100) /
                                PLAYERFUSE;
                }
        }
}

static void udplayers_palive_cool_engines(struct player *j)
{
        /* cool engine */
        j->p_etemp -= j->p_ship.s_egncoolrate;
        if (j->p_etemp < 0)
                j->p_etemp = 0;
        if (j->p_flags & PFENG) {
                if (--j->p_etime <= 0)
                        j->p_flags &= ~PFENG;
        } else if (j->p_etemp > j->p_ship.s_maxegntemp) {
                if (!(random() % 40)) {
                        j->p_flags |= PFENG;
                        /* todo: fps support, use of fuse for duration */
                        j->p_etime = ((short) (random() % 150) + 100) /
                                PLAYERFUSE;
                        j->p_desspeed = 0;
                }
        }
}

static void udplayers_palive_fuel_cloak(struct player *j)
{
        /* Charge for cloaking */
        if (j->p_flags & PFCLOAK) {
                if (j->p_fuel < j->p_ship.s_cloakcost) {
                        j->p_flags &= ~PFCLOAK;
                } else {
                        j->p_fuel -= j->p_ship.s_cloakcost;
                }
        }
}

static void udplayers_palive_make_fuel(struct player *j)
{
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
}

static void udplayers_palive_repair(struct player *j)
{
        int repair_needed, repair_progress_old, repair_gained, repair_time = 0;

        /* repair shields */
        if (j->p_shield < j->p_ship.s_maxshield) {
                repair_progress_old = j->p_subshield;
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
                } else {
                        j->p_subshield += j->p_ship.s_repair * 2;
                }
                /* 1000 subshield  =  1 shield or hull repaired
                   This routine is used by server every update
                   Repair time assumes 10 updates/sec */
                repair_needed = j->p_ship.s_maxshield - j->p_shield;
                /* How much repair would be gained, normalized to 1 second */
                repair_gained = j->p_subshield - repair_progress_old;
                repair_time = repair_needed * 100 / repair_gained;
                j->p_repair_time = repair_time;
                if (j->p_subshield / 1000) {
#ifdef LTD_STATS
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
                repair_progress_old = j->p_subdamage;
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
                } else {
                        j->p_subdamage += j->p_ship.s_repair;
                }
                repair_needed = j->p_damage;
                repair_gained = j->p_subdamage - repair_progress_old;
                if (j->p_shield != j->p_ship.s_maxshield)
                        j->p_repair_time = MAX(repair_time,
                                               repair_needed * 100 / repair_gained);
                else
                        j->p_repair_time = repair_needed * 100 / repair_gained;
                if (j->p_subdamage / 1000) {
#ifdef LTD_STATS
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
}

static void udplayers_palive_set_alert(struct player *j)
{
        int k;

        /* Set player's alert status */
#define YRANGE ((GWIDTH)/7)
#define RRANGE ((GWIDTH)/10)
        j->p_flags |= PFGREEN;
        j->p_flags &= ~(PFRED|PFYELLOW);
        for (k = 0; k < MAXPLAYER; k++) {
                int dx, dy, dist;
                if ((players[k].p_status != PALIVE) ||
                    ((!(j->p_war & players[k].p_team)) &&
                     (!(players[k].p_war & j->p_team))) ||
                    is_idle(&players[k])) {
                        continue;
                } else if (j == &players[k]) {
                        continue;
                } else {
                        dx = j->p_x - players[k].p_x;
                        dy = j->p_y - players[k].p_y;
                        if (ABS(dx) > YRANGE || ABS(dy) > YRANGE)
                                continue;
                        dist = dx * dx + dy * dy;
                        if (dist <  RRANGE * RRANGE) {
                                j->p_flags |= PFRED;
                                j->p_flags &= ~(PFGREEN|PFYELLOW);
                                /* jac: can't get any worse, should we break; ? */
                        } else if ((dist <  YRANGE * YRANGE) &&
                                   (!(j->p_flags & PFRED))) {
                                j->p_flags |= PFYELLOW;
                                j->p_flags &= ~(PFGREEN|PFRED);
                        }
                }
        }
}

static void udplayers_palive_self_destruct_credit(struct player *j)
{
        struct player *k, *enemy;
        int dist, max_dist;

        enemy = NULL;
        max_dist = GWIDTH * 2;
        for (k = firstPlayer; k <= lastPlayer; k++) {
                if (k->p_status != PALIVE) continue;
                if (k->p_team == j->p_team) continue;
                dist = hypot((double) (j->p_x - k->p_x),
                             (double) (j->p_y - k->p_y));
                if (dist > (GWIDTH/4)) continue;
                if (dist < max_dist) {
                        max_dist = dist;
                        enemy = k;
                }
        }
        if (enemy != NULL) {
                killer_credit(enemy, j);
        }
}

static void udplayers_palive_self_destruct(struct player *j)
{
        if (!(j->p_flags & PFSELFDEST)) return;
        if ((j->p_updates >= j->p_selfdest) ||
            ((j->p_flags & PFGREEN) && (j->p_damage == 0)
                && (j->p_shield == j->p_ship.s_maxshield))) {
                j->p_flags &= ~PFSELFDEST;
                p_explosion(j, KQUIT, j->p_no);
                if (self_destruct_credit) {
                        udplayers_palive_self_destruct_credit(j);
                }
        }
}

static void udplayers_palive(struct player *j)
{
        if ((j->p_flags & PFORBIT) && !(j->p_flags & PFDOCK)) {
                /* move player in orbit */
                udplayers_palive_move_in_orbit(j);
        } else if (!(j->p_flags & PFDOCK)) {
                /* move player through space */
                udplayers_palive_move_in_space(j);
        } else if (j->p_flags & PFDOCK) {
                /* move player in dock */
                udplayers_palive_move_in_dock(j);
        }

        /* If player is actually dead, don't do anything below ... */
        if (j->p_status == PEXPLODE ||
            j->p_status == PDEAD ||
            j->p_status == POBSERV)
                return;

        udplayers_palive_update_stats(j);
        udplayers_palive_check_ghostbuster(j);
        j->p_updates++;

        udplayers_palive_fuel_shields(j);
        udplayers_palive_tractor(j);
        udplayers_palive_cool_weapons(j);
        udplayers_palive_cool_engines(j);
        udplayers_palive_fuel_cloak(j);
        udplayers_palive_make_fuel(j);
        udplayers_palive_repair(j);
        udplayers_palive_set_alert(j);
        udplayers_palive_self_destruct(j);
}

/*! @brief Update players.
    @details Update players structure for next simulation frame. */
static void udplayers(void)
{
    int i;
    struct player *j;

    int nfree = 0;
    tcount[FED] = tcount[ROM] = tcount[KLI] = tcount[ORI] = 0;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {

        switch (j->p_status) {
            case POUTFIT:
                udplayers_poutfit(j);
                continue;
            case PFREE:
                nfree++;                /* count slot toward empty server */
                j->p_ghostbuster = 0;   /* stop from hosing new players */
                continue;
            case POBSERV:
                udplayers_pobserv(j);
                /* count observer slot toward empty server */
                if (!observer_keeps_game_alive) nfree++;
                continue;
            case PDEAD:
                udplayers_pdead(j);
                continue;
            case PEXPLODE:
                udplayers_pexplode(j);
                /* fall through to alive so explosions move */
            case PALIVE:
                tcount[j->p_team]++;
                udplayers_palive(j);
                break;
        } /* end switch */
    }
    if (nfree == MAXPLAYER) {
        if (dietime == -1) {
            if (status->gameup & GU_GAMEOK) {
                dietime = context->frame + seconds_to_frames(60);
            } else {
                dietime = context->frame + seconds_to_frames(1);
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
        /* todo: fps support */
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

/*
                     time - - - >

In the following figure, ! represents 10, @ represents 11

frames at 10fps      .....................................
keyboard                c                   c
p_flags & PFCLOAK    0000111111111111111111110000000000000
p_cloakphase         0000123456789!@@@@@@@@@@!987654321000
position hidden      0000000000000011111111110000000000000

*/

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

static void udtwarp(void)
{
    int i, dist;
    struct player *j, *k;

    for (i=0; i<MAXPLAYER; i++) {
        j = &players[i];
        if (j->p_status != PALIVE) continue;
        if (!(j->p_flags & PFTWARP)) continue;
        /* ship is alive and in transwarp, accelerate if needed */
        if ((j->p_speed *= 2) > (j->p_desspeed))
            j->p_speed = j->p_desspeed;
        /* deduct fuel and raise etemp */
        j->p_fuel -= (int)((j->p_ship.s_warpcost * j->p_speed) * 0.8);
        j->p_etemp += j->p_ship.s_maxspeed;
#ifndef SB_CALVINWARP
        /* decelerate if needed */
        k = &players[j->p_playerl];
        if (k->p_ship.s_type == STARBASE) {
            dist = hypot((double) (j->p_x - k->p_x),
                         (double) (j->p_y - k->p_y));

            if ((dist-(DOCKDIST/2) < (11500 * j->p_speed * j->p_speed) /
                j->p_ship.s_decint) && j->p_desspeed > 2) {
                    j->p_desspeed--;
            }
        }
#endif
    }
}

/*
 * Find nearest hostile vessel and turn toward it
 */
static void t_track(struct torp *t)
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
    if ((j->p_team == owner->p_team) && !(j->p_flags & PFPRACTR))
      continue;
    if (! ((t->t_war & j->p_team) || (t->t_team & j->p_war)))
      continue;

    /*
     * Before doing the fancy computations, don't bother exploring the
     * tracking of any player more distant than our current target.   */
    dx = spo(t->t_x_internal - j->p_x_internal);
    if ((dx >= range) || (dx <= -range))
      continue;
    dy = spo(t->t_y_internal - j->p_y_internal);
    if ((dy >= range) || (dy <= -range))
      continue;

    /*
     * Get the direction that the potential target is from the torp:  */
    heading = -64 + rint(atan2((double) dy, (double) dx) / 3.14159 * 128.0);
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
    /* todo: fps support, use of a fuse */
    if ((t->t_turns * t->t_fuse / T_FUSE_SCALE > 127) ||
        (bearing < t->t_turns * t->t_fuse / T_FUSE_SCALE) ||
        (bearing > (256 - t->t_turns * t->t_fuse / T_FUSE_SCALE ))) {
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

static void t_move(struct torp *t)
{
        t->t_x_internal += (double) (SPM * t->t_gspeed) * Cos[t->t_dir] / TPF;
        t->t_y_internal += (double) (SPM * t->t_gspeed) * Sin[t->t_dir] / TPF;
        t->t_x = spo(t->t_x_internal);
        t->t_y = spo(t->t_y_internal);
}

static int t_check_wall(struct torp *t)
{
        int hit = 0;
        int sgw = spi(GWIDTH);

        if (t->t_x_internal < 0) {
                if (!wrap_galaxy) {
                        t->t_x_internal = 0;
                        hit++;
                } else {
                        t->t_x_internal = sgw;
                }
                t->t_x = spo(t->t_x_internal);
        } else if (t->t_x_internal > sgw) {
                if (!wrap_galaxy) {
                        t->t_x_internal = sgw;
                        hit++;
                } else {
                        t->t_x_internal = 0;
                }
                t->t_x = spo(t->t_x_internal);
        }

        if (t->t_y_internal < 0) {
                if (!wrap_galaxy) {
                        t->t_y_internal = 0;
                        hit++;
                } else {
                        t->t_y_internal = sgw;
                }
                t->t_y = spo(t->t_y_internal);
        } else if (t->t_y_internal > sgw) {
                if (!wrap_galaxy) {
                        t->t_y_internal = sgw;
                        hit++;
                } else {
                        t->t_y_internal = 0;
                }
                t->t_y = spo(t->t_y_internal);
        }

        return hit;
}

static void t_hit_wall(struct torp *t)
{
        t->t_status = TDET;
        t->t_whodet = t->t_owner;
        t_explosion(t);

#ifdef LTD_STATS
        /* update torp hit wall stat */
        switch(t->t_type) {
        case TPHOTON:
                ltd_update_torp_wall(&(players[t->t_owner]));
                break;
        case TPLASMA:
                ltd_update_plasma_wall(&(players[t->t_owner]));
                break;
        }
#endif
}

static void t_wobble(struct torp *t)
{
#ifdef STURGEON
        /* mines spin in a constant position */
        if (sturgeon && t->t_spinspeed) {
                t->t_dir = (t->t_dir + t->t_spinspeed) % 256;
                return;
        }
#endif
        t->t_dir += (random() % 3) - 1;
}

/* see if there is someone close enough to explode for */
static int t_near(struct torp *t)
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
                if (is_idle(j))
                        continue;
                dx = spo(t->t_x_internal - j->p_x_internal);
                if ((dx < -EXPDIST) || (dx > EXPDIST))
                        continue;
                dy = spo(t->t_y_internal - j->p_y_internal);
                if ((dy < -EXPDIST) || (dy > EXPDIST))
                        continue;
                if (dx*dx + dy*dy <= EXPDIST * EXPDIST)
                        return 1;
        }
        return 0;
}


static void t_hit_ship_credit(struct torp *t)
{
#ifdef LTD_STATS
        switch(t->t_type) {
          case TPHOTON:
            ltd_update_torp_hit(&(players[t->t_owner]));
            break;
          case TPLASMA:
            ltd_update_plasma_hit(&(players[t->t_owner]));
            break;
        }
#endif
}

static void t_off(struct torp *t)
{
#ifdef STURGEON
        /* detting your own torps may do damage */
        if (sturgeon && players[t->t_owner].p_upgradelist[UPG_DETDMG]) {
                t->t_status = TDET;
                t->t_damage /= 4;
                t->t_whodet = players[t->t_owner].p_no;
                t_explosion(t);
        }
#endif
}

static void udtorps(void)
{
        struct torp *t;

        /* update all torps and plasmas; they are in the same array */
        for (t = firstTorp; t <= lastPlasma; t++) {
                switch (t->t_status) {

                case TFREE: /* this is an empty torp slot, skip it */
                        continue;

                case TMOVE: /* this is a moving torp */
                        /* is torp out of time? */
                        if (t->t_fuse-- <= 0)
                                break; /* fall through to torp slot free */

                        /* tracking torps change direction */
                        if (t->t_fuse % T_FUSE_SCALE == 0)
                                if (t->t_turns > 0)
                                        t_track(t);

                        /* move the torp, checking for wall hits */
                        t_move(t);
                        if (t_check_wall(t)) {
                                t_hit_wall(t);
                        }

                        /* do rest of work on torp at normal rate */
                        if (t->t_fuse % T_FUSE_SCALE != 0) continue;

                        /* wobble the torp, changing direction */
                        if (t->t_attribute & TWOBBLE)
                                t_wobble(t);

                        /* is torp position near enough to hit a ship? */
                        if (t_near(t)) {
                                t_hit_ship_credit(t);
                                t_explosion(t);
                        }
                        continue;

                case TDET: /* this is a detonating torp */
                        t_explosion(t);
                        continue;

                case TEXPLODE: /* this is an exploding torp */
                        if (t->t_fuse-- > 0)
                                continue;
                        break;

                case TOFF: /* this is an owner-detonated torp */
                        t_off(t);
                        break; /* fall through to torp slot free */
                }

                t->t_status = TFREE; /* free torp slot */
                switch (t->t_type) {
                case TPHOTON: players[t->t_owner].p_ntorp--; break;
                case TPLASMA: players[t->t_owner].p_nplasmatorp--; break;
                }
        }
}

/* trigger a ship explosion */
static void p_explosion(struct player *player, int why, int who)
{
    player->p_status = PEXPLODE;
    /* todo: fps support, use of a fuse */
    player->p_explode =
        (player->p_ship.s_type == STARBASE ? 2*SBEXPVIEWS : 10) / PLAYERFUSE;
    player->p_whydead = why;
    player->p_whodead = who;
    /* damage resulting from ship explosion is done in subsequent frames */
}

static int t_whydead(struct player *k, struct torp *torp)
{
    /*! @bug KTORP2 is defined as killed by detted torps, not own torps */
    /*! @bug KPLASMA2 is defined as killed by zapped plasma, not own plasma */
    if (k->p_no != torp->t_owner)
        return torp->t_type == TPHOTON ? KTORP2 : KPLASMA2;
    else
        return torp->t_type == TPHOTON ? KTORP : KPLASMA;
}

#ifdef STURGEON
/* nuke has exploded on a planet, damage planet accordingly */
static void t_nuke_planet(struct torp *torp)
{
  struct planet *l;
  struct player *me;
  char buf[80];
  char addrbuf[80];
  int oldowner, h;

  l = &planets[torp->t_plbombed];
  me = &players[torp->t_owner];
  h = torp->t_pldamage;
  if (h > l->pl_armies)
    h = l->pl_armies;

  if ((l->pl_armies >= 5) && (l->pl_armies - h < 5))
    l->pl_flags |= PLREDRAW;

  l->pl_armies -= h;

  me->p_armsbomb += h;
  /* Give him bombing stats if he is bombing a team with 3+ */
  if (status->tourn && (realNumShips(l->pl_owner) >= 3)) {
    me->p_kills += 0.02 * h;
    me->p_genoarmsbomb += h;
#ifndef LTD_STATS
    checkmaxkills(me->p_no);
    me->p_stats.st_tarmsbomb += h;
#endif
    status->armsbomb += h;
  } else {
#ifndef LTD_STATS
    me->p_stats.st_armsbomb += h;
#endif
  }

  if (l->pl_armies > 0)
    sprintf(buf,
            "Help!  %c%c (%s) nuked %d of our armies (%d left).",
            teamlet[me->p_team], shipnos[me->p_no], me->p_name,
            h, l->pl_armies);
  else
    sprintf(buf, "Help!  %c%c (%s) is nuking... <crackle>",
            teamlet[me->p_team], shipnos[me->p_no], me->p_name);

  sprintf(addrbuf, "%-3s->%s ", l->pl_name, teamshort[l->pl_owner]);
  pmessage(l->pl_owner, MTEAM, addrbuf,buf);

  if (l->pl_armies == 0) {
    if (status->tourn && realNumShips(l->pl_owner) < 2 &&
        realNumShips(l->pl_flags & (FED|ROM|ORI|KLI)) < 2) {
      l->pl_flags |= PLCHEAP;
    }
    oldowner = l->pl_owner;
    l->pl_owner = NOBODY;
    checkgen(oldowner, me);
  }
  plrescue(l);
}
#endif


/* trigger a torpedo explosion, and damage surrounding players */
static void t_explosion(struct torp *torp)
{
  int dx, dy, dist;
  int damage, damdist;
  struct player *j;
  struct player *k;

#ifdef STURGEON
  if (sturgeon && torp->t_pldamage > 0) {
    t_nuke_planet(torp);
  }
#endif

  damdist = (torp->t_type == TPHOTON) ? DAMDIST : PLASDAMDIST;
  for (j = firstPlayer; j <= lastPlayer; j++) {
    if (j->p_status != PALIVE)
      continue;

    /*
     * Check to see if this player is safe for some reason  */
    if (is_idle(j))
        continue;
    if (j->p_no == torp->t_owner) {
      if (torp->t_attribute & TOWNERSAFE)
        continue;
    }
    else {
      if ((torp->t_attribute & TOWNTEAMSAFE) && (torp->t_team == j->p_team))
        continue;
    }
    if (j->p_flags & PFPRACTR) {
      if (torp->t_attribute & TPRACTICE)
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
    dx = spo(torp->t_x_internal - j->p_x_internal);
    if ((dx < -damdist) || (dx > damdist))
      continue;
    dy = spo(torp->t_y_internal - j->p_y_internal);
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
          ltd_update_torp_damage(&(players[torp->t_owner]), j, damage);
          break;
        case TPLASMA:
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
        if ((torp->t_whodet != NODET)
            && (torp->t_whodet != j->p_no)
            && (players[(int)torp->t_whodet].p_team != j->p_team))
          k = player_(torp->t_whodet);
        else
          k = player_(torp->t_owner);
        p_explosion(j, t_whydead(k, torp), k->p_no);
        if ((k->p_team != j->p_team) ||
            ((k->p_team == j->p_team) && (j->p_flags & PFPRACTR)))
        {
          killer_credit(k, j);
#ifndef LTD_STATS       /* ltd_update_kills automatically checks max kills */
          checkmaxkills(k->p_no);
#endif /* LTD_STATS */
        }

#ifndef LTD_STATS
        killerstats(k->p_no, torp->t_owner, j);
        loserstats(j->p_no);
#endif /* LTD_STATS */


#ifdef LTD_STATS

        /* k = killer to be credited
           torp->t_owner = killer who did the damage
           j = victim */

        if (status->tourn) {
          status->kills++;
          status->losses++;
        }

        ltd_update_kills(k, &players[torp->t_owner], j);
        ltd_update_deaths(j, k);

#endif /* LTD_STATS */

        killmess(j, k, player_(torp->t_owner),
                 (k->p_no != torp->t_owner) ?
                 ((torp->t_type == TPHOTON) ? KTORP2 : KPLASMA2)
                 : j->p_whydead);

      }
    }
  }
  torp->t_status = TEXPLODE;
  torp->t_fuse = 10 * T_FUSE_SCALE;
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
}

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

    if(opt_debug)printf("populating planet %d\n", plnum);

    if( plnum <0 || plnum >= MAXPLANETS) return; /* arg sanity */

    l = &planets[plnum];

    if (l->pl_armies == 0)
        return;

    if ((l->pl_armies >= max_pop) && !(status->tourn))
        return;

    if (planet_plague) {
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
    }

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

     if(opt_debug) printf("Randomizing planetary pop order!\n");

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

     if(opt_debug){
         printf("Planet pop order:");
         for(i=0;i<MAXPLANETS;i++)printf(" %d", pl_poporder[i]);
         printf("\n");
     }

     lastpop=MAXPLANETS-1;
}

/* new code TC */
static void udsurrend(void)
{
    register int i, t;
    struct planet *l;
    struct player *j;

    /* Decrement SB reconstruction timer code moved here (from udplanets()) */

    for (i = 0; i <= MAXTEAM; i++) {
        teams[i].te_turns--;
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
            (j->p_status != POBSERV) &&
            (j->p_team != NOBODY) &&
            (realNumShips(j->p_team) < tournplayers) &&
            (random()%5 == 0))
            rescue(TERMINATOR, j->p_no);
    }

    /*
     * Bump observers of non-Tmode races to selection screen to choose
     * T mode teams
     */
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
        if( (j->p_status == POBSERV) &&
            (j->p_team != NOBODY) &&
            (realNumShips(j->p_team) < tournplayers) &&
            !j->p_authorised) {
            j->p_status = PEXPLODE;
            j->p_whydead = KPROVIDENCE;
            j->p_flags &= ~(PFPLOCK | PFPLLOCK);
            j->p_x = -100000;         /* place me off the tactical */
            j->p_y = -100000;
            p_x_y_to_internal(j);
        }
    }

    for (t = 0; t <= MAXTEAM; t++) { /* maint: was "<" 6/22/92 TC */
        /* "suspend" countdown if less than Tmode players */
        if ((teams[t].te_surrender == 0) || (realNumShips(t) < tournplayers)) {
            teams[t].te_surrender_pause = TE_SURRENDER_PAUSE_ON_TOURN;
            continue;
        }

        /* also if team has more than surrenderStart planets */
        if (teams[t].te_plcount > surrenderStart) {
            teams[t].te_surrender_pause = TE_SURRENDER_PAUSE_ON_PLANETS ;
            continue;
        }

        teams[t].te_surrender_pause = TE_SURRENDER_PAUSE_OFF;
        teams[t].te_surrender--;
        teams[t].te_surrender_frame = context->frame;
        if (teams[t].te_surrender == 0) {

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
                    p_explosion(j, KPROVIDENCE, 0);
                }
            }                   /* end for */

        }                       /* end if */
        else {                  /* surrender countdown */
            if ((teams[t].te_surrender % 5) == 0) {
                pmessage(0, MALL, " ", " ");
            }
            if ((teams[t].te_surrender % 5) == 0 ||
                teams[t].te_surrender < 5) {
                pmessage(0, MALL, "GOD->ALL", "The %s %s %d minute%s remaining.",
                        team_name(t), team_verb(t),
                        teams[t].te_surrender, (teams[t].te_surrender == 1) ? "" : "s");
            }
            if ((teams[t].te_surrender % 5) == 0) {
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
                /* todo: fps support, use of a fuse */
                if (j->ph_fuse-- == 1)
                    j->ph_status = PHFREE;
                break;
            case PHHIT:
                if (j->ph_fuse-- == players[i].p_ship.s_phaserfuse) {
                    victim = player_(j->ph_target);

                    /* Check to see if this player is safe for some reason  */
                    if (is_idle(victim))
                        break;

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
                            p_explosion(victim, KPHASER, i);
                            killer_credit(&players[i], victim);
#ifndef LTD_STATS
                            killerstats(i, i, victim);
                            checkmaxkills(i);
                            killmess(victim, &players[i], &players[i],
                                     KPHASER);

                            loserstats(j->ph_target);
#endif /* LTD_STATS */

#ifdef LTD_STATS
                            /* i = credit killer = actual killer
                               victim = dead guy */

                            if (status->tourn) {
                              status->kills++;
                              status->losses++;
                            }

                            ltd_update_kills(&(players[i]), &(players[i]), victim);
                            ltd_update_deaths(victim, &players[i]);

                            killmess(victim, &players[i], &players[i], KPHASER);
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

static void teamtimers(void)
{
    register int i;
    for (i = 0; i <= MAXTEAM; i++) {
        if (tm_robots[i] > 0)
            tm_robots[i]--;
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
      p_explosion(j, KPLANET, l->pl_no);

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
      strcat(buf,"   [planet]");
      arg[5] = KPLANET;
      pmessage(0, MALL | MKILLP, "GOD->ALL", buf);

#ifdef LTD_STATS

      /* j = dead guy
         NULL = killed by planet */

      if (status->tourn) {
        status->losses++;
      }
      ltd_update_deaths(j, NULL);

#endif /* LTD_STATS */

    }
  }
}

static void plrescue(struct planet *l)
{
    /* Send in a robot if there are no other defenders (or not Tmode)
       and the planet is in the team's home space */

    if (!(inl_mode)) {
      if (((tcount[l->pl_owner] == 0) || NotTmode) &&
          (l->pl_flags & l->pl_owner) &&
#ifdef PRETSERVER
          !bot_in_game &&
#endif
          tm_robots[l->pl_owner] <= 0) {

        rescue(l->pl_owner, NotTmode);
        /* todo: fps support, use of a fuse */
        tm_robots[l->pl_owner] = (1800 + (random() % 1800)) / TEAMFUSE;
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
      /* todo: fps support, use of a fuse */
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
    }

    /* j hasn't killed anyone, but does get kill credit for bombing */

    ltd_update_kills_max(j);

    /* j = player who has bombed the planet
       l = planet that was just bombed
       ab = number of armies that were just bombed */

    ltd_update_bomb(j, l, ab);

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

    plrescue(l);
  }
}

static void beam(void)
{
    register int i;
    register struct player *j;
    register struct planet *l = NULL;
    char buf1[MSG_LEN];
#ifdef STURGEON
    int k = 0, n = 0;
    struct player *p;
#endif

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
                ltd_update_armies_carried(j, NULL);
#endif
            } else if (j->p_flags & PFDOCK) {
                struct player *base = bay_owner(j);
                base->p_armies--;
                army_track(AMT_TRANSUP, j, base, 1);

#ifdef LTD_STATS
                ltd_update_armies_carried(j, base);
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

                        /* credit the player for beamdown
                           j = player who just beamed down 1 army
                           l = planet that just lost 1 army */

                        ltd_update_armies(j, l);

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

                            ltd_update_planets(j, l);

#endif /* LTD_STATS */

                            /* out of Tmode?  Terminate.  8/2/91 TC */

                            if (NotTmode
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
                        if (status->tourn)
                        {
                            status->planets++;
                        }

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
#ifdef STURGEON
                        /* Free random upgrades for your team when you take planet */
                        if (sturgeon && sturgeon_planetupgrades) {
                          /* Don't want temporary or non-working upgrades */
                          do {
                            k = random() % UPG_OFFSET;
                          } while (k == UPG_TEMPSHIELD || k == UPG_CLOAK);
                          for (n = 0, p = &players[0]; n < MAXPLAYER; n++, p++) {
                            if (p->p_team != l->pl_owner || p->p_ship.s_type == STARBASE
                                || p->p_status == PFREE
                                || (sturgeon_maxupgrades && p->p_upgrades >= sturgeon_maxupgrades))
                              continue;
                            p->p_free_upgrade = k;
                          }
                          /* Now tell the team */
                          sprintf(buf1, "UPG->%-3s", teamshort[l->pl_owner]);
                          pmessage(l->pl_owner, MTEAM | MTAKE, buf1, "All teammates have gained a free %s upgrade", upgradename[k]);
                        }
#endif
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

                    ltd_update_armies(j, l);
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
                    ltd_update_armies_ferried(j, base);
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
    register struct player *k;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PALIVE)
            continue;
        if (sh == j)
            continue;
        /* the following keeps people from blowing up on others */
        me = sh;
        /* Check to see if this player is safe for some reason  */
        if (is_idle(j))
            continue;
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
#ifdef STURGEON
        if (sturgeon) {
            /* If you die with nukes on board, your explosion gets bonus damage */
            for (i = 0; i < NUMSPECIAL; i++) {
                if (sh->p_weapons[i].sw_type == SPECBOMB &&
                    sh->p_weapons[i].sw_number > 0)
                damage += (sh->p_weapons[i].sw_damage * 10 *
                sh->p_weapons[i].sw_number);
            }
        }
#endif
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
                if ((j->p_no != sh->p_whodead) && (j->p_team != players[sh->p_whodead].p_team)
                    && (sh->p_whydead == KSHIP ||  sh->p_whydead == KTORP ||
                        sh->p_whydead == KPHASER || sh->p_whydead == KPLASMA))
                  k = &players[sh->p_whodead];
                else
                  k = sh;
                if (k->p_no != sh->p_no)
                    p_explosion(j, KSHIP2, k->p_no);
                else
                    p_explosion(j, KSHIP, k->p_no);

                if ((k->p_team != j->p_team) ||
                    ((k->p_team == j->p_team) && !(j->p_flags & PFPRACTR))){
                  killer_credit(k, j);
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

#ifdef LTD_STATS

                if (status->tourn) {
                  status->kills++;
                  status->losses++;
                }

                /* k = killer to be credited
                   sh = ship explosion that dealt the killing damage
                   j = victim */

                ltd_update_kills(k, sh, j);

                /* j = victim
                   k = killer that was credited */

                ltd_update_deaths(j, k);

#endif /* LTD_STATS */

                killmess(j, k, sh, (k->p_no == sh->p_no) ? KSHIP : KSHIP2);
            }
        }
    }
}

static void exitDaemon(int sig)
{
    register int i;
    register struct player *j;

    if (sig) signal(sig,exitDaemon);
    if (sig!=SIGINT) {
        if(sig) {
            ERROR(2,("daemon: got signal %d\n", sig));
            fflush(stderr);
        }

        if (sig == SIGFPE) return;  /* Too many errors occur ... ignore them. */

        /* some signals can be ignored */
        if (sig == SIGCHLD || sig == SIGCONT)
            return;

        if (sig) {
            ERROR(1,( "daemon: going down on signal %d\n", sig));
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
            ERROR(8,("daemon: %s: sending SIGTERM\n", j->p_mapchars));
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
        ERROR(1,("daemon: cannot removed shared memory segment"));

#ifdef PUCK_FIRST
    if(pucksem_id != -1)
        if (semctl(pucksem_id, 0, IPC_RMID, pucksem_arg) == -1)
          ERROR(1,("daemon: cannot remove puck semaphore"));
#endif /* PUCK_FIRST */

    switch(sig){
        case SIGSEGV:
        case SIGBUS:
        case SIGILL:
        case SIGFPE:
#ifdef SIGSYS
        case SIGSYS:
#endif
            (void) signal(SIGABRT, SIG_DFL);
            abort();       /* get the coredump */
        default:
            break;
    }
    ERROR(1,("daemon: normal exit, pid %d\n", getpid()));
    exit(0);
}

static void save_planets(void)
{
    int plfd, glfd;

    plfd = open(PlFile, O_RDWR|O_CREAT, 0744);
    glfd = open(Global, O_RDWR|O_CREAT, 0744);
    if (plfd >= 0) {
        int psize = sizeof(struct planet) * MAXPLANETS;
        lseek(plfd, (off_t) 0, 0);
        write(plfd, (char *) planets, psize);
        close(plfd);
    }
    if (glfd >= 0) {
        lseek(glfd, (off_t) 0, 0);
        write(glfd, (char *) status, sizeof(struct status));
        close(glfd);
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
        teams[i].te_plcount = 0;

    for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
        teams[l->pl_owner].te_plcount++; /* for now, recompute here */
/*      if (l->pl_owner == loser)
            return;*/
        if (((l->pl_flags & ALLTEAM) == loser) && (l->pl_flags & PLHOME))
            homep = l;          /* Keep track of his home planet for marking */
    }

    /* check to initiate surrender */

    if ((teams[loser].te_surrender == 0) &&
        (teams[loser].te_plcount == surrenderStart)) {
        /* shorter for borgs? */
        /* start the clock 1/27/92 TC */
        int minutes = binconfirm ? SURRLENGTH : SURRLENGTH*2/3;
        blog_printf("racial", "%s collapsing\n\n"
                    "The %s %s %d minute%s before the empire collapses.  "
                    "%d planets are needed to sustain the empire.",
                    team_name(loser), team_name(loser), team_verb(loser),
                    minutes, (minutes == 1) ? "" : "s", SURREND);
        pmessage(0, MALL, " ", " ");
        pmessage(0, MALL, "GOD->ALL",
                "The %s %s %d minute%s before the empire collapses.",
                team_name(loser), team_verb(loser), minutes, (minutes == 1) ? "" : "s");
        pmessage(0, MALL, "GOD->ALL",
                "%d planets are needed to sustain the empire.",
                SURREND);
        pmessage(0, MALL, " ", " ");

        teams[loser].te_surrender = minutes;
        teams[loser].te_surrender_pause = TE_SURRENDER_PAUSE_OFF;
        teams[loser].te_surrender_frame = context->frame;
    }

    if (teams[loser].te_plcount > 0) return;

    teams[loser].te_surrender = 0;       /* stop the clock */
    teams[loser].te_surrender_frame = context->frame;

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
            p_explosion(j, KGENOCIDE, winner->p_no);
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

    teams[0].te_plcount = 0;
    for (i = 0; i < 4; i++) {
        team[1<<i] = 0;
        teams[1<<i].te_plcount = 0;
    }

    /* count the number of home systems owned by one team */
    for (i = 0; i < 4; i++) {
        teamtemp[NOBODY] = teamtemp[FED] = teamtemp[ROM] = teamtemp[KLI] =
            teamtemp[ORI] = 0;

        for (h = 0, l = &planets[i*10]; h < 10; h++, l++) {
            teams[l->pl_owner].te_plcount++;
            teamtemp[l->pl_owner]++;
        }

        /* count neutral planets towards conquer 3/24/92 TC */

        if ((teamtemp[FED]+teamtemp[ROM]+teamtemp[KLI]) == 0) team[ORI]++;
        else if ((teamtemp[FED]+teamtemp[ROM]+teamtemp[ORI]) == 0) team[KLI]++;
        else if ((teamtemp[FED]+teamtemp[KLI]+teamtemp[ORI]) == 0) team[ROM]++;
        else if ((teamtemp[ROM]+teamtemp[KLI]+teamtemp[ORI]) == 0) team[FED]++;
    }

    /* check to end surrender */

    if ((teams[winner->p_team].te_surrender > 0) &&
             (teams[winner->p_team].te_plcount == SURREND)) {
        blog_printf("racial", "%s recovered\n\n"
                    "The %s %s prevented collapse of the empire, "
                    "they now have %d planets.",
                    team_name(winner->p_team),
                    team_name(winner->p_team), team_verb(winner->p_team),
                    teams[winner->p_team].te_plcount);
        pmessage(0, MALL, " ", " ");
        pmessage(0, MALL, "GOD->ALL",
                "The %s %s prevented collapse of the empire.",
                team_name(winner->p_team), team_verb(winner->p_team));
        pmessage(0, MALL, " ", " ");
        teams[winner->p_team].te_surrender = 0; /* stop the clock */
        teams[winner->p_team].te_surrender_frame = context->frame;
    }

    for (i = 0; i < 4; i++) {
        if (team[1<<i] >= VICTORY) {
            conquerMessage(winner->p_team);
            conquer_begin(winner);
            return 1;
        }
    }

    /* pre-t round win check */
    if (!status->tourn) {
        if (status->gameup & GU_PRET) {
            int need = 10 + pret_planets - teams[winner->p_team].te_plcount;
            if (need < 1) {
                conquer_begin_pret(winner);
            } else {
                pmessage(0, MALL, "GOD->ALL",
                         "%s need %d more planet%s to win this round!",
                         team_name(winner->p_team), need,
                         (need == 1) ? "" : "s");
            }
        }
    }

    return 0;
}

static void ghostmess(struct player *victim, char *reason)
{
    static float        ghostkills = 0.0;
    int                 i,k;

    ghostkills += 1.0 + victim->p_armies * 0.1 + victim->p_kills * 0.1;
#ifdef STURGEON
    if (sturgeon && sturgeon_extrakills)
        ghostkills += victim->p_upgrades * 0.15;
#endif
                arg[0] = DGHOSTKILL; /* type */
                arg[1] = victim->p_no;
                arg[2] = 100 * ghostkills;
    pmessage(0, MALL, "GOD->ALL",
        "%s was kill %0.2f for the GhostBusters, %s",
        victim->p_longname, ghostkills, reason);
    /* if ghostbusting an observer do not attempt carried army rescue */
    if (victim->p_flags & PFOBSERV) return;
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

static char *whydeadmess[] = {
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
  "[zapped plasma]",   /* KPLASMA2     */
  " "};
#if 0
#define numwhymess (sizeof(whydeadmess)/sizeof(char *))
#endif

/*
 *
 */
static void killmess(struct player *victim, struct player *credit_killer,
              struct player *k, int whydead)
{
    char *why;
    char kills[20];

    if (credit_killer->p_team == k->p_team)
      credit_killer = k;

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
    if (recreational_dogfight_mode) {
        pmessage(0, MALL, "GOD->ALL",
            "WINNER: %s (%s) Shields: %d%%   Damage: %d%%   Fuel: %d%%",
                k->p_name,
                k->p_mapchars,
                (100*k->p_shield)/(k->p_ship.s_maxshield),
                (100*k->p_damage)/(k->p_ship.s_maxdamage),
                (100*k->p_fuel)/(k->p_ship.s_maxfuel));
        /* repair the winner */
        p_heal(k);
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

    if ((pid = fork()) == 0) {
        if (!opt_debug) {
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
        if (opt_debug)
                argv[argc++] =  "-d";
        argv[argc] = (char *) 0;
        execv(Robot, argv);
        perror("execv(Robot)");
        fflush(stderr);
        _exit(1);                /* NBT exit just in case a robot couldn't
                                    be started                              */
    } else {
        ERROR(8,( "daemon: forked rescue robot pid %d\n", pid));
    }
}

#include <sys/resource.h>

/* Don't fear the ... */

static void reaper(int sig)
{
    int pid, status;

    ERROR(1,("daemon: reaper on signal %d\n", sig));
    if (sig != SIGCHLD) return;

    while (1) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid == 0) return;
        if (pid < 0) {
            if (errno == ECHILD) return;
            perror("waitpid");
        }
        if (WIFEXITED(status)) {
            ERROR(1,("daemon: child %d exited, status=%d\n",
                     pid, WEXITSTATUS(status)));
        } else if (WIFSIGNALED(status)) {
            ERROR(1,("daemon: child %d killed by signal %d\n",
                     pid, WTERMSIG(status)));
#ifdef WCOREDUMP
            if (WCOREDUMP(status)) {
                ERROR(1,("daemon: child %d dumped core\n", pid));
            }
#endif
        } else if (WIFSTOPPED(status)) {
            ERROR(1,("daemon: child %d stopped by signal %d\n",
                     pid, WSTOPSIG(status)));
            continue;
        } else if (WIFCONTINUED(status)) {
            ERROR(1,("daemon: child %d continued\n", pid));
            continue;
        }
        if (pid == manager_pid) {
            ERROR(1,("daemon: manager died\n"));
            manager_pid = 0;
        }
    }
}

static void reaper_start()
{
  /* ask for the reaper to be called on SIGCHLD */
  struct sigaction action;
  action.sa_handler = reaper;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if (sigaction(SIGCHLD, &action, NULL) < 0) {
    perror("sigaction: SIGCHLD");
  }

  /* unblock the SIGCHLD signal, as netrekd normally blocks it */
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
    perror("sigprocmask: SIG_UNBLOCK SIGCHLD");
  }
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

static char conqfile_name[MSG_LEN];

static FILE *conqfile_open()
{
    struct timeval tv;
    FILE *conqfile;

    gettimeofday(&tv, (struct timezone *) 0);
    sprintf(conqfile_name, "%s.%d.txt", ConqFile, (int) tv.tv_sec);
    conqfile = fopen(conqfile_name, "w");
    if (conqfile == NULL) conqfile = stderr;
    return conqfile;
}

static int conqfile_close(FILE *conqfile)
{
    int status;

    if (conqfile == stderr) return 0;

    status = fclose(conqfile);
    if (status != 0) return status;

    blog_file("racial", conqfile_name);
    return 0;
}

static void surrenderMessage(int loser)
{
    FILE *conqfile;

    conqfile = conqfile_open();
    fprintf(conqfile, "%s %s surrendered\n\n", team_name(loser),
        team_verb(loser));
    conqfile_close(conqfile);
}

static void genocideMessage(int loser, int winner)
{
    char buf[MSG_LEN];
    FILE *conqfile;

    conqfile = conqfile_open();
    fprintf(conqfile, "%s %s been genocided by the %s\n\n",
            team_name(loser), team_verb(loser), team_name(winner));

    pmessage(0, MALL | MGENO, " ","%s",
        "***********************************************************");
    sprintf(buf, "The %s %s been genocided by the %s.", team_name(loser),
        team_verb(loser), team_name(winner));
    pmessage(0, MALL | MGENO, " ","%s",buf);

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
    conqfile_close(conqfile);
}

static void conquerMessage(int winner)
{
    char buf[MSG_LEN];
    FILE *conqfile;

    conqfile = conqfile_open();
    fprintf(conqfile, "Galaxy conquered by the %s\n\n", team_name(winner));

    pmessage(0, MALL | MCONQ, " ","%s",
        "***********************************************************");
    sprintf(buf, "The galaxy has been conquered by the %s:", team_name(winner));
    pmessage(0, MALL | MCONQ, " ","%s",buf);
    sprintf(buf, "The %s:", team_name(winner));
    pmessage(0, MALL | MCONQ, " ","%s",buf);
    displayBest(conqfile, winner, KWINNER);
    pmessage(0, MALL | MCONQ, " ","%s",
        "***********************************************************");
    fprintf(conqfile, "\n");
    conqfile_close(conqfile);
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
    memset(winners, 0, sizeof(Players) * (MAXPLAYER+1));
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
        strncpy(winners[k].mapchars, j->p_mapchars, 2);
        strncpy(winners[k].name, j->p_name, NAME_LEN);
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
   if ((manager_pid = fork()) == 0) {
      alarm_prevent_inheritance();
      switch (robot) {
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
        default: ERROR(1,("daemon: unknown robot: %d\n", robot ));
      }
      _exit(1);
   } else {
      ERROR(8,( "daemon: forked game manager pid %d\n", manager_pid));
   }
}

#ifdef PUCK_FIRST
static void do_nuttin (int sig) { }

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
            strcmp(j->p_name, "Puck") == 0) {
            /* Set semaphore to 0 in case there are multiple pucks. Yuck. */
            if (pucksem_id != -1 &&
                semctl(pucksem_id, 0, SETVAL, pucksem_arg) == -1) {
                perror("signal_puck semctl");
                pucksem_id = -1;
                /* are there any errors that would 'fix themselves?' */
            }
            if (alarm_send(j->p_process) < 0) {
                if (errno == ESRCH) {
                    ERROR(1,("daemon/signal_puck: slot %c missing\n", slot_char(i)));
                    freeslot(j);
                }
            } else if (pucksem_id != -1) {
                puckwait = 1;
            }
        }

    if (puckwait) {
        semop(pucksem_id, pucksem_op, 1);
    }
}
#endif /* PUCK_FIRST */

static void signal_servers(void)
{
    int i, t;
    struct player *j;
    static int counter;

    counter++;

    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE) continue;
        if (j->p_process <= 1) continue;

#ifdef PUCK_FIRST
        if (j->p_status != PFREE && j->w_queue == QU_ROBOT &&
            strcmp(j->p_name, "Puck") == 0) {
            continue;
        }
#endif /* PUCK_FIRST */

        t = j->p_fpu;
        if (!t) t = 5; /* paranoia */

        /* adding i to counter interleaves update signals to the
         ntserv processes: at 25 ups, 1/2 the processes are signalled
         on every update, at 10 ups, 1/5 are signalled, etc.  (Of
         course, if everyone is at 50 ups we have to signal them all
         every time) */

        if (t == 1 || (((counter + i) % t) == 0)) {
            if (alarm_send(j->p_process) < 0) {
                if (errno == ESRCH) {
                    ERROR(1,("daemon: signal_servers: slot %c missing\n",
                             slot_char(i)));
                    if (j->p_process == manager_pid) {
                        ERROR(1,("daemon: manager missing pid %d\n", manager_pid));
                        manager_pid = 0;
                    }
                    freeslot(j);
                }
            }
        }
    }
}

static void message_flag(struct message *cur, char *address)
{
    if (arg[0] != DINVALID) {
        cur->args[0] = arg[0];
        cur->args[1] = arg[1];
        cur->args[2] = arg[2];
        cur->args[3] = arg[3];
        cur->args[4] = arg[4];
        cur->args[5] = arg[5];
    } else {
        if (strstr(address, "GOD->") != NULL)
            /* has a real header */
            cur->args[0] = SVALID;
        else
            /* invalid to send over SP_S_WARNING or SP_S_MESSAGE HW */
            cur->args[0] = SINVALID;
    }
    arg[0] = DINVALID;
}

/*  Hey Emacs!
 * Local Variables:
 * c-file-style:"bsd"
 * End:
 */
