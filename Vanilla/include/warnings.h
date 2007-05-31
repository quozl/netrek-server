/*	Here are all warnings that are send with SP_S_WARNING */
/*		HW 93		*/
/* ab handleTractorReq  socket.c */

#define TEXTE 0
#define PHASER_HIT_TEXT 1
#define BOMB_INEFFECTIVE 2
#define BOMB_TEXT 3
#define BEAMUP_TEXT 4
#define BEAMUP2_TEXT 5
#define BEAMUPSTARBASE_TEXT 6
#define BEAMDOWNSTARBASE_TEXT 7
#define BEAMDOWNPLANET_TEXT 8
#define SBREPORT 9
#define ONEARG_TEXT 10
#define BEAM_D_PLANET_TEXT 11
#define ARGUMENTS 12
#define BEAM_U_TEXT 13
#define LOCKPLANET_TEXT 14
#define LOCKPLAYER_TEXT 15
#define SBRANK_TEXT 16
#define SBDOCKREFUSE_TEXT 17
#define SBDOCKDENIED_TEXT 18
#define SBLOCKSTRANGER 19
#define SBLOCKMYTEAM 20
/*	Daemon messages */
#define DMKILL 21
#define KILLARGS 22
#define DMKILLP 23
#define DMBOMB 24
#define DMDEST 25
#define DMTAKE 26
#define DGHOSTKILL 27
/*	INL	messages		*/
#define INLDMKILLP 28
#define INLDMKILL 29	/* Because of shiptypes */
#define INLDRESUME 30
#define INLDTEXTE 31
/* Variable warning stuff */
#define STEXTE 32		/* static text that the server needs to send to the client first */
#define SHORT_WARNING 33 /* like CP_S_MESSAGE */
#define STEXTE_STRING 34
#define KILLARGS2 35		/* For why dead messages */

#define DINVALID 255


/* DaemonMessages */
char *daemon_texts[]={
/* Game_Paused() */
"Game is paused.  CONTINUE to continue.",	/* 0 */
"Game is no-longer paused!",			/* 1 */
"Game is paused. Captains CONTINUE to continue.",	/* 2 */
"Game will continue in 10 seconds",		/* 3 */
/* send_about_to_start() */
"Teams chosen.  Game will start in 1 minute.",	/* 4 */
"----------- Game will start in 1 minute -------------",		/* 5 */
};

/* VARITEXTE = warnings with 1 or more arguments argument */
char *vari_texts[]={
/* redraw.c */
"Engineering:  Energizing transporters in %d seconds",		/* 0 */
"Stand By ... Self Destruct in %d seconds",			/* 1 */
"Helmsman:  Docking manuever completed Captain.  All moorings secured at port %d.",	/* 2 */
/* interface.c from INL server */
"Not constructed yet. %d minutes required for completion",		/* 3 */

};



char *w_texts[]={
/* socket.c		*/
"Tractor beams haven't been invented yet.",			/* 0 */
"Weapons's Officer:  Cannot tractor while cloaked, sir!",		/* 1 */
"Weapon's Officer:  Vessel is out of range of our tractor beam.",	/* 2 */

/* handleRepressReq */
/****************	coup.c	***********************/
/*	coup()	*/
"You must have one kill to throw a coup",			/* 3 */
"You must orbit your home planet to throw a coup",		/* 4 */
"You already own a planet!!!",					/* 5 */
"You must orbit your home planet to throw a coup",		/* 6 */
"Too many armies on planet to throw a coup",			/* 7 */
"Planet not yet ready for a coup",				/* 8 */
/*	getentry.c		*/
/*	getentry()		*/
"I cannot allow that.  Pick another team",			/* 9 */
"Please confirm change of teams.  Select the new team again.",	/* 10 */
"That is an illegal ship type.  Try again.",				/* 11 */
"That ship hasn't beed designed yet.  Try again.",			/* 12 */
"Your new starbase is still under construction",			/* 13 */
"Your team is not capable of defending such an expensive ship!",	/* 14 */
"Your team's stuggling economy cannot support such an expenditure!",	/* 15 */
"Your side already has a starbase!",				/* 16 */
/*	plasma.c	*/
/* nplasmatorp(course, type)	*/
"Plasmas haven't been invented yet.",				/* 17 */
"Weapon's Officer:  Captain, this ship is not armed with plasma torpedoes!",	/* 18 */
"Plasma torpedo launch tube has exceeded the maximum safe temperature!",	/* 19 */
"Our fire control system limits us to 1 live torpedo at a time captain!",		/* 20 */
"Our fire control system limits us to 1 live torpedo at a time captain!",		/* 21 */
"We don't have enough fuel to fire a plasma torpedo!",		/* 22 */
"We cannot fire while our vessel is undergoing repairs.",		/* 23 */
"We are unable to fire while in cloak, captain!",			/* 24 */
/********	torp.c	*********/
/*	ntorp(course, type)	*/
"Torpedo launch tubes have exceeded maximum safe temperature!",	/* 25 */
"Our computers limit us to having 8 live torpedos at a time captain!",	/* 26 */
"We don't have enough fuel to fire photon torpedos!",		/* 27 */
"We cannot fire while our vessel is in repair mode.",		/* 28 */
"We are unable to fire while in cloak, captain!",			/* 29 */
"We only have forward mounted cannons.",			/* 30 */
/*	phasers.c	*/
/*	phaser(course) */
"Weapons Officer:  This ship is not armed with phasers, captain!",	/* 31 */
"Phasers have not recharged",					/* 32 */
"Not enough fuel for phaser",					/* 33 */
"Can't fire while repairing",					/* 34 */
"Weapons overheated",					/* 35 */
"Cannot fire while cloaked",					/* 36 */
"Phaser missed!!!",						/* 37 */
"You destroyed the plasma torpedo!",				/* 38 */
/*	interface.c	*/
/* bomb_planet()	*/
"Must be orbiting to bomb",					/* 39 */
"Can't bomb your own armies.  Have you been reading Catch-22 again?", /* 40 */
"Must declare war first (no Pearl Harbor syndrome allowed here).",	/* 41 */
"Bomb out of T-mode?  Please verify your order to bomb.",		/* 42 */
"Hoser!",							/* 43 */
/*	beam_up()	*/
"Must be orbiting or docked to beam up.",			/* 44 */
"Those aren't our men.",					/* 45 */
"Comm Officer: We're not authorized to beam foreign troops on board!", /* 46 */
/* beam_down() */
"Must be orbiting or docked to beam down.",			/* 47 */
"Comm Officer: Starbase refuses permission to beam our troops over.", /* 48 */
/*	declare_war(mask, refitdelay)	*/
"Pausing ten seconds to re-program battle computers.",		/* 49 */
/* do_refit(type) */
"You must orbit your HOME planet to apply for command reassignment!", /* 50 */
"You must orbit your home planet to apply for command reassignment!",	/* 51 */
"Can only refit to starbase on your home planet.",			/* 52 */
"You must dock YOUR starbase to apply for command reassignment!", /* 53 */
"Must orbit home planet or dock your starbase to apply for command reassignment!", /* 54 */
"Central Command refuses to accept a ship in this condition!",	/*  55 */
"You must beam your armies down before moving to your new ship",	/* 56 */
"That ship hasn't been designed yet.",				/* 57 */
"Your side already has a starbase!",				/* 58 */
"Your team is not capable of defending such an expensive ship",	/* 59 */
"Your new starbase is still under construction",			/* 60 */
"Your team's stuggling economy cannot support such an expenditure!", /* 61 */
"You are being transported to your new vessel .... ",		/* 62 */
/* redraw.c */
/* auto_features()  */
"Engineering:  Energize. [ SFX: chimes ]",			/* 63 */
"Wait, you forgot your toothbrush!",				/* 64 */
"Nothing like turning in a used ship for a new one.",		/* 65 */
"First officer:  Oh no, not you again... we're doomed!",		/* 66 */
"First officer:  Uh, I'd better run diagnostics on the escape pods.",	/* 67 */
"Shipyard controller:  This time, *please* be more careful, okay?",	/* 68 */
"Weapons officer:  Not again!  This is absurd...",			/* 69 */
"Weapons officer:  ... the whole ship's computer is down?",		/* 70 */
"Weapons officer:  Just to twiddle a few bits of the ship's memory?",	/* 71 */
"Weapons officer:  Bah! [ bangs fist on inoperative console ]",	/* 72 */
"First Officer:  Easy, big guy... it's just one of those mysterious",	/* 73 */
"First Officer:  laws of the universe, like 'tires on the ether'.",		/* 74 */
"First Officer:  laws of the universe, like 'Klingon bitmaps are ugly'.",	/* 75 */
"First Officer:  laws of the universe, like 'all admirals have scummed'.",	/* 76 */
"First Officer:  laws of the universe, like 'Mucus Pig exists'.",		/* 77 */
"First Officer:  laws of the universe, like 'guests advance 5x faster'.",	/* 78 */
/* orbit.c */
/*orbit() */
 "Helmsman: Captain, the maximum safe speed for docking or orbiting is warp 2!",	 /* 79 */
"Central Command regulations prohibit you from orbiting foreign planets",	/* 80 */
 "Helmsman:  Sensors read no valid targets in range to dock or orbit sir!",	/* 81 */
/* redraw.c */
"No more room on board for armies",				/* 82 */
"You notice everyone on the bridge is staring at you.",		/* 83 */
/* startdaemon.c */
/* practice_robo() */
"Can't send in practice robot with other players in the game.",	/* 84 */
/*	socket.c */
/*	doRead(asock) */
"Self Destruct has been canceled",				/* 85 */
/* handleMessageReq(packet) */
"Be quiet",						/* 86 */
"You are censured.  Message was not sent.",			/* 87 */
"You are ignoring that player.  Message was not sent.",		/* 88 */
"That player is censured.  Message was not sent.",			/* 89 */
/* handleQuitReq(packet) */
"Self destruct initiated",					/* 90 */
/* handleScan(packet) */
"Scanners haven't been invented yet",				/* 91 */
/* handleUdpReq(packet) */
"WARNING: BROKEN mode is enabled",				/* 92 */
"Server can't do that UDP mode",				/* 93 */
"Server will send with TCP only",				/* 94 */
"Server will send with simple UDP",				/* 95 */
"Request for fat UDP DENIED (set to simple)",			/* 96 */
"Request for double UDP DENIED (set to simple)",			/* 97 */
/* forceUpdate() */
"Update request DENIED (chill out!)",				/* 98 */
/* INL redraw.c	*/
"Player lock lost while player rejoins.",			/* 99 */
"Can only lock on own team.",					/* 100 */
"You can only warp to your own team's planets!",			/* 101 */
"Planet lock lost on change of ownership.",			/* 102 */
" Weapons officer: Finally! systems are back online!",		/* 103 */

};

#define NUMWTEXTS (sizeof w_texts / sizeof w_texts[0])
#define NUMVARITEXTS ( sizeof vari_texts / sizeof   vari_texts[0])
#define NUMDAEMONTEXTS ( sizeof daemon_texts / sizeof daemon_texts[0])
/* That was it */
