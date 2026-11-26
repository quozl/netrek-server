#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"

extern int ignored[];
extern int sock;
extern int udpSock;
extern int clientDead;
char *myname = "fuzz";

/* Minimal stubs for symbols referenced by socket.o / genspkt.o / smessage.o */
int do_check_command(struct message *mess) { (void) mess; return 0; }
void encryptReservedPacket(struct reserved_spacket *sp,
                           struct reserved_cpacket *cp,
                           char *server, int pno)
{
    (void) server; (void) pno;
    memset(cp, 0, sizeof(*cp));
    memcpy(cp->data, sp->data, sizeof(cp->data));
}
void db_save(void) {}
void clue_check(void) {}
void exitGame(int why) { (void) why; }
void cloak_on(void) {}
void cloak_off(void) {}
void shield_up(void) {}
void shield_down(void) {}
void repair(void) {}
void orbit(void) {}
void beam_up(void) {}
void beam_down(void) {}
void bomb_planet(void) {}
void set_speed(int s) { (void) s; }
void set_course(u_char d) { (void) d; }
void do_refit(int t) { (void) t; }
void tractor_player(int target) { (void) target; }
void repress_player(int target) { (void) target; }
void lock_planet(int p) { (void) p; }
void lock_player(int p) { (void) p; }
void declare_war(int mask, int refitdelay) { (void) mask; (void) refitdelay; }
void coup(void) {}
void ntorp(u_char d, int attr) { (void) d; (void) attr; }
void nplasmatorp(u_char d, int attr) { (void) d; (void) attr; }
void phaser(u_char d) { (void) d; }
void detothers(void) {}
void detmine(void) {}
void bay_release(struct player *p) { (void) p; }
void bay_release_all(struct player *base) { (void) base; }
int freeslot(struct player *who) { (void) who; return 0; }
void lock_on(int lock) { (void) lock; }
void lock_off(int lock) { (void) lock; }
int handleTranswarp(void) { return 0; }
int practice_robo(void) { return 0; }
void sigpipe_close(void) {}
void sigpipe_suspend(int i) { (void) i; }
int bounceSessionStats(int from) { (void) from; return 0; }
int bounceSBStats(int from) { (void) from; return 0; }
int bouncePingStats(int from) { (void) from; return 0; }
int bounceWhois(int from) { (void) from; return 0; }
int bounceUDPStats(int from) { (void) from; return 0; }

static struct memory g_shm;
static struct stats g_stats;

static int g_tcp_sv[2] = { -1, -1 };
static int g_udp_sv[2] = { -1, -1 };

static void drain_fd(int fd)
{
    char tmp[1024];
    while (read(fd, tmp, sizeof(tmp)) > 0)
        ;
}

static void init_fuzz_env(void)
{
    int i, flags;
    if (g_tcp_sv[0] >= 0)
        return;

    signal(SIGPIPE, SIG_IGN);

    memset(&g_shm, 0, sizeof(g_shm));
    memset(&g_stats, 0, sizeof(g_stats));
    memset(ignored, 0, sizeof(int) * MAXPLAYER);

    context = g_shm.context;
    status = g_shm.status;
    players = g_shm.players;
    torps = g_shm.torps;
    planets = g_shm.planets;
    phasers = g_shm.phasers;
    mctl = g_shm.mctl;
    messages = g_shm.messages;
    uplink = g_shm.uplink;
    oob = g_shm.oob;
    teams = g_shm.teams;
    shipvals = g_shm.shipvals;
    queues = g_shm.queues;
    waiting = g_shm.waiting;
    bans = g_shm.bans;

    getshipdefaults();

    fps = 50;
    distortion = 10;
    minups = 1;
    defups = 10;
    maxups = 50;
    ping_freq = 2;
    ping_iloss_interval = 10;

    for (i = 0; i < MAXPLAYER; i++) {
        players[i].p_no = i;
        players[i].p_status = PALIVE;
        players[i].p_team = FED;
        players[i].p_mapchars[0] = 'F';
        players[i].p_mapchars[1] = '0';
        strcpy(players[i].p_name, "Fuzzer");
        strcpy(players[i].p_login, "fuzz");
        strcpy(players[i].p_ip, "127.0.0.1");
        getship(&players[i].p_ship, CRUISER);
        players[i].p_ups = defups;
        players[i].p_fpu = fps / defups;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, g_tcp_sv) != 0)
        abort();
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, g_udp_sv) != 0)
        abort();

    for (i = 0; i < 2; i++) {
        flags = fcntl(g_tcp_sv[i], F_GETFL, 0);
        fcntl(g_tcp_sv[i], F_SETFL, flags | O_NONBLOCK);
        flags = fcntl(g_udp_sv[i], F_GETFL, 0);
        fcntl(g_udp_sv[i], F_SETFL, flags | O_NONBLOCK);
    }

    for (i = 1; i < 64; i++)
        FD_SET(i, &inputMask);
    F_check_planets = 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t mode;
    const uint8_t *payload;
    size_t plen;

    init_fuzz_env();
    if (size < 2)
        return 0;

    mode = data[0];
    payload = data + 1;
    plen = size - 1;
    if (plen > 4096)
        plen = 4096;

    drain_fd(g_tcp_sv[0]);
    drain_fd(g_tcp_sv[1]);
    drain_fd(g_udp_sv[0]);
    drain_fd(g_udp_sv[1]);

    clientDead = 0;
    me = (mode & 1) ? &players[0] : NULL;
    myship = (mode & 1) ? &players[0].p_ship : NULL;
    mystats = (mode & 1) ? &g_stats : NULL;
    FD_ZERO(&inputMask);
    if (mode & 1) {
        int j;
        for (j = 1; j < 64; j++)
            FD_SET(j, &inputMask);
    }

    if (mode & 2) {
        sock = g_tcp_sv[0];
        udpSock = g_udp_sv[0];
        if (write(g_udp_sv[1], payload, plen) > 0)
            readFromClient();
    } else {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0) {
            int fl = fcntl(sv[0], F_GETFL, 0);
            fcntl(sv[0], F_SETFL, fl | O_NONBLOCK);
            if (plen > 0) {
                ssize_t w = write(sv[1], payload, plen);
                (void) w;
            }
            close(sv[1]); /* EOF prevents 20s wait inside doReadRest() */
            sock = sv[0];
            udpSock = -1;
            readFromClient();
            close(sv[0]);
            sock = g_tcp_sv[0];
        }
    }

    drain_fd(g_tcp_sv[0]);
    drain_fd(g_tcp_sv[1]);
    drain_fd(g_udp_sv[0]);
    drain_fd(g_udp_sv[1]);
    return 0;
}
