/*
 * shm-stress-monitor -- watch the netrek shared memory for signs of
 * races between the daemon, ntserv and other processes, while
 * tests/shm-stress drives clients against the server.
 *
 * It polls continuously, so it occupies one CPU.  It also keeps the
 * stress test's ships fuelled, cool, healed and in place, by writing
 * fields that the checks below do not depend on.
 *
 * Checks, each reported as an event:
 *
 *   ntorp-drift      p_ntorp differs from the number of live photon
 *                    torpedoes the player owns, for longer than a
 *                    legitimate update takes (100 ms)
 *   nplasma-drift    the same for p_nplasmatorp and plasma torpedoes
 *   torp-slot        a live torpedo in a player's slots is owned by
 *                    another player, as when ntorp() runs off the end
 *                    of its own slots
 *   torp-uninit      a torpedo was seen moving before ntorp() had set
 *                    its position, so the daemon could move it from
 *                    the last torpedo's position
 *   phaser-stale     ph_target or ph_damage changed after the daemon
 *                    had already applied the hit, so the hit used the
 *                    previous shot's target and damage
 *
 * Ships are recognised by login name: torper, attacker and victim.
 *
 * Build: cc -O2 -I<configured source>/include tests/shm-stress-monitor.c
 * Usage: shm-stress-monitor SECONDS
 */

#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "defs.h"
#include "struct.h"

#define NTORPS   (MAXPLAYER * MAXTORP)
#define NPLASMAS (MAXPLAYER * MAXPLASMA)
#define REPORT_LIMIT 20

static volatile struct memory *m;

enum { E_NTORP, E_NPLASMA, E_SLOT, E_UNINIT, E_PHASER, E_MAX };
static const char *ename[E_MAX] = {
  "ntorp-drift", "nplasma-drift", "torp-slot", "torp-uninit", "phaser-stale"
};
static long events[E_MAX];

/* activity seen, so that zero events can be put in proportion */
static long torps_seen, dets_seen, plasmas_seen, phaser_hits_seen,
  phaser_hits_consumed, polls, stress_queued;
static long queued_by[MAXPLAYER];

static double now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double t0;

static void event(int type, const char *fmt, ...)
  __attribute__((format(printf, 2, 3)));

static void event(int type, const char *fmt, ...)
{
  va_list ap;

  events[type]++;
  if (events[type] > REPORT_LIMIT) return;
  printf("EVENT %.3f %s ", now() - t0, ename[type]);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
  fflush(stdout);
}

static int role(volatile struct player *p, const char *name)
{
  return strncmp((const char *) p->p_login, name, sizeof(p->p_login)) == 0;
}

/* is the distance between two points more than d */
static int farther(long x1, long y1, long x2, long y2, long d)
{
  long dx = x1 - x2, dy = y1 - y2;
  return dx * dx + dy * dy > d * d;
}

/* torpedo slot tracking */
static int t_last[NTORPS + NPLASMAS];
static int t_check[NTORPS];     /* waiting to see the new torp initialised */
static int t_frame[NTORPS];     /* frame in which the new torp was first seen */
static int t_bad[NTORPS];       /* first seen far from its owner */
static int slot_reported[NTORPS];
static double slot_wrong[NTORPS];   /* when a wrong owner was first seen */

static void poll_torps(void)
{
  int s, frame = m->context->frame;

  for (s = 0; s < NTORPS; s++) {
    volatile struct torp *t = &m->torps[s];
    int st = t->t_status;
    int owner = s / MAXTORP;

    /* ntorp() sets t_status before t_owner, so a slot used for the
       first time briefly shows owner 0; a torp in the wrong slot stays
       there for its life */
    if (st != TFREE && t->t_owner != owner) {
      if (slot_wrong[s] == 0) {
        slot_wrong[s] = now();
      } else if (slot_reported[s] != t->t_owner + 1 &&
                 now() - slot_wrong[s] > 0.02) {
        slot_reported[s] = t->t_owner + 1;
        event(E_SLOT, "slot %d of player %d holds a torp of player %d"
              " (status %d)", s, owner, t->t_owner, st);
      }
    } else {
      slot_wrong[s] = 0;
      if (st == TFREE) slot_reported[s] = 0;
    }

    if (t_last[s] == TFREE && st == TMOVE) {
      volatile struct player *p = &m->players[owner];
      torps_seen++;
      t_check[s] = 1;
      t_frame[s] = frame;
      t_bad[s] = farther(t->t_x, t->t_y, p->p_x, p->p_y, 1000);
    } else if (t_check[s]) {
      if (st != TMOVE || frame != t_frame[s]) {
        t_check[s] = 0;
      } else if (t_bad[s]) {
        volatile struct player *p = &m->players[owner];
        if (!farther(t->t_x, t->t_y, p->p_x, p->p_y, 300)) {
          event(E_UNINIT, "torp %d of player %d was moving before its"
                " position was set", s, owner);
          t_check[s] = 0;
        }
      } else {
        t_check[s] = 0;
      }
    }
    if (t_last[s] == TMOVE && st == TOFF) dets_seen++;
    t_last[s] = st;
  }
  for (s = 0; s < NPLASMAS; s++) {
    int st = m->torps[NTORPS + s].t_status;
    if (t_last[NTORPS + s] == TFREE && st == TMOVE) plasmas_seen++;
    t_last[NTORPS + s] = st;
  }
}

/* phaser shot tracking, see phaser() in ntserv/phaser.c and udphaser()
   in ntserv/daemon.c; the daemon applies a PHHIT once, on the first
   update after ph_fuse is set, by decrementing ph_fuse */
static int ph_last_status[MAXPLAYER], ph_last_fuse[MAXPLAYER];
static int ph_consumed[MAXPLAYER], ph_target[MAXPLAYER], ph_damage[MAXPLAYER];

static void poll_phasers(void)
{
  int i;

  for (i = 0; i < MAXPLAYER; i++) {
    volatile struct phaser *ph = &m->phasers[i];
    int st = ph->ph_status;
    int fuse = ph->ph_fuse;
    int full = m->players[i].p_ship.s_phaserfuse;

    if (st != PHFREE &&
        (ph_last_status[i] == PHFREE || fuse > ph_last_fuse[i])) {
      /* a new shot */
      ph_consumed[i] = 0;
      if (st == PHHIT) phaser_hits_seen++;
    }
    if (st == PHHIT && fuse < full) {
      int target = ph->ph_target, damage = ph->ph_damage;
      if (!ph_consumed[i]) {
        ph_consumed[i] = 1;
        ph_target[i] = target;
        ph_damage[i] = damage;
        phaser_hits_consumed++;
      } else if (ph_consumed[i] == 1 &&
                 (target != ph_target[i] || damage != ph_damage[i])) {
        ph_consumed[i] = 2;
        event(E_PHASER, "player %d phaser hit applied as target %d"
              " damage %d, then set to target %d damage %d", i,
              ph_target[i], ph_damage[i], target, damage);
      }
    }
    ph_last_status[i] = st;
    ph_last_fuse[i] = fuse;
  }
}

/* torpedo counts, a mismatch must persist to count */
static int c_value[2][MAXPLAYER], c_reported[2][MAXPLAYER];
static double c_since[2][MAXPLAYER];
static int was_alive[MAXPLAYER], died[MAXPLAYER];
static long deaths;

static void check_count(int kind, int i, int counted, int live, double t)
{
  int mismatch = counted - live;

  if (mismatch != c_value[kind][i]) {
    c_value[kind][i] = mismatch;
    c_since[kind][i] = t;
    c_reported[kind][i] = 0;
    return;
  }
  if (mismatch == 0 || c_reported[kind][i]) return;
  if (t - c_since[kind][i] < 0.1) return;
  c_reported[kind][i] = 1;
  event(kind == 0 ? E_NTORP : E_NPLASMA, "player %d %s %d but owns %d"
        " live", i, kind == 0 ? "p_ntorp" : "p_nplasmatorp", counted, live);
}

static void check_counts(double t)
{
  int live[MAXPLAYER], plive[MAXPLAYER];
  int s, i;

  memset(live, 0, sizeof(live));
  memset(plive, 0, sizeof(plive));
  for (s = 0; s < NTORPS; s++) {
    volatile struct torp *k = &m->torps[s];
    if (k->t_status != TFREE && k->t_owner >= 0 && k->t_owner < MAXPLAYER)
      live[k->t_owner]++;
  }
  for (s = 0; s < NPLASMAS; s++) {
    volatile struct torp *k = &m->torps[NTORPS + s];
    if (k->t_status != TFREE && k->t_owner >= 0 && k->t_owner < MAXPLAYER)
      plive[k->t_owner]++;
  }
  for (i = 0; i < MAXPLAYER; i++) {
    volatile struct player *p = &m->players[i];
    /* a dead ship's torps fly on, and enter() zeroes p_ntorp for the
       next ship, so only check ships that have stayed alive */
    if (p->p_status != PALIVE) {
      if (was_alive[i]) {
        was_alive[i] = 0;
        died[i] = 1;
        if (p->p_status == PEXPLODE || p->p_status == PDEAD) deaths++;
      }
      c_value[0][i] = c_value[1][i] = 0;
      continue;
    }
    was_alive[i] = 1;
    if (died[i]) continue;
    check_count(0, i, p->p_ntorp, live[i], t);
    check_count(1, i, p->p_nplasmatorp, plive[i], t);
  }
}

/* count stress test messages as the daemon puts them in the main
   queue; an entry is read a little after the index moves, as the daemon
   advances mc_current before it copies the message in */
static int msg_last = -1;
static double msg_seen[MAXMESSAGE];

static void poll_messages(double t)
{
  int current = m->mctl->mc_current, i;

  if (msg_last < 0) msg_last = current;
  while (msg_last != current) {
    msg_last = (msg_last + 1) % MAXMESSAGE;
    msg_seen[msg_last] = t;
  }
  for (i = 0; i < MAXMESSAGE; i++) {
    if (msg_seen[i] == 0 || t - msg_seen[i] < 0.002) continue;
    msg_seen[i] = 0;
    if (strstr((const char *) m->messages[i].m_data, "stress ")) {
      int from = m->messages[i].m_from;
      stress_queued++;
      if (from >= 0 && from < MAXPLAYER) queued_by[from]++;
    }
  }
}

/* the phaser arena, away from planets */
static int arena_x, arena_y;

static void choose_arena(void)
{
  long best = -1;
  int x, y, k;

  for (x = 10000; x <= GWIDTH - 10000; x += 2500)
    for (y = 10000; y <= GWIDTH - 10000; y += 2500) {
      long nearest = (long) GWIDTH * GWIDTH * 4;
      for (k = 0; k < MAXPLANETS; k++) {
        long dx = x - m->planets[k].pl_x, dy = y - m->planets[k].pl_y;
        if (dx * dx + dy * dy < nearest) nearest = dx * dx + dy * dy;
      }
      if (nearest > best) {
        best = nearest;
        arena_x = x;
        arena_y = y;
      }
    }
  for (k = 0; (long) k * k < best; k += 10);
  printf("ARENA %d %d, nearest planet about %d\n", arena_x, arena_y, k);
  fflush(stdout);
}

/* victims sit close together, attackers around them within phaser range */
static const int victim_offset[4][2] = {
  { -700, -700 }, { 700, -700 }, { 700, 700 }, { -700, 700 }
};
static const int attacker_offset[4][2] = {
  { 0, -3000 }, { 3000, 0 }, { 0, 3000 }, { -3000, 0 }
};

static void place(volatile struct player *p, int x, int y)
{
  if (!farther(p->p_x, p->p_y, x, y, 300)) return;
  if (p->p_x_y_set) return;
  p->p_x_input = spi(x);
  p->p_y_input = spi(y);
  p->p_x_y_set = 1;
}

static void maintain(void)
{
  int i, attackers = 0, victims = 0;

  for (i = 0; i < MAXPLAYER; i++) {
    volatile struct player *p = &m->players[i];
    int a = role(p, "attacker"), v = role(p, "victim");

    if (p->p_status != PALIVE) continue;
    if (!a && !v && !role(p, "torper")) continue;
    p->p_fuel = p->p_ship.s_maxfuel;
    p->p_wtemp = 0;
    p->p_etemp = 0;
    if (v) {
      p->p_damage = 0;
      p->p_shield = p->p_ship.s_maxshield;
      place(p, arena_x + victim_offset[victims % 4][0],
            arena_y + victim_offset[victims % 4][1]);
      victims++;
    }
    if (a) {
      place(p, arena_x + attacker_offset[attackers % 4][0],
            arena_y + attacker_offset[attackers % 4][1]);
      attackers++;
    }
  }
}

static void report(const char *tag, double t)
{
  int e;

  printf("%s %.0f s: polls %ld torps %ld dets %ld plasmas %ld"
         " phaser-hits %ld consumed %ld deaths %ld messages-queued %ld",
         tag, t - t0, polls, torps_seen, dets_seen, plasmas_seen,
         phaser_hits_seen, phaser_hits_consumed, deaths, stress_queued);
  for (e = 0; e < E_MAX; e++) printf(" %s %ld", ename[e], events[e]);
  printf("\n");
  fflush(stdout);
}

int main(int argc, char **argv)
{
  double duration, end, t, next_count, next_maint, next_report;
  int key = PKEY, shmid = -1;
  char *ev;
  void *addr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s SECONDS\n", argv[0]);
    return 2;
  }
  duration = atof(argv[1]);
  if ((ev = getenv("PKEY")) != NULL && atoi(ev) > 0) key = atoi(ev);

  /* the daemon creates the segment when the first client arrives */
  t0 = now();
  while ((shmid = shmget(key, 0, 0)) < 0) {
    if (now() - t0 > 60) {
      fprintf(stderr, "shm-stress-monitor: no shared memory key %d\n", key);
      return 2;
    }
    usleep(100000);
  }
  addr = shmat(shmid, NULL, 0);
  if (addr == (void *) -1) {
    perror("shm-stress-monitor: shmat");
    return 2;
  }
  m = addr;
  while (!(m->status->gameup & GU_GAMEOK) && now() - t0 < 60)
    usleep(100000);

  choose_arena();
  t0 = now();
  end = t0 + duration;
  next_count = next_maint = t0;
  next_report = t0 + 10;
  for (;;) {
    t = now();
    if (t >= end) break;
    poll_phasers();
    poll_torps();
    polls++;
    if (t >= next_count) {
      check_counts(t);
      poll_messages(t);
      next_count = t + 0.001;
    }
    if (t >= next_maint) {
      maintain();
      next_maint = t + 0.02;
    }
    if (t >= next_report) {
      report("PROGRESS", t);
      next_report = t + 10;
    }
  }
  report("SUMMARY", now());
  for (int i = 0; i < MAXPLAYER; i++)
    if (queued_by[i]) printf("QUEUED %d %ld\n", i, queued_by[i]);
  return 0;
}
