/*
 * main.c
 */
#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "defs.h"
#include INC_UNISTD
#include INC_SYS_WAIT
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "patchlevel.h"
#define SVERS
#include "version.h"
#ifdef SENDFLAGS
#include "../cflags.h"
#endif
#include "proto.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* file scope prototypes */
static void reaper(int);
static void printUsage(char *prog);
static void sendMotd(void);
static void sendConfigMsg(void);
static void printStats(void);
static void bans_check();

extern int ignored[MAXPLAYER];
int indie = 0;			/* always be indie 8/28/91 TC */
int living = 1;			/* our ship is living, reset by death */

int main(int argc, char **argv)
{
    int team, s_type, w_queue = 0;
    int pno;
    int usage = 0;
    int err = 0;
    char *name, *ptr;
    struct passwd *pwent;
    int callHost=0;
    time_t starttime;
    int i;
    extern void forceShutdown ();
    char pseudo[PSEUDOSIZE];  /* Was a global - MK 9/30/94 */

    getpath();	/* added 11/6/92 DRG */
    errors = open(Error_File, O_WRONLY | O_APPEND | O_CREAT, 0644);
    dup2(errors,2);
    setbuf(stderr, NULL);
    dup2(errors,1);
    setbuf(stdout, NULL);

    name = *argv++;
    argc--;
    if ((ptr = RINDEX(name,'/')) != NULL)
	name = ptr + 1;
    while (*argv) {
	if (**argv == '-')
	    ++*argv;
	else
	    break;

	argc--;
	ptr = *argv++;
	while (*ptr) {
	    switch (*ptr) {
	    case 'u': 
		usage++; 
		break;
	    case 'i': 
		indie++; 
		break; /* 8/28/91 TC */
            case 'q': w_queue=atoi(*argv); argv++; argc--; break;
	    case 's': 
		xtrekPort=atoi(*argv); 
		callHost=1; 
		argv++; 
		argc--; 
		break;
#ifdef OBSERVERS
	    case 'O':
		Observer = 1;
		clueVerified = 1;
		break;
#endif
	    case 'd':
		host = *argv;
		argc--;
		argv++;
		break;
	    default: 
		ERROR(1,("%s: unknown option '%c'\n", name, *ptr));
		err++;
		break;
	    }
	    ptr++;
	}
    }
    if (usage || err) {
	printUsage(name);
	exit(err);
    }
    /* accept host name from newstartd */
    if (argc > 0)
	host = argv[0];
    srandom((int)getpid() * time((time_t *) 0));

    /* avoid having daemon (started by openmem) inherit our socket */
    fcntl(0, F_SETFD, FD_CLOEXEC);

    /* this finds the shared memory information */
    openmem(1);
    do_message_post_set(do_check_command);
    readsysdefaults();

#ifdef FEATURE_PACKETS
    readFeatures();
#endif

    me = NULL;			/* UDP fix (?) */
    if (callHost) {
	if (!connectToClient(host,xtrekPort)) {
	    exit(0);
	}
    } else {
	sock=0;			/* newstartd gives us our connection on 0 */
	checkSocket();
	setNoDelay(sock);
	initClientData();	/* "normally" called by connectToClient() */
    }
    starttime=time(NULL);
    while (userVersion==0) {
	time_t t;
	/* Waiting for user to send his version number.
	 * We give him thirty seconds to do so...
	 */
	if (isClientDead()) {
	    ERROR(2,("ntserv/main.c: disconnect waiting for version packet from %s\n", host));
	    exit(1);
	}
	if (starttime+30 < time(&t)) {
	    ERROR(2,("ntserv/main.c: no version packet received from %s\n",
		     host));
	    exit(1);
	}
	socketPause(starttime+30-t);
	readFromClient();
    }
    if (!checkVersion()) {
        ERROR(2,("ntserv/main.c: bad version packet from %s\n", host));
	exit(1);
    }

    SIGNAL(SIGALRM, SIG_IGN);
    SIGNAL(SIGTERM, forceShutdown);
    SIGNAL(SIGPIPE, SIG_IGN);

    sendMotd();

    /* wait for a slot to become free */
    pno = findslot(w_queue, inet_ntoa(*(struct in_addr *)&remoteaddr));
    if (pno < 0) {
	/* trigger client's "Sorry, but you cannot play xtrek now.
	   Try again later." */
	struct badversion_spacket packet;
	packet.type = SP_BADVERSION;
	packet.why = 1;
	sendClientPacket (&packet);
	flushSockBuf ();
	ERROR(2,("ntserv/main.c: Quitting: No slot available on queue %d\n",w_queue));
	exit(1);
    }

    me = &players[pno];

    me->p_process = getpid ();
    me->p_timerdelay = defskip;

    me->p_mapchars[0] = 'X';
    me->p_mapchars[1] = shipnos[pno];
    me->p_mapchars[2] = '\0';
    me->p_whydead=KLOGIN;

    myship = &me->p_ship;
    mystats = &me->p_stats;
    lastm = mctl->mc_current;
    SIGNAL(SIGINT, SIG_IGN);
    (void) SIGNAL(SIGCHLD, reaper);

    /* erase prior slot login identity */
    me->p_login[0] = '\0';

    /* We set these so we won't bother updating him on the location of the
     *  other players in the galaxy which he is not near.  There is no
     *  real harm to doing this, except that he would then get more information
     *  than he deserves.
     * It is kind of a hack, but should be harmless.
     */
    me->p_x= -100000;	
    me->p_y= -100000;
    me->p_team=0;
    updateSelf(FALSE);	/* so he gets info on who he is */
    			/* updateSelf(TRUE) shouldn't be necessary */
    updateShips();
    updatePlanets();
    flushSockBuf();

    /* Get login name */

    if ((pwent = getpwuid(getuid())) != NULL)
	STRNCPY(login, pwent->pw_name, NAME_LEN);
    else
	STRNCPY(login, "Bozo", NAME_LEN);
    login[NAME_LEN - 1] = '\0';
    endpwent();

    strcpy(pseudo, "Guest");

    strcpy(me->p_name, pseudo);
    me->p_team=ALLTEAM;
    getname();
    strcpy(pseudo, me->p_name);

    ERROR(7,("%s: is %s@%s as %s\n", me->p_mapchars, login, host, pseudo));

    keeppeace = (me->p_stats.st_flags / ST_KEEPPEACE) & 1;
/*    showgalactic = 1 + (me->p_stats.st_flags / ST_GALFREQUENT) & 1;*/

    /* Set p_hostile to hostile, so if keeppeace is on, the guy starts off
       hating everyone (like a good fighter should) */
    me->p_hostile = (FED|ROM|KLI|ORI);
    me->p_war = me->p_hostile;

    s_type = CRUISER;
    me->p_planets=0;
    me->p_genoplanets=0;
    me->p_armsbomb=0;
    me->p_genoarmsbomb=0;
    me->p_candock = 1;

    STRNCPY(me->p_login, login, NAME_LEN);
    me->p_login[NAME_LEN - 1] = '\0';

    {
	int i;
	for (i = 0; i < MAXPLAYER; i++) ignored[i] = 0;
    }

    /* store client IP address from socket peer obtained by checkSocket() */
    if (remoteaddr != -1)
      strcpy(me->p_ip, inet_ntoa(*(struct in_addr *)&remoteaddr));

#ifdef REVERSED_HOSTNAMES
    if (strlen(host) >= NAME_LEN) {
      STRNCPY(me->p_monitor, host + (strlen(host) - NAME_LEN + 1), NAME_LEN);
      /* The # denotes truncation */
      me->p_monitor[0] = '#';
    }
    else
#endif
    STRNCPY(me->p_monitor, host, NAME_LEN);
    me->p_monitor[NAME_LEN - 1] = '\0';

#ifdef FULL_HOSTNAMES
    /* assume this is only place p_monitor is set, and mirror accordingly */
    /* 4/13/92 TC */
    STRNCPY(me->p_full_hostname, host, sizeof(me->p_full_hostname));
    me->p_full_hostname[sizeof(me->p_full_hostname) - 1] = '\0';
#endif

    logEntry(); /* moved down to get login/monitor 2/12/92 TMC */

#ifdef NO_HOSTNAMES
    strcpy(me->p_full_hostname, "hidden");
    strcpy(me->p_monitor, "hidden");
    strcpy(me->p_login, "anonymous");
#endif

#ifdef CONTINUUM_COMINDICO
    /* 2005-01-26 temporary address hiding for me */
    if (strstr(me->p_full_hostname, "comindico.com.au")) {
      whitelisted = 1;
    }
#endif

    if (whitelisted) {
      strcpy(me->p_full_hostname, "hidden");
      strcpy(me->p_monitor, "hidden");
    }

#ifdef PING     /* 0 might just be legit for a local player */
    me->p_avrt = -1;
    me->p_stdv = -1;
    me->p_pkls_c_s = -1;
    me->p_pkls_s_c = -1;
#endif

#ifdef LTD_STATS

    startTkills   = ltd_kills(me, LTD_TOTAL);
    startTlosses  = ltd_deaths(me, LTD_TOTAL);
    startTarms    = ltd_armies_bombed(me, LTD_TOTAL);
    startTplanets = ltd_planets_taken(me, LTD_TOTAL);
    startTticks   = ltd_ticks(me, LTD_TOTAL);

    startSBkills  = ltd_kills(me, LTD_SB);
    startSBlosses = ltd_deaths(me, LTD_SB);
    startSBticks  = ltd_ticks(me, LTD_SB);

#else

    startTkills = me->p_stats.st_tkills;
    startTlosses = me->p_stats.st_tlosses;
    startTarms = me->p_stats.st_tarmsbomb;
    startTplanets = me->p_stats.st_tplanets;
    startTticks = me->p_stats.st_tticks;

    startSBkills = me->p_stats.st_sbkills;
    startSBlosses = me->p_stats.st_sblosses;
    startSBticks = me->p_stats.st_sbticks;

#endif /* LTD_STATS */

    for (;;) {
	/* give the player the motd and find out which team he wants */
    if (me->p_status != PALIVE) {
	me->p_x= -100000;
	me->p_y= -100000;
	updateSelf(FALSE);	/* updateSelf(TRUE) isn't necessary */
	updateShips();
	teamPick= -1;
	flushSockBuf();
	getEntry(&team, &s_type);
	repCount=0;		/* Make sure he gets an update immediately */
    }
    if (team == -1) {
	exitGame();
    }
    if(CheckBypass(login,host,Bypass_File)==TRUE)
      bypassed=1;
    else bypassed=0;


    if (indie) team = 4;	/* force to independent 8/28/91 TC */
    redrawall = 1;

    for (i = 0; i <= MAX_CP_PACKETS; i++)
	FD_SET (i, &inputMask);	/* Allow all input now */

    enter(team, 0, pno, s_type, pseudo);
   
#ifndef DEBUG
    for (i = 1; i < NSIG; i++) {
	SIGNAL(i, SIG_IGN);
    }
#endif   

/* +++ 2.6pl2 kantner@hot.caltech.edu 22-NOV-1994 09:46:00.74 */
    /*
    **  The daemon sends ntserv a SIGTERM after the slot is freed during a
    **  ghostbust.  Without this line the SIGTERM does nothing.  The slot is
    **  marked free, but really isn't.  Someone else joins the slot, and a
    **  copilot is formed.
    */
    SIGNAL(SIGTERM, forceShutdown);
/* --- */
    SIGNAL(SIGILL, SIG_DFL);		/* since illegal instruction is
					   so rare this is an useful one to
					   use to make core files for debugging
					   ie.  kill -SIGILL <process #> */
#ifdef OBSERVERS
    if (Observer) {
	me->p_status = POBSERV;     /* put observer in game */
        new_warning(UNDEF,"Lock onto a teammate or planet to see the action.");
        pmessage(me->p_no, MINDIV, addr_mess(me->p_no,MINDIV),
		 "Lock onto a teammate or planet to see the action.");
#ifdef CONTINUUM_MUTING
	/* 2005-01-26 temporary default observer muting, for all but me */
	if (!whitelisted) {
	  mute = 1;
	  pmessage(me->p_no, MINDIV, addr_mess(me->p_no,MINDIV),
		   "Policy: observers may not speak.");
	}
#endif
      }
    else
#endif
    me->p_status = PALIVE;			/* Put player in game */
    me->p_ghostbuster = 0;
    bans_check();

    /* Get input until the player quits or dies */
    living++;
    while (living) input();
    }
}

void exitGame(void)
{
    char addrbuf[20];

    if (me != NULL && me->p_team != ALLTEAM) {
        sprintf(addrbuf, " %c%c->ALL",
	   teamlet[me->p_team], shipnos[me->p_no]);
#ifndef FULL_HOSTNAMES
	/* old-style leaving message 4/13/92 TC */
	pmessage2(0, MALL | MLEAVE, addrbuf, me->p_no,
	    "%s %s (%s) leaving the game (%.16s@%.16s)", 
	    ranks[me->p_stats.st_rank].name,
	    me->p_name,
	    me->p_mapchars, 
	    me->p_login,
	    me->p_monitor);
#else
	/* new-style leaving message 4/13/92 TC */
	pmessage2(0, MALL | MLEAVE, addrbuf, me->p_no, 
	    "%s %s (%s) leaving game (%.16s@%.32s)", 
	    ranks[me->p_stats.st_rank].name,
	    me->p_name,
	    me->p_mapchars, 
	    me->p_login,
	    me->p_full_hostname);
#endif
	me->p_stats.st_flags &= ~ST_CYBORG; /* clear this flag 8/27/91 TC */
	savestats();
	printStats();
    }

    if (me) {
	if(me->p_process != getpid()){
            ERROR(1,( "main/exitGame: process exiting from co-pilot!\n"));
            ERROR(1,( "%d != pid: %d\n", (int) me->p_process, (int) getpid()));
            fflush(stderr);
            exit(0);
        }
	freeslot(me);
    }

    exit(0);
}
    

static void printUsage(char *prog)
{
    char *text = "\
Usage: %s [-u] [-q n] [-i] [-O] [-s n] [-d a]\n\
    -u       displays program usage\n\
    -q n     specifies queue number to join\n\
    -i       player is to be independant\n\
    -O       player is to be an observer\n\
\n\
Manual Connection\n\
    -s n     call client on socket number 'n'\n\
    -d a     call client on machine address 'a'\n";
    
    ERROR(1,(text,prog));
}

static void sendMotd(void)
{
    FILE *motd;
    char buf[100];	/* big enough... */
    char motd_file[255];

    time_t curtime;
    struct tm *tmstruct;
    int hour;

#ifdef SENDPATCHLEVEL
    sprintf(buf, "Welcome to %s, patchlevel %d", serv_version, PATCHLEVEL);
    sendMotdLine(buf);
#endif
#ifdef SENDFLAGS
    sendMotdLine("Compiler option flags used for this server:");
    sendMotdLine(cflags);
    sendMotdLine(" ");
#endif
    time(&curtime);
    tmstruct = localtime(&curtime);
    if (!(hour = tmstruct->tm_hour%12)) hour = 12;
    sprintf(buf,"Connection to server established at %d:%02d%s.", hour,
	    tmstruct->tm_min,
	    tmstruct->tm_hour >= 12 ? "pm" : "am");
    if (!time_access()) {
	strcat(buf, "  WE'RE CLOSED.  SEE HOURS BELOW.");
        sendMotdLine(buf);
        sendMotdLine(" ");
    }
    strcpy(motd_file,Motd_Path);
    if (clue) 				/* added 2/6/93 NBT */
	strcat(motd_file,N_MOTD_CLUE);
    else strcat(motd_file,Motd);
 
    /* the following will read a motd */
    if ((motd = fopen(motd_file, "r")) != NULL) {
	while (fgets(buf, sizeof (buf), motd) != NULL) {
	    buf[strlen(buf)-1] = '\0';
	    sendMotdLine(buf);
	}
	(void) fclose(motd);
    }
    if (show_sysdef)
        sendConfigMsg();
}

static void sendConfigMsg(void)
{
    register int i, flag;
    char buf[100];      /* big enough... */
    extern char *shiptypes[];

    /* ATM: send current configuration info as well */
    sendMotdLine(STATUS_TOKEN); /* indicate start of config info */

    if (clue)
	sendMotdLine("Clue verification enabled");

    sprintf(buf, "%-30s: ", "Binary Verification");
    switch (binconfirm) {
    case 0:
        strcat(buf, "disabled");
        break;
    case 1:
        strcat(buf, "enabled, RSA only");
        break;
    case 2:
        strcat(buf, "enabled, RSA and reserved.c");
        break;
    }
    sendMotdLine(buf);

    if (tournplayers > 8)
        sprintf(buf, "%-30s: disabled", "Tournament Mode");
    else
        sprintf(buf, "%-30s: %d players / side","Tournament Mode",tournplayers);
    sendMotdLine(buf);

    sprintf(buf, "%-30s: ", "Ships Allowed");
    for (i = 0; i < NUM_TYPES; i++) {
        if (i == ATT) continue;
        if (shipsallowed[i]) {
            strcat(buf, shiptypes[i]);
            strcat(buf, " ");
        }
    }
    sendMotdLine(buf);

    sprintf(buf, "%-30s: %s", "Tractor/Pressor Beams",
        weaponsallowed[WP_TRACTOR] ? "enabled" : "disabled");
    sendMotdLine(buf);

    sprintf(buf, "%-30s: %s", "Plasma Torpedoes",
        weaponsallowed[WP_PLASMA] ? "enabled" : "disabled");
    sendMotdLine(buf);

    if (weaponsallowed[WP_PLASMA]) {
        sprintf(buf, "%-30s: %d", "Kills Required for Plasma", plkills);
        sendMotdLine(buf);
    }

#ifdef nodef
    sprintf(buf, "%-30s: %s", "Scanning Beams",
        weaponsallowed[WP_SCANNER] ? "enabled" : "disabled");
    sendMotdLine(buf);
#endif
    if (shipsallowed[STARBASE]) {
        sprintf(buf, "%-30s: %s (%d)", "Rank Required for SB",
                ranks[sbrank].name, sbrank);
	sendMotdLine(buf);	
    }

    sprintf(buf, "%-30s: %d", "Planets Required for SB", sbplanets);
    sendMotdLine(buf);

    sprintf(buf, "%-30s: ", "Hidden Mode");
    switch (hiddenenemy) {
    case 0:
        strcat(buf, "inactive");
        break;
    case 1:
        strcat(buf, "tournament only");
        break;
    case 2:
        strcat(buf, "active");
        break;
    }
    sendMotdLine(buf);

    flag = 0;           /* see if non-home worlds are allowable entry points */
    for (i = 0; i < MAXPLANETS; i++) {
        if (startplanets[i] && (i % 10)) {
            flag = 1;
            break;
        }
    }
    sprintf(buf, "%-30s: %s", "Multiple Entry Planets", flag ? "yes" : "no");
    sendMotdLine(buf);

    sprintf(buf, "%-30s: %s", "Chaos Mode", chaos ? "enabled" : "disabled");
    sendMotdLine(buf);



#ifdef TORPCONFIG
    sprintf(buf, "%-30s: %s", "Vector Torps", vectortorps ? "yes" :
       "no");
    sendMotdLine(buf);
#endif

    sprintf(buf,"%-30s: ", "Hunter Killer");
    if (killer)
      strcat(buf, "Yes");
    else 
      strcat(buf, "No");
    sendMotdLine(buf);

#ifdef CHECKMESG
    sprintf(buf, "%-30s: ", "Message to GOD Log");
    if(loggod)
      strcat(buf, "Yes");
    else
      strcat(buf, "No");
    sendMotdLine(buf);
#endif

#ifdef SB_TRANSWARP
   sprintf(buf, "%-30s: ", "SB Transwarp");
   if (twarpMode)
      strcat(buf, "Yes");
   else
      strcat(buf, "No");
   sendMotdLine(buf);
#else
   sprintf(buf, "%-30s: No", "SB Transwarp");
   sendMotdLine(buf);
#endif

   sprintf(buf, "%-30s: %d", "Surrender Counter", surrenderStart);
   sendMotdLine(buf);
}

int CheckBypass(char *login, char *host, char *file)
{
  FILE  *bypassfile;
  char  log_buf[64], host_buf[64], line_buf[160];
  char  *position;
  int   num1; 
  int Hits=0;

  if ((bypassfile = fopen(file, "r")) == NULL) {
    ERROR(1,( "No bypass file %s\n", file));
    fflush(stderr);
    return(FALSE);
  }
  while(fgets(line_buf, 160, bypassfile) != NULL) {
    /* Split line up */
    if((*line_buf=='#')||(*line_buf=='\n'))
      continue;
    if ((position = (char *) RINDEX(line_buf, '@')) == 0) {
      ERROR(1,( "Bad line in bypass file\n"));
      fflush(stderr);
      continue;
    }

    num1 = position - line_buf;
    STRNCPY(log_buf, line_buf, num1); /* copy login name into log_buf */
    log_buf[num1] = '\0';
    STRNCPY(host_buf, position + 1, 64); /* copy host name into host_buf */
    /* Cut off any extra spaces on the host buffer */
    position = host_buf;
    while (!isspace((int) (*position)))
      position++;
    *position = '\0';

    /*
    ERROR(1,( "Login: <%s>; host: <%s>\n", login, host));
    ERROR(1,("    Checking Bypass <%s> and <%s>.\n",log_buf,host_buf));
    */

    if (*log_buf=='*')
      Hits=1;
    else if (!strcasecmp(login, log_buf)){
      Hits=1;
    }
    if(Hits==1)
      {
        if (*host_buf == '*'){  /* Bypass any host */
          Hits++;
          break;                /* break out now. otherwise Hits will get reset
                                   to one */
        }
        else if (!strcasecmp(host,host_buf) ||   /* Bypass subdomains */
	        (strstr(host,host_buf)!=NULL)) { /* (eg, "*@usc.edu" */
          Hits++;
          break;                /* break out now. otherwise Hits will get reset
                                   to one */
        }
      }
  }
  fclose(bypassfile);
  if(Hits>=2)
    {
      /*
      ERROR(1,("Bypassing %s@%s\n",login,host));
      fflush(stderr);
      */
      return(TRUE);
    }
  else{
    /*
    ERROR(1,("NOT Bypassing %s@%s\n",login,host));
    */
    return(FALSE);
  }
}

/*ARGSUSED*/		/* some systems pass the signal when caught */
static void reaper(int sig)
{
    WAIT_TYPE stat=0;
    static int pid;

    MZERO( &stat, sizeof(WAIT_TYPE) );

    while ((pid = WAIT3(&stat, WNOHANG, 0)) > 0) ;
    HANDLE_SIG(SIGCHLD,reaper);

/* added the below code to catch the reason for a child dying - NBT 9/28/92 */
	if (!WIFEXITED(stat)) 
		ERROR(1,("Process # %d signal caught\n",pid));
	if (WEXITSTATUS(stat)) ERROR(1,("exited with state %d\n",stat));
	if (WIFSIGNALED(stat)) ERROR(1,("exited because of signal\n"));
	if (WTERMSIG(stat)) ERROR(1,("signal state %d\n",stat));
#ifdef WCOREDUMP
	if (WCOREDUMP(stat)) ERROR(1,("core image made\n"));
#endif
	if (WIFSTOPPED(stat)) ERROR(1,("stopped\n"));
	if (WSTOPSIG(stat)) ERROR(1,("stop signal was %d\n",stat));
}

static void printStats(void)
{
    FILE *logfile;
    time_t curtime;

    logfile=fopen(LogFileName, "a");
    if (!logfile) return;
    curtime=time(NULL);
    fprintf(logfile, "Leaving: %-16s (%c%c) %3dP %3dA %3dW/%3dL %3dmin %2d\n\t<%s@%s> %s", 
	    me->p_name, 
	    teamlet[me->p_team],
	    shipnos[me->p_no],
#ifdef LTD_STATS
            ltd_planets_taken(me, LTD_TOTAL) - startTplanets,
            ltd_armies_bombed(me, LTD_TOTAL) - startTarms,
            ltd_kills(me, LTD_TOTAL) - startTkills,
            ltd_deaths(me, LTD_TOTAL) - startTlosses,
            (ltd_ticks(me, LTD_TOTAL) - startTticks)/600,
#else
	    me->p_stats.st_tplanets - startTplanets,
	    me->p_stats.st_tarmsbomb - startTarms,
	    me->p_stats.st_tkills - startTkills,
	    me->p_stats.st_tlosses - startTlosses,
	    (me->p_stats.st_tticks - startTticks)/600,
#endif /* LTD_STATS */
	    numPlanets(me->p_team),
	    me->p_login,
#ifndef FULL_HOSTNAMES
	    me->p_monitor,
#else
	    me->p_full_hostname,
#endif
	    ctime(&curtime));
    fclose(logfile);
}

/* check temporary bans voted */
static int bans_check_temporary(char *ip)
{
  int i;
  for(i=0; i<MAXBANS; i++) {
    struct ban *b = &bans[i];
    if (b->b_expire) {
      if (!strcmp(b->b_ip, ip)) {
	ERROR(2,( "ban of %s has been probed\n", b->b_ip));
	b->b_expire += ban_vote_offset;
	return TRUE;
      }
      b->b_expire--;
      if (!b->b_expire) {
	ERROR(2,( "ban of %s has expired\n", b->b_ip));
      }
    }
  }
  return FALSE;
}

/* check permanent bans in file */
static int bans_check_permanent(char *login, char *host)
{
  FILE  *bannedfile;
  char  log_buf[64], host_buf[64], line_buf[160];
  char  *position;
  int   num1;
  int   Hits=0;                 /* Hits==2 means we're banned */

  if ((bannedfile = fopen(Banned_File, "r")) == NULL) {
    ERROR(1,( "No banned file %s\n", Banned_File));
    fflush(stderr);
    return(FALSE);
  }
  while(fgets(line_buf, 160, bannedfile) != NULL) {
    /* Split line up */
    if((*line_buf=='#')||(*line_buf=='\n'))
      continue;
    if ((position = (char *) RINDEX(line_buf, '@')) == 0) {
      ERROR(1,( "Bad line in banned file\n"));
      fflush(stderr);
      continue;
    }
    num1 = position - line_buf;
    STRNCPY(log_buf, line_buf, num1); /* copy login name into log_buf */
    log_buf[num1] = '\0';
    STRNCPY(host_buf, position + 1, 64); /* copy host name into host_buf */
    /* Cut off any extra spaces on the host buffer */
    position = host_buf;
    while (!isspace(*position))
      position++;
    *position = '\0';

    /*
      ERROR(1,( "Login: <%s>; host: <%s>\n", login, host));
    ERROR(1,("    Checking Banned <%s> and <%s>.\n",log_buf,host_buf));
    */
    if(*log_buf=='*')

      Hits=1;
    else if (!strcmp(login, log_buf))
      Hits=1;
    if(Hits==1)
      {
        if (*host_buf == '*'){  /* Lock out any host */
          Hits++;
          break;                /* break out now. otherwise Hits will get reset
                                   to one */
        }
        else if(strstr(host,host_buf)!=NULL){ /* Lock out subdomains (eg, "*@usc
.edu" */
          Hits++;
          break;                /* break out now. otherwise Hits will get reset
                                   to one */
        }
        else if (!strcmp(host, host_buf)){ /* Lock out specific host */
          Hits++;
          break;                /* break out now. otherwise Hits will get reset
                                   to one */
        }
      }
  }
  fclose(bannedfile);
  if(Hits>=2)
    return(TRUE);
  else
    return(FALSE);
}

static void bans_log()
{
  FILE *logfile;
  time_t curtime;
  
  logfile = fopen(LogFileName, "a");
  if (logfile) {
    curtime = time(NULL);
    fprintf(logfile, "Banned: %s (%s@%s), (%c), %s",
	    me->p_name, me->p_login, host, shipnos[me->p_no], ctime(&curtime)
	    );
    fclose(logfile);
  }
}

static void bans_enforce_alive()
{
  new_warning(UNDEF,"You are banned from the game, goodbye.");
  me->p_explode=100;
  me->p_status=PEXPLODE;
  me->p_whydead=KQUIT;   /* make him self destruct */
  me->p_whodead=0;
}

static void bans_enforce_observ()
{
  mute = 1;
  new_warning(UNDEF,"You are banned from the game, you may not speak.");
}

static void bans_enforce(char *reason)
{
  pmessage(0, MALL, "GOD->ALL","%s (%s@%s) is %s.",
	   me->p_name, me->p_login, me->p_ip, reason);
  bans_log();
  if (me->p_status == PALIVE) {
    bans_enforce_alive();
  } else if (me->p_status == POBSERV) {
    bans_enforce_observ();
  }
}

static void bans_check()
{
  if (!bypassed) {
    char *reason = NULL;
    if (bans_check_permanent(login, host) == TRUE) {
      reason = "banned by the administrator";
    } else if (bans_check_permanent(login, me->p_ip) == TRUE) {
      reason = "banned by the administrator";
    } else if (bans_check_temporary(me->p_ip) == TRUE) {
      reason = "banned by the players";
    }
    if (reason != NULL) {
      bans_enforce(reason);
    }
  }
}

/* ARGSUSED */
void message_flag(struct message *cur, char *address)
{
    /* This is to prevent false sending with SP_S_WARNING */
    cur->args[0] = DINVALID;
}
