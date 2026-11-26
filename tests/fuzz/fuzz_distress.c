#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

static struct player g_players[MAXPLAYER];
static struct planet g_planets[MAXPLANETS];
static int g_init = 0;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char mdata[MSG_LEN];
    char out[MSG_LEN];
    char macro_buf[MSG_LEN];
    struct distress dist;
    u_char from, to;
    size_t cpy;

    if (!g_init) {
        int i;
        memset(g_players, 0, sizeof(g_players));
        memset(g_planets, 0, sizeof(g_planets));
        for (i = 0; i < MAXPLAYER; i++) {
            g_players[i].p_no = i;
            g_players[i].p_team = FED;
            g_players[i].p_mapchars[0] = 'F';
            g_players[i].p_mapchars[1] = '0';
            g_players[i].p_ship.s_type = CRUISER;
            strcpy(g_players[i].p_name, "FuzzPlayer");
        }
        for (i = 0; i < MAXPLANETS; i++) {
            g_planets[i].pl_no = i;
            g_planets[i].pl_owner = FED;
            g_planets[i].pl_info = FED;
            g_planets[i].pl_armies = 5;
            strcpy(g_planets[i].pl_name, "Earth");
        }
        players = g_players;
        planets = g_planets;
        me = &g_players[0];
        g_init = 1;
    }

    if (size < 2)
        return 0;

    from = data[0];
    to = data[1];
    memset(mdata, 0, sizeof(mdata));
    cpy = size - 2;
    if (cpy > sizeof(mdata) - 1)
        cpy = sizeof(mdata) - 1;
    memcpy(mdata, data + 2, cpy);

    HandleGenDistr(mdata, from, to, &dist);
    if (dist.distype > 0 && dist.distype < num_distress &&
        distmacro[dist.distype].macro != NULL) {
        makedistress(&dist, out, distmacro[dist.distype].macro);
    }

    /* Also exercise custom macro formatting with NUL-terminated raw input */
    memset(macro_buf, 0, sizeof(macro_buf));
    memcpy(macro_buf, data + 2, cpy);
    macro_buf[sizeof(macro_buf) - 1] = '\0';
    dist.sender = from;
    makedistress(&dist, out, macro_buf);

    return 0;
}
