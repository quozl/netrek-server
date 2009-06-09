/*
 * cambot.c - Robot designed to watch the shared memory segment, and generate
 *            packets to be stored on disk for future playback.
 */

#include "config.h"
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "genspkt.h"
#include "alarm.h"
#include "util.h"

#define UPDT_ALL 0
#define UPDT_MOST 1
#define PERSEC 5

#define RECORDFILE "cambot.pkt"
FILE *packetsFile;

struct you_short_spacket clientSelfs[MAXPLAYER];
struct youss_spacket clientSelfShips[MAXPLAYER];
struct flags_spacket clientFlags[MAXPLAYER];
struct player cambot_me;
struct itimerval udt;
char	*cb_from = {"GOD->ALL"};
extern int msgCurrent;
int use_newyou=1;
int f_many_self=1;
static LONG sequence;

struct feature_spacket Many_Self_F = {SP_FEATURE, 'S', 0, 0, 1, "MANY_SELF"};

/* Make linker happy... */
int ignored[MAXPLAYER];
int debug;

void message_flag(struct message *cur, char *address) {}
void flushSockBuf(void) {}
int bounceSBStats(int from) {return 0;}
int bouncePingStats(int from) {return 0;}
int bounceSessionStats(int from) {return 0;}
int bounceWhois(int from) {return 0;}
int bounceUDPStats(int from) {return 0;}
#ifdef RSA
int bounceRSAClientType (int from) { if (from) ; return 0; }
#endif

extern void sndTorp();

extern struct torp_info_spacket clientTorpsInfo[MAXPLAYER*MAXTORP];
extern struct torp_spacket clientTorps[MAXPLAYER*MAXTORP];
extern struct phaser_spacket clientPhasers[MAXPLAYER];
extern struct phaser_s_spacket client_s_Phasers[MAXPLAYER];
extern struct plasma_info_spacket clientPlasmasInfo[MAXPLAYER*MAXPLASMA];
extern struct plasma_spacket clientPlasmas[MAXPLAYER*MAXPLASMA];
extern int mustUpdate[MAXPLAYER];
extern int clientVKillsCount;
extern struct plyr_info_spacket clientPlayersInfo[MAXPLAYER];
extern struct plyr_login_spacket clientLogin[MAXPLAYER];
extern struct player_spacket clientPlayers[MAXPLAYER];
extern struct kills_spacket clientKills[MAXPLAYER];
extern struct pstatus_spacket clientPStatus[MAXPLAYER];
extern struct stats_spacket clientStats[MAXPLAYER];
extern int	clientVPlanetCount;
extern struct planet_spacket clientPlanets[MAXPLANETS];
extern struct planet_loc_spacket clientPlanetLocs[MAXPLANETS];
extern int sizes[TOTAL_SPACKETS];
extern int use_newyou;
extern int f_many_self;

void
cleanup(int unused)
{
    fclose(packetsFile);
    exit(0);
}

/* HACK - Make the socket.c routines call this routine instead
 * of the normal socket stuff */
void sendClientPacket(void *void_packet)
{
    struct player_spacket *packet = (struct player_spacket *) void_packet;
    if (!fwrite(packet, 1, sizes[(int)packet->type], packetsFile))
    {
	ERROR(1,("Bad write on packets file.\n"));
	cleanup(0);
    }
}

/* Main routine */
/* This is a mimic of updateClient from socket.c - It will call the
   necessary routines to generate the desired packets */
void
cb_updt(int unused)
{
    signal(SIGALRM, cb_updt);

    /*  updateTorps(); */
    {
	struct torp *t;
	int i;
	struct torp_info_spacket *tpi;
	struct torp_spacket *tp;

	for (i=0, t=firstTorp, tpi=clientTorpsInfo, tp=clientTorps; 
	     t<=lastTorp; i++, t++, tpi++, tp++)
	    sndTorp(tpi, tp, t, i, UPDT_ALL);
    }

    /*  updatePlasmas(); */
    {
	struct torp *t;
	int i;
	struct plasma_info_spacket *tpi;
	struct plasma_spacket *tp;

	for (i=0, t=firstPlasma, tpi=clientPlasmasInfo, tp=clientPlasmas; 
	     t<=lastPlasma; i++, t++, tpi++, tp++)
	    sndPlasma(tpi, tp, t, i, UPDT_ALL);
    }

    updateStatus(0);

    /*  updatePhasers(); */
    {
	int i;
	struct phaser_spacket *ph;
	struct phaser *phase;
	struct player *pl;
	struct phaser_s_spacket *phs;

	for (i = 0, ph = clientPhasers,phs = client_s_Phasers,
		 phase = phasers, pl = players; 
	     i < MAXPLAYER; i++, ph++, phs++, phase++, pl++) { 
	    if (phase->ph_status==PHHIT) {
		mustUpdate[phase->ph_target]=1;
	    }
	    sndPhaser(ph, phs, phase, i, UPDT_ALL);
	}
    }

    /*  updateSelf(); */
    /*  updateShips(); */
    {
	int i;
	struct player *pl;
	struct plyr_info_spacket *cpli;
	struct player_spacket *cpl;
	struct kills_spacket *kills;
	struct pstatus_spacket *pstatus;
	struct plyr_login_spacket *login;
	struct stats_spacket *stats;
	struct you_short_spacket *self;
	struct youss_spacket *self2;
	struct flags_spacket *flags;

	clientVKillsCount = 0;

	/* Please excuse the ugliness of this loop declaration */
	for (i=0, pl=players, cpli=clientPlayersInfo, cpl=clientPlayers,
		 kills=clientKills, pstatus=clientPStatus,
		 login=clientLogin, stats=clientStats,
		 self=clientSelfs, self2=clientSelfShips,
		 flags=clientFlags;
	     i<MAXPLAYER;
	     i++, pl++, cpli++, cpl++, kills++, pstatus++, login++,
		 stats++, self++, self2++) {
	    sndSelfShip(self2, pl);
	    sndSSelf(self, pl, UPDT_MOST);
	    sndLogin(login, pl);
	    sndPlayerInfo(cpli, pl);
	    sndKills(kills, pl);
	    sndPStatus(pstatus, pl);
	    sndFlags(flags, pl, UPDT_ALL);

	    /* NOTE: Can't send VPlayer packets, it only sends player positions
	     * relative to me->p_x/me->p_y.  We need to use the older packets
	     * so that absolute positions are recorded */
	    if (updtPlayer(cpl, pl, UPDT_ALL))
		sendClientPacket(cpl);
	}

	sendVKills();
    }

    /*  updatePlanets(); */
    {
	int i;
	struct planet_spacket *pl;
	struct planet *plan;
	struct planet_loc_spacket *pll;

	clientVPlanetCount = 0;

	for (i=0, pl=clientPlanets, plan=planets, pll=clientPlanetLocs; 
	     i<MAXPLANETS;
	     i++, pl++, plan++, pll++) {
	    sndPlanet(pl, plan, UPDT_ALL);
	    sndPlanetLoc(pll, plan);
	}

	sendVPlanets();

    }

    /*  updateMessages(); */
    {
	int i;
	struct message *cur;
	struct mesg_spacket msg;

	for (i=msgCurrent; i!=(mctl->mc_current+1) % MAXMESSAGE; i=(i+1) % MAXMESSAGE) {
	    if (i>=MAXMESSAGE) i=0;
	    cur= &messages[i];

	    if (cur->m_flags & MVALID) {
		if(send_short && (cur->m_from == 255)) {
		    /* Test if possible to send with SP_S_WARNING */
		    if (updtMessageSMessage(cur)) {
			msgCurrent=(msgCurrent+1) % MAXMESSAGE;
			continue;
		    }
		}
		updtMessage(&msg, cur);
	    }
	    msgCurrent=(msgCurrent+1) % MAXMESSAGE;
	}
    }

    updatePlayerStats();

    /* flushSockBuf() */
    {
	char buf[256];

	addSequence(buf, &sequence);
	sendClientPacket((struct player_spacket *)buf);
    }

    repCount++;
}

int
main(int argc, char *argv[])
{
    int i, verbose = 0, updates = 0;
    char *recordfile = RECORDFILE;

    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--verbose")) {
            verbose++;
        }
        if (!strcmp(argv[i], "--updates")) {
            if (++i < argc) {
                updates = atoi(argv[i]);
                if (verbose) fprintf(stderr,
                                     "cambot: updates set to %d\n", updates);
            }
        }
        if (!strcmp(argv[i], "--output")) {
            if (++i < argc) {
                recordfile = strdup(argv[i]);
                if (verbose) fprintf(stderr,
                                     "cambot: recordfile set to %s\n",
                                     recordfile);
            }
        }
    }

    srandom(time(NULL));

    getpath();
    openmem(0);
    readsysdefaults();
    if (updates == 0) updates = fps;

    if ((packetsFile = fopen(recordfile, "wb"))==NULL) {
        ERROR(1,("Could not open recording file.\n"));
        exit(1);
    } 

    initSPackets();
    {
        int i;

        for (i=0; i<MAXPLAYER; i++)
        {
            clientSelfs[i].pnum=-1;
            clientSelfShips[i].damage=-1;
        }
    }

    send_short=2;

    /* HACK..  Some of the sockets code is hard linked to me->...
     * create a bogus player struct to prevent seg faults */
    me = &cambot_me;
    memset(me, 0, sizeof(struct player));
    p_ups_set(me, updates);

    /* Add setup packets now */
    sendFeature(&Many_Self_F);

    signal(SIGALRM, cb_updt);
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    alarm_setitimer(distortion, updates);
    if (verbose) fprintf(stderr, "cambot: recording ...\n");
    for (;;) {
        pause();
    }
}

/*  Hey Emacs!
 * Local Variables:
 * c-file-style:"bsd"
 * End:
 */
