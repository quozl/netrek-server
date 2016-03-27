/*
 * main.c
 */
#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#ifdef RO
#include <setjmp.h>
#endif
#include "Wlib.h"
#include "defs.h"
#include <time.h>
/*
 * Why is the following ifndef's?
 */
#ifndef hpux
#include <sys/wait.h>
#endif /*hpux*/

#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"
#include "version.h"
#include "patchlevel.h"

char *getrf();
char *shrink();

#ifdef hpux
void setbuffer(FILE *, char *, int);
#endif
#ifdef Solaris
void setbuffer(FILE *, char *, int);
#endif


extern void	intrupt_setflag();		/* input.c */

main(argc, argv)
int argc;
char **argv;
{
    int intrupt();
    char *host = NULL;
    int usage = 0;
    int err = 0;
    char *defaultsFile=NULL;
    char *name, *ptr;
    FILE	*fi;

    name = *argv++;
    argc--;

#ifdef NBR
    getpath();
#endif

    if ((ptr = strrchr(name, '/')) != NULL)
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
	    case 'u': usage++; break;
	    case 'f':
		recfile = *argv;
		if(!(fi=fopen(recfile, "r"))){
		  perror(recfile);
		  exit(1);
	        }
		fclose(fi);
		playback = 1;
		argc --;
		argv++;
		break;
	    case 'd':
		host = *argv;
		argc--;
		argv++;
		break;
            case 'r':
                defaultsFile = *argv;
                argc--;
                argv++;
                break;
	    default: 
		fprintf(stderr, "xsg: unknown option '%c'\n", *ptr);
		err++;
		break;
	    }
	    ptr++;
	}
    }
    initDefaults(defaultsFile);
    reinitPlanets=1;		/* reinit planet table */
    if (usage || err) {
	printUsage(name);
	exit(err);
    }

    /* this creates the necessary X windows for the game */
    newwin(host, name);

    openmem();

    lastm = mctl->mc_current;

    mapAll();

    /* get interesting defaults */
    msgBeep = booleanDefault("msgbeep", msgBeep);
    showShields = booleanDefault("showshields", showShields);
    reportKills = booleanDefault("reportkills", reportKills);
    show_xsg_posn = booleanDefault("showposn", show_xsg_posn);
    mapfire = booleanDefault("mapfire", mapfire);	/* TSH 2/93 */
    runclock = booleanDefault("clock", runclock);	/* TSH 2/93 */
    record = booleanDefault("recordenable", record);    /* JAC 11/94 */
    if ((ptr = getdefault("mapmode")) != NULL)
	mapmode = atoi(ptr);
    if ((ptr = getdefault("showlocal")) != NULL)
	showlocal = atoi(ptr);
    if ((ptr = getdefault("showgalactic")) != NULL)
	showgalactic = atoi(ptr);
    if ((ptr = getdefault("updatespeed")) != NULL)
	updateSpeed = atoi(ptr);
    if ((ptr = getdefault("recordspeed")) != NULL)
	recordSpeed = atoi(ptr);
    if ((ptr = getdefault("godsname")) != NULL){
	strncpy(godsname, ptr, 32);
	godsname[31] = '\0';
    }
    else
	strcpy(godsname, "GOD");

    if ((ptr = getdefault("frameskip")) != NULL){
	frameskip = atoi(ptr);
	if(frameskip == 0) frameskip = 1;
    }
    if ((ptr = getdefault("allocframes")) != NULL)
        allocFrames = atoi(ptr);
      
    if(playback)
	updateSpeed = recordSpeed;

    redrawall = 1;
    W_ClearWindow(w);

    initData();				/* init me-> stuff */

    if((UPS(recordSpeed) % UPS(updateSpeed)) != 0){	/* sanity check */
      /*  hmm... not technically an error yet
      fprintf(stderr, "update speed must be divisible by record speed.\n");
      */
      recordSpeed = 3;	/* 1 per second */
    }

    signal(SIGALRM, intrupt_setflag);

    updatetimer();
    input();
}

printUsage(prog)
char	*prog;
{
    printf("Usage: %s [-d display-name] [-r defaultsfile] [-f playback-file]\n",prog);
}

initData()
{
    int i, mbs;

    /* create phony "me" */
    me = (struct player *) malloc(sizeof(struct player));
    memset(me, 0, sizeof(struct player));		/* TSH 2/93 */
    me->p_no = -1;
    me->p_x = me->p_y = GWIDTH/2;
    me->p_team = NOBODY;

    me->p_ship.s_maxspeed = 40;	/* anything faster is just a blur */

    /* create "universe" from shared memory segment */
    memcpy(universe.players, players, sizeof(universe.players));

    if(playback)	/* no information until first frame read */
        return;

    /* first time through, display all of the recent messages */
    mbs = msgBeep;
    msgBeep = 0;

    for (i=1+mctl->mc_current; i<MAXMESSAGE; i++) {
       /* potentially exploring uncharted territory here */
	if(messages[i].m_flags & MVALID)
	   dmessage(i);
    }
    oldmctl=mctl->mc_current;
    for (i=0; i<=oldmctl; i++) {
        dmessage(i);
    }
    msgBeep = mbs;
}


/*
 * This runs off of the interval timer; it asynchronously updates the
 * map and tactical displays.
 */
intrupt()
{
    showVersion();

    if(playback){
       if(udcounter == 30) showVersion2();
       else {
	  if(playMode & DO_STEP) {
	    if(playMode & T_FF) {
	      register i;
	      for(i=0; i< frameskip; i++){
	          if(loadframe() == -1)
	             shutdown("playback error.");
	      }
            }
	    else if(loadframe() == -1){
	       shutdown("playback error.");
	    }
	  }
       }
    }

    /* stop updating screen */
    if(playback) if (!(playMode & DO_STEP)) return;
    
    /* exit as soon as the daemon does, or we could see old shmem segment */
    if (!status->gameup & GU_GAMEOK) shutdown("daemon terminated game");

    udcounter++;
    checkUpdates();		/* set updatePlayer and redrawPlayer arrays */

    move();			/* move us across galaxy */
    redraw();
    if (udcounter & 0x1) plstat2();	/* update every other time */
    if (reinitPlanets) {
	initPlanets();
	reinitPlanets=0;
    }

    if(record && (udcounter % (UPS(updateSpeed)/UPS(recordSpeed)) == 0))
       dumpframe();

    if(playback && extracting)
       dumpframe();

    /* every 20 updates, check to see if the shmem segment is gone */
    if (!playback && (udcounter % 20) == 0){		/* TSH 2/93 */
      if(shmget(PKEY, 0, 0) < 0){
	 shutdown("daemon exited.");
      }
    }
    
    /* look for & display new messages */
    if((!playback)|(playMode & FORWARD))checkMsgs();

    W_Flush();
    
    if(playback) if(playMode & SINGLE_STEP) playMode &= ~DO_STEP;
}


/*
 * If a player has been changed (new position, different ship type, etc),
 * set updatePlayer[] and/or redrawPlayer[].  This is normally done within
 * socket.c in the client.
 *
 * This also handles the new updatePlanet array.
 */
checkUpdates()
{
    struct player *old, *new;
    struct planet *opl, *npl;
    int i;

    old = universe.players;		/* local copy */
    new = players;			/* shared mem */

    for (i = 0; i < MAXPLAYER; i++, old++, new++) {
	if (	old->p_kills != new->p_kills ||
		strcmp(old->p_name, new->p_name) ||
		strcmp(old->p_monitor, new->p_monitor) ||
		strcmp(old->p_login, new->p_login) ||
		memcmp(&old->p_stats, &new->p_stats, sizeof(struct stats))
		) {
	    updatePlayer[i] = 1;
	}

	if (	old->p_dir != new->p_dir ||
		old->p_speed != new->p_speed ||
		old->p_x != new->p_x ||
		old->p_y != new->p_y ||
		old->p_flags != new->p_flags
		) {
	    redrawPlayer[i] = 1;
	}

	if (	old->p_status != new->p_status ||
		old->p_swar != new->p_swar ||
		old->p_hostile != new->p_hostile ||
		memcmp(&old->p_ship, &new->p_ship, sizeof(struct ship)) ||
		old->p_team != new->p_team
		) {
	    updatePlayer[i] = redrawPlayer[i] = 1;
	}
    }

    memcpy(universe.players, players, sizeof(universe.players));

    /*
     * For planets, daemon sets PLREDRAW, so we don't have to worry about
     * the galactic map.  However, we need something different for the
     * planetlist display, so that we can have it (a) update everything on
     * request, and (b) not update planets which are simply in range of ships.
     */
    opl = universe.planets;
    npl = planets;

    for (i = 0; i < MAXPLANETS; i++, opl++, npl++) {
	if (	opl->pl_owner != npl->pl_owner ||
		opl->pl_x != npl->pl_x ||
		opl->pl_y != npl->pl_y ||
		strcmp(opl->pl_name, npl->pl_name) ||
		opl->pl_armies != npl->pl_armies ||
		opl->pl_info != npl->pl_info
		) {
	    updatePlanet[i] = 1;
	}
    }

    memcpy(universe.planets, planets, sizeof(universe.planets));
}

/*
 * This moves us around.  Deals with locks and other goodies.
 */
move()
{
    extern int truespeed, uspeeds[];
    struct player *pp;
    double factor;
    extern int mapStats;

    /* if locked onto something, hang onto it */
    if (me->p_flags & PFPLLOCK) {
	me->p_x = planets[me->p_planet].pl_x;
	me->p_y = planets[me->p_planet].pl_y;
	if(W_IsMapped(statwin))
	     W_UnmapWindow(statwin);
	return;
    }
    if (me->p_flags & PFPLOCK) {
	pp = &players[me->p_playerl];
	if (pp->p_status == PFREE) {
	    warning("Player has left game, lock broken");
	    me->p_flags &= ~PFPLOCK;
	    if(W_IsMapped(statwin))
	       W_UnmapWindow(statwin);
	    return;
	}
	if(pp->p_x >= 0)
	   me->p_x = pp->p_x;
        if(pp->p_y >= 0)
	   me->p_y = pp->p_y;
	me->p_speed = pp->p_speed;	/* make transition off of */
	me->p_dir = pp->p_dir;		/* lock smoother */

	if(mapStats && !W_IsMapped(statwin))
	    W_MapWindow(statwin);
	return;
    }

    factor = (double) (uspeeds[truespeed] / 100000.0); /* adj for upd rate */
    me->p_x += (double) me->p_speed * Cos[me->p_dir] * WARP1 * factor;
    me->p_y += (double) me->p_speed * Sin[me->p_dir] * WARP1 * factor;

    /* Bounce off the side of the galaxy */
    if (me->p_x < 0) {
	me->p_x = -me->p_x;
	if (me->p_dir == 192) 
	    me->p_dir = me->p_desdir = 64;
	else 
	    me->p_dir = me->p_desdir = 64 - (me->p_dir - 192);
    } else if (me->p_x > GWIDTH) {
	me->p_x = GWIDTH - (me->p_x - GWIDTH);
	if (me->p_dir == 64) 
	    me->p_dir = me->p_desdir = 192;
	else 
	    me->p_dir = me->p_desdir = 192 - (me->p_dir - 64);
    } if (me->p_y < 0) {
	me->p_y = -me->p_y;
	if (me->p_dir == 0) 
	    me->p_dir = me->p_desdir = 128;
	else 
	    me->p_dir = me->p_desdir = 128 - me->p_dir;
    } else if (me->p_y > GWIDTH) {
	me->p_y = GWIDTH - (me->p_y - GWIDTH);
	if (me->p_dir == 128) 
	    me->p_dir = me->p_desdir = 0;
	else 
	    me->p_dir = me->p_desdir = 0 - (me->p_dir - 128);
    }
}


shutdown(s)

   char	*s;
{
    signal(SIGALRM, SIG_IGN);

    printf("xsg: %s\n", s);
    exit(0);		/* should have it wait & watch... */
}


showVersion()
{
    static int	done=0;
    char	buf[80];

    if(done) return;

    if(udcounter % UPS(updateSpeed))
      return;

    switch(udcounter / UPS(updateSpeed)){
      case 0:
	 break;
      case 1:
      case 2:
      case 3:
       sprintf(buf, "%s (patchlevel %d), originally by Andy McFadden", xsg_version, PATCHLEVEL);
       break;
      case 4:
       strcpy(buf, "Contributors to date:");
       break;
      case 5:
      case 6:
      case 7:
       strcpy(buf, "Bharat Mediratta,  Brian Paulsen,  Tedd Hadley");
       break;
      case 8:
       strcpy(buf, "Send bug reports, questions, comments to hadley@uci.edu");
       break;
      default:
       done=1;
       return;
   }
   warning(buf);
}

showVersion2()
{
    warning("Replay v1.0 by Bharat Mediratta (Bharat.Mediratta@Eng.Sun.Com)");
}

dumpframe()
{
   static int fd2 = -1;
   FILE *popen(), *fp2;
   int err = 0;
   int sync = 0;
   static char pipe[256];
   static char *pipe_buf = NULL;


   if (fd2 == -1){
#ifdef SHRINKFRAME
      initmbufs();
#endif
      /* done once only */
      if (pipe_buf == NULL){
         pipe_buf = (char *)malloc(53248);  /* 52K */
         /* why this number?  cameron@stl.dec.com 30-Apr-1997 */
         
         if (pipe_buf == NULL){
            fprintf(stderr, "xsg: Can't allocate pipe buffer, continuing\n");
         }
      }

      sprintf(pipe, "nice gzip - > %s", getrf());
      fp2 = popen(pipe, "w");
      sprintf(pipe, "recording to \"%s\"", getrf());
      warning(pipe);
      if(!fp2){
	 perror("popen");
	 record = 0;
	 fd2 = -1;
	 pipe_buf = NULL;
	 return;
      }
      if(pipe_buf != NULL) setbuffer(fp2, pipe_buf, sizeof(pipe_buf));
      fd2 = fileno(fp2);
   }
 
   if (fd2 >= 0){
#ifdef SHRINKFRAME
      int	l;
      char	*b;

      b = shrink(sharedMemory, &l);
      while(l > 0){
         for(;;){
            err = write(fd2, b, l);
            if ( err >= 0 ) break;
            if ( errno != EINTR ) break;
         }
	 if(err < 0){
	    perror("write");
	    record = 0;
	    fd2 = -1;
	    pipe_buf = NULL;
	    return;
	 }
	 b += err;
	 l -= err;
      }
#else
      for(;;){
         err = write(fd2, (char *)sharedMemory, sizeof(struct memory));
         if ( err >= 0 ) break;
         if ( errno != EINTR ) break;
      }
      if(err < 0){
	 perror("write");
	 record = 0;
	 fd2 = -1;
	 pipe_buf = NULL;
	 return;
      }
#endif
   }
 
   /* nice for nfs */
   if (sync++ == 10){
       fsync(fd2);
       sync = 0;
   }
}

char *
getrf()
{
   char		*s, *t;
   static char	n[80];

   if((s=getdefault("recordfile"))){
      int l = strlen(s);
      if(s[l-1] == '/'){
	 t = (char *)malloc(l+20);
	 sprintf(t, "%srf-%d.xsg", s, getpid());
	 return t;
      }
      else
	 return s;
   }
   else {
      sprintf(n, "rf-%d.xsg", getpid());
      return n;
   }
}

#ifdef SHRINKFRAME
static char			*mbuf;
static char			*mptr;
static struct memory		*mbufold;
#endif


/*
 *  Load a new frame
 *
 */
loadframe()
{
  int err, amt, zerocnt = 0;
  static char pipe[50];
  static int fd = -1;
  FILE *popen();
  static FILE *fp;
  struct memory *smPtr;
  int skippedFrames = 0;
  static char buf[40];
  static char *pipe_buf = NULL;
  static int firstFrame = 1;
  W_Event data;
#ifdef SHRINKFRAME
  int	  size;
#endif

  if (fd == -1) {
#ifdef SHRINKFRAME
    initmbufs();
#endif
    sprintf (pipe, "gunzip - < %s", recfile);
      
    if (pipe_buf == NULL) {
      pipe_buf = (char *)malloc(53248);  /* 52K */
      /* why this number?  cameron@stl.dec.com 30-Apr-1997 */
          
      if (pipe_buf == NULL) {
        fprintf(stderr, "xsg: Can't allocate pipe buffer, continuing\n");
      }
    }
  } /* fd == -1 */
  
  /* restart playback ... rewind the file by re-opening the gunzip stream */
  if (restarting) {
    warning ("Restarting ...");
    if (fd != -1) {
      pclose(fp);
      fd = -1;
    }
    playMode &= ~BACKWARD;
    playMode |= (FORWARD);
    restarting = 0;
    currentFrame = 0;
    whichFrame = 0;
    latestFrame = -1;
  } /* restarting */
  
  /* open the gunzip pipe */
  if (fd == -1) {
    fp = popen(pipe, "r");
    if (pipe_buf != NULL) setbuffer(fp, pipe_buf, sizeof(pipe_buf));
    fd = fileno(fp);
    
    if (fd < 0) {
      fprintf(stderr, "xsg: Can't read record file\n");
      exit(-1);
    }
  }

  if (playMode & (FORWARD|T_SKIP)) {
    if (whichFrame != latestFrame && latestFrame != -1) {
      /* We've been rewinding ... come up to the present before playing */
      if (++whichFrame == allocFrames) whichFrame = 0;
      currentFrame++;
    } else {
      skippedFrames = 0;
      do {
        /* Read from the pipe */
        if (fd > 0) {
          amt = 0;
          
          if (++latestFrame == allocFrames) {
            latestFrame = 0;
            if (firstFrame == 1) firstFrame = 0;
          }
          
          /* Read into the latest frame */
          smPtr = &sharedMemory[latestFrame];
#ifdef SHRINKFRAME
          /* read the size first */
          while (amt < sizeof(int)) {
            for (;;) {
              err = read(fd, (char *) &size+amt, sizeof(int)-amt);
              if ( err >= 0 ) break;
              if ( errno != EINTR ) break;
            }
            
            if (err == 0)
              zerocnt++;
            else
              zerocnt = 0;
            
            if (zerocnt == 20) {
              /* 20 consecutive zero reads, log's over! */
              playMode &= ~FORWARD;
              playMode |= (BACKWARD|SINGLE_STEP);
              warning("End of record file. Entering single-step mode.");
              latestFrame--;
              return 0;
            }
            
            if (err < 0) {
              perror("read");
              fprintf(stderr, "xsg: Error reading record file\n");
              exit(-1);
            }
            
            amt += err;
          } /* while (amt < sizeof(int)) */
          amt = 0;
          
          size -= sizeof(int);
          
          /* now read data */
          while(amt < size) {
            for (;;) {
              err = read(fd, (char *) mbuf+amt, size-amt);
              if ( err >= 0 ) break;
              if ( errno != EINTR ) break;
            }
            
            if (err == 0)
              zerocnt++;
            else
              zerocnt = 0;
            
            if (zerocnt == 20) {
              /* 20 consecutive zero reads, log's over! */
              playMode &= ~FORWARD;
              playMode |= (BACKWARD|SINGLE_STEP);
              warning("End of record file. Entering single-step mode.");
              latestFrame--;
              return 0;
            }
            
            if (err < 0) {
              perror("read");
              fprintf(stderr, "xsg: Error reading record file\n");
              exit(-1);
            }
            
            amt += err;
          } /* while (amt < size) */
          
          /* and transfer data to new frame */
          fillframe (smPtr, (latestFrame > 0) ? 
            &sharedMemory[latestFrame-1]:
            &sharedMemory[allocFrames-1], mbuf, size);
#else
          while (amt < sizeof(struct memory)) {
            for (;;) {
              err = read(fd, (char *)smPtr+amt, sizeof(struct memory)-amt);
              if ( err >= 0 ) break;
              if ( errno != EINTR ) break;
            }
            if (err == 0)
              zerocnt++;
            else
              zerocnt = 0;
            
            if (zerocnt == 20) {
              /* 20 consecutive zero reads, log's over! */
              playMode &= ~FORWARD;
              playMode |= (BACKWARD|SINGLE_STEP);
              warning("End of record file. Entering single-step mode.");
              latestFrame--;
              return 0;
            }
            
            if (err < 0) {
              fprintf(stderr, "xsg: Error reading record file\n");
              exit(-1);
            }
            
            amt += err;
          } /* while (amt < sizeof(struct memory)) */
#endif
          /* force gameup status for all playback frames read in*/
          status->gameup |= GU_GAMEOK;
          
          whichFrame = latestFrame;
        } /* if (fd > 0) */
        
        if (playMode & T_SKIP) {
          skippedFrames++;
          
          if (sharedMemory[whichFrame].status->tourn) {
            playMode &= ~T_SKIP;
            playMode |= SINGLE_STEP;
          }
          
          if (W_EventsPending()) {
            W_NextEvent(&data);
            if (data.type == W_EV_KEY) {
              sprintf(buf, "Skip aborted after %d frames...", skippedFrames);
              warning(buf);
              playMode &= ~T_SKIP;
            }
          }
          show_playback(1);
        }
        
        currentFrame++;
      } while (playMode & T_SKIP);
    }
    /* end of playmode forward or t-skip */
  } else if (playMode & BACKWARD) {
    if (--whichFrame < 0) whichFrame = allocFrames - 1;
    
    if (whichFrame == latestFrame || (whichFrame == 0 && firstFrame)) {
      /* We've wrapped around */
      playMode &= ~BACKWARD;
      playMode |= (FORWARD|SINGLE_STEP);
      warning("Cannot rewind any farther.  Entering single-step mode.");
      if (++whichFrame == allocFrames) whichFrame = 0;
    } else {
      currentFrame--;
    }
  }

  players = sharedMemory[whichFrame].players;
  torps = sharedMemory[whichFrame].torps;
  status = sharedMemory[whichFrame].status;
  planets = sharedMemory[whichFrame].planets;
  phasers = sharedMemory[whichFrame].phasers;
  mctl = sharedMemory[whichFrame].mctl;
  messages = sharedMemory[whichFrame].messages;

  return 0;
}

#ifdef SHRINKFRAME

initmbufs()
{
   mbufold = (struct memory *) malloc(sizeof(struct memory));
   if(!mbufold){
      perror("initmbufs: mbufold: malloc");
      exit(1);
   }
   memset(mbufold, 0, sizeof(struct memory));

   mbuf = (char *) malloc(sizeof(struct memory) + 
			  sizeof(int) +	/* size of frame */
			  /* offset and size, players*/
			  2*sizeof(int) * MAXPLAYER +
			  /* offset and size, torps */
			  2*sizeof(int) * MAXPLAYER * MAXTORP +
			  /* offset and size, plasmatorps */
			  2*sizeof(int) * MAXPLAYER * MAXPLASMA +
			  /* offset and size, status */
			  2*sizeof(int) +
			  /* offset and size, planets */
			  2*sizeof(int) * MAXPLANETS +
			  /* offset and size, phasers */
			  2*sizeof(int) * MAXPLAYER +
			  /* offset and size, mctl */
			  2*sizeof(int) +
			  /* offset and size, message */
			  2*sizeof(int) * MAXMESSAGE);
   if(!mbuf){
      perror("initmbufs: mbuf: malloc");
      exit(1);
   }
}
   

/*
 * Check first for differences in current state.  If so, record offset
 * integer into struct memory followed by size of record followed by record.
 * First 4 bytes in each frame is integer equal to to size of frame.
 */

char *
shrink(m, len)

   struct memory	*m;
   int			*len;	/* returned */
{
   register			i,h;
   struct player		*jn, *jo;
   struct torp			*tn, *to;
   struct message		*l;
   struct planet		*pn, *po;
/*   struct plasmatorp		*ptn, *pto; */
   struct phaser		*phn, *pho;
   int				size, offset;
   static int			s_oldmctl;

   /* mbufold : copy of last state */

   mptr = mbuf;
   mptr += sizeof(int);	/* save room for size */

   for(i=0,jn=players,jo=mbufold->players; i < MAXPLAYER; i++,jn++,jo++){
      /* greatest room for improvement here .. 90% of player structure
	 doesn't change, only p_fuel, p_shields, etc., yet we have to
	 dump the whole thing each time (p_ship, p_stats, p_login, etc). */
      if(memcmp(jn, jo, sizeof(struct player)) != 0){
	 offset = (int) &mbufold->players[i] - (int) mbufold;
	 size = sizeof(struct player);
	 memcpy(mptr, &offset, sizeof(offset));
	 mptr += sizeof(offset);
	 memcpy(mptr, &size, sizeof(size));
	 mptr += sizeof(size);
	 memcpy(mptr, jn, size);
	 mptr += size;
	 memcpy(jo, jn, sizeof(struct player));	/* update mbufold */
      }
   }
   for(i=0,to=mbufold->torps,tn=firstTorp; tn<=lastPlasma; tn++,i++,to++){
      if(memcmp(tn, to, sizeof(struct torp)) != 0){
	 offset = (int) &mbufold->torps[i] - (int)mbufold;
	 size = sizeof(struct torp);
	 memcpy(mptr, &offset, sizeof(offset));
	 mptr += sizeof(offset);
	 memcpy(mptr, &size, sizeof(size));
	 mptr += sizeof(size);
	 memcpy(mptr, tn, size);
	 mptr += size;
	 memcpy(to, tn, sizeof(struct torp));	/* update mbufold */
      }
   }
   if(memcmp(status, mbufold->status, sizeof(struct status)) != 0){
      offset = (int) mbufold->status - (int) mbufold;
      size = sizeof(struct status);
      memcpy(mptr, &offset, sizeof(offset));
      mptr += sizeof(offset);
      memcpy(mptr, &size, sizeof(size));
      mptr += sizeof(size);
      memcpy(mptr, status, size);
      mptr += size;
      memcpy(mbufold->status, status, sizeof(struct status));
   }
   for(i=0,pn=planets,po=mbufold->planets; i< MAXPLANETS; i++,pn++,po++){
      if(memcmp(pn, po, sizeof(struct planet)) != 0){
	 offset = (int) &mbufold->planets[i] - (int)mbufold;
	 size = sizeof(struct planet);
	 memcpy(mptr, &offset, sizeof(offset));
	 mptr += sizeof(offset);
	 memcpy(mptr, &size, sizeof(size));
	 mptr += sizeof(size);
	 memcpy(mptr, pn, size);
	 mptr += size;
	 memcpy(po, pn, sizeof(struct planet)); /* update mbufold */
      }
   }
   for(i=0,phn=phasers,pho=mbufold->phasers; i< MAXPLAYER; i++,phn++,pho++){
      if(memcmp(phn, pho, sizeof(struct phaser)) != 0){
	 offset = (int) &mbufold->phasers[i] - (int) mbufold;
	 size = sizeof(struct phaser);
	 memcpy(mptr, &offset, sizeof(offset));
	 mptr += sizeof(offset);
	 memcpy(mptr, &size, sizeof(size));
	 mptr += sizeof(size);
	 memcpy(mptr, phn, size);
	 mptr += size;
	 memcpy(pho, phn, sizeof(struct phaser)); /* update mbufold */
      }
   }
   if(memcmp(mctl, mbufold->mctl, sizeof(struct mctl)) != 0){
      offset = (int) mbufold->mctl - (int) mbufold;
      size = sizeof(struct mctl);
      memcpy(mptr, &offset, sizeof(offset));
      mptr += sizeof(offset);
      memcpy(mptr, &size, sizeof(size));
      mptr += sizeof(size);
      memcpy(mptr, mctl, size);
      mptr += size;
      memcpy(mbufold->mctl, mctl, sizeof(struct mctl)); /* update mbufold */
   }
   while(s_oldmctl != mctl->mc_current){
      s_oldmctl ++;
      if(s_oldmctl >= MAXMESSAGE) s_oldmctl = 0;
	 offset = (int) &mbufold->messages[s_oldmctl] - (int) mbufold;
	 size = sizeof(struct message);
	 memcpy(mptr, &offset, sizeof(offset));
	 mptr += sizeof(offset);
	 memcpy(mptr, &size, sizeof(size));
	 mptr += sizeof(size);
	 memcpy(mptr, &messages[s_oldmctl], size);
	 mptr += size;
   }

   *len = (int)(mptr - mbuf);
   memcpy(mbuf, len, sizeof(int));
   return mbuf;
}

fillframe(new_frame, last_frame, data, tsize)

   struct memory	*new_frame;
   struct memory	*last_frame;
   char			*data;
   int			tsize;
{
   int			offset,
			size;
   char			*nf = (char *) new_frame;
   mptr = data;

   /* use the last frame as the basis for new changes if there is
      a last frame, otherwise zero it and start fresh */
   if(last_frame && udcounter > 0)
      memcpy(nf, (char *)last_frame, sizeof(struct memory));
   else
      memset(nf, 0, sizeof(struct memory));

   /* read offset, read size, read data, move data to proper offset,
      continue */
   
   while(tsize > 0){
      memcpy(&offset, mptr, sizeof(int));
      mptr += sizeof(int);
      memcpy(&size, mptr, sizeof(int));
      mptr += sizeof(int);
      if(offset > sizeof(struct memory)){
	 fprintf(stderr, "xsg: Corrupted or unknown format.\n");
	 exit(1);
      }
      if(size > sizeof(struct memory)){
	 fprintf(stderr, "xsg: Corrupted or unknown format.\n");
	 exit(1);
      }
      memcpy(&nf[offset], mptr, size);
      mptr += size;
      tsize -= sizeof(int) + sizeof(int) + size;
   }
}

#endif


#ifdef hpux
/* I know HPs suck but a 750 is a fast netrek host, WOT NO setbuf! ok lets
 * wibble one together. (jwh 1/3/93)
 */

void setbuffer(fp, buf, size)
register FILE *fp;
char *buf;
int size;
{

  (void) setvbuf(fp, buf, buf ? _IOFBF : _IONBF, size);
}

#endif

#ifdef Solaris
/* The above hack is also needed for SOlaris 2.4
 * tl 17/6/95)
 */

void setbuffer(fp, buf, size)
register FILE *fp;
char *buf;
int size;
{

  (void) setvbuf(fp, buf, buf ? _IOFBF : _IONBF, size);
}

#endif
