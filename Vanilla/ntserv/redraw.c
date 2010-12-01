/*
 * redraw.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "sturgeon.h"
#include "util.h"

/* file scope prototypes */
static u_char newcourse(int x, int y);
static void auto_features(void);

extern int living;

void intrupt(void)
{
    if (testtime == -1) {
        struct reserved_spacket sp;

        /* Give a reasonable period of time to respond to query (and test code
           if they need to) */
#define RSAREPLYTIMER 30

#ifdef RSA
        RSA_Client = 0;
#endif
        testtime = RSAREPLYTIMER * 10;
        makeReservedPacket(&sp);
        memcpy(testdata, sp.data, RESERVED_SIZE);
        sendClientPacket(&sp);
    } else if (testtime != 0) {
        testtime--;
        if (testtime==0) {
            if (report_ident)
                pmessage2(0, MALL | MJOIN, "GOD->ALL", me->p_no,
                          "%s using %s", me->p_mapchars, me->p_ident);
#if defined(RSA) && defined(SHOW_RSA) && defined(SHOW_RSA_FAILURE)
            if (!hidden && !whitelisted && !bypassed)
                pmessage2(0, MALL | MJOIN, "GOD->ALL", me->p_no,
                          "%s %.16s is not using a trusted client",
                          ranks[me->p_stats.st_rank].name,
                          me->p_name);
#endif
            if (bypassed) {                     /* Deal with .bypass entries */
                ERROR(3,(".bypass person : %s - logged on\n",me->p_login));
                me->p_stats.st_flags |= ST_CYBORG; /* mark for reference */
                new_warning(UNDEF,"You are in the .bypass file, be good");
            } else {
                /* User failed to respond to verification query.  Bye! */
                if (!binconfirm)
                    me->p_stats.st_flags |= ST_CYBORG; /* mark for reference 7/27/91 TC */
                else {
                    me->p_explode = 10;
                    me->p_whydead = KQUIT;
                    me->p_status = PEXPLODE;
                    ERROR(3,("User binary failed to verify\n"));
#ifdef RSA
#ifdef SHOW_RSA
                    pmessage2(0, MALL | MJOIN, "GOD->ALL", me->p_no,
                              "%s %.16s failed to verify",
                              ranks[me->p_stats.st_rank].name,
                              me->p_name);
#endif
                    if (RSA_Client==1)
                        new_warning(UNDEF,"No customized binaries.  Please use a blessed one.");
                    else if (RSA_Client==2)
                        new_warning(UNDEF,"Wrong Client Version Number!");
                    else
                        new_warning(UNDEF,"You need a spiffy new RSA client for this server!");
#else
                    new_warning(UNDEF,"No customized binaries. Please use a blessed one.");
#endif
                }
            }
        }
    }
    if (me->p_status == PFREE) {
	ERROR(1,("intrupt: exitGame() on PFREE (pid %i)\n",getpid()));
	exitGame(0);
    }

    if (!Observer || me->p_status == PEXPLODE)
    {
	if (me->p_status == PEXPLODE || me->p_status == PDEAD) {
	    FD_ZERO(&inputMask);
	}
	if (((me->p_status == PDEAD) || (me->p_status == POUTFIT))
	      && (me->p_ntorp <= 0) && (me->p_nplasmatorp <= 0)) {
	    death();
            return;
	}
    }

    if (clue) {
        if (RECHECK_CLUE && (clueFuse > CLUEINTERVAL)) {
            clueVerified = 0;
            clueFuse = 0;
            clueCount = 0;
        }
        if (!clueVerified && (inl_mode || (queues[QU_PICKUP].count != 0))) {
            clueFuse++;
            clue_check();
        }
    }
    auto_features();
    updateClient();
}

/* observer is usually status POBSERV which is sort of like PDEAD */
/* may get set to PDEAD or PEXPLODE by daemon events. deal with */
/* those cases  reasonably gracefully */

static void check_observs(void)
{
    struct player *pl;
    struct planet *pln;

    if (!Observer) return;
    if (me->p_status != POBSERV) {
	if (me->p_status == PDEAD || me->p_status == PEXPLODE) {
            death ();
            return;
        }else{
	    if (me->p_status == PFREE){
		ERROR(1,("check_observs:  changing a PFREE!\n"));
	    }
            me->p_status = POBSERV;
	}
    }
    if (FD_ISSET (CP_SPEED, &inputMask)) {
        /* only let observer do a limited set of actions. */
        FD_ZERO (&inputMask);
        FD_SET (CP_MESSAGE, &inputMask);
        FD_SET (CP_QUIT, &inputMask);
        FD_SET (CP_LOGIN, &inputMask);
        FD_SET (CP_OUTFIT, &inputMask);
        FD_SET (CP_PLANLOCK, &inputMask);
        FD_SET (CP_PLAYLOCK, &inputMask);
        FD_SET (CP_SOCKET, &inputMask);
        FD_SET (CP_OPTIONS, &inputMask);
        FD_SET (CP_BYE, &inputMask);
        FD_SET (CP_UPDATES, &inputMask);
        FD_SET (CP_RESERVED, &inputMask);
        FD_SET (CP_UDP_REQ, &inputMask);
        FD_SET (CP_SEQUENCE, &inputMask);
#ifdef RSA
        FD_SET (CP_RSA_KEY, &inputMask);
#endif
        FD_SET (CP_PING_RESPONSE, &inputMask);
        FD_SET (CP_S_REQ, &inputMask);
        FD_SET (CP_S_THRS, &inputMask);
        FD_SET (CP_S_MESSAGE, &inputMask);
        FD_SET (CP_S_RESERVED, &inputMask);
        FD_SET (MAX_CP_PACKETS, &inputMask);
    }
    /* handle quitting  -- it just happens for the observer */
    if (me->p_flags & PFSELFDEST) {
        me->p_status = POUTFIT;
        me->p_whydead = KQUIT;
        death ();
        return;
    }

    me->p_flags |= PFOBSERV;  /* future clients may understand this */
    /* Check if I am locked onto a player */
    if (me->p_flags & PFPLOCK) {
        if ( (me->p_playerl < 0) || (me->p_playerl >= MAXPLAYER) ){
	    /* I am not locked onto a valid player */
            me->p_playerl = 0;         /* ensure a safe value */
	    me->p_flags &= ~(PFPLOCK); /* turn off player lock */
            me->p_x = -100000;         /* place me off the tactical */
            me->p_y = -100000;
	    return;
	}

	/* I am locked onto a valid player */
	pl = &players[me->p_playerl];
	if ( (pl->p_status != PALIVE) && (pl->p_status != PEXPLODE) && 
	     (pl->p_status != PDEAD) ) {
	  me->p_x = -100000;
	  me->p_y = -100000;
	  new_warning (99,"Player lock lost while player rejoins.");
	}
	else if ( (pl->p_team != me->p_team) && 
                  (pl->p_team != 0) && 
                  (me->p_team != 0)) {
	  /* allow locking on independent players (robots) */
	  new_warning (100,"Can only lock on own team.");
	  me->p_x = -100000;
	  me->p_y = -100000;
	  me->p_flags &= ~(PFPLOCK); /* turn off player lock */
	} else {
	  /* follow a particular player */
	  me->p_ship.s_type = pl->p_ship.s_type;
	  me->p_x = pl->p_x;
	  me->p_y = pl->p_y;
	  me->p_shield = pl->p_shield;
	  me->p_damage = pl->p_damage;
	  me->p_armies = pl->p_armies;
	  me->p_fuel = pl->p_fuel;
	  me->p_wtemp = pl->p_wtemp;
	  me->p_etemp = pl->p_etemp;
	  /* These flags shouldn't be propagated to observers */
#define NOOBSMASK (PFSELFDEST|PFPLOCK|PFPLLOCK|PFOBSERV)
	  me->p_flags = (pl->p_flags & ~NOOBSMASK) | (me->p_flags & NOOBSMASK);
	  
	  me->p_dir = pl->p_dir;
	  me->p_tractor = pl->p_tractor;
	  me->p_dock_with = pl->p_dock_with;
	  me->p_dock_bay = pl->p_dock_bay;
	  me->p_planet = pl->p_planet;
	  me->p_speed = pl->p_speed;
	}
	return;
    }  /* end if I am locked onto a player */
    
    /* Check if I am locked onto a planet */
    if (me->p_flags & PFPLLOCK) {
        if ( (me->p_planet < 0) || (me->p_planet >= MAXPLANETS) ){
	    /* I am not locked onto a valid planet */
  	    me->p_planet = 0;            /* ensure a safe value */
	    me->p_flags &= ~(PFPLLOCK);  /* turn off planet lock */
	    me->p_x = -100000;           /* place me off the tactical */
	    me->p_y = -100000;
	    return;
        }

	/* I am locked onto a valid planet */
	pln = &planets[me->p_planet];
	if ((pln->pl_owner != me->p_team) && (me->p_team != 0)) {
	    if ( (me->p_x == pln->pl_x) && (me->p_y == pln->pl_y) )
	      /* we are sitting on the planet already, but it is not ours */
	      /* [some day we should fall back to nearest team ship] */
  	      new_warning(102,"Planet lock lost on change of ownership.");
	    else
	      new_warning(101,"You can only warp to your own team's planets!");
	    me->p_x = -100000;
	    me->p_y = -100000;
	    me->p_flags &= ~(PFPLLOCK); /* turn off planet lock */
	} else {
	    me->p_x = pln->pl_x;
	    me->p_y = pln->pl_y;
	}
	return;
    }   /* end if I am locked onto a planet */

    /* We are not locked onto anything */
    me->p_x = -100000;
    me->p_y = -100000;

    return;
}



/* These are routines that need to be done on interrupts but
   don't belong in the redraw routine and particularly don't
   belong in the daemon. */

static void auto_features(void)
{
    struct player *pl;
    struct planet *pln;
    u_char course;
    int troop_capacity=0;
    static int sd_time_last = -1;
    int sd_time;

    check_observs();
    if (!living) return;

    if (me->p_flags & PFSELFDEST) {
        sd_time = (me->p_selfdest - me->p_updates) / 10;
        if (sd_time != sd_time_last) {
            sd_time_last = sd_time;
            switch (sd_time) {
            case 4:
            case 5:
              new_warning(UNDEF, "You notice everyone on the bridge is staring at you.");
              break;
            default:
              new_warning(UNDEF, "Stand By ... Self Destruct in %d seconds", sd_time);
              break;
            }
        }
    } else {
        sd_time_last = -1;
    }

    /* provide a refit countdown 4/6/92 TC */

    if (me->p_flags & PFREFITTING) {
	static int lastRefitValue = 0; /* for smooth display */
	if (lastRefitValue != (rdelay - me->p_updates)/10) {
	    lastRefitValue = (rdelay - me->p_updates)/10; /* CSE to the rescue? */
	    switch (lastRefitValue) {
	      case 3:
	      case 2:
		new_warning(UNDEF, "Engineering:  Energizing transporters in %d seconds", lastRefitValue);
		break;
	      case 1:
		new_warning(UNDEF, "Engineering:  Energize. [ SFX: chimes ]");
		break;
	      case 0:
		switch (random()%5) {
		  case 0:
		    new_warning(UNDEF,"Wait, you forgot your toothbrush!");
		    break;
		  case 1:
		    new_warning(UNDEF,"Nothing like turning in a used ship for a new one.");
		    break;
		  case 2:
		    new_warning(UNDEF,"First officer:  Oh no, not you again... we're doomed!");
		    break;
		  case 3:
		    new_warning(UNDEF,"First officer:  Uh, I'd better run diagnostics on the escape pods.");
		    break;
		  case 4:
		    new_warning(UNDEF,"Shipyard controller:  This time, *please* be more careful, okay?");
		    break;
		}
		break;
	    }
	}
    }

    /* provide a war declaration countdown 4/6/92 TC */

    if (me->p_flags & PFWAR) {
	static int lastWarValue = 0;
	if (lastWarValue != (delay - me->p_updates)/10) {
	    lastWarValue = (delay - me->p_updates)/10; /* CSE to the rescue? */
	    switch (lastWarValue) {
	      case 9:
		new_warning(UNDEF,"Weapons officer:  Not again!  This is absurd...");
		break;
	      case 8:
		break;
	      case 7:
		new_warning(UNDEF,"Weapons officer:  ... the whole ship's computer is down?");
		break;
	      case 6:
		break;
	      case 5:
		new_warning(UNDEF,"Weapons officer:  Just to twiddle a few bits of the ship's memory?");
		break;
	      case 4:
		break;
	      case 3:
		new_warning(UNDEF,"Weapons officer:  Bah! [ bangs fist on inoperative console ]");
		break;
	      case 2:
		break;
	      case 1:
		new_warning(UNDEF,"First Officer:  Easy, big guy... it's just one of those mysterious");
		break;
	      case 0:
		switch (random()%5) {
		  case 0:
		    new_warning(UNDEF,"First Officer:  laws of the universe, like 'tires on the ether'.");
		    break;
		  case 1:
		    new_warning(UNDEF,"First Officer:  laws of the universe, like 'Klingon bitmaps are ugly'.");
		    break;
		  case 2:
		    new_warning(UNDEF,"First Officer:  laws of the universe, like 'all admirals have scummed'.");
		    break;
		  case 3:
		    new_warning(UNDEF,"First Officer:  laws of the universe, like 'Mucus Pig exists'.");
		    break;
		  case 4:
		    new_warning(UNDEF,"First Officer:  laws of the universe, like 'guests advance 5x faster'.");
		    break;
		}
		break;
	    }
	}
    }

    /* give certain information about bombing or beaming */
    if (me->p_flags & PFBOMB) {
	if (planets[me->p_planet].pl_armies < 5) {
	    new_warning(UNDEF, "Weapons Officer: Bombarding %s... Ineffective, %d armies left.",
			planets[me->p_planet].pl_name,    /* Planet name for stats 10/20/96 [007] */
		    planets[me->p_planet].pl_armies); /* nifty info feature 2/14/92 TMC */
	    me->p_flags &= ~PFBOMB;
	}
	else {
	    new_warning(UNDEF, "Weapons Officer: Bombarding %s...  Sensors read %d armies left.",
		planets[me->p_planet].pl_name,
		planets[me->p_planet].pl_armies);
	}
    }

    /* Use person observed for kills if we are an observer */
    if (Observer && (me->p_flags & PFPLOCK))
        pl = &players[me->p_playerl];
    else
        pl = me;	/* Not observer, just use my kills */

    troop_capacity = (int)((float)((int)(pl->p_kills*100)/100.0) * (myship->s_type == ASSAULT?3:2));
    if (myship->s_type == STARBASE || troop_capacity > myship->s_maxarmies)
    	troop_capacity = myship->s_maxarmies;

    if (me->p_flags & PFBEAMUP) {
        /* Messages rewritten for stats 10/20/96 [007] */
        char *txt;
	if (me->p_flags & PFORBIT) {
	  if (planets[me->p_planet].pl_armies < 5) {
	    txt = "Too few armies to beam up";
	    me->p_flags &= ~PFBEAMUP;
	  } else if (me->p_armies == troop_capacity) {
	    txt = "No more room on board for armies";
	    me->p_flags &= ~PFBEAMUP;
	  } else {
	    txt = "Beaming up";
	  }
	  new_warning(UNDEF, "%s: (%d/%d) %s has %d armies left",
		      txt,
		      me->p_armies,
		      troop_capacity,
		      planets[me->p_planet].pl_name,	
		      planets[me->p_planet].pl_armies);
	    
	} else if (me->p_flags & PFDOCK) {
	    struct player *base = bay_owner(me);
	    if (base->p_armies <= 0) {
		txt = "Too few armies to beam up";
		me->p_flags &= ~PFBEAMUP;
	    } else if (me->p_armies >= troop_capacity) {
		txt = "No more room on board for armies";
		me->p_flags &= ~PFBEAMUP;
	    } else {
	        txt = "Beaming up";
	    }
	    new_warning(UNDEF, "%s: (%d/%d) Starbase %s has %d armies left",
			txt,
			me->p_armies,
			troop_capacity,
			base->p_name,
			base->p_armies);
	}
    }

    if (me->p_flags & PFBEAMDOWN) {
        /* Messages rewritten for stats 10/20/96 [007] */
        char *txt;
	if (me->p_flags & PFORBIT) {
	  if (me->p_armies == 0) {
	    txt = "No more armies to beam down";
	    me->p_flags &= ~PFBEAMDOWN;
	  } else {
	    txt = "Beaming down";
	  }
	  new_warning(UNDEF, "%s: (%d/%d) %s has %d armies left",
		      txt,
		      me->p_armies,
		      troop_capacity,
		      planets[me->p_planet].pl_name,	
		      planets[me->p_planet].pl_armies);

	} else if (me->p_flags & PFDOCK) {
	    struct player *base = bay_owner(me);
	    if (me->p_armies <= 0) {
		txt = "No more armies to beam down";
		me->p_flags &= ~PFBEAMDOWN;
	    } else if (base->p_armies >= base->p_ship.s_maxarmies) {
	        txt = "All troop bunkers are full";
	        me->p_flags &= ~PFBEAMDOWN;
	    } else {
	        txt = "Transfering ground units";
	    }
	    new_warning(UNDEF, "%s: (%d/%d) Starbase %s has %d armies left",
			txt,
			me->p_armies,
			troop_capacity,
			base->p_name,
			base->p_armies);
	}
    }

    if (me->p_flags & PFREPAIR) {
	if ((me->p_damage == 0) && (me->p_shield == me->p_ship.s_maxshield))
	    me->p_flags &= ~PFREPAIR;
    }
    if ((me->p_flags & PFPLOCK) && (!Observer)) { /* set course to player x */
	int dist;

	pl = &players[me->p_playerl];
	if (pl->p_status != PALIVE)
	    me->p_flags &= ~PFPLOCK;
	if (me->p_flags & PFTWARP){
	  if (pl->p_status != PALIVE){
	     new_warning(UNDEF, "Starbase is dead; Transwarp aborted!", -1);
	     me->p_flags &= ~PFTWARP;
	     me->p_speed = 3;
	     set_speed(0);
	  }
	  if (pl->p_ship.s_type != STARBASE){
	     new_warning(UNDEF, "Starbase has been turned in; Transwarp aborted!", -1);
	     me->p_flags &= ~PFTWARP;
	     me->p_speed = 3;
	     set_speed(0);
	  }
	  if(me->p_etemp > me->p_ship.s_maxegntemp - 10){
	     new_warning(UNDEF,"Warp drive reaching critical temperature!", -1);
	     me->p_flags &= ~PFTWARP;
	     me->p_speed = 3;
	     set_speed(0);
	  }
 	  else
	  if (me->p_fuel < (int) (me->p_ship.s_maxfuel/8)){
	     new_warning(UNDEF, "Not enough fuel to continue transwarp!", -1);
	     me->p_flags &= ~PFTWARP;
	     me->p_speed = 3;
	     set_speed(0);
	  }
	  else if(!(me->p_flags & PFPLOCK)){
	     new_warning(UNDEF, "Lost player lock!", -1);
	     me->p_flags &= ~PFTWARP;
	     me->p_speed = 3;
	     set_speed(0);
	  }
	}
	if (pl->p_ship.s_type == STARBASE) {
	    dist = hypot((double) (me->p_x - pl->p_x),
		(double) (me->p_y - pl->p_y));
	    if (!(me->p_flags & PFTWARP)) {
	        if (dist-(DOCKDIST/2) < (11500 * me->p_speed * me->p_speed) /
		        me->p_ship.s_decint) {
		    if (me->p_desspeed > 2) {
		        set_speed(me->p_desspeed-1);
		    }
	        }
	    }

	    if ((dist < 2*DOCKDIST) && (me->p_flags & PFTWARP)){
	        p_x_y_join(me, pl);
	        me->p_flags &= ~(PFPLOCK);
	        me->p_flags &= ~(PFTWARP);
#ifdef SB_CALVINWARP
                if (!(pl->p_flags & PFDOCKOK) ||
                    ((pl->p_flags & PFPRESS) &&
                     (pl->p_tractor == me->p_no))) ;
                else {
#endif

	            me->p_speed=2;
	            orbit();
#ifdef SB_CALVINWARP
                }
#endif
	    } else {
	        if ((dist < DOCKDIST) && (me->p_speed <= 2))  {
		    me->p_flags &= ~PFPLOCK;
		    orbit();
	        }
	    }
	}
	if (me->p_flags & PFPLOCK) {
	    course = newcourse(pl->p_x, pl->p_y);
	    if (me->p_flags & PFTWARP)
		me->p_dir = course;
	    else
	        set_course(course);
	}
    }
    if ((me->p_flags & PFPLLOCK) && (!Observer) ) { /* set course to planet */
	int dist;

	pln = &planets[me->p_planet];
	dist = hypot((double) (me->p_x - pln->pl_x),
	    (double) (me->p_y - pln->pl_y));

	/* This is magic.  It should be based on defines, but it slows
	   the ship down to warp two an appropriate distance from the
	   planet for orbit */

	if (dist-(ORBDIST/2) < (11500 * me->p_speed * me->p_speed) /
	        me->p_ship.s_decint) {
	    if (me->p_desspeed > 2) {
		set_speed(2);
	    }
	}

	if ((dist < ENTORBDIST) && (me->p_speed <= 2))  {
	    me->p_flags &= ~PFPLLOCK;
	    orbit();
	}
	else {
	    int ax, ay, ad, missing;
	    extern int nowobble;

	    /* calculate course to planet from current coordinates */
	    course = newcourse(pln->pl_x, pln->pl_y);

            /* avoid superfluous midflight wobble */
	    if (nowobble) {
	      /* test case; at 6 o'clock on earth, lock on altair, warp 8 */
	      /* calculate arrival point at current course */
	      ax = (double) (me->p_x + Cos[me->p_desdir] * dist);
	      ay = (double) (me->p_y + Sin[me->p_desdir] * dist);
	      ad = hypot((double) (ax - pln->pl_x),
			 (double) (ay - pln->pl_y));
	      
	      /* change to the corrected course if the expected error
		 exceeds the remaining distance divided by the nowobble
		 factor (25 works well) */
	      missing = (ad > dist / nowobble);
	      if (missing)
		set_course(course);
	    } else {
	      /* classical behaviour */
	      if ( (ABS(course-me->p_desdir) > 2) || (dist < ENTORBDIST*10) )
		set_course(course);
	    }
	}
    }
    /* Send ship cap if necessary */
    sndShipCap();

#ifdef STURGEON
    /* Check if eligible for free upgrade */
    if (sturgeon) {
        if (me->p_free_upgrade) {
            me->p_upgrades += baseupgradecost[me->p_free_upgrade] + me->p_upgradelist[me->p_free_upgrade]*adderupgradecost[me->p_free_upgrade];
            me->p_upgradelist[me->p_free_upgrade]++;
            sturgeon_apply_upgrade(me->p_free_upgrade, me, 1);
            me->p_free_upgrade = 0;
        }
    }
#endif
}

static u_char newcourse(int x, int y)
{
	return((u_char) rint(atan2((double) (x - me->p_x),
	    (double) (me->p_y - y)) / 3.14159 * 128.));
}

/*

However, I have added a tail wag fix to continuum.  Send 'nowobble 25' to
yourself to enable it.  Send 'nowobble 0' to disable it.  The change only
affects your heading while you are locked on a planet.

What it does is avoid turning your ship to the exact heading if it can be
proven that you'll get into orbit anyway at the heading you are on.  It
still does some correction, but it doesn't oscillate back and forth
between the two suboptimal courses.

The test case, if you want to see the tail wag, is to stop your orbit at
the six o'clock position at Earth, lock on Altair, and then set a speed.

The cause of the problem is trigonometry on integer math.  The coordinate
system of Netrek is integer based, for efficiency of calculation and
information transmission.  Ships have an (x,y) coordinate, and a course.
The course is in 256ths of a full circle, about 1.4 degrees between each
possible course.

From one end of the galactic to another, 1.4 degrees of error is about
one and a half planet widths.  So we must be able to turn your ship as
you get closer.

Often, the best course is between two of the integer courses.  No matter
what course is chosen, a correction is needed by swapping to the other
course.

What the change does is (a) calculate the arrival point at the current
integer course you have, (b) calculate the distance from the arrival
point to the planet's centre, (c) changes ship heading only if the
expected arrival error exceeds the remaining distance divided by a
'nowobble factor'.

Through trial and error, I've found 25 works well.  Set it to a
pathalogically low value like 1 and you take a circuitous route to the
planet, which is quite funny.  Set it to 10,000,000 and you wobble like
you always did.  Set it to 0 to turn it off.

*/
