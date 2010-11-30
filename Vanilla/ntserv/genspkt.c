/*
 * GenSPkt.c
 *
 * Kevin O'Connor 10/3/97
 *
 * Provides the means to generate server packets.
 *
 * Code to establish connection, maintain connection, and to send
 * packets remains in socket.c.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <signal.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "genspkt.h"
#include "ip.h"
#include "util.h"

/* file scope prototypes */
static void updateFlagsAll(int offset);
static void updateVPlayer(struct player_spacket *p);
static void sendVPlayers(void);
void sendVPlanets(void);
void sendVKills(void);
int check_sendself_critical(struct player* pl, u_int flags, char armies, char swar,
                            short whydead, short whodead, char pnum);

int sizes[TOTAL_SPACKETS] = {
    0,	/* record 0 */
    sizeof(struct mesg_spacket),		/* SP_MESSAGE */
    sizeof(struct plyr_info_spacket), 		/* SP_PLAYER_INFO */
    sizeof(struct kills_spacket),		/* SP_KILLS */
    sizeof(struct player_spacket),		/* SP_PLAYER */
    sizeof(struct torp_info_spacket),		/* SP_TORP_INFO */
    sizeof(struct torp_spacket), 		/* SP_TORP */
    sizeof(struct phaser_spacket),		/* SP_PHASER */
    sizeof(struct plasma_info_spacket),		/* SP_PLASMA_INFO */
    sizeof(struct plasma_spacket),		/* SP_PLASMA */
    sizeof(struct warning_spacket),		/* SP_WARNING */
    sizeof(struct motd_spacket),		/* SP_MOTD */
    sizeof(struct you_spacket),			/* SP_YOU */
    sizeof(struct queue_spacket),		/* SP_QUEUE */
    sizeof(struct status_spacket),		/* SP_STATUS */
    sizeof(struct planet_spacket),		/* SP_PLANET */
    sizeof(struct pickok_spacket),		/* SP_PICKOK */
    sizeof(struct login_spacket),		/* SP_LOGIN */
    sizeof(struct flags_spacket),		/* SP_FLAGS */
    sizeof(struct mask_spacket),		/* SP_MASK */
    sizeof(struct pstatus_spacket),		/* SP_PSTATUS */
    sizeof(struct badversion_spacket),		/* SP_BADVERSION */
    sizeof(struct hostile_spacket),		/* SP_HOSTILE */
    sizeof(struct stats_spacket),		/* SP_STATS */
    sizeof(struct plyr_login_spacket),		/* SP_PL_LOGIN */
    sizeof(struct reserved_spacket), 		/* SP_RESERVED */
    sizeof(struct planet_loc_spacket),		/* SP_PLANET_LOC */
    sizeof(struct scan_spacket),		/* SP_SCAN (ATM) */
    sizeof(struct udp_reply_spacket),		/* SP_UDP_REPLY */
    sizeof(struct sequence_spacket),		/* SP_SEQUENCE */
    sizeof(struct sc_sequence_spacket),		/* SP_SC_SEQUENCE */
#ifdef RSA
    sizeof(struct rsa_key_spacket),		/* SP_RSA_KEY */
#else
    0,						/* 31 */
#endif
    sizeof(struct generic_32_spacket),		/* SP_GENERIC_32 */
    sizeof(struct flags_all_spacket),		/* SP_FLAGS_ALL */
    0,						/* 34 */
    0,						/* 35 */
    0,						/* 36 */
    0,						/* 37 */
    0,						/* 38 */
    sizeof(struct ship_cap_spacket),		/* SP_SHIP_CAP */
    sizeof(struct shortreply_spacket),          /* SP_S_REPLY */
    -1,                                         /* SP_S_MESSAGE */
    sizeof(struct warning_s_spacket),           /* SP_S_WARNING */
    sizeof(struct you_short_spacket),		/* SP_S_YOU */
    sizeof(struct youss_spacket),               /* SP_S_YOU_SS */
    -1, /* variable */                          /* SP_S_PLAYER */
    sizeof(struct ping_spacket),                /* SP_PING */
    -1, /* variable */                          /* SP_S_TORP  */
    -1, /* variable */				/* SP_S_TORP_INFO */
    20,                                         /* SP_S_8_TORP */
    -1, /* variable */                          /* SP_S_PLANET */
    0,						/* 51 */
    0,						/* 52 */
    0,						/* 53 */
    0,						/* 54 */
    0,						/* 55 */
    0,						/* 56 */
    -1, /* variable  S_P2 */			/* SP_S_PHASER */
    -1, /* variable */				/* SP_S_KILLS */
    sizeof (struct stats_s_spacket),           /* SP_S_STATS */
    sizeof(struct feature_spacket),		/* SP_FEATURE */
    sizeof(struct rank_spacket),		/* SP_RANK */
#ifdef LTD_STATS
    sizeof(struct ltd_spacket),			/* SP_LTD */
#endif
    0,						/* 63 */
};

/* a collection of previously sent packets by packet type and slot,
used to determine whether to send another one if critical values
change */
struct plyr_info_spacket clientPlayersInfo[MAXPLAYER];
struct plyr_login_spacket clientLogin[MAXPLAYER];
struct hostile_spacket clientHostile[MAXPLAYER];
struct stats_spacket clientStats[MAXPLAYER];
struct player_spacket clientPlayers[MAXPLAYER];
struct kills_spacket clientKills[MAXPLAYER];
struct flags_spacket clientFlags[MAXPLAYER];
struct pstatus_spacket clientPStatus[MAXPLAYER];
int msgCurrent;
struct torp_info_spacket clientTorpsInfo[MAXPLAYER*MAXTORP];
struct torp_spacket clientTorps[MAXPLAYER*MAXTORP];
struct phaser_spacket clientPhasers[MAXPLAYER];
struct phaser_s_spacket client_s_Phasers[MAXPLAYER];
struct you_spacket clientSelf;
struct status_spacket clientStatus;
struct planet_spacket clientPlanets[MAXPLANETS];
struct planet_loc_spacket clientPlanetLocs[MAXPLANETS];
struct plasma_info_spacket clientPlasmasInfo[MAXPLAYER*MAXPLASMA];
struct plasma_spacket clientPlasmas[MAXPLAYER*MAXPLASMA];
int mustUpdate[MAXPLAYER];


struct youss_spacket		clientSelfShip;
struct you_short_spacket	clientSelfShort;
struct generic_32_spacket	clientGeneric32;

/* HW */
u_char clientVPlanets[MAXPLANETS*sizeof(struct planet_s_spacket)+2 +6];
int    clientVPlanetCount;
static int	vtsize[9] ={ 4,8,8,12,12,16,20,20,24 }; /* How big is the
							 * SP_S_TORP packet */
static int	vtdata[9] ={0,3,5,7,9,12,14,16,18 }; /* How big is Torpdata */
static int	mustsend;    /* Flag to remind me that i must send SP_S_TORP */

static u_char	clientVTorps[40];

static u_char	clientVTorpsInfo[16];

static u_char    clientVPlayers[MAXPLAYER*VPLAYER_SIZE + 16];

/* S_P2 */
u_char clientVKills[MAXPLAYER*2+2 +4];
int clientVKillsCount;
struct stats_s_spacket singleStats;
static unsigned char n_flags[MAXPLAYER];
static int highest_active_player;

#if MAXPLAYER > 32
static u_char	clientVXPlayers[33*4];
#endif
static int	clientVPlayerCount;
static int	clientVXPlayerCount;
static int	big;

extern int ignored[];

/* Variables referenced from Socket.c */
extern int sizes[];
extern int use_newyou;
extern int f_many_self;
typedef void * PTR;			/* adjust this if you lack (void *) */
typedef struct fat_node_t {
    PTR packet;
    int pkt_size;
    struct fat_node_t *prev;
    struct fat_node_t *next;
} FAT_NODE;


/*****  Low level checks for packets *****/

#define UPDT_ALL 0
#define UPDT_MOST 1
#define UPDT_MANY 2
#define UPDT_LITTLE 3
#define UPDT_LEAST 4

int sndLogin( struct plyr_login_spacket* login, struct player* pl)
{
  static int f_sp_rank_was = 0;

  /* force resend if SP_RANK feature arrived since previous send */
  if (f_sp_rank_was != F_sp_rank) {
    if (F_sp_rank && pl->p_stats.st_rank > RANK_ADMIRAL) {
      login->rank = -1; /* invalidate previous packet data */
      f_sp_rank_was = F_sp_rank; /* indicate correct rank was sent */
    }
  }

  /* avoid resend if input data unchanged */
  if (pl->p_stats.st_rank == login->rank &&
      strcmp(pl->p_name, login->name) == 0 &&
      strcmp(pl->p_monitor, login->monitor) == 0 &&
      strcmp(pl->p_login, login->login) == 0) {
    return FALSE;
  }

  memset(login, 0, sizeof(struct plyr_login_spacket));

  login->type = SP_PL_LOGIN;
  login->pnum = pl->p_no;
  strncpy(login->name, pl->p_name, NAME_LEN);
  login->name[NAME_LEN-1] = 0;
  strncpy(login->monitor, pl->p_monitor, NAME_LEN);
  login->monitor[NAME_LEN-1] = 0;
  strncpy(login->login, pl->p_login, NAME_LEN);
  login->login[NAME_LEN-1] = 0;

  /* limit the rank sent to clients that do not understand higher ranks */
  login->rank = pl->p_stats.st_rank;
  if (pl->p_stats.st_rank > RANK_ADMIRAL && !F_sp_rank) {
    struct plyr_login_spacket limited;
    memcpy(&limited, login, sizeof(struct plyr_login_spacket));
    limited.rank = RANK_ADMIRAL;
    f_sp_rank_was = 0; /* indicated limited rank was sent */
    sendClientPacket(&limited);
  } else {
    sendClientPacket(login);
  }

  /* on every change to player list, check saved ignore status */
  if (me != pl && pl->p_status == PFREE) ip_ignore_login(me, pl);

  return TRUE;
}

inline static int
sndHostile( struct hostile_spacket *hostile, struct player *pl, int howmuch)
{
    if (howmuch == UPDT_ALL) {
	if ( (pl->p_swar != hostile->war)
	     || (pl->p_hostile != hostile->hostile)) {
	    hostile->type=SP_HOSTILE;
	    hostile->war=pl->p_swar;
	    hostile->hostile=pl->p_hostile;
	    hostile->pnum=pl->p_no;
	    sendClientPacket(hostile);
	    return TRUE;
	}
    } else {
	if ( (pl->p_swar & me->p_team)!=hostile->war
	     || (pl->p_hostile & me->p_team)!=hostile->hostile) {
	    hostile->type=SP_HOSTILE;
	    hostile->war=(pl->p_swar & me->p_team);
	    hostile->hostile=(pl->p_hostile & me->p_team);
	    hostile->pnum=pl->p_no;
	    sendClientPacket(hostile);
	    return TRUE;
	}
    }
    return FALSE;
}

int sndPlayerInfo( struct plyr_info_spacket *cpli, struct player *pl)
{
    if (pl->p_ship.s_type != cpli->shiptype ||
	pl->p_team != cpli->team) {
	cpli->type=SP_PLAYER_INFO;
	cpli->pnum=pl->p_no;
	cpli->shiptype=pl->p_ship.s_type;
	cpli->team=pl->p_team;
	sendClientPacket(cpli);
	return TRUE;
    }
    return FALSE;
}

inline static int updtKills( struct kills_spacket *kills, struct player *pl)
{
    if ( kills->kills != htonl((int) (pl->p_kills*100))) {
	kills->type=SP_KILLS;
	kills->pnum=pl->p_no;
	kills->kills=htonl((int) (pl->p_kills*100));
	return TRUE;
    }
    return FALSE;
}

inline static void addVKills( struct player *pl)
{
    /* S_P2 */
    static unsigned char *vkills;
    unsigned short shiftkills;

    if (!clientVKillsCount)
	vkills = &clientVKills[2];

    clientVKillsCount++;
    shiftkills = pl->p_kills * 100;
    shiftkills |= (pl->p_no << 10);
    *vkills++ = (unsigned char) shiftkills & 0xff;
    *vkills++ = (shiftkills >> 8) & 0xff;
}

int sndKills( struct kills_spacket *kills, struct player *pl)
{
    if (updtKills(kills, pl)) {
	/* S_P2 */
	if (send_short > 1 && (pl->p_kills*100 < 1001) && pl->p_no < 64)
	    addVKills(pl);
	else
	    sendClientPacket(kills);
	return TRUE;
    }
    return FALSE;
}

void sendVKills(void)
{
    if ( clientVKillsCount) {
	clientVKills[0] = SP_S_KILLS;
	clientVKills[1] = (unsigned char) clientVKillsCount;
	clientVKillsCount = 2+ (clientVKillsCount *2);
	if((clientVKillsCount % 4) != 0)
	    clientVKillsCount += (4 - (clientVKillsCount % 4));

	sizes[SP_S_KILLS] = clientVKillsCount;
	sendClientPacket((struct kills_spacket *) clientVKills);
    }

}

/* This function is for sending the flags of OTHER players, the player's
   own flags are handled elsewhere.  The cambot will set f_many_self,
   and use this function to record the flags of all players.  */
int sndFlags( struct flags_spacket *flags, struct player *pl, int howmuch)
{
/*#define FLAGMASK (PFSHIELD|PFBOMB|PFORBIT|PFCLOAK|PFROBOT|PFBEAMUP|PFBEAMDOWN|PFPRACTR|PFDOCK|PFTRACT|PFPRESS|PFDOCKOK) aieee, too much.  7/27/91 TC */ 
/*#define FLAGMASK (PFSHIELD|PFBOMB|PFORBIT|PFCLOAK|PFROBOT|                    PFPRACTR|PFDOCK|PFTRACT|PFPRESS|PFDOCKOK) still more than needed */

/* Flags we get to know about players not seen */
#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
#define INVISOMASK (PFCLOAK|PFROBOT|PFPRACTR|PFDOCKOK|PFOBSERV|PFBPROBOT)
#else
#define INVISOMASK (PFCLOAK|PFROBOT|PFPRACTR|PFDOCKOK|PFOBSERV)
#endif
/* Flags we get to know about players who are seen */
#define FLAGMASK   (PFSHIELD | INVISOMASK)

    int mask, masked;
    int tractor = ((F_show_all_tractors || f_many_self) && pl->p_flags&PFTRACT)?
                      (pl->p_tractor|0x40):0;

    if (!f_many_self && pl->p_status == POBSERV) {
        mask = PFOBSERV;        /* All we need to know about observers. */
        tractor = 0;
    } else if (howmuch == UPDT_ALL) {
        mask = FLAGMASK;
        if (F_show_all_tractors || f_many_self) mask |= PFTRACT|PFPRESS;
    } else {
        mask = INVISOMASK;
    }
    masked = htonl(pl->p_flags & mask);
    if (flags->flags == masked && flags->tractor == tractor)
        /* Nothing has changed, don't send a packet */
        return FALSE;

    flags->type = SP_FLAGS;
    flags->pnum = pl->p_no;
    flags->flags = masked;
    flags->tractor = tractor;

    sendClientPacket(flags);
    return TRUE;
}

/* Only called if clients send SP_FLAGS_ALL,
   sends all players' cloak, shield, and PALIVE status.
   The data is stored in the n_flags[i] array, similiar to how it is set in
   updateShips() for those using S_P2.  The packet format is designed to be
   compatable with how clients would handle a short packets flags header, see
   new_flags() in any COW-derived client. */
void updateFlagsAll(int offset)
{
    struct player *pl;
    struct flags_all_spacket flags_all;
    int i, j, new = 0;

    flags_all.type = SP_FLAGS_ALL;
    flags_all.offset = offset;
    for (i=flags_all.offset * 16, j=0;
         i < (flags_all.offset + 1) * 16 && i < MAXPLAYER;
         i++, j += 2) {
        pl = &players[i];
        switch (pl->p_status) {
            case POBSERV:
            case PALIVE:
                if (pl->p_flags & PFCLOAK)
                    n_flags[i] = FLAGS_ALL_CLOAK_ON;
                else if (pl->p_flags & PFSHIELD)
                    n_flags[i] = FLAGS_ALL_CLOAK_OFF_SHIELDS_UP;
                else
                    n_flags[i] = FLAGS_ALL_CLOAK_OFF_SHIELDS_DOWN;
                break;
            case PEXPLODE:
            case PDEAD:
                n_flags[i] = FLAGS_ALL_DEAD;
                break;
            default: /* Treat as dead */
                n_flags[i] = FLAGS_ALL_DEAD;
                break;
        }
        new = new | ((unsigned int)n_flags[i] << j);
    }
    flags_all.flags = (long) htonl(new);
    sendClientPacket(&flags_all);
}

static int observed_status(struct player *pl)
{
    if (pl->p_status != POBSERV) return pl->p_status;
    return (pl == me) ? PALIVE : POUTFIT;
}

int sndPStatus (struct pstatus_spacket *pstatus, struct player *pl)
{
    int ostatus = observed_status(pl);
    if (pstatus->status != ostatus) {
        pstatus->type = SP_PSTATUS;
        pstatus->pnum = pl->p_no;
        pstatus->status = ostatus;
        sendClientPacket(pstatus);
        if (dead_warp) dead_warp = 1;
        return TRUE;
    }
    return FALSE;
}

int updtPlayer( struct player_spacket *cpl, struct player *pl, int howmuch)
/* UPDT_ALL   : If needed send accurate position; accurate speed. (normal)
 * UPDT_MOST  : If needed send accurate position; speed = 15      (cloaking)
 * UPDT_MANY  : If needed send inacurate position; speed = 15     (cloaked)
 * UPDT_LITTLE: If needed send outdated packet              (cloaked; no update)
 * UPDT_LEAST : If needed send bogus position (-10000);     (player hidden)
 */
{
    if ( howmuch == UPDT_LEAST ) {
	if (ntohl(cpl->x) == -10000 && ntohl(cpl->y) == -10000)
	    return FALSE;

	/* Make player disappear */
	cpl->type = SP_PLAYER;
	cpl->x = cpl->y = htonl(-10000);
	cpl->pnum = pl->p_no;

	/* FEATURE: dead_warp */
	if ( (pl->p_status == PDEAD || pl->p_status == PEXPLODE) &&
	     dead_warp) {
	    cpl->dir = dead_warp++;
	    cpl->speed = 0xe;                   /* 14 = dead */
	} else {
	    cpl->dir = 0;
	    cpl->speed = 0;
	}
	return TRUE;
    }

    if ( pl->p_x != ntohl(cpl->x) || pl->p_y != ntohl(cpl->y)
	 || pl->p_dir != cpl->dir || pl->p_speed != cpl->speed) {

	/* FEATURE: dead_warp */
	if ( (pl->p_status == PDEAD || pl->p_status == PEXPLODE)
	     && dead_warp) {
	    cpl->type=SP_PLAYER;
	    cpl->pnum=pl->p_no;
	    cpl->dir = dead_warp++;
	    cpl->speed = 0xe;                   /* 14 = dead */
	    cpl->x = cpl->y = htonl(-10000);
	    return TRUE;
	}

	if (howmuch == UPDT_LITTLE) {
	    /* NEW: Send old position again */
	    return TRUE;
	}

	cpl->type=SP_PLAYER;
	cpl->pnum=pl->p_no;

	if (howmuch == UPDT_MANY) {
	    cpl->speed = 0xf;   /* NEW: Joe Rumsey, 15 == cloaked */
#ifdef AS_CLOAK
	    if (pl->p_ship.s_type == ASSAULT) {
		cpl->x=htonl(pl->p_x+(random() % 3000)-1500);
		cpl->y=htonl(pl->p_y+(random() % 3000)-1500);
	    } else
#endif
	    {
		cpl->x=htonl(pl->p_x+(random() % 2000)-1000);
		cpl->y=htonl(pl->p_y+(random() % 2000)-1000);
	    }
	} else {
	    cpl->dir = pl->p_dir;
	    cpl->x = htonl(pl->p_x);
	    cpl->y = htonl(pl->p_y);
	    if (howmuch == UPDT_ALL)
		cpl->speed = pl->p_speed;
	    else
		/* Feature: CLOAK_MAXWARP */
		cpl->speed = 0xf;   /* NEW: Joe Rumsey, 15 == cloaked */
	}

	return TRUE;
    }
    return FALSE;
}

inline static int
sndPlayer( struct player_spacket *cpl, struct player *pl, int howmuch)
{
    if (updtPlayer(cpl, pl, howmuch)) {
	if (send_short && !F_full_direction_resolution)
	    updateVPlayer(cpl);
	else
	    sendClientPacket(cpl);
	return TRUE;
    }
    return FALSE;
}

int sndSelfShip(struct youss_spacket *self, struct player *pl)
{
    if(ntohs(self->damage) != pl->p_damage ||
       ntohs(self->shield) != pl->p_shield ||
       ntohs(self->fuel) != pl->p_fuel ||
       ntohs(self->etemp) != pl->p_etemp ||
       ntohs(self->wtemp) != pl->p_wtemp) {

	self->type = SP_S_YOU_SS;

	if (f_many_self) {
	    /* f_many_self is turned on by cambot when it uses this code
	       to record the game to a file.  The normal server doesn't use
	       it.  Since the player number is stuck in pad1, it can't be
	       used to send player flags */
	    self->pad1 = pl->p_no;
	} else {
	    /* See if we can stuff some flags in the pad byte */
	    u_int f=0;
	    if (F_19flags) {
		f = pl->p_flags;
		self->pad1 = ((f&PFGREEN )?2:(f&PFYELLOW  )?1:0) +
			     ((f&PFBEAMUP)?2:(f&PFBEAMDOWN)?1:0)*3 +
			     ((f&PFPLOCK )?2:(f&PFPLLOCK  )?1:0)*9 +
			     ((f&PFPRESS )?1:(f&PFTRACT   )?2:0)*27 +
			     ((f&PFORBIT )?2:(f&PFDOCK    )?1:0)*81;
		f = (PFGREEN|PFYELLOW|PFRED|PFBEAMUP|PFBEAMDOWN|PFPLOCK|
		     PFPLLOCK|PFPRESS|PFTRACT|PFORBIT|PFDOCK);
	    } else if (F_self_8flags) {
		self->pad1 = pl->p_flags & 0xff;
		f = 0xff;
	    }
	    /* since flags are sent now, note the update */
	    if(f)
		clientSelfShort.flags = (clientSelfShort.flags & htonl(~f)) |
					htonl(pl->p_flags & f);
	}

	self->damage=htons(pl->p_damage);
	self->shield=htons(pl->p_shield);
	self->fuel=htons(pl->p_fuel);
	self->etemp=htons(pl->p_etemp);
	self->wtemp=htons(pl->p_wtemp);
	sendClientPacket((CVOID) self);
	return TRUE;
    }
    return FALSE;
}

int sndTorp(struct torp_info_spacket *tpi, struct torp_spacket *tp,
	    struct torp *t, int i, int howmuch)
{
    /*
     * If it was free before, and is still, do nothing.   */
    if ((t->t_status == TFREE) && (tpi->status == TFREE))
	return FALSE;

    if (howmuch == UPDT_ALL) {
	if ((t->t_war != tpi->war) || (t->t_status != tpi->status)) {
	    tpi->type   = SP_TORP_INFO;
	    tpi->war    = t->t_war;
	    tpi->status = t->t_status;
	    tpi->tnum   = htons(i);
	    sendClientPacket(tpi);
/*	    return TRUE;*/
	}
	if ((tp->x != htonl(t->t_x)) || (tp->y != htonl(t->t_y))) {
	    tp->type = SP_TORP;
	    tp->x    = htonl(t->t_x);
	    tp->y    = htonl(t->t_y);
	    tp->dir  = t->t_dir;
	    tp->tnum = htons(i);
	    sendClientPacket(tp);
	    return TRUE;
	}
    } else if (howmuch == UPDT_MOST) {
	tp->type = SP_TORP;
	tp->x    = htonl(t->t_x);
	tp->y    = htonl(t->t_y);
	tp->dir  = t->t_dir; /* The robot needs this */
	tp->tnum = htons(i);
	sendClientPacket(tp);
	if ( (t->t_status != tpi->status)
	     || ((t->t_war & me->p_team) != tpi->war)) {
	    tpi->type   = SP_TORP_INFO;
	    tpi->war    = t->t_war & me->p_team;
	    tpi->tnum   = htons(i);
	    tpi->status = t->t_status;
	    sendClientPacket(tpi);
	}
	return TRUE;
    } else {
	if ((t->t_status == TFREE) && (tpi->status == TEXPLODE)) {
	    tpi->status = TFREE;
	    return FALSE;
	}
	if (tpi->status != TFREE) {
	    tpi->type   = SP_TORP_INFO;
	    tpi->status = TFREE;
	    tpi->tnum   = htons(i);
	    sendClientPacket(tpi);
	    return TRUE;
	}
    }
    return FALSE;
}

int sndPlasma(struct plasma_info_spacket *tpi, struct plasma_spacket *tp,
	      struct torp *t, int i, int howmuch)
{
    /*
     * If it was free before, and is still, do nothing.   */
    if ((t->t_status == TFREE) && (tpi->status == TFREE))
	return FALSE;

    if (howmuch == UPDT_ALL) {
	if ((t->t_war != tpi->war) || (t->t_status != tpi->status)) {
	    tpi->type   = SP_PLASMA_INFO;
	    tpi->war    = t->t_war;
	    tpi->status = t->t_status;
	    tpi->pnum   = htons(i);
	    sendClientPacket(tpi);
/*	    return TRUE;*/
	}
	if ((tp->x != htonl(t->t_x)) || (tp->y != htonl(t->t_y))) {
	    tp->type = SP_PLASMA;
	    tp->x    = htonl(t->t_x);
	    tp->y    = htonl(t->t_y);
	    tp->pnum = htons(i);
	    sendClientPacket(tp);
	    return TRUE;
	}
    } else if (howmuch == UPDT_MOST) {
	tp->type = SP_PLASMA;
	tp->x    = htonl(t->t_x);
	tp->y    = htonl(t->t_y);
	tp->pnum = htons(i);
	sendClientPacket(tp);
	if ( (t->t_status != tpi->status)
	     || ((t->t_war & me->p_team) != tpi->war)) {
	    tpi->type   = SP_PLASMA_INFO;
	    tpi->war    = t->t_war & me->p_team;
	    /* In PP plasma mode, plasmas change teams, but client doesn't
	       know.  If player is at war with plasma's new team, set the
	       plasma's war flag, so client knows it is hostile. */
	    if (pingpong_plasma && (t->t_team & me->p_war))
		tpi->war |= me->p_team;
	    tpi->pnum   = htons(i);
	    tpi->status = t->t_status;
	    sendClientPacket(tpi);
	}
	return TRUE;
    } else {
	if ((t->t_status == TFREE) && (tpi->status == TEXPLODE)) {
	    tpi->status = TFREE;
	    return FALSE;
	}
	if (tpi->status != TFREE) {
	    tpi->type   = SP_PLASMA_INFO;
	    tpi->status = TFREE;
	    tpi->pnum   = htons(i);
	    sendClientPacket(tpi);
	    return TRUE;
	}
    }
    return FALSE;
}

inline static int
updtPhaser(struct phaser_spacket *ph, struct phaser *phase,
	   int i, int howmuch)
{
    if (howmuch == UPDT_ALL) {
	if ( ph->status!=phase->ph_status
	     || ph->dir!=phase->ph_dir
	     || ph->target!=htonl(phase->ph_target)) {
	    ph->pnum=i;
	    ph->type=SP_PHASER;
	    ph->status=phase->ph_status;
	    ph->dir=phase->ph_dir;
	    ph->x=htonl(phase->ph_x);
	    ph->y=htonl(phase->ph_y);
	    ph->target=htonl(phase->ph_target);
	    return TRUE;
	}
    } else {
	if (ph->status!=PHFREE) {
	    ph->pnum=i;
	    ph->type=SP_PHASER;
	    ph->status=PHFREE;
	    return TRUE;
	}
    }
    return FALSE;
}

inline static int
addVPhaser(struct phaser_spacket *ph, struct phaser_s_spacket *phs,
	   struct phaser *phase, int i, int howmuch)
{
    unsigned char damage;

    /* get rid of annoying compiler message */
    if (howmuch) ;

    switch(ph->status)
    {
    case PHFREE:
	phs->pnum = i;
	phs->type = SP_S_PHASER;
	phs->status = PHFREE;
	sizes[SP_S_PHASER] = 4;
	sendClientPacket (phs);
	break;
    case PHHIT:
	if(phase->ph_damage > 255)
	    damage = 255; /* Could be used as flag */
	else
	    damage = phase->ph_damage;
	phs->pnum = (i & 0x3f)| (damage & 0x30) << 2 ;
	phs->type = SP_S_PHASER;
	phs->status = (unsigned char)ph->status |(damage & 0x0f) << 4;
	phs->target = (phase->ph_target & 0x3f)|(damage & 192); 
	sizes[SP_S_PHASER] = 4;
	sendClientPacket (phs);
	break;
    case PHMISS:
	phs->pnum = i;
	phs->type = SP_S_PHASER;
	phs->status = ph->status;
	phs->target = ph->dir; /* Do not get confused */ 
	sizes[SP_S_PHASER] = 4;
	sendClientPacket (phs);
	break;
    case PHHIT2:
	phs->pnum = i;
	phs->type = SP_S_PHASER;
	phs->status = ph->status;
	phs->x = htons ((phase->ph_x+SCALE/2) / SCALE);
	phs->y = htons ((phase->ph_y+SCALE/2) / SCALE);
	phs->target = phase->ph_target; 
	sizes[SP_S_PHASER]= 8;
	sendClientPacket (phs);
	break;
    default:
	phs->pnum = i;
	phs->type = SP_S_PHASER;
	phs->status = ph->status;
	phs->dir = ph->dir;
	phs->x = htons ((phase->ph_x+SCALE/2) / SCALE);
	phs->y = htons ((phase->ph_y+SCALE/2) / SCALE);
	phs->target = phase->ph_target; 
	sizes[SP_S_PHASER]= sizeof(struct phaser_s_spacket);
	sendClientPacket (phs);
	break;
    }
    return TRUE;
}

int sndPhaser(struct phaser_spacket *ph, struct phaser_s_spacket *phs,
	      struct phaser *phase, int i, int howmuch)
{
    if (updtPhaser(ph, phase, i, howmuch)) {
	if ( send_short > 1 && (!(ph->status & 0xf0))
#if MAXPLAYER >= 65
	     && i < 64
#endif
#ifdef STURGEON
             /* Sturgeon phasers can vary from default length, need
                to send the end coordinate, so avoid short packet */
             && ((sturgeon && ph->status == PHMISS) ? 0 : 1)
#endif
	    )
	    addVPhaser(ph, phs, phase, i, howmuch);
	else
	    sendClientPacket(ph);
	return TRUE;
    }
    return FALSE;
}

/* Determines if the planet's info has changed since it was last sent to the
   client.  If so, it updates the planet packet with current data and returns
   true. */
inline static int
updtPlanet(struct planet_spacket *pl, struct planet *plan, int howmuch)
{
    if (howmuch == UPDT_ALL) {
	if ( pl->info != plan->pl_info
	     || pl->armies != htonl(plan->pl_armies)
	     || pl->owner != plan->pl_owner
	     || pl->flags != htons((short) (plan->pl_flags & PLFLAGMASK))) {
	    pl->type=SP_PLANET;
	    pl->pnum=plan->pl_no;
	    pl->info=plan->pl_info;
	    pl->flags=htons((short) (plan->pl_flags & PLFLAGMASK));
	    pl->armies=htonl(plan->pl_armies);
	    pl->owner=plan->pl_owner;
	    return TRUE;
	}
    } else { /* UPDT_LITTLE */
	if (pl->info & me->p_team) {
	    pl->type=SP_PLANET;
	    pl->pnum=plan->pl_no;
	    pl->info=0;
	    pl->flags=0;
	    pl->armies=0;
	    pl->owner=0;
	    return TRUE;
	}
    }
    return FALSE;
}

/* Given a normal planet packet in pl, pack the data onto the end of the
   VPlanet buffer and increment the count.  The vplanet packet is a more
   efficient way to send multiple planet updates with typical values.  */
inline static int
addVPlanet(struct planet_spacket *pl)
{
    static struct planet_s_spacket *npl;

    if (!clientVPlanetCount)
	npl = (struct planet_s_spacket *) &clientVPlanets[2];
    npl->pnum=pl->pnum;
    npl->info=pl->info;
    npl->flags=pl->flags;
    npl->armies=(u_char) ntohl(pl->armies);
    npl->owner=pl->owner;
    npl++;
    clientVPlanetCount++;
    return TRUE;
}

/* howmuch is how much info to send, UPDT_LITTLE means just the information
   sent for unscouted planets, UPDT_ALL means send everything.  The return
   value isn't actually used.  */
int sndPlanet(struct planet_spacket *pl, struct planet *plan, int howmuch)
{
    if (updtPlanet(pl, plan, howmuch)) {
	/* Vplanet is more efficient, but only works when there are less than
	   256 armies */
	if ( send_short  && plan->pl_armies < 256 )
	    addVPlanet(pl);
	else
	    sendClientPacket(pl);
	return TRUE;
    }
    return FALSE;
}

void
sendVPlanets(void)
{
    if (clientVPlanetCount != 0) {
	clientVPlanets[0] = SP_S_PLANET; /* The type */
	clientVPlanets[1] = clientVPlanetCount;
	clientVPlanetCount = 2 +(clientVPlanetCount * 6);
	/* make sure on machine word boundary (assuming 4 bytes per word) */
	if((clientVPlanetCount % 4) != 0)
	    clientVPlanetCount += (4 - (clientVPlanetCount % 4));

	sizes[SP_S_PLANET] = clientVPlanetCount;
	sendClientPacket((struct planet_spacket *) clientVPlanets);
    }
}

int sndPlanetLoc(struct planet_loc_spacket *pll, struct planet *plan)
{
    if ((pll->x != htonl(plan->pl_x)) || (pll->y != htonl(plan->pl_y))) {
	pll->x=htonl(plan->pl_x);
	pll->y=htonl(plan->pl_y);
	pll->pnum=plan->pl_no;
	strcpy(pll->name, plan->pl_name);
	pll->type=SP_PLANET_LOC;
	sendClientPacket(pll);
	return TRUE;
    }
    return FALSE;
    /* TODO: planet name changes by server admin are not propogated to client until planet is moved */
}

int updtMessageSMessage(struct message *cur)
{
    switch(cur->args[0]) {
    case DMKILL:
	if ( cur->args[3] < 32000 && cur->args[4] < 32
	     && cur->args[1] < 64 && cur->args[2] < 64 ) {
	    u_short tmp;

	    tmp = cur->args[3] | ((cur->args[4] & 16) << 11);
	    swarning(KILLARGS,(u_char)(tmp & 0xff),
		     (u_char)((tmp >> 8) & 0xff));
	    if (why_dead) {
		swarning(KILLARGS2,(u_char)cur->args[5],0);
	    }
	    tmp = (u_char)(cur->args[2] |
			  ((cur->args[4] & 12)<< 4));
	    swarning(DMKILL,(u_char)(cur->args[1]
				     | ((cur->args[4] & 3)<< 6)), tmp);
	    return TRUE;
	}
	break;
    case KILLARGS: /* Only to help the compiler */
	break;
    case KILLARGS2: /* Only to help the compiler */
	break;
    case DMKILLP:
	if (why_dead) {
	    swarning(KILLARGS2,(u_char)cur->args[5],(u_char)cur->args[4]);
	}
	swarning(DMKILLP, (u_char)cur->args[1], (u_char)cur->args[2]);
	return TRUE;
    case DMBOMB:
	swarning(ARGUMENTS, (u_char)cur->args[2],0); /* Damage */
	swarning(DMBOMB, (u_char)cur->args[1], (u_char)cur->args[3]); /* To get the vital info in one packet */
	return TRUE;
    case DMDEST:
	swarning(DMDEST, (u_char)cur->args[1], (u_char)cur->args[2]);
	return TRUE;
    case DMTAKE:
	swarning(DMTAKE, (u_char)cur->args[1], (u_char)cur->args[2]);
	return TRUE;
    case DGHOSTKILL:
	if(cur->args[2] < 64000){
	    swarning(KILLARGS,(u_char)(cur->args[2] & 0xff), (u_char)((cur->args[2] >> 8) & 0xff));
	    if (why_dead) {
		swarning(KILLARGS2,(u_char)cur->args[5],0);
	    }
	    swarning(DGHOSTKILL, (u_char)cur->args[1], 0);
	    return TRUE;
	}
	break;
    case SVALID:      /* We can send it over SP_S_MESSAGE HW */
	break;
    case SINVALID:    /* Can't be send over SP_S_MESSAGE */
	break;

    default:
	/* I must test if the message from GOD has a real header (== "GOD->") */
	/* This tests gets all messages from tools */
	if (strstr(cur->m_data,"GOD->") != NULL)
	    cur->args[0] = SVALID; /* It has a real header */
	break;
    }
    return FALSE;
}

void updtMessage(struct mesg_spacket *msg, struct message *cur)
{
    if ( send_short && (cur->m_from < MAXPLAYER || cur->args[0] == SVALID)
	 && (!(cur->m_flags & MLEAVE)) ) {
	/* This is the SAFE version.(All playermessages must have an header.)
	 * It's nicer without MLEAVE */

	struct mesg_s_spacket *shortmsg = (struct mesg_s_spacket *) msg;
	int size;

	shortmsg->type = SP_S_MESSAGE;
	shortmsg->m_flags=cur->m_flags;
	shortmsg->m_recpt=cur->m_recpt;
	shortmsg->m_from=cur->m_from;
	cur->m_data[MSG_LEN-1]='\0'; /* Is this all right ? */
	strcpy(&shortmsg->mesg,&cur->m_data[9]); /* 9 because of GOD messages */
	/* Now the size */
	size = strlen(&shortmsg->mesg);
	size += 6; /* 1 for '\0', 5 packetheader */
	if((size % 4) != 0)
	    size += (4 - (size % 4));
	shortmsg->length = size;
	sizes[SP_S_MESSAGE] = size;
    }
    else {
	msg->type=SP_MESSAGE;
	strncpy(msg->mesg, cur->m_data, MSG_LEN);
	msg->mesg[MSG_LEN-1]='\0';
	msg->m_flags=cur->m_flags;
	msg->m_recpt=cur->m_recpt;
	msg->m_from=cur->m_from;

	/* kludge for clients that can't handle RCD's */
	/* if I understood heiko's short_packet code I would put this in
	   the above ... but I dont - jmn */

	if ((cur->m_recpt == me->p_team) && !F_rc_distress && 
	    (cur->m_flags == (MTEAM | MDISTR | MVALID))) {

	    struct distress dist;
	    char buf[MSG_LEN];

	    buf[0] = '\0';
	    msg->m_flags ^= MDISTR; /* get rid of MDISTR flag so client
				      isn't confused */
	    HandleGenDistr(cur->m_data,cur->m_from,cur->m_recpt,&dist);
	    makedistress(&dist,buf,distmacro[dist.distype].macro);
	    /* note that we do NOT send the F0->FED part 
	       so strncat is fine */
	    strcpy(msg->mesg, buf);
	}

    }
    if (cur->m_from==DOOSHMSG) msg->m_from=255;	/* god */
    sendClientPacket((CVOID) msg);
}

/* End of low level packet update routines */

void
updateStatus(int force)
/* if force is false, packet only sent when status->tourn changes */
/* if force is true, send every 10 seconds as long as timeprod has changed */
{
    if ((clientStatus.tourn != status->tourn) ||
    	(force && !(repCount%efticks(50)) &&
	 ntohl(clientStatus.timeprod) != timeprod_int()))  {
#ifdef LTD_STATS
	/* Hey, Tmode changed.  Do I have an enemy? */
	setEnemy(me->p_team, me);
#endif /* LTD_STATS */
	clientStatus.type=SP_STATUS;
	clientStatus.tourn=status->tourn;
	clientStatus.armsbomb=htonl(status->armsbomb/10);
	clientStatus.planets=htonl(status->planets/10);
	clientStatus.kills=htonl(status->kills/10);
	clientStatus.losses=htonl(status->losses/10);
	clientStatus.time=htonl(status->time/10);
	clientStatus.timeprod=htonl(timeprod_int());
	sendClientPacket((CVOID) &clientStatus);
    }
}

/*! @brief translate p_whydead to older protocol meanings.
    @details if the WHY_DEAD_2 feature F_why_dead_2 is set, then the
    daemon values of p_whydead can be sent as is to the client,
    otherwise the values that are unique to WHY_DEAD_2 feature are
    translated to the older values. */
static int legacy_whydead(int p_whydead)
{
  if (F_why_dead_2) return p_whydead;
  if (p_whydead == KTORP2) return KTORP;
  if (p_whydead == KSHIP2) return KSHIP;
  if (p_whydead == KPLASMA2) return KPLASMA;
  return p_whydead;
}

/* Determine if sendself packet should be flagged as critical */
int check_sendself_critical(struct player* pl, u_int flags, char armies, char swar,
                            short whydead, short whodead, char pnum)
{
    int type;

    /* Determine packet type, as per updateSelf check */
    if(send_short && pl->p_fuel < 61000)
        type = SP_S_YOU;
    else
        type = SP_YOU;

    if (commMode == COMM_UDP) {
        if (    flags != pl->p_flags ||
                armies != pl->p_armies ||
                swar != pl->p_swar) {
            type = type | 0x40;     /* mark as semi-critical */
        }
        if (    whydead != legacy_whydead(pl->p_whydead) ||
                whodead != pl->p_whodead ||
                pnum != pl->p_no) {
            type = type | 0x80;     /* mark as critical */
        }
    }
    return type;
}

int sndSSelf(struct you_short_spacket *youp, struct player* pl, int howmuch)
{
    if ( howmuch == UPDT_ALL
	 || youp->pnum != pl->p_no
	 || youp->hostile != pl->p_hostile
	 || youp->swar != pl->p_swar
	 || youp->armies != pl->p_armies
	 || youp->whydead != legacy_whydead(pl->p_whydead)
	 || youp->whodead != pl->p_whodead
	 || ntohl(youp->flags) != pl->p_flags ) {

	/* we want to send it, but how? */
	youp->type = check_sendself_critical(pl, ntohl(youp->flags), youp->armies,
                                             youp->swar, youp->whydead,
                                             youp->whodead, youp->pnum);

	youp->pnum = pl->p_no;
	youp->hostile = pl->p_hostile;
	youp->swar = pl->p_swar;
	youp->armies = pl->p_armies;
	youp->whydead = legacy_whydead(pl->p_whydead);
	youp->whodead = pl->p_whodead;
	youp->flags = htonl(pl->p_flags);
	sendClientPacket((CVOID) youp);
	return TRUE;
    }
    return FALSE;
}

inline static int
sndSelf(struct you_spacket* youp, struct player* pl, int howmuch)
{
    int tractor = (pl->p_flags&PFTRACT)?(char)(pl->p_tractor|0x40):0;
    if ( howmuch == UPDT_ALL
	 || ntohl(youp->fuel) != pl->p_fuel
	 || ntohl(youp->shield) != pl->p_shield
	 || ntohl(youp->damage) != pl->p_damage
	 || ntohs(youp->etemp) != pl->p_etemp
	 || ntohs(youp->wtemp) != pl->p_wtemp
	 || ntohl(youp->flags) != pl->p_flags
	 || youp->armies != pl->p_armies
	 || youp->swar != pl->p_swar
	 || ntohs(youp->whydead) != legacy_whydead(pl->p_whydead)
	 || ntohs(youp->whodead) != pl->p_whodead
	 || youp->tractor != tractor
	 || youp->pnum != pl->p_no) {

	/* we want to send it, but how? */
	youp->type = check_sendself_critical(pl, ntohl(youp->flags), youp->armies,
                                             youp->swar, ntohs(youp->whydead),
                                             ntohs(youp->whodead), youp->pnum);

	youp->pnum=pl->p_no;
	youp->flags=htonl(pl->p_flags);
	youp->swar=pl->p_swar;
	youp->hostile=pl->p_hostile;
	youp->armies=pl->p_armies;
	youp->shield=htonl(pl->p_shield);
	youp->fuel=htonl(pl->p_fuel);
	youp->etemp=htons(pl->p_etemp);
	youp->wtemp=htons(pl->p_wtemp);
	youp->whydead=htons(legacy_whydead(pl->p_whydead));
	youp->whodead=htons(pl->p_whodead);
	youp->damage=htonl(pl->p_damage);
	youp->tractor=tractor;
	sendClientPacket((CVOID) youp);
	return TRUE;
    }
    return FALSE;
}

void
updateSelf(int force)
/* If force is true, a packet will be guarenteed to be sent.
 */
{
    static int again = 0; /* Flag, SP_S_YOU is always (almost) send two times. *
			   * Because SP_S_YOU is now send via UDP  HW 27.06.93 */

    if(send_short &&  me->p_fuel < 61000  ) { /* A little margin ... */

	sndSelfShip(&clientSelfShip, me);

	{
	    /* Send Self using short packets */

	    if (!sndSSelf(&clientSelfShort, me, UPDT_MOST))
		if (again || force) {
		    sndSSelf(&clientSelfShort, me, UPDT_ALL);
		    if (!force)
			/* again - sortof boolean HW,sent SP_S_YOU again */
			again ^= 1;
		}
	}

    } else
	/* Send Self using normal packets */
	sndSelf(&clientSelf, me, force ? UPDT_ALL : UPDT_MOST);
    sendGeneric32Packet();
}

void
updateShips(void)
{
    int i;
    struct player *pl;
    struct plyr_info_spacket *cpli;
    struct player_spacket *cpl;
    struct kills_spacket *kills;
    struct flags_spacket *flags;
    struct pstatus_spacket *pstatus;
    struct plyr_login_spacket *login;
    struct hostile_spacket *hostile;
    struct stats_spacket *stats;
    int update;

    clientVKillsCount = 0;
    highest_active_player = 0;
    /* S_P2 end */

    clientVPlayerCount = 12;
    clientVXPlayerCount = 4;
    clientVPlayers[1]   = 0; /* How many packets are in the Big... */
#if MAXPLAYER > 32
    clientVXPlayers[1] = 0;
#endif
    big=0;

    /* Please excuse the ugliness of this loop declaration */
    for (i=0, pl=players, cpli=clientPlayersInfo, cpl=clientPlayers,
	     kills=clientKills, flags=clientFlags, pstatus=clientPStatus,
	     login=clientLogin, hostile=clientHostile, stats=clientStats;
	 i<MAXPLAYER; 
	 i++, pl++, cpli++, cpl++, kills++, flags++, pstatus++, login++,
	     hostile++, stats++) {

	update=0;

	sndLogin(login, pl);

	if ( (pl==me) ||
	     ( !(me->p_flags & PFROBOT) && (me->p_team == NOBODY) ) )
	    sndHostile(hostile, pl, UPDT_ALL);
	else if (is_invisible_due_idle(pl))
	    sndHostile(hostile, pl, UPDT_LEAST);
	else
	    sndHostile(hostile, pl, UPDT_MOST);

	sndPlayerInfo(cpli, pl);
	sndKills(kills, pl);

	/* S_P2 new flag sampling */
	if (send_short > 1) {
	    u_int oldflags;
	    /* Skip observers' flags, unless I am the observer. */
	    if (pl->p_status != POBSERV || pl == me) {
		switch(pl->p_status){
		case POBSERV:
		case PALIVE: /* huh, we must work */
		    highest_active_player = i;
		    if(pl->p_flags & PFCLOAK){
			n_flags[i] = 1;
		    }
		    else if(pl->p_flags & PFSHIELD){
			n_flags[i] = 2;
		    }
		    else
			n_flags[i] = 3;
		    break;

		case PEXPLODE:
		case PDEAD:
		    highest_active_player = i;
		    n_flags[i] = 0;
		    break; 
		default: 
		    n_flags[i] = 0; 
		    /* Is it ok to send the old value ? */
		    break;
		}
		/* Mark shield and cloak as updated, so they won't be resent
		   again with a flags packet. */
		if (F_full_direction_resolution && !F_flags_all) ;
		else {
		    oldflags = ntohl(flags->flags);
		    oldflags &= ~(PFSHIELD|PFCLOAK);
		    oldflags |= pl->p_flags&(PFSHIELD|PFCLOAK);
		    flags->flags = htonl(oldflags);
		}
	    }
	}

	if (sndPStatus(pstatus, pl)) {
	    update=1;
	    if (pl->p_status == PFREE) { /* I think this will turn off ignores
					    for players that leave. 7/24/91 TC */
		ignored[i] = 0;
	    }
	}

	/* Used to send flags here, see below 8/7/91 TC */

	/* hidden enemies?  -- note OBSERVer is always inviso to everyone
	   else */

	if ( (hiddenenemy && pl->p_team != me->p_team
	      && !(pl->p_flags & PFSEEN) && status->tourn)
	     || (pl->p_status == POBSERV && pl != me)
	     || (is_invisible_due_idle(pl) && pl != me)
	    ) {

	    /* Player is invisible, send limited flags information,
	     * and bogus player position.
	     */

	    if (me!=pl)
		sndFlags(flags, pl, UPDT_MOST);

	    sndPlayer(cpl, pl, UPDT_LEAST);
	    continue;
	}

	if (me!=pl)
	    sndFlags(flags, pl, UPDT_ALL);

 	/* We update the player if any of the following are true:
	 * 0) galactic_smooth is set in sysdef
	 * 1) haven't updated him for 9 intervals.
	 * 2) he is on the screen
	 * 3) he was on the screen recently.
	 */
	if (!update && !galactic_smooth && repCount % efticks(9) != 0 &&
	    (ntohl(cpl->x) < me->p_x - SCALE*WINSIDE/2 ||
	     ntohl(cpl->x) > me->p_x + SCALE*WINSIDE/2 ||
	     ntohl(cpl->y) > me->p_y + SCALE*WINSIDE/2 ||
	     ntohl(cpl->y) < me->p_y - SCALE*WINSIDE/2) &&
	    (pl->p_y > me->p_y + SCALE*WINSIDE/2 ||
	     pl->p_x > me->p_x + SCALE*WINSIDE/2 ||
	     pl->p_x < me->p_x - SCALE*WINSIDE/2 ||
	     pl->p_y < me->p_y - SCALE*WINSIDE/2)) continue;

	/* If the guy is cloaked, give information only occasionally,
	 *  and make it slightly inaccurate.
	 * Also, we don't give a direction.  The client has no reason
	 *  to know.
	 */
	if ( (pl->p_flags & PFCLOAK) &&
	     (pl->p_cloakphase == (CLOAK_PHASES - 1)) &&
	     (me != pl) && !mustUpdate[i]) {
	    if (repCount % efticks(9) != 0) {
		/* Player is cloaked on users screen,
		 * but is not ready for another update.
		 * Resend old packet.
		 */
		sndPlayer(cpl, pl, UPDT_LITTLE);
		continue;
	    }

	    /* Player is cloaked on users screen,
	     * update his position with innacurate location
	     */
	    sndPlayer(cpl, pl, UPDT_MANY);
	    continue;
	}
	if (pl != me && (pl->p_flags & PFCLOAK))
	    /* Player is cloaking on user screen */
	    sndPlayer(cpl, pl, UPDT_MOST);
	else
	    /* Send normal postion for player */
	    sndPlayer(cpl, pl, UPDT_ALL);
    }
    if(send_short && (clientVPlayerCount > 12 ||        clientVXPlayerCount > 4
		      || big != 0 || send_short > 1 /* S_P2 fl;agsampling */))
	sendVPlayers();

    /* S_P2 */
    if(send_short > 1)
	sendVKills();

    if (F_flags_all) {
	if (send_short) {
	    if (F_full_direction_resolution) {
		updateFlagsAll(0);
		if (highest_active_player > 15)
		    updateFlagsAll(1);
	    }
	}
	else {
	    updateFlagsAll(0);
	    if (highest_active_player > 15)
		updateFlagsAll(1);
	}
    }
}

void
updateTorps(void)
{
    struct torp *t;
    int i;
    struct torp_info_spacket *tpi;
    struct torp_spacket *tp;

    for (i=0, t=firstTorp, tpi=clientTorpsInfo, tp=clientTorps; 
	 t<=lastTorp; i++, t++, tpi++, tp++) {

	/*
	 * If it's my torp, send info on it regardless of position;
	 * helps clients to display number of torps in flight, and allows
	 * observers to show all torps */
	if (myTorp(t)
	    || (F_full_weapon_resolution && me->p_status == POBSERV)
	) {
	    sndTorp(tpi, tp, t, i, UPDT_ALL);
	    continue;
	}

	/* 
	 * Not my torp.
	 * If not in view, only send limited information
	 * Full weapon resolution torps are skipped, they are instead
	 * sent via SupdateTorps */
	if (F_full_weapon_resolution)
	    continue;
	if ((t->t_y > me->p_y + SCALE*WINSIDE/2) ||
	    (t->t_y < me->p_y - SCALE*WINSIDE/2) ||
	    (t->t_x > me->p_x + SCALE*WINSIDE/2) ||
	    (t->t_x < me->p_x - SCALE*WINSIDE/2) ||
	    (t->t_status == TFREE)) {
	    sndTorp(tpi, tp, t, i, UPDT_LITTLE);
	    continue;
	} 

	/* 
	 * Not my torp, and in view.
	 * Send torp info (we assume it moved) */
	sndTorp(tpi, tp, t, i, UPDT_MOST);
    }
}

void
updatePlasmas(void)
{
    struct torp *t;
    int i;
    struct plasma_info_spacket *tpi;
    struct plasma_spacket *tp;

    for (i=0, t=firstPlasma, tpi=clientPlasmasInfo, tp=clientPlasmas; 
	 t<=lastPlasma; i++, t++, tpi++, tp++) {
	if (myTorp(t)
	    || (F_full_weapon_resolution && me->p_status == POBSERV)
        ) {
	    sndPlasma(tpi, tp, t, i, UPDT_ALL);
	    continue;
	}

	/* 
	 * Not my torp.
	 * If not in view, only send limited information   */
	if ((t->t_y > me->p_y + SCALE*WINSIDE/2) ||
	    (t->t_y < me->p_y - SCALE*WINSIDE/2) ||
	    (t->t_x > me->p_x + SCALE*WINSIDE/2) ||
	    (t->t_x < me->p_x - SCALE*WINSIDE/2) ||
	    (t->t_status == TFREE)) {
	    sndPlasma(tpi, tp, t, i, UPDT_LITTLE);
	    continue;
	} 

	/* 
	 * Not my torp, and in view.
	 * Send torp info (we assume it moved) */
	sndPlasma(tpi, tp, t, i, UPDT_MOST);
    }
}

void
updatePhasers(void)
{
    int i;
    struct phaser_spacket *ph;
    struct phaser *phase;
    struct player *pl;
    struct phaser_s_spacket *phs;

    /* Clear old mustUpdate values */
    for (i=0; i<MAXPLAYER; i++) {
	mustUpdate[i]=0;
    }

    for (i = 0, ph = clientPhasers,phs = client_s_Phasers,
	     phase = phasers, pl = players; 
	 i < MAXPLAYER; i++, ph++, phs++, phase++, pl++) { 
	if ((pl->p_y > me->p_y + SCALE*WINSIDE/2 ||
	     pl->p_x > me->p_x + SCALE*WINSIDE/2 ||
	     pl->p_x < me->p_x - SCALE*WINSIDE/2 ||
	     pl->p_y < me->p_y - SCALE*WINSIDE/2)
	    && ((F_full_weapon_resolution && me->p_status == POBSERV) ? 0: 1)
	) {
	    sndPhaser(ph, phs, phase, i, UPDT_LITTLE);
	} else {
	    if (phase->ph_status==PHHIT) {
		mustUpdate[phase->ph_target]=1;
	    }
	    sndPhaser(ph, phs, phase, i, UPDT_ALL);
	}
    }
}

void updatePlanets(void)
{
    int i;
    struct planet_spacket *pl;
    struct planet *plan;
    struct planet_loc_spacket *pll;

    clientVPlanetCount = 0;

    for (i=0, pl=clientPlanets, plan=planets, pll=clientPlanetLocs; 
	 i<MAXPLANETS;
	 i++, pl++, plan++, pll++) {
	/* Send him info about him not having info if he doesn't but thinks
	 *  he does.
	 * Also send him info on every fifth cycle if the planet needs to
	 *  be redrawn.
	 * Indi sees all planets.  New joiners are indi, but x and y 
	 *  are negative for new joiners. Don't want to send them all info.
	 */
	if ((me->p_x > 0) && (me->p_y > 0) && (me->p_team == NOBODY) ) {
	    sndPlanet(pl, plan, UPDT_ALL);
	} else if ( plan->pl_info & me->p_team ) {
	    /* scouted */
	    sndPlanet(pl, plan, UPDT_ALL);
	} else {
	    /* Not scouted */
	    sndPlanet(pl, plan, UPDT_LITTLE);
	}
	/* Assume that the planet only needs to be updated once... */

	/* Odd, changes in pl_y not supported.  5/31/92 TC */

	sndPlanetLoc(pll, plan);
    }

    /* Now we can send our Planet Packet */
    sendVPlanets();
}


/* give session stats if you send yourself a '?' 2/27/92 TC */
/* return true if you should eat message */

static int parseQuery(struct message *msg)
{
    /* 0-8 for address, 9 is space */
    char *cchar = &msg->m_data[10];

    if (*cchar == '?' && *(cchar+1) == '\0')
	return bounceSessionStats(msg->m_from);
    if (*cchar == '!' && *(cchar+1) == '\0')
	return bouncePingStats(msg->m_from);
#ifdef RSA
    if (*cchar == '#' && *(cchar+1) == '\0')
	return bounceRSAClientType(msg->m_from);
#endif
    if (*cchar == '^' && *(cchar+1) == '\0')
	return bounceSBStats(msg->m_from);
    if (*cchar == '@' && *(cchar+1) == '\0')
	return bounceWhois(msg->m_from);
    if (strcmp(cchar, "udpstats") == 0)
	return bounceUDPStats(msg->m_from);

    return FALSE;
}

void updateMessages(void)
{
    struct message *cur;
    struct mesg_spacket msg;
    int reset_flags = 0;
    int send_msg;
    struct player *p_from;
    int is_whitelisted = 0;
    int warning_shown = 0;

    for (; msgCurrent!=(mctl->mc_current+1) % MAXMESSAGE;
         msgCurrent=(msgCurrent+1) % MAXMESSAGE) {
        if (msgCurrent>=MAXMESSAGE)
            msgCurrent=0;
        cur = &messages[msgCurrent];

        if ((macroignore && cur->m_flags & MMACRO)
            || (dooshignore && cur->m_from == DOOSHMSG)) {
            continue;
        }

        if (cur->m_flags & MVALID &&
            (cur->m_flags & MALL ||
             (cur->m_flags & MTEAM && cur->m_recpt & me->p_team) ||
             (cur->m_flags & MINDIV && cur->m_recpt == me->p_no)
                )) {

/* hack for displaying MINDIV && MCONQ messages to stdout on the clients.  
   Clients only show MALL && MCONQ.  but we don't want to send to all.
   So at this point, MINDIV has already directed it to the right guy.
   Now, cancel MINDIV and set MALL, for the client's benefit.
   ATH, 11/5/95 */

            if ((cur->m_flags & MINDIV) && (cur->m_flags & MCONQ)) {
                cur->m_flags &= ~MINDIV;
                cur->m_flags |= MALL;
                reset_flags = 1;
            }

            if(send_short && (cur->m_from == 255)) {
                /* Test if possible to send with SP_S_WARNING */
                if (updtMessageSMessage(cur)) {
                    continue;
                }
            }

            send_msg = FALSE;

            if ((cur->m_from < 0) || (cur->m_from > MAXPLAYER)) {
                is_whitelisted = 0;
            } else {
                p_from = p_no(cur->m_from);
                is_whitelisted = ip_whitelisted(p_from->p_ip);
            }

            if ((cur->m_from < 0) || (cur->m_from > MAXPLAYER))
                send_msg = TRUE;
            else if (cur->m_flags & MALL) {
                if (ignored[cur->m_from] & MALL) {
                    if (is_whitelisted && whitelist_all)
                        send_msg = TRUE;
                } else {
                    send_msg = TRUE;
                }
            } else if (cur->m_flags & MTEAM) {
                if (ignored[cur->m_from] & MTEAM) {
                    if (is_whitelisted && whitelist_team)
                        send_msg = TRUE;
                } else {
                    send_msg = TRUE;
                }
            } else if (cur->m_flags & MINDIV) {
                int query = 0;

                /* session stats now parsed here.  parseQuery == true */
                /* means eat message 4/17/92 TC */
                query = parseQuery(cur);
                if (!query) {
                    if (ignored[cur->m_from] & MINDIV) {
                        if (is_whitelisted && whitelist_indiv) {
                            send_msg = TRUE;
                            if (!warning_shown) {
                                god(cur->m_from,
                                    "Player is ignoring you but your whitelist entry overrides it.");
                                warning_shown = 1;
                            }
                        } else {
                            god(cur->m_from,
                                "That player is currently ignoring you.");
                        }
                    } else {
                        send_msg = TRUE;
                    }
                }
            }
#ifdef CONTINUUM_MUTE_COMMANDS
            /* observer muting commands, available to players */
            if (send_msg) {
                char *cchar = &cur->m_data[10];
                if (!strcasecmp(cchar, "mute on")) {
                    if (me->p_status == POBSERV) {
                        if (!mute) {
                            god(cur->m_from, "Mute enabled.");
                            mute = TRUE;
                        }
                        send_msg = FALSE;
                    }
                } else if (!strcasecmp(cchar, "mute off")) {
                    if (me->p_status == POBSERV) {
                        if (mute) {
                            god(cur->m_from, "Mute disabled.");
                            mute = FALSE;
                        }
                        send_msg = FALSE;
                    }
                }
            }
#endif
            if (send_msg)
                updtMessage(&msg, cur);
        }
/* Put eject message back as MINDIV, so everyone doesn't get it later */
        if (reset_flags) {
            cur->m_flags &= ~MALL;
            cur->m_flags |= MINDIV;
            reset_flags = 0;
        }
    }
}

static void
updateVPlayer(struct player_spacket *p)
{
    int		i = clientVPlayerCount; 
#if MAXPLAYER > 32
    int		j = clientVXPlayerCount;
#endif
    U_LONG	x = ntohl(p->x), y = ntohl(p->y);
    int 	dir,dx,dy;
    int     		view = SCALE * WINSIDE / 2;

    if (p->pnum == me->p_no){
	if ( send_short > 1) { /* S_P2 */
	    struct player_s2_spacket  *pl = (struct player_s2_spacket  *) &clientVPlayers[0];
	    pl->x = (short) htons((x+SCALE/2)/SCALE) /* p->x*/ ;
	    pl->y = (short) htons((y+SCALE/2)/SCALE) /*p->y*/;
	    pl->speed = p->speed;
	    pl->dir = p->dir;
	    big = 1; /* Flag to remind me that we must send big header */
	}
	else {
	    struct player_s_spacket  *pl = (struct player_s_spacket  *) &clientVPlayers[0];
	    pl->x = (long) htonl(x)/* p->x*/ ;
	    pl->y = (long) htonl(y) /*p->y*/;
	    pl->speed = p->speed;
	    pl->dir = p->dir;
	    big = 1; /* Flag to remind me that we must send big header */
	}
    }
    else
#if (MAXPLAYER > 32)
	if ( p->pnum < 32)
#endif
	{
	    clientVPlayers[1] += 1; /* Another packet... */
	    /* Where is the ship? On local or galactic map. */
	    dx = (int) x - me->p_x;
	    dy = (int) y - me->p_y;
	    if (dx > view || dx < -view || dy > view || dy < -view) { /* Galactic */
		dx = (int) x * WINSIDE / GWIDTH; /* Orig. */
		dy = (int) y * WINSIDE / GWIDTH;
		if ( dx < 0 || dy < 0 || dx >= WINSIDE || dy >= WINSIDE) {
		    dx = 501; /* clipped */
		    dy = 501;
		}
		dir = rosette (p->dir);
		clientVPlayers[i++] = (u_char) ((p->pnum)|((dx  & 256) >> 2)|((dy & 256)>>1) | 32);
		clientVPlayers[i++] = ( u_char)(( p->speed & 0x0f)|(dir << 4));
		clientVPlayers[i++] = (u_char) (dx & 0xff);
		clientVPlayers[i++] = (u_char) ( dy & 0xff);
		clientVPlayerCount = i;		    
	    }
	    else {   /* Local */
		/*
		  dx = dx / SCALE + WINSIDE / 2;
		  dy = dy / SCALE + WINSIDE / 2;
		  */

		/* NEW -- fix precision loss -tsh */
		dx = (int) x/SCALE - me->p_x/SCALE + WINSIDE/2;
		dy = (int) y/SCALE - me->p_y/SCALE + WINSIDE/2;

		dir = rosette (p->dir);
		clientVPlayers[i++] = (u_char) ((p->pnum)|((dx  & 256) >> 2)|((dy & 256)>>1) );
		clientVPlayers[i++] = ( u_char)(( p->speed & 0x0f)|(dir << 4));
		clientVPlayers[i++] = (u_char) (dx & 0xff);
		clientVPlayers[i++] = (u_char) ( dy & 0xff);
		clientVPlayerCount = i;		
	    }
	}
#if (MAXPLAYER > 32)
	else
#if (MAXPLAYER > 64)
	    if ( p->pnum < 64)
#endif
	    {
		clientVXPlayers[1] += 1;
		/* Where is the ship? On local or galactic map. */
		dx = x - me->p_x;
		dy = y - me->p_y;
		if (dx > view || dx < -view || dy > view || dy < -view) { /* Galactic */
		    dx = x * WINSIDE / GWIDTH; /* Orig. */
		    dy = y * WINSIDE / GWIDTH;
		    if ( dx < 0 || dy < 0 || dx >= WINSIDE || dy >= WINSIDE) {
			dx = 501; /* clipped */
			dy = 501;
		    }
		    dir = rosette (p->dir);
		    clientVXPlayers[j++] = (u_char) ((p->pnum - 32)|((dx  & 256) >> 2)|((dy & 256)>>1) | 32);
		    clientVXPlayers[j++] = (u_char)(( p->speed & 0x0f)|(dir << 4));
		    clientVXPlayers[j++] = (u_char) (dx & 0xff);
		    clientVXPlayers[j++] = (u_char) ( dy & 0xff);
		    clientVXPlayerCount = j;		    
		}
		else {   /* Local */
		    dx = dx / SCALE + WINSIDE / 2;
		    dy = dy / SCALE + WINSIDE / 2;
		    dir = rosette (p->dir);
		    clientVXPlayers[j++] = (u_char) ((p->pnum-32)|((dx  & 256) >> 2)|((dy & 256)>>1) );
		    clientVXPlayers[j++] = (u_char)(( p->speed & 0x0f)|(dir << 4));
		    clientVXPlayers[j++] = (u_char) (dx & 0xff);
		    clientVXPlayers[j++] = (u_char) ( dy & 0xff);
		    clientVXPlayerCount = j;
		}
	    }
#if (MAXPLAYER > 64)
	    else  sendClientPacket((struct player_spacket *) p);
#endif

#endif /* MAXPLAYER > 32 */
}

static void sendVPlayers(void)
{
    unsigned int size,i,j,new;
    /* initialize type & length fields */
    clientVPlayers[0] = (u_char) SP_S_PLAYER;
#if MAXPLAYER > 32
    clientVXPlayers[0] = (u_char) SP_S_PLAYER;
#endif


    if ( big != 0) {	/* Big Packet */
	if ( send_short > 1) { /* S_P2 */
	    struct player_s2_spacket  *pa = (struct player_s2_spacket  *) &clientVPlayers[0];
	    pa->flags = 0;
            new = 0;

	    for(i=0,j=0; i < 16 && i < MAXPLAYER;i++ , j += 2) {
		new = new | ((unsigned int)n_flags[i] << j); 	
	    } /* for */
	    pa->flags = (long) htonl(new);
	    size = 12+(clientVPlayers[1]*4);
	    sizes[SP_S_PLAYER] = size;
	    sendClientPacket((struct player_spacket *) clientVPlayers);	
	    if(highest_active_player > 15 && highest_active_player < 32) {
		struct top_flags *ptr = (struct top_flags *) &clientVPlayers[4];
		new = 0;
		for(i=16,j=0; i < 32 && i < MAXPLAYER; i++, j +=2) {
		    new = new | ((unsigned int)n_flags[i] << j);
		} /* for */
		ptr->tflags = htonl(new);
		ptr->type   = (u_char) SP_S_PLAYER;
		ptr->packets = (unsigned char) 64;
		ptr->numflags = 1;
		ptr->index    = 1;
		size = 8; 
		sizes[SP_S_PLAYER] = size;
		sendClientPacket((struct player_spacket *) &clientVPlayers[4]);
	    }
	} /* if */
	else {
	    size = 12+( clientVPlayers[1]*4);
	    sizes[SP_S_PLAYER] = size;
	    sendClientPacket((struct player_spacket *) clientVPlayers);
	}
    }
    else if ( clientVPlayerCount > 12){ /* Small packet */

	if (send_short > 1) { /* S_P2 */
	    if(highest_active_player > 15 && highest_active_player < 32) {
		struct top_flags *ptr = (struct top_flags *) &clientVPlayers[0];
		new = 0;
		size = 12 + ( clientVPlayers[1]*4);
		for(i=16,j=0; i < 32 && i < MAXPLAYER; i++, j +=2) {
		    new = new | ((unsigned int)n_flags[i] << j);
		} /* for */
		ptr->tflags = htonl(new);
		new=0;
		for(i=0,j=0; i < 16 && i < MAXPLAYER;i++ , j += 2) {
		    new = new | ((unsigned int)n_flags[i] << j);
		} /* for */
		ptr->tflags2 = htonl(new);
		ptr->type   = (u_char) SP_S_PLAYER;
		ptr->packets = (unsigned char) clientVPlayers[1] | 64;
		ptr->numflags = 2;
		ptr->index    = 1;
		sizes[SP_S_PLAYER] = size;
		sendClientPacket((struct player_spacket *) &clientVPlayers[0]);
	    }
	    else { /* No players > 15 */
		struct top_flags *ptr = (struct top_flags *) &clientVPlayers[4];
		new = 0;
		size = 8 + ( clientVPlayers[1]*4);
		for(i=0,j=0; i < 16 && i < MAXPLAYER; i++, j +=2) {
		    new = new | ((unsigned int)n_flags[i] << j);
		} /* for */
		ptr->tflags = htonl(new);
		ptr->type   = (u_char) SP_S_PLAYER;
		ptr->packets = (unsigned char) clientVPlayers[1] | 64;
		ptr->numflags = 1;
		ptr->index    = 0;
		sizes[SP_S_PLAYER] = size;
		sendClientPacket((struct player_spacket *) &clientVPlayers[4]);
	    } /* active player if */ 
	} /* S_P2 */
	else { 
	    size = 4 + ( clientVPlayers[1]*4);
	    sizes[SP_S_PLAYER] = size;
	    clientVPlayers[8] = (u_char) SP_S_PLAYER;
	    clientVPlayers[9] = clientVPlayers[1] | 64;
	    clientVPlayers[10]=0;
	    clientVPlayers[11]=0;
	    sendClientPacket((struct player_spacket *) &clientVPlayers[8]);
	} 
    }

#if MAXPLAYER > 32
    if ( clientVXPlayerCount > 4) { /* Small + extended Packet */
	sizes[SP_S_PLAYER] = (clientVXPlayers[1]*4)+4;
	clientVXPlayers[1] |= 128; /* To mark it as a short header + Extended */
	sendClientPacket((struct player_spacket *) clientVXPlayers);
    }
#endif
}

/* New Torp Routine */
void SupdateTorps(void)
{
    struct torp *torp;
    int i,j;
    struct torp_info_spacket *tpi;
    struct torp_spacket *tp;
    u_char  *tinfo; 	/* TorpInfo data   */
    u_char  *torp_xy; /* The torp coord. */
    u_char  infobitset, torpbitset;
    int numtorps ; /* How many torps ? */

    int shift; /* Shift for 9 bit xy coordinate */
    int dx,dy; 
    int     view = SCALE * WINSIDE / 2;
    mustsend = 0;

    for (   j=0, torp=torps, tpi=clientTorpsInfo, tp=clientTorps; 
#if ((MAXPLAYER*MAXTORP) % 8 == 0)
	    j<	(MAXPLAYER * MAXTORP)/8;
#else
/* This normally wont test true..  But this will make the code more flexible,
 * in case someone starts messing with the defaults.. */
	    j<  (MAXPLAYER*MAXTORP + 7)/8;
#endif
	    j++) {

	shift = 0;
	numtorps = 0;
	infobitset 	= 0;
	torpbitset = 0;
	torp_xy = &clientVTorps[4];
	*torp_xy = 0; /* Clear first data byte */
	tinfo = & clientVTorpsInfo[0];

	for (i=0;  i<8;  i++, torp++, tpi++, tp++) {

#if ((MAXPLAYER*MAXTORP) % 8 != 0)
/* This normally wont test true..  But this will make the code more flexible,
 * in case someone starts messing with the defaults.. */
	    if (j*8+i >= (MAXPLAYER * MAXTORP))
		break;
#endif

	    if (torp->t_owner==me->p_no) {
		/* With full weapon resolution, self torps sent via
		   updateTorps */
		if (F_full_weapon_resolution)
		    continue;
		if (torp->t_war!=tpi->war ||
		    torp->t_status!=tpi->status) {
		    if (torp->t_war == tpi->war){
			/* Don't send TMOVE/TFREE . It's encoded in
			   the SP_S_TORP bitset. */
			switch(torp->t_status){
			case TFREE:
			    if (tpi->status == TEXPLODE){
				/* Send real TFREE if torp exploded */

				/* set bit for torp */
				infobitset = infobitset | ( 01  << i);
				*tinfo++ = ((u_char)tpi->war & 0x0f)
				    | ((u_char) torp->t_status << 4);
			    }
			    else
				mustsend = 1;
			    tpi->status=torp->t_status;
			    /* Don't forget... */
			    tp->x=htonl(torp->t_x);
			    tp->y=htonl(torp->t_y);
			    tp->dir=torp->t_dir;

			    continue; /* We continue , because server sends
					 obsolete data otherwise */
			case TMOVE:
			    tpi->status=torp->t_status;
			    break;
			default:
			    tpi->status=torp->t_status;
			    /* set bit for torp */
			    infobitset = infobitset | ( 01  << i);
			    *tinfo++ = ((u_char)tpi->war & 0x0f)
				|((u_char) tpi->status << 4);
			    break;
			}
		    }
		    else {
			tpi->war=torp->t_war;
			/* set bit for torp */
			tpi->status=torp->t_status;
			infobitset = infobitset | ( 01  << i);
			*tinfo++ = ((u_char)tpi->war & 0x0f)
			    |((u_char) tpi->status << 4);
		    }
		}
		if ( tp->x!=htonl(torp->t_x)
		     || tp->y!=htonl(torp->t_y)
		     || ( (torp->t_gspeed == 0)
			  && (torp->t_status != TFREE))) { /* Hadley */
		    tp->type=SP_TORP;
		    tp->x=htonl(torp->t_x);
		    tp->y=htonl(torp->t_y);
		    tp->dir=torp->t_dir;
		    /* Now the fun begins .. */

		    /* set bit for torp */
		    torpbitset = torpbitset | ( 01  << i);
		    numtorps++;
		    /* Now the Coordi. */
		    dx = torp->t_x - me->p_x;
		    dy = torp->t_y - me->p_y;
		    if (dx > view || dx < -view || dy > view || dy < -view) {
			/* any torps off screen appear at 501 = clipped */
			dx = dy = 501;
			dx <<= shift;
			*torp_xy++  |= ( dx & 255);
			*torp_xy = (((u_int) dx >> 8) & 255);
			shift++;
			dy <<= shift;
			*torp_xy++  |= (dy & 255);
			*torp_xy = (((u_int) dy >> 8) & 255);
			shift++;
			if ( shift == 8){
			    shift = 0;
			    torp_xy++;
			    *torp_xy = 0;
			}
			continue;
		    }
		    /*
		      dx = dx / SCALE + WINSIDE / 2;
		      dy = dy / SCALE + WINSIDE / 2;
		      */
		    /* NEW: fix precision loss -tsh */
		    dx = torp->t_x/SCALE - me->p_x/SCALE + WINSIDE/2;
		    dy = torp->t_y/SCALE - me->p_y/SCALE + WINSIDE/2;

		    dx <<= shift;
		    *torp_xy++  |= (dx & 0xff );
		    *torp_xy = (((u_int) dx >> 8) & 255);
		    shift++;
		    dy <<= shift;
		    *torp_xy++  |= (dy & 255);
		    *torp_xy = (((u_int) dy >> 8) & 255);
		    shift++;
		    if ( shift == 8){
			shift = 0;	     	
			torp_xy++;
			*torp_xy = 0;
		    }
		}
	    } else {	/* Someone else's torp... */
		if (torp->t_y > me->p_y + SCALE*WINSIDE/2 ||
		    torp->t_x > me->p_x + SCALE*WINSIDE/2 ||
		    torp->t_x < me->p_x - SCALE*WINSIDE/2 ||
		    torp->t_y < me->p_y - SCALE*WINSIDE/2 ||
		    torp->t_status==TFREE) {
		    if (torp->t_status==TFREE && tpi->status==TEXPLODE) {
			tpi->status=TFREE;
			continue;
		    }
		    if (tpi->status!=TFREE) {
			tpi->status=TFREE;
			mustsend = 1;
		    }
		} else {	/* in view */
		    if (torp->t_status!=tpi->status ||
			(torp->t_war & me->p_team)!=tpi->war) {
			if ( (torp->t_war & me->p_team) == tpi->war){
			    /* Don't send TMOVE/TFREE . It's encoded
			       in the SP_S_TORP bitset. */
			    switch(torp->t_status){
			    case TFREE:
				/* We continue , because server sends
				   obsolete data otherwise */
				tpi->status=torp->t_status;
				mustsend = 1;
				/* Don't forget... */
				tp->x=htonl(torp->t_x);
				tp->y=htonl(torp->t_y);
				tp->dir=torp->t_dir;
				continue;
			    case TMOVE:
				tpi->status=torp->t_status;
				break;				
			    default:
				tpi->status=torp->t_status; 
				infobitset = infobitset | ( 01  << i);   /* set bit for torp */
				*tinfo++ = ((u_char)tpi->war & 0x0f)|((u_char) tpi->status << 4);
				break;
			    }
			}
			else {
			    /* Let the client fade away the explosion on its own */
			    tpi->war=torp->t_war & me->p_team;
			    tpi->status=torp->t_status;
			    infobitset = infobitset | ( 01  << i);   /* set bit for torp */
			    *tinfo++ = ((u_char)tpi->war & 0x0f)|((u_char) tpi->status << 4);
			}
		    }
		    if (tp->x!=htonl(torp->t_x) ||
			tp->y!=htonl(torp->t_y) ||
			(torp->t_gspeed == 0 && torp->t_status != TFREE)) {/*Hadley*/
			tp->x=htonl(torp->t_x);
			tp->y=htonl(torp->t_y);
			tp->dir=torp->t_dir;
			/* Now the fun begins .. */
			torpbitset = torpbitset | ( 01  << i);   /* set bit for torp */
			numtorps++;
			/* Now the Coordi. */
			/*
			  dx = torp->t_x - me->p_x;
			  dx = dx / SCALE + WINSIDE / 2;
			  dy = torp->t_y - me->p_y;
			  dy = dy / SCALE + WINSIDE / 2;
			  */
			/* NEW: fix precision loss -tsh */
			dx = torp->t_x/SCALE - me->p_x/SCALE + WINSIDE/2;
			dy = torp->t_y/SCALE - me->p_y/SCALE + WINSIDE/2;

			dx <<= shift;
			*torp_xy++  |= (dx & 255);
			*torp_xy = (((u_int) dx >> 8) & 255);
			shift++;
			dy <<= shift;
			*torp_xy++  |= (dy & 255);
			*torp_xy = (((u_int) dy >> 8) & 255);
			shift++;
			if ( shift == 8){ 
			    shift = 0;
			    torp_xy++;
			    *torp_xy = 0;
			}	     		     	     
		    } /* second if */
		} 
	    } 
	}	/* 2. for */

/* Is there something to send ? */
	if ( infobitset != 0) {
	    int size = tinfo -clientVTorpsInfo;
	    u_char *dest = &clientVTorps[4+ vtdata[numtorps]];
	    u_char *source  =  &clientVTorpsInfo[0];

	    /* I must copy the Torpinfo behind the Torpdata */
	    while ( --size >= 0) { /* size doubly used */
		*dest++ = *source++;
	    }
	    size = 4 +  vtdata[numtorps]+ (tinfo -clientVTorpsInfo);
	    if((size % 4) != 0)
		size += (4 - (size % 4));
		
	    sizes[SP_S_TORP_INFO] = size;
	    clientVTorps[0] = SP_S_TORP_INFO; /* The type */
	    clientVTorps[1] = (u_char) torpbitset;
	    clientVTorps[2] = (u_char) j; /* which torps */
	    clientVTorps[3] = (u_char) infobitset;
	    sendClientPacket((struct torp_spacket *) &clientVTorps[0]);
	    mustsend = 0;
	}	
	else if ( torpbitset != 0){ /* NO TorpInfo */
	    mustsend = 0; 
	    if ( numtorps == 8) { /* we do not need bitset */
		clientVTorps[2] = SP_S_8_TORP; /* The type */
		clientVTorps[3] = (u_char) j; /* which torps */
		sendClientPacket((struct torp_spacket *) &clientVTorps[2]);
	    }
	    else {
		sizes[SP_S_TORP] = vtsize[numtorps];
		clientVTorps[1] = SP_S_TORP; /* The type */
		clientVTorps[2] = (u_char) torpbitset;
		clientVTorps[3] = (u_char) j; /* which torps */
		sendClientPacket((struct torp_spacket *) &clientVTorps[1]);
	    }
	}
	else if (mustsend != 0){ /* TFREE's */
	    mustsend = 0; 
	    sizes[SP_S_TORP] = 4;
	    clientVTorps[1] = SP_S_TORP; /* The type */
	    clientVTorps[2] = (u_char)torpbitset; 
	    clientVTorps[3] = (u_char) j; /* which torps */
	    sendClientPacket((struct torp_spacket *) &clientVTorps[1]);	
	}
    } /* auesseres for */
} /* Function */


void updatePlayerStats(void)
{
    int i;
    struct stats_spacket   *stats;
    struct stats_s_spacket *s_stats = &singleStats;
    struct player          *pl;
    static int		   lastpno = -1;

    /* Variables for stats */
    int    kills, losses, armsbomb, planets;
    int    tkills, tlosses, tarmsbomb, tplanets;
    int    sbkills, sblosses, sbticks, tticks;
    double maxkills, sbmaxkills;
    int    my_tkills, my_tlosses, my_tarmsbomb, my_tplanets, my_tticks;

    /* Wait at least 1 second before sending another stats packet */
    if(repCount % efticks(5) != 0) return;

    /* Look for the next non-empty non-observer slot */
    for(i=1;i<=MAXPLAYER;i++) {
	pl = &players[(lastpno+i)%MAXPLAYER];
	if(pl->p_status!=PFREE && pl->p_status!=POBSERV) break;
    }
    /* No one playing...? */
    if(i>MAXPLAYER) return;

    lastpno = pl->p_no;
    stats = &clientStats[pl->p_no];

    /* Put the stats into local variables, one version for LTD
       and one version for non-LTD */
#ifdef LTD_STATS
    tkills     = ltd_kills(pl, LTD_TOTAL);
    tlosses    = ltd_deaths(pl, LTD_TOTAL);
    tarmsbomb  = ltd_armies_bombed(pl, LTD_TOTAL);
    tplanets   = ltd_planets_taken(pl, LTD_TOTAL);
    kills      = 0;
    losses     = 0;
    armsbomb   = 0;
    planets    = 0;
    sbkills    = ltd_kills(pl, LTD_SB);
    sblosses   = ltd_deaths(pl, LTD_SB);
    sbticks    = ltd_ticks(pl, LTD_SB);
    tticks     = ltd_ticks(pl, LTD_TOTAL);
    maxkills   = ltd_kills_max(pl, LTD_TOTAL);
    sbmaxkills = ltd_kills_max(pl, LTD_SB);

    my_tkills    = ltd_kills(me, LTD_TOTAL);
    my_tlosses   = ltd_deaths(me, LTD_TOTAL);
    my_tarmsbomb = ltd_armies_bombed(me, LTD_TOTAL);
    my_tplanets  = ltd_planets_taken(me, LTD_TOTAL);
    my_tticks    = ltd_ticks(me, LTD_TOTAL);
#else  /* not LTD_STATS */
    tkills     = pl->p_stats.st_tkills;
    tlosses    = pl->p_stats.st_tlosses;
    tarmsbomb  = pl->p_stats.st_tarmsbomb;
    tplanets   = pl->p_stats.st_tplanets;
    kills      = pl->p_stats.st_kills;
    losses     = pl->p_stats.st_losses;
    armsbomb   = pl->p_stats.st_armsbomb;
    planets    = pl->p_stats.st_planets;
    sbkills    = pl->p_stats.st_sbkills;
    sblosses   = pl->p_stats.st_sblosses;
    sbticks    = pl->p_stats.st_sbticks;
    tticks     = pl->p_stats.st_tticks;
    maxkills   = pl->p_stats.st_maxkills;
    sbmaxkills = pl->p_stats.st_sbmaxkills;

    my_tkills     = me->p_stats.st_tkills;
    my_tlosses    = me->p_stats.st_tlosses;
    my_tarmsbomb  = me->p_stats.st_tarmsbomb;
    my_tplanets   = me->p_stats.st_tplanets;
    my_tticks     = me->p_stats.st_tticks;
#endif /* LTD_STATS */

    /* Taken from inl source, socket.c.  Hack for INL robot (when stats are
       reset).  Stick it in here for lack of a better place.  Needed because
       these are ntserv process variables, so robot can't touch them. */
    if (startTkills > my_tkills   || startTlosses > my_tlosses ||
        startTarms > my_tarmsbomb || startTplanets > my_tplanets ||
        startTticks > my_tticks)
    {
        startTkills = my_tkills; startTlosses = my_tlosses;
        startTarms = my_tarmsbomb; startTplanets = my_tplanets;
        startTticks = my_tticks;
    }

    /*
     * Send stat packets once per five updates. But, only send one.  We
     * will cycle through them all eventually.
     */
    if (stats->tkills!=htonl(tkills)     || stats->tlosses!=htonl(tlosses) ||
	stats->kills!=htonl(kills)       || stats->losses!=htonl(losses) ||
	stats->sbkills!=htonl(sbkills)   || stats->sblosses!=htonl(sblosses) ||
	stats->tplanets!=htonl(tplanets) || stats->tarmies!=htonl(tarmsbomb) ||
        stats->planets!=htonl(planets)   || stats->armies!=htonl(armsbomb) || 
	stats->tticks!=htonl(tticks) ) {
	    
	stats->type = SP_STATS;
	stats->pnum = pl->p_no;
	stats->tkills = htonl(tkills);
	stats->tlosses = htonl(tlosses);
	stats->kills = htonl(kills);
	stats->losses = htonl(losses);
	stats->tticks = htonl(tticks);
	stats->tplanets = htonl(tplanets);
	stats->tarmies = htonl(tarmsbomb);
	stats->sbkills = htonl(sbkills);
	stats->sblosses = htonl(sblosses);
	stats->armies = htonl(armsbomb);
	stats->planets = htonl(planets);
	if ((pl->p_ship.s_type == STARBASE) && (SBhours)) {
	    stats->maxkills=htonl((int) (sbticks));
	} else {
	    stats->maxkills=htonl((int) (maxkills * 100));
	}
	stats->sbmaxkills = htonl((int) (sbmaxkills * 100));

	/* Should we send the short packets version instead? */
	if (send_short > 1 &&
	    tkills < 0xffff   && tlosses < 0xffff  && 
	    kills < 0xffff    && losses < 0xffff   &&
	    sbkills < 0xffff  && sblosses < 0xffff &&
	    armsbomb < 0xffff && planets < 0xffff  && tplanets < 0xffff ) {

	    s_stats->type=SP_S_STATS;
	    s_stats->pnum=pl->p_no;
	    s_stats->tkills = htons(tkills);
	    s_stats->tlosses = htons(tlosses);
	    s_stats->kills = htons(kills);
	    s_stats->losses = htons(losses);
	    s_stats->armies = htons(armsbomb);
	    s_stats->planets = htons(planets);
	    s_stats->tplanets = htons(tplanets);
	    s_stats->sbkills = htons(sbkills);
	    s_stats->sblosses = htons(sblosses);
	    s_stats->tticks = htonl(tticks);
	    s_stats->tarmies = htonl(tarmsbomb);
	    s_stats->maxkills = stats->maxkills;
	    s_stats->sbmaxkills = stats->sbmaxkills;
	    sendClientPacket(s_stats);
	} else
	    sendClientPacket((CVOID) stats);
    }
}


/* Called by initClientData - This routine initialized the local packet
   information to an unobtainable state. (Which then forces updates to
   occur) */
void initSPackets(void)
{
    int i;

    for (i=0; i<MAXPLAYER; i++) {
	clientHostile[i].hostile= -1;
	clientStats[i].losses= -1;
	clientLogin[i].rank= -1;
	clientPlayersInfo[i].shiptype= -1;
	clientPStatus[i].status= -1;
	clientPlayers[i].x= htonl(-1);
	clientPhasers[i].status= -1;
	client_s_Phasers[i].status = -1;
	clientKills[i].kills= htonl(-1);
	clientFlags[i].flags= htonl(-1);
	mustUpdate[i]=0;
    }
    for (i=0; i<MAXPLAYER*MAXTORP; i++) {
	clientTorpsInfo[i].status= -1;
	clientTorps[i].x= -1;
	clientTorpsInfo[i].war = 0;
    }
    for (i=0; i<MAXPLAYER*MAXPLASMA; i++) {
	clientPlasmasInfo[i].status= -1;
	clientPlasmas[i].x= -1;
    }
    for (i=0; i<MAXPLANETS; i++) {
	clientPlanets[i].armies= htonl(-1);
	clientPlanetLocs[i].x= htonl(-1);
    }
    msgCurrent=(mctl->mc_current+1) % MAXMESSAGE;
    clientSelf.pnum= -1;

    clientSelfShip.damage = -1;
    clientSelfShort.pnum = -1;
    clientGeneric32.type = 0;
    if (sizeof(struct generic_32_spacket_a) != GENERIC_32_LENGTH) {
        fprintf(stderr, "SP_GENERIC_32 size a wrong at %u bytes\n",
                sizeof(struct generic_32_spacket_a));
    }
    if (sizeof(struct generic_32_spacket_b) != GENERIC_32_LENGTH) {
        fprintf(stderr, "SP_GENERIC_32 size b wrong at %u bytes\n",
                sizeof(struct generic_32_spacket_b));
    }
}

/* Routine called by forceUpdate to clear local packet info, and force
   an update */
void
clearSPackets(int update_all, int update_sall)
{
    int i;


/* WE must do this or change the torp implementation ( It is now changed )*/
/* But it should be no problem because normally NO packet is send */
    if ( send_short) {
	for (i=0; i<MAXPLAYER*MAXTORP; i++) {
	    clientTorpsInfo[i].status= TFREE; /* That's the trick */
	}
	/* And again */
	for (i=0; i<MAXPLAYER*MAXPLASMA; i++)
	    clientPlasmasInfo[i].status= TFREE;
	for (i=0; i<MAXPLAYER; i++)
	    clientPhasers[i].status = PHFREE;
    }

/*    clientDead=0;*/
    for (i=0; i<MAXPLAYER; i++) {

	/* Do not update free Slots when a small, or medium
	   update is requested */
	if((update_all || update_sall)
	   && players[i].p_status == PFREE) continue;

	if (!update_sall) {

	    if(!update_all)
		/* Update only with a large request */
		clientStats[i].losses= -1;

	    /* Update these with large, or medium request */
	    clientHostile[i].hostile= -1;
	    /*	clientLogin[i].rank= -1;		(critical) */
	    /*	clientPlayersInfo[i].shiptype= -1;	(critical) */
	    /*	clientPStatus[i].status= -1;		(critical) */
	    clientPlayers[i].x= htonl(-1);		/* (non-critical, but nice) */
	    if(!send_short) clientPhasers[i].status= -1;
	    clientFlags[i].flags= htonl(-1);
	}

	/* Update these with large, medium, or small request */
	clientKills[i].kills= htonl(-1);	/* (non-critical, but nice) */
	mustUpdate[i]=0;
    }

    if ( !send_short ){
	for (i=0; i<MAXPLAYER*MAXTORP; i++) {
	    clientTorpsInfo[i].status= -1;
	    clientTorps[i].x= -1;		/*	(non-critical) */
	}
	for (i=0; i<MAXPLAYER*MAXPLASMA; i++) {
	    clientPlasmasInfo[i].status= -1;
/*	clientPlasmas[i].x= -1;			(non-critical) */
	}
    }
    for (i=0; i<MAXPLANETS; i++) {
	clientPlanets[i].info |= me->p_team;
	clientPlanets[i].armies= htonl(-1);
/*	clientPlanetLocs[i].x= htonl(-1);	(critical) */
    }
/*    msgCurrent=(mctl->mc_current+1) % MAXMESSAGE;*/
    clientSelf.pnum= -1;
    clientSelfShort.pnum = -1;
    clientSelfShip.damage = -1;
}

void
sendFeature(struct feature_spacket *packet)
{
    packet->type = SP_FEATURE;
    packet->value = htonl(packet->value);
    sendClientPacket((CVOID) packet);
}

static void
send_server_feature(char *name, int value)
{
    struct feature_spacket fp;
    memset(&fp, 0, sizeof(struct feature_spacket));
    fp.type = SP_FEATURE;
    fp.feature_type = 'S';
    fp.value = htonl(value);
    strcpy(fp.name, name);
    sendClientPacket(&fp);
}

void
sendFeatureFps()
{
    send_server_feature("FPS", fps);
}

void
sendFeatureUps()
{
    send_server_feature("UPS", me == NULL ? defups : me->p_ups);
}

void
sendLameRefit()
{
    if (lame_refit != 1)
        send_server_feature("LAME_REFIT", lame_refit);
}

void
sendLameBaseRefit()
{
    if (lame_base_refit != 1)
        send_server_feature("LAME_BASE_REFIT", lame_base_refit);
}

/*
 * If we're in UDP mode, add a sequence number to the transmission buffer.
 * Returns the #of bytes inserted.
 *
 * This will add a sequence # to transmissions on either channel.  However,
 * the current implementation doesn't put sequences on TCP transmissions
 * because mixed TCP packets and UDP packets rarely arrive in the order
 * in which they were sent.
 */
int
addSequence(char *outbuf, LONG *seq_no)
{
    struct sequence_spacket *ssp;

    packets_sent++;
    ssp = (struct sequence_spacket *) outbuf;
    ssp->type = SP_SEQUENCE;
    ssp->sequence = htons((u_short) *seq_no);
    (*seq_no)++;

    return (sizeof(struct sequence_spacket));
}

/* Add some of the player flags into the sequence packet.  This isn't done
   when the sequence packet is made, because that is at the beginning of the
   update.  The flags sent should be from the end of the update, nominally
   100ms later. */
void
addSequenceFlags(void *buf)
{
    struct sequence_spacket *ssp = (struct sequence_spacket *) buf;
    u_int f=0;

    /* Doesn't look like a sequence packet */
    if(ssp->type != SP_SEQUENCE) return;

    /* In cambot mode, we are dumping packets about all players, not 
       just one. So sending the flags for "me" makes no sense.  */
    if(f_many_self) return;

    if (F_19flags) {
	/* Send the most useful flags that aren't packed into the
	   SelfShip packet. */
	ssp->flag8 = (me->p_flags&PFSHIELD    ? 0x01 : 0) |
		     (me->p_flags&PFREPAIR    ? 0x02 : 0) |
		     (me->p_flags&PFBOMB      ? 0x04 : 0) |
		     (me->p_flags&PFCLOAK     ? 0x08 : 0) |
		     (me->p_flags&PFWEP       ? 0x10 : 0) |
		     (me->p_flags&PFENG       ? 0x20 : 0) |
		     (me->p_flags&PFREFITTING ? 0x40 : 0) |
		     (me->p_flags&PFDOCKOK    ? 0x80 : 0);
	f = (PFSHIELD|PFREPAIR|PFBOMB|PFCLOAK|
	     PFWEP|PFENG|PFREFITTING|PFDOCKOK);
    } else {
	/* S_P2 mode, send flags 8-15.  Flags 0-7 are in the SelfShip
	   packet. */
	ssp->flag8 = (((unsigned int)me->p_flags >> 8) & 0xff); /* S_P2 */
	if(send_short > 1 ) {
	    /* In S_P2 mode, consider the flags sent to the client */
	    f = 0xff00;
	} else {
	    /* Non-SP2, probably the client doesn't understand these flags 
	       but maybe it does.  Sent them, but consider them not sent. */
	    f = 0;
	}
    }
    /* Consider the flags sent in this packet as sent, and do not resend
       with a SelfShort or Self packet */
    clientSelfShort.flags = (clientSelfShort.flags & htonl(~f)) |
			    htonl(me->p_flags & f);
}

void
sendQueuePacket(short pos)
{
    struct queue_spacket qPacket;

    qPacket.type=SP_QUEUE;
    qPacket.pos=htons(pos);
    sendClientPacket((CVOID) &qPacket);
    flushSockBuf();
}

void
sendPickokPacket(int state)
{
    struct pickok_spacket pickPack;

    pickPack.type=SP_PICKOK;
    pickPack.state=state;
    sendClientPacket((CVOID) &pickPack);
}

void 
sendClientLogin(struct stats *stats)
{
    struct login_spacket logPacket;

    logPacket.type=SP_LOGIN;
    if (stats==NULL) {
	logPacket.accept=0;
    } else {
	logPacket.accept=1;
	logPacket.flags=htonl(stats->st_flags);
	memcpy(logPacket.keymap, stats->st_keymap, 96);
    }
    sendClientPacket((CVOID) &logPacket);
}

void
sendMotdLine(char *line)
{
    struct motd_spacket motdPacket;
    int len;

/* Well it seems that we can really only send 79 characters for the
   motd. Seems kinda hosed...

   Added wraparound capability (mostly for the compiler options)  DRG Jun 93
   */
    if ((len = strlen(line)) < (MSG_LEN - 1)) { /* no need to wrap around */
	memset(&motdPacket, 0, sizeof(struct motd_spacket));
	motdPacket.type = SP_MOTD;
	strncpy(motdPacket.line, line, MSG_LEN);
	motdPacket.line[MSG_LEN-1]='\0';
	sendClientPacket((CVOID) &motdPacket);
    }
    else {
	char dmy[MSG_LEN];
	char *ptr1,*ptr2;
	int i,j;
	dmy[MSG_LEN-1] ='\0';
	for (i=0;i<(int)(((float)(len)/(float)(MSG_LEN-1.0))+0.99);i++) {
	    ptr1 = dmy; ptr2 = &(line[i*(MSG_LEN-1)]);
	    for (j=0;j<MSG_LEN-1;j++) *ptr1++ = *ptr2++;
	    memset(&motdPacket, 0, sizeof(struct motd_spacket));
	    motdPacket.type = SP_MOTD;
	    strncpy(motdPacket.line, dmy, MSG_LEN);
	    motdPacket.line[MSG_LEN-1]='\0';
	    sendClientPacket((CVOID) &motdPacket);
	}
    }

}

void
sendMaskPacket(int mask)
{
    struct mask_spacket maskPacket;

    maskPacket.type=SP_MASK;
    maskPacket.mask=mask;
    sendClientPacket((CVOID) &maskPacket);
}

static struct generic_32_spacket_a *
sendGeneric32PacketA(struct player *pl, struct generic_32_spacket_a *ga)
{
    ga->type = SP_GENERIC_32;
    ga->version = 'a';
    /*! we did not use network byte order for these two fields */
    ga->repair_time = pl->p_repair_time;
    ga->pl_orbit = pl->p_flags & PFORBIT ? pl->p_planet : -1;
    return ga;
}

static struct generic_32_spacket_b *
sendGeneric32PacketB(struct player *pl, struct generic_32_spacket_b *gb)
{
    int v, t;

    gb->type = SP_GENERIC_32;
    gb->version = 'b';
    gb->repair_time = htons(pl->p_repair_time);
    gb->pl_orbit = pl->p_flags & PFORBIT ? pl->p_planet : -1;

    v = (status->gameup & ~GU_UNSAFE) | (is_idle(pl) ? 0 : GU_UNSAFE);
    gb->gameup = htons(v & 0xffff);

    gb->tournament_teams = ((context->quorum[1] << 4) & 0xf0) |
                            (context->quorum[0] & 0xf);

    v = (context->frame - context->frame_tourn_start) / fps;
    s2du(v, &gb->tournament_age, &gb->tournament_age_units);

    v = context->inl_remaining / fps;
    s2du(v, &gb->tournament_remain, &gb->tournament_remain_units);

    v = teams[pl->p_team].te_turns;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    gb->starbase_remain = v;

    for (t=0;((t<=MAXTEAM)&&(teams[t].te_surrender==0));t++);
    if (t > MAXTEAM) {
        gb->team_remain = 0; /* no one is considering surrender now */
    } else {
        v = teams[t].te_surrender * 60 +
            ((context->frame - teams[t].te_surrender_frame) / fps);
        gb->team_remain = (v > 255) ? 255 : v;
    }

    return gb;
}

void
sendGeneric32Packet(void)
{
    struct generic_32_spacket g, *gp;
    int len = GENERIC_32_LENGTH;

    if (!F_sp_generic_32) return;
    memset(&g, 0, len);

    gp = NULL;
    if (A_sp_generic_32 == GENERIC_32_VERSION_A ||
        A_sp_generic_32 == 0)
        gp = (struct generic_32_spacket *)
                sendGeneric32PacketA(my(), (struct generic_32_spacket_a *)&g);
    if (A_sp_generic_32 == GENERIC_32_VERSION_B)
        gp = (struct generic_32_spacket *)
                sendGeneric32PacketB(my(), (struct generic_32_spacket_b *)&g);
    if (gp == NULL) return;

    if (memcmp(&clientGeneric32, gp, len) != 0) {
        memcpy(&clientGeneric32, gp, len);
        sendClientPacket(gp);
    }
}

void
sendRankPackets()
{
    static int sent = 0;
    struct rank_spacket rp;
    int i;

    if (!F_sp_rank || sent)
        return;

    memset(&rp, 0, sizeof(struct rank_spacket));
    for (i = 0; i < NUMRANKS; i++) {
        rp.type = SP_RANK;
        rp.rnum = i;
        rp.rmax = NUMRANKS - 1;
        strncpy(rp.name, ranks[i].name, NAME_LEN);
        strncpy(rp.cname, ranks[i].cname, 8);
        rp.hours = htonl((int) (ranks[i].hours * 100 + 0.5));
        rp.ratings = htonl((int) (ranks[i].ratings * 100 + 0.5));
        if (offense_rank)
            rp.offense = htonl((int) (ranks[i].offense * 100 + 0.5));
        else
            rp.offense = 0;
        sendClientPacket(&rp);
    }
    sent = 1;
}

#ifdef LTD_STATS
void
sendLtdPacket()
{
  struct ltd_spacket lp;
  struct ltd_stats *ltd;

  if (!F_sp_ltd)
    return;

  ltd = ltd_stat_ptr_total(me);

  memset(&lp, 0, sizeof(struct ltd_spacket));
  lp.type = SP_LTD;
  lp.version = LTD_VERSION;

  lp.kt   = htonl(ltd->kills.total);
  lp.kmax = htonl((int)(ltd->kills.max*100.0 + 0.5));
  lp.k1   = htonl(ltd->kills.first);
  lp.k1p  = htonl(ltd->kills.first_potential);
  lp.k1c  = htonl(ltd->kills.first_converted);
  lp.k2   = htonl(ltd->kills.second);
  lp.k2p  = htonl(ltd->kills.second_potential);
  lp.k2c  = htonl(ltd->kills.second_converted);
  lp.kbp  = htonl(ltd->kills.phasered);
  lp.kbt  = htonl(ltd->kills.torped);
  lp.kbs  = htonl(ltd->kills.plasmaed);
  lp.dt   = htonl(ltd->deaths.total);
  lp.dpc  = htonl(ltd->deaths.potential);
  lp.dcc  = htonl(ltd->deaths.converted);
  lp.ddc  = htonl(ltd->deaths.dooshed);
  lp.dbp  = htonl(ltd->deaths.phasered);
  lp.dbt  = htonl(ltd->deaths.torped);
  lp.dbs  = htonl(ltd->deaths.plasmaed);
  lp.acc  = htonl(ltd->deaths.acc);
  lp.ptt  = htonl(ltd->planets.taken);
  lp.pdt  = htonl(ltd->planets.destroyed);
  lp.bpt  = htonl(ltd->bomb.planets);
  lp.bp8  = htonl(ltd->bomb.planets_8);
  lp.bpc  = htonl(ltd->bomb.planets_core);
  lp.bat  = htonl(ltd->bomb.armies);
  lp.ba8  = htonl(ltd->bomb.armies_8);
  lp.bac  = htonl(ltd->bomb.armies_core);
  lp.oat  = htonl(ltd->ogged.armies);
  lp.odc  = htonl(ltd->ogged.dooshed);
  lp.occ  = htonl(ltd->ogged.converted);
  lp.opc  = htonl(ltd->ogged.potential);
  lp.ogc  = htonl(ltd->ogged.bigger_ship);
  lp.oec  = htonl(ltd->ogged.same_ship);
  lp.olc  = htonl(ltd->ogged.smaller_ship);
  lp.osba = htonl(ltd->ogged.sb_armies);
  lp.ofc  = htonl(ltd->ogged.friendly);
  lp.ofa  = htonl(ltd->ogged.friendly_armies);
  lp.at   = htonl(ltd->armies.total);
  lp.aa   = htonl(ltd->armies.attack);
  lp.ar   = htonl(ltd->armies.reinforce);
  lp.af   = htonl(ltd->armies.ferries);
  lp.ak   = htonl(ltd->armies.killed);
  lp.ct   = htonl(ltd->carries.total);
  lp.cp   = htonl(ltd->carries.partial);
  lp.cc   = htonl(ltd->carries.completed);
  lp.ca   = htonl(ltd->carries.attack);
  lp.cr   = htonl(ltd->carries.reinforce);
  lp.cf   = htonl(ltd->carries.ferries);
  lp.tt   = htonl(ltd->ticks.total);
  lp.tyel = htonl(ltd->ticks.yellow);
  lp.tred = htonl(ltd->ticks.red);
  lp.tz0  = htonl(ltd->ticks.zone[0]);
  lp.tz1  = htonl(ltd->ticks.zone[1]);
  lp.tz2  = htonl(ltd->ticks.zone[2]);
  lp.tz3  = htonl(ltd->ticks.zone[3]);
  lp.tz4  = htonl(ltd->ticks.zone[4]);
  lp.tz5  = htonl(ltd->ticks.zone[5]);
  lp.tz6  = htonl(ltd->ticks.zone[6]);
  lp.tz7  = htonl(ltd->ticks.zone[7]);
  lp.tpc  = htonl(ltd->ticks.potential);
  lp.tcc  = htonl(ltd->ticks.carrier);
  lp.tr   = htonl(ltd->ticks.repair);
  lp.dr   = htonl(ltd->damage_repaired);
  lp.wpf  = htonl(ltd->weapons.phaser.fired);
  lp.wph  = htonl(ltd->weapons.phaser.hit);
  lp.wpdi = htonl(ltd->weapons.phaser.damage.inflicted);
  lp.wpdt = htonl(ltd->weapons.phaser.damage.taken);
  lp.wtf  = htonl(ltd->weapons.torps.fired);
  lp.wth  = htonl(ltd->weapons.torps.hit);
  lp.wtd  = htonl(ltd->weapons.torps.detted);
  lp.wts  = htonl(ltd->weapons.torps.selfdetted);
  lp.wtw  = htonl(ltd->weapons.torps.wall);
  lp.wtdi = htonl(ltd->weapons.torps.damage.inflicted);
  lp.wtdt = htonl(ltd->weapons.torps.damage.taken);
  lp.wsf  = htonl(ltd->weapons.plasma.fired);
  lp.wsh  = htonl(ltd->weapons.plasma.hit);
  lp.wsp  = htonl(ltd->weapons.plasma.phasered);
  lp.wsw  = htonl(ltd->weapons.plasma.wall);
  lp.wsdi = htonl(ltd->weapons.plasma.damage.inflicted);
  lp.wsdt = htonl(ltd->weapons.plasma.damage.taken);

  sendClientPacket(&lp);
}
#endif /* LTD_STATS */

/* Hey Emacs! -*- Mode: C; c-file-style: "bsd"; indent-tabs-mode: nil -*- */
