#include "config.h"
#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "test_harness.h"

static struct player g_players[MAXPLAYER];
static struct planet g_planets[MAXPLANETS];

static void setup_world(void)
{
    memset(g_players, 0, sizeof(g_players));
    memset(g_planets, 0, sizeof(g_planets));
    players = g_players;
    planets = g_planets;
    me = &g_players[0];

    strcpy(g_players[0].p_name, "TestPlayer12345");
    g_players[0].p_team = FED;
    g_players[0].p_mapchars[0] = 'F';
    g_players[0].p_mapchars[1] = '0';
    g_players[0].p_ship.s_type = CRUISER;

    strcpy(g_planets[0].pl_name, "Earth");
    g_planets[0].pl_owner = FED;
    g_planets[0].pl_armies = 10;
    g_planets[0].pl_info = FED;
}

static void test_num_distress_bounds(void)
{
    ASSERT_EQ(num_distress, 26);
    ASSERT_TRUE(distmacro[num_distress].macro == NULL);
    ASSERT_TRUE(distmacro[num_distress - 1].macro != NULL);
}

static void test_handle_gen_distr_full_memset_and_clamping(void)
{
    char mdata[MSG_LEN];
    struct distress dist;

    /* Pre-fill with canary bytes to verify sizeof(*dist) memset, not sizeof(dist) */
    memset(&dist, 0xAA, sizeof(dist));
    memset(mdata, 0xFF, sizeof(mdata));

    HandleGenDistr(mdata, 0, FED, &dist);

    /* distype 0x1F (31) >= num_distress (26) must clamp to 0 */
    ASSERT_EQ(dist.distype, 0);
    /* preappend must be NUL-terminated and never read past mdata[MSG_LEN - 1] */
    ASSERT_TRUE(strlen(dist.preappend) < sizeof(dist.preappend));
    ASSERT_EQ(dist.pre_app, 0);
}

static void test_handle_gen_distr_cclist_overflow(void)
{
    char mdata[MSG_LEN];
    struct distress dist;

    memset(mdata, 0, sizeof(mdata));
    /* Fill 16 bytes header + 10 carbon-copy entries (0xc0 | slot) */
    memset(&mdata[10], 0x81, 16);
    memset(&mdata[10 + 16], 0xc1, 10);
    mdata[10 + 16 + 10] = (char) 0x80;

    HandleGenDistr(mdata, 0, FED, &dist);
    /* cclist has 7 bytes; loop must stop at index 6 leaving room for terminator */
    ASSERT_EQ(dist.cclist[5], 1);
    ASSERT_EQ(dist.cclist[6], (unsigned char) 0xc1);
}

static void test_makedistress_oob_indices_and_max_length(void)
{
    struct distress dist;
    char out[MSG_LEN];
    int len;

    setup_world();
    memset(&dist, 0, sizeof(dist));

    /* Invalid sender must return 0 safely */
    dist.sender = MAXPLAYER;
    len = makedistress(&dist, out, distmacro[1].macro);
    ASSERT_EQ(len, 0);
    ASSERT_STREQ(out, "");

    /* Out-of-range player/planet indices + max-length help macro must stay < MSG_LEN */
    dist.sender = 0;
    dist.distype = 25;
    dist.close_pl = 127;
    dist.tclose_pl = 127;
    dist.close_en = 127;
    dist.tclose_en = 127;
    dist.close_j = 127;
    dist.tclose_j = 127;
    dist.close_fr = 127;
    dist.tclose_fr = 127;
    dist.wtmpflag = 1;
    dist.etempflag = 1;
    dist.shld = 100;
    dist.dam = 100;
    dist.fuelp = 100;
    dist.arms = 10;
    g_players[0].p_ship.s_type = STARBASE;

    len = makedistress(&dist, out, distmacro[25].macro);
    ASSERT_TRUE(len > 0 && len < MSG_LEN);
    ASSERT_EQ((int) strlen(out), len);

    /* Invalid sender p_team (ALLTEAM == 15) and invalid s_type must not OOB */
    g_players[0].p_team = ALLTEAM;
    g_players[0].p_ship.s_type = 99;
    len = makedistress(&dist, out, "%T%O%S %z%t %K %U");
    ASSERT_TRUE(len >= 0 && len < MSG_LEN);
}

static void test_makedistress_edge_macros_and_valid_expansion(void)
{
    struct distress dist;
    char out[MSG_LEN];
    int len;

    setup_world();
    memset(&dist, 0, sizeof(dist));
    dist.sender = 0;
    dist.tclose_pl = 0;
    dist.tclose_j = 0;
    dist.arms = 10;

    /* Trailing '%' must not read past NUL */
    len = makedistress(&dist, out, "abc%");
    ASSERT_EQ(len, 3);
    ASSERT_STREQ(out, "abc");

    /* Incomplete conditional must not advance past NUL */
    len = makedistress(&dist, out, "%?abc");
    ASSERT_EQ(len, 1);
    ASSERT_STREQ(out, "1");

    /* NULL me during dist_error must not crash */
    me = NULL;
    len = makedistress(&dist, out, "x%@y");
    ASSERT_STREQ(out, "xy");
    me = &g_players[0];

    /* Valid taking distress expands cleanly */
    len = makedistress(&dist, out, distmacro[1].macro);
    ASSERT_TRUE(len > 0 && len < MSG_LEN);
    ASSERT_TRUE(strstr(out, "Carrying 10 to ear @ 10") != NULL);
}

int main(void)
{
    RUN_TEST(test_num_distress_bounds);
    RUN_TEST(test_handle_gen_distr_full_memset_and_clamping);
    RUN_TEST(test_handle_gen_distr_cclist_overflow);
    RUN_TEST(test_makedistress_oob_indices_and_max_length);
    RUN_TEST(test_makedistress_edge_macros_and_valid_expansion);
    TEST_SUMMARY("test_distress");
}
