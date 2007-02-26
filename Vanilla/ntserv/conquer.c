#include <stdlib.h>
#include "copyright.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "daemon.h"
#include "conquer.h"
#include "util.h"

/* conquest gloating parade */

static int conquer_player;	/* player number that won	*/
static int conquer_planet;	/* planet they won at		*/
static int conquer_timer;	/* countdown timer		*/

#define CONQUER_TIMER_BEGIN	100	/* total parade length in 1/10th
					   seconds */
#define CONQUER_TIMER_DECLOAK	 90	/* threshold to decloak all	*/
#define CONQUER_TIMER_PARADE	 40	/* threshold to complete parade	*/
#define CONQUER_TIMER_GOODBYE	 10	/* threshold to say goodbye	*/

#define CONQUER_RING_RADIUS	  5	/* orbital radius of ring	*/

/* total bandwidth cost is 2000 bytes per second per slot over ten seconds 
   if update rate is 10 updates/sec */

/* borrowed from daemonII.c until we find a better place */
#define TORPFUSE 1
#define PLASMAFUSE 1
#define PLAYERFUSE 1

/* force decloak and visibility of all ships */
static void conquer_decloak()
{
	int h;
	struct player *j;

	for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
		if (j->p_status == PFREE) continue;
		j->p_flags &= ~PFCLOAK;
		j->p_flags |= PFSEEN;
	}
}

/* cancel any inflight plasma, we want the plasma slots */
static void conquer_deplasma()
{
	struct torp *t;
	for (t = firstPlasma; t <= lastPlasma; t++) {
		t->t_status = TFREE;
	}
}

/* count the number of players to be paraded */
static int conquer_count_players()
{
	int n, h;
	struct player *j;
	for (n = 0, h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
		if (j->p_status == PFREE) continue;
		if (j->p_no == conquer_player) continue;
		n++;
	}
	return n;
}

/* calculate parade ring coordinates */
static void conquer_ring_coordinates(struct player *j, int h, int n, int *x, int *y)
{
	j->p_dir = h*256/(n+1);
	j->p_desdir = j->p_dir;
	*x = planets[conquer_planet].pl_x + (ORBDIST * CONQUER_RING_RADIUS)
		* Cos[(u_char) (j->p_dir - (u_char) 64)];
	*y = planets[conquer_planet].pl_y + (ORBDIST * CONQUER_RING_RADIUS)
		* Sin[(u_char) (j->p_dir - (u_char) 64)];
}

/* animate a ring of plasma, in expanding spiral */
static void conquer_plasma_ring()
{
	struct torp *k;
	int np = 0, pn = 0;
	int radius = ((CONQUER_TIMER_BEGIN * fps / 10) - conquer_timer) 
		* CONQUER_RING_RADIUS * ORBDIST / (CONQUER_TIMER_BEGIN * fps / 10);

	for (k = firstPlasma; k <= lastPlasma; k++, k++) {
		np++;
	}

	for (k = firstPlasma; k <= lastPlasma; k++, k++, pn++) {
		k->t_status = TMOVE;
		k->t_type = TPLASMA;
		k->t_attribute = 0;
		k->t_owner = conquer_player;
		k->t_dir = (pn*256/np)+conquer_timer;
		k->t_x = planets[conquer_planet].pl_x + radius
			* Cos[(u_char) (k->t_dir - (u_char) 64)];
		k->t_y = planets[conquer_planet].pl_y + radius
			* Sin[(u_char) (k->t_dir - (u_char) 64)];
		k->t_x_internal = spi(k->t_x);
		k->t_y_internal = spi(k->t_y);
		k->t_turns = k->t_damage = k->t_gspeed = k->t_fuse = 1;
		k->t_war = players[conquer_player].p_war;
		k->t_team = players[conquer_player].p_team;
		k->t_whodet = NODET;
	}
}

/* explode the ring of plasma */
static void conquer_plasma_explode()
{
	struct torp *k;
	int np = 0, pn = 0;
	for (k = firstPlasma; k <= lastPlasma; k++, k++) {
		np++;
	}
	for (k = firstPlasma; k <= lastPlasma; k++, k++, pn++) {
		k->t_status = TEXPLODE;
		k->t_fuse = 10 / TORPFUSE * T_FUSE_SCALE; 
	}
}

/* animate a ring of ships, move them slowly into position */
static void conquer_ships_ring()
{
	int n, h, k;
	struct player *j;

	conquer_decloak();
	n = conquer_count_players();
	for (h = 0, j = &players[0], k = 0; h < MAXPLAYER; h++, j++) {
		int x, y, dx, dy;
		if (j->p_status == PFREE) continue;
		if (j->p_no == conquer_player) continue;
		conquer_ring_coordinates(j, k++, n, &x, &y);
		dx = j->p_x - x;
		dy = j->p_y - y;
		if (abs(dx) < 50) {
			p_x_y_set(j, x, y);
		} else {
			p_x_y_set(j, j->p_x - (dx / fps), j->p_y - (dy / fps));
		}
		p_x_y_commit(j);
		j->p_x = spo(j->p_x_internal);
		j->p_y = spo(j->p_y_internal);
	}
}

/* force the ring of ships into final position */
static void conquer_parade()
{
	int n, h, k, x, y;
	struct player *j;

	n = conquer_count_players();
	for (h = 0, j = &players[0], k = 0; h < MAXPLAYER; h++, j++) {
		if (j->p_status == PFREE) continue;
		if (j->p_no == conquer_player) continue;
		conquer_ring_coordinates(j, k++, n, &x, &y);
		p_x_y_set(j, x, y);
		p_x_y_commit(j);
		j->p_x = spo(j->p_x_internal);
		j->p_y = spo(j->p_y_internal);
		j->p_speed = 0;
	}
}

/* explode the ring of ships */
static void conquer_ships_explode()
{
	int h;
	struct player *j;


	for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
		if (j->p_status == PFREE) continue;
#ifdef NEWBIESERVER
		/* Don't kill newbie robot. */
		if (newbie_mode && j->p_flags & PFROBOT) continue;
#endif
#ifdef PRETSERVER
		/* Don't kill pre-T robot. */
		if (pre_t_mode && j->p_flags & PFROBOT) continue;
#endif
		j->p_status = PEXPLODE;
		j->p_whydead = KWINNER;
		j->p_whodead = conquer_player;
		if (j->p_ship.s_type == STARBASE)
			j->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
		else
			j->p_explode = 10/PLAYERFUSE;
		j->p_ntorp = 0;
		j->p_nplasmatorp = 0;
	}
}

/* reset the galaxy after conquer */
static void conquer_galaxy_reset()
{
	int i;

	doResources();
	for (i = 0; i <= MAXTEAM; i++) {
		teams[i].s_turns = 0;
		teams[i].s_surrender = 0;
	}
}

static void conquer_end()
{
	conquer_ships_explode();
	conquer_galaxy_reset();
}

/* manage the animation sequence */
/* called by daemon once per tick while in a GU_CONQUER pause */
void conquer_update()
{
	conquer_timer--;
	if (conquer_timer == CONQUER_TIMER_DECLOAK * fps / 10) {
		conquer_decloak();
		conquer_deplasma();
	}
	if (conquer_timer < CONQUER_TIMER_DECLOAK * fps / 10) {
		conquer_plasma_ring();
		conquer_ships_ring();
	}
	if (conquer_timer == CONQUER_TIMER_PARADE * fps / 10)
		conquer_parade();
	if (conquer_timer == CONQUER_TIMER_GOODBYE * fps / 10)
		pmessage(0, MALL, "GOD->ALL", "Goodbye.");
	if (conquer_timer > 0) return;
	status->gameup &= ~(GU_PAUSED|GU_CONQUER);
	conquer_plasma_explode();
	conquer_end();
}

/* begin a conquer parade sequence */
void conquer_begin(struct player *winner)
{
#ifdef NOCONQUERPARADE
	conquer_end();
#else
	status->gameup |= GU_CONQUER|GU_PAUSED;
	conquer_player = winner->p_no;
	conquer_planet = winner->p_planet;
	conquer_timer = CONQUER_TIMER_BEGIN * fps / 10;
#endif
}

/*  Hey Emacs!
 * Local Variables:
 * c-file-style:"bsd"
 * End:
 */
