#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "ip.h"
#include "util.h"
#include "proto.h"
#include "slotmaint.h"
#include "shmatomic.h"
#include "test_harness.h"

extern int queues_init(void);
extern int queue_add(int w_queue);
extern int queue_exit(int waitindex);
extern int queue_update(int w_queue);
extern int queues_purge(void);

/* Stubs for queue.c dependencies on ip.c */
int ip_hide(char *addr) { (void) addr; return 0; }
void ip_lookup(char *addr, char *full, char *dns,
               int *xbl, int *sorbs, int *njabl, int len)
{
    (void) addr; (void) full; (void) dns;
    (void) xbl; (void) sorbs; (void) njabl; (void) len;
}
void ip_waitpid(void) {}

/* Track SysV lock acquisitions in-process without needing real IPC semaphores */
static int g_lock_held[MAXLOCKS];
static int g_lock_ops[MAXLOCKS];

void lock_on(int lock)
{
    if (lock >= 0 && lock < MAXLOCKS) {
        g_lock_held[lock]++;
        g_lock_ops[lock]++;
    }
}

void lock_off(int lock)
{
    if (lock >= 0 && lock < MAXLOCKS) {
        g_lock_held[lock]--;
    }
}

static struct player g_players[MAXPLAYER];
static struct planet g_planets[MAXPLANETS];
static struct pqueue g_queues[MAXQUEUE];
static struct queuewait g_waiting[MAXWAITING];

static void setup_shm_tables(void)
{
    int i;
    memset(g_players, 0, sizeof(g_players));
    memset(g_planets, 0, sizeof(g_planets));
    memset(g_queues, 0, sizeof(g_queues));
    memset(g_waiting, 0, sizeof(g_waiting));
    memset(g_lock_held, 0, sizeof(g_lock_held));
    memset(g_lock_ops, 0, sizeof(g_lock_ops));

    players = g_players;
    planets = g_planets;
    queues = g_queues;
    waiting = g_waiting;
    ip = "127.0.0.1";
    host = "localhost";

    for (i = 0; i < MAXPLAYER; i++) {
        g_players[i].p_no = i;
        g_players[i].p_status = PFREE;
        bay_init(&g_players[i]);
    }

    g_queues[QU_PICKUP].q_flags = QU_OPEN;
    g_queues[QU_PICKUP].low_slot = 0;
    g_queues[QU_PICKUP].high_slot = 16;
    g_queues[QU_PICKUP].max_slots = 16;
    g_queues[QU_PICKUP].free_slots = 16;
}

static void test_bay_claim_release_and_ownership_cas(void)
{
    struct player *base, *shipA, *shipB;

    setup_shm_tables();
    base = &g_players[0];
    shipA = &g_players[1];
    shipB = &g_players[2];

    /* Ship A docks in bay 0 */
    bay_claim(base, shipA, 0);
    shipA->p_flags |= PFDOCK;
    ASSERT_EQ(base->p_bays[0], shipA->p_no);

    /* Base releases all bays; ship A still has stale p_dock_with/p_dock_bay */
    bay_release_all(base);
    ASSERT_EQ(base->p_bays[0], VACANT);
    ASSERT_EQ(shipA->p_flags & PFDOCK, 0);

    /* Ship B docks in bay 0 */
    bay_claim(base, shipB, 0);
    shipB->p_flags |= PFDOCK;
    ASSERT_EQ(base->p_bays[0], shipB->p_no);

    /* Stale bay_release on ship A (even if PFDOCK were raced) must NOT evict ship B */
    shipA->p_flags |= PFDOCK;
    bay_release(shipA);
    ASSERT_EQ(base->p_bays[0], shipB->p_no);
    ASSERT_EQ(shipA->p_flags & PFDOCK, 0);

    /* Ship B releases cleanly */
    bay_release(shipB);
    ASSERT_EQ(base->p_bays[0], VACANT);
    ASSERT_EQ(shipB->p_flags & PFDOCK, 0);
}

static void test_bay_consistency_and_corrupt_indices(void)
{
    struct player *base, *ship;

    setup_shm_tables();
    base = &g_players[0];
    ship = &g_players[3];

    bay_claim(base, ship, 1);
    ship->p_flags |= PFDOCK;

    /* Inject corrupt out-of-range indices and orphaned bay */
    base->p_bays[0] = MAXPLAYER + 5;
    base->p_bays[2] = -99;
    base->p_bays[3] = 4; /* player 4 does not have PFDOCK */

    bay_consistency_check(base);
    ASSERT_EQ(base->p_bays[0], VACANT);
    ASSERT_EQ(base->p_bays[1], ship->p_no);
    ASSERT_EQ(base->p_bays[2], VACANT);
    ASSERT_EQ(base->p_bays[3], VACANT);

    /* bay_release_all with corrupt indices must not crash or access OOB */
    base->p_bays[0] = MAXPLAYER;
    base->p_bays[2] = -2;
    bay_release_all(base);
    ASSERT_EQ(base->p_bays[0], VACANT);
    ASSERT_EQ(base->p_bays[1], VACANT);
    ASSERT_EQ(base->p_bays[2], VACANT);
    ASSERT_EQ(ship->p_flags & PFDOCK, 0);
}

static void test_pickslot_and_freeslot_invariants(void)
{
    int slot;

    setup_shm_tables();

    /* Closed queue returns -2 without locking */
    g_queues[QU_PICKUP].q_flags = 0;
    ASSERT_EQ(pickslot(QU_PICKUP), -2);
    ASSERT_EQ(g_lock_held[LOCK_PICKSLOT], 0);

    /* Open queue allocates slot 0 and decrements free_slots under LOCK_PICKSLOT */
    g_queues[QU_PICKUP].q_flags = QU_OPEN;
    slot = pickslot(QU_PICKUP);
    ASSERT_EQ(slot, 0);
    ASSERT_EQ(g_players[0].p_status, POUTFIT);
    ASSERT_EQ(g_queues[QU_PICKUP].free_slots, 15);
    ASSERT_EQ(slots_free(QU_PICKUP), 15);
    ASSERT_EQ(g_lock_held[LOCK_PICKSLOT], 0);
    ASSERT_TRUE(g_lock_ops[LOCK_PICKSLOT] > 0);

    /* Populate slot fields and free it */
    g_players[0].p_flags = PFSHIELD | PFCLOAK;
    g_players[0].p_process = 12345;
    ASSERT_EQ(freeslot(&g_players[0]), 1);
    ASSERT_EQ(g_players[0].p_status, PFREE);
    ASSERT_EQ(g_players[0].p_flags, 0);
    ASSERT_EQ(g_players[0].p_process, 0);
    ASSERT_EQ(g_queues[QU_PICKUP].free_slots, 16);
    ASSERT_EQ(g_lock_held[LOCK_PICKSLOT], 0);

    /* Double-free must return -2 and leave free_slots unchanged */
    ASSERT_EQ(freeslot(&g_players[0]), -2);
    ASSERT_EQ(g_queues[QU_PICKUP].free_slots, 16);
    ASSERT_EQ(g_lock_held[LOCK_PICKSLOT], 0);

    /* NULL freeslot returns -1 */
    ASSERT_EQ(freeslot(NULL), -1);
}

static void test_p_x_y_set_and_commit_ordering(void)
{
    setup_shm_tables();
    p_x_y_set(&g_players[0], 1234, 5678);
    ASSERT_TRUE(load_acquire(&g_players[0].p_x_y_set) > 0);
    p_x_y_commit(&g_players[0]);
    ASSERT_EQ(load_acquire(&g_players[0].p_x_y_set), 0);
    ASSERT_EQ(g_players[0].p_x_internal, spi(1234));
    ASSERT_EQ(g_players[0].p_y_internal, spi(5678));
}

static void test_shm_bounds_accessors(void)
{
    setup_shm_tables();

    ASSERT_TRUE(player_at(0) == &g_players[0]);
    ASSERT_TRUE(player_at(MAXPLAYER - 1) == &g_players[MAXPLAYER - 1]);
    ASSERT_TRUE(player_at(-1) == NULL);
    ASSERT_TRUE(player_at(MAXPLAYER) == NULL);
    ASSERT_TRUE(player_at(INT_MIN) == NULL);
    ASSERT_TRUE(player_at(INT_MAX) == NULL);

    ASSERT_TRUE(planet_at(0) == &g_planets[0]);
    ASSERT_TRUE(planet_at(MAXPLANETS - 1) == &g_planets[MAXPLANETS - 1]);
    ASSERT_TRUE(planet_at(-1) == NULL);
    ASSERT_TRUE(planet_at(MAXPLANETS) == NULL);
    ASSERT_TRUE(planet_at(INT_MIN) == NULL);
    ASSERT_TRUE(planet_at(INT_MAX) == NULL);

    ASSERT_TRUE(waiter_at(0) == &g_waiting[0]);
    ASSERT_TRUE(waiter_at(MAXWAITING - 1) == &g_waiting[MAXWAITING - 1]);
    ASSERT_TRUE(waiter_at(-1) == NULL);
    ASSERT_TRUE(waiter_at(MAXWAITING) == NULL);
    ASSERT_TRUE(waiter_at(INT_MIN) == NULL);
    ASSERT_TRUE(waiter_at(INT_MAX) == NULL);

    players = NULL;
    planets = NULL;
    waiting = NULL;
    ASSERT_TRUE(player_at(0) == NULL);
    ASSERT_TRUE(planet_at(0) == NULL);
    ASSERT_TRUE(waiter_at(0) == NULL);
    players = g_players;
    planets = g_planets;
    waiting = g_waiting;
}

static void test_queue_lifecycle_and_lock_balance(void)
{
    int w0, w1, w2;

    setup_shm_tables();
    ASSERT_EQ(queues_init(), 1);
    ASSERT_EQ(g_queues[QU_PICKUP].first, -1);
    ASSERT_EQ(g_queues[QU_PICKUP].last, -1);
    ASSERT_EQ(g_queues[QU_PICKUP].count, 0);

    w0 = queue_add(QU_PICKUP);
    w1 = queue_add(QU_PICKUP);
    w2 = queue_add(QU_PICKUP);
    ASSERT_TRUE(w0 >= 0 && w1 >= 0 && w2 >= 0);
    ASSERT_EQ(g_queues[QU_PICKUP].first, w0);
    ASSERT_EQ(g_queues[QU_PICKUP].last, w2);
    ASSERT_EQ(g_queues[QU_PICKUP].count, 3);
    ASSERT_EQ(g_waiting[w0].previous, -1);
    ASSERT_EQ(g_waiting[w0].next, w1);
    ASSERT_EQ(g_waiting[w1].previous, w0);
    ASSERT_EQ(g_waiting[w1].next, w2);
    ASSERT_EQ(g_waiting[w2].previous, w1);
    ASSERT_EQ(g_waiting[w2].next, -1);
    ASSERT_EQ(g_lock_held[LOCK_QUEUE_ADD], 0);

    /* Remove middle element */
    ASSERT_EQ(queue_exit(w1), 1);
    ASSERT_EQ(g_waiting[w1].inuse, 0);
    ASSERT_EQ(g_waiting[w0].next, w2);
    ASSERT_EQ(g_waiting[w2].previous, w0);
    ASSERT_EQ(g_queues[QU_PICKUP].count, 2);
    ASSERT_EQ(g_lock_held[LOCK_QUEUE_ADD], 0);

    /* Double removal & OOB waitindex must be no-ops with balanced locks */
    ASSERT_EQ(queue_exit(w1), 0);
    ASSERT_EQ(queue_exit(-1), 0);
    ASSERT_EQ(queue_exit(MAXWAITING), 0);
    ASSERT_EQ(g_lock_held[LOCK_QUEUE_ADD], 0);

    ASSERT_EQ(queue_update(QU_PICKUP), 1);
    ASSERT_EQ(g_waiting[w0].count, 0);
    ASSERT_EQ(g_waiting[w2].count, 1);
    ASSERT_EQ(g_lock_held[LOCK_QUEUE_ADD], 0);

    /* queues_purge clamps inflated free_slots and keeps locks balanced */
    g_queues[QU_PICKUP].free_slots = g_queues[QU_PICKUP].max_slots + 3;
    ASSERT_EQ(queues_purge(), 1);
    ASSERT_EQ(g_queues[QU_PICKUP].free_slots, g_queues[QU_PICKUP].max_slots);
    ASSERT_EQ(g_lock_held[LOCK_QUEUE_ADD], 0);
    ASSERT_EQ(g_lock_held[LOCK_PICKSLOT], 0);

    /* Drain remaining entries */
    ASSERT_EQ(queue_exit(w0), 1);
    ASSERT_EQ(queue_exit(w2), 1);
    ASSERT_EQ(g_queues[QU_PICKUP].first, -1);
    ASSERT_EQ(g_queues[QU_PICKUP].last, -1);
    ASSERT_EQ(g_queues[QU_PICKUP].count, 0);
    ASSERT_EQ(g_lock_held[LOCK_QUEUE_ADD], 0);

    /* Closed queue refuses queue_add cleanly */
    g_queues[QU_PICKUP].q_flags = 0;
    ASSERT_EQ(queue_add(QU_PICKUP), -1);
    ASSERT_EQ(g_lock_held[LOCK_QUEUE_ADD], 0);
}

int main(void)
{
    RUN_TEST(test_bay_claim_release_and_ownership_cas);
    RUN_TEST(test_bay_consistency_and_corrupt_indices);
    RUN_TEST(test_pickslot_and_freeslot_invariants);
    RUN_TEST(test_p_x_y_set_and_commit_ordering);
    RUN_TEST(test_shm_bounds_accessors);
    RUN_TEST(test_queue_lifecycle_and_lock_balance);
    TEST_SUMMARY("test_bay_slot");
}
