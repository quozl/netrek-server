#define ATM 1
#define PING 1
#include <stdlib.h>
#include <setjmp.h>

static jmp_buf g_fuzz_exit_jmp;
static __attribute__((noreturn)) void fuzz_robotd_exit(int code)
{
    (void) code;
    longjmp(g_fuzz_exit_jmp, 1);
}
#define exit(c) fuzz_robotd_exit(c)
#include "../../robotd/socket.c"
#undef exit
#include <stdarg.h>
#include <stdint.h>

/* Stubs for robotd symbols referenced by robotd/socket.c */
void mprintf(char *format, ...) { (void) format; }
void mfprintf(FILE *fo, char *format, ...) { (void) fo; (void) format; }
void warning(char *s, int o) { (void) s; (void) o; }
int mtime(int x) { (void) x; return 0; }
int mod_torp_speed(unsigned char pdir, int pspeed, unsigned char tdir, int tspeed)
{
    (void) pdir; (void) pspeed; (void) tdir; (void) tspeed;
    return 0;
}
int calc_speed(Player *p, struct player *j) { (void) p; (void) j; return 0; }
void init_phasers(Player *p, struct player *j, struct phaser *ph)
{
    (void) p; (void) j; (void) ph;
}
void dmessage(char *message, unsigned char flags, unsigned char from, unsigned char to)
{
    (void) message; (void) flags; (void) from; (void) to;
}
void do_plasmadamage(Player *po, struct player *jo, struct plasmatorp *pt)
{
    (void) po; (void) jo; (void) pt;
}
u_char to_dir(int origin_x, int origin_y, int destination_x, int destination_y)
{
    (void) origin_x; (void) origin_y; (void) destination_x; (void) destination_y;
    return 0;
}
void init_plarray(void) {}
void encryptReservedPacket(struct reserved_spacket *spacket,
                           struct reserved_cpacket *cpacket,
                           char *server, int pno)
{
    (void) server; (void) pno;
    memset(cpacket, 0, sizeof(*cpacket));
    memcpy(cpacket->data, spacket->data, sizeof(cpacket->data));
}
void recv_from_master(struct mastercomm_spacket *p) { (void) p; }

/* Minimal UI/game-loop symbols referenced in socket.c handlers (others come from robotd/data.o) */
State _state;
Player *me_p;
int _tcheck;
void sendServerPingResponse(int number) { (void) number; }
void handlePing(char *buf) { (void) buf; }
void handleScan(char *buf) { (void) buf; }
void r_signal(int sig) { (void) sig; }
int ihypot(double xd1, double yd1) { return (int) sqrt(xd1 * xd1 + yd1 * yd1); }
int nint(double x) { return (int) x; }
void response(char *buf) { (void) buf; }

static int g_init = 0;

static void init_robotd_state(void)
{
    int i;
    if (g_init)
        return;

    players = universe.players;
    torps = universe.torps;
    plasmatorps = universe.plasmatorps;
    status = universe.status;
    planets = universe.planets;
    phasers = universe.phasers;
    messages = universe.messages;
    mctl = universe.mctl;

    memset(&universe, 0, sizeof(universe));
    memset(&_state, 0, sizeof(_state));
    me = &players[0];
    me->p_no = 0;
    me->p_team = FED;
    myship = &me->p_ship;
    mystats = &me->p_stats;
    getship(myship, CRUISER);
    for (i = 0; i < MAXPLAYER; i++) {
        players[i].p_no = i;
        getship(&players[i].p_ship, CRUISER);
    }
    _state.players = calloc(MAXPLAYER, sizeof(Player));
    for (i = 0; i < MAXPLAYER; i++) {
        _state.players[i].p = &players[i];
    }
    me_p = &_state.players[0];
    sock = -1;
    udpSock = -1;
    rsock = -1;
    _master_sock = -1;
    serveraddr = htonl(INADDR_LOOPBACK);
    g_init = 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    size_t offset = 0;
    char pktbuf[256];

    init_robotd_state();
    if (setjmp(g_fuzz_exit_jmp) != 0)
        return 0;

    while (offset < size) {
        int type = data[offset];
        int psize;
        if (type < 1 || type > (int) NUM_PACKETS)
            break;
        psize = handlers[type].size;
        if (psize <= 0 || psize > (int) sizeof(pktbuf))
            break;
        if (offset + (size_t) psize > size)
            break;

        memcpy(pktbuf, data + offset, (size_t) psize);
        if (valid_index(type, pktbuf) && handlers[type].handler != NULL)
            handlers[type].handler(pktbuf);
        offset += (size_t) psize;
    }
    return 0;
}
