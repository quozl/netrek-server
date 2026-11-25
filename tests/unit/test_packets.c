#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "genspkt.h"
#include "test_harness.h"

/* Globals normally defined in data.c / socket.c / main.c */
extern int ignored[];
extern int sock;
extern int udpSock;
extern int clientDead;
char *myname = "test";

static int g_sent_motd_count = 0;

/* Stubs for main.c / command / action symbols referenced by socket.c / genspkt.c */
int do_check_command(struct message *mess)
{
    (void) mess;
    return 0;
}

void encryptReservedPacket(struct reserved_spacket *sp,
                           struct reserved_cpacket *cp,
                           char *server, int pno)
{
    (void) server; (void) pno;
    memset(cp, 0, sizeof(*cp));
    memcpy(cp->data, sp->data, sizeof(cp->data));
}

void sendMotdLine_hook(void) { g_sent_motd_count++; }

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
void plasmatorp(u_char d, int attr) { (void) d; (void) attr; }
void phaser(u_char d) { (void) d; }
void detothers(void) {}
void detmine(void) {}
void bay_release(struct player *me_p) { (void) me_p; }
void bay_release_all(struct player *base) { (void) base; }
int freeslot(struct player *who) { (void) who; return 0; }
void lock_on(int lock) { (void) lock; }
void lock_off(int lock) { (void) lock; }
int handleTranswarp(void) { return 0; }
void nplasmatorp(u_char d, int attr) { (void) d; (void) attr; }
int practice_robo(void) { return 0; }
void sigpipe_close(void) {}
void sigpipe_suspend(int i) { (void) i; }
int bounceSessionStats(int from) { (void) from; return 0; }
int bounceSBStats(int from) { (void) from; return 0; }
int bouncePingStats(int from) { (void) from; return 0; }
int bounceWhois(int from) { (void) from; return 0; }
int bounceUDPStats(int from) { (void) from; return 0; }

static struct player g_players[MAXPLAYER];
static struct planet g_planets[MAXPLANETS];
static struct status g_status;
static struct context g_context;
static struct stats g_stats;
static struct mctl g_mctl;
static struct message g_messages[MAXMESSAGE];

static int g_tcp_sv[2] = { -1, -1 };
static int g_udp_sv[2] = { -1, -1 };

static void drain_fd(int fd)
{
    char tmp[1024];
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    while (read(fd, tmp, sizeof(tmp)) > 0)
        ;
    fcntl(fd, F_SETFL, flags);
}

static void setup_net(void)
{
    memset(g_players, 0, sizeof(g_players));
    memset(g_planets, 0, sizeof(g_planets));
    memset(&g_status, 0, sizeof(g_status));
    memset(&g_context, 0, sizeof(g_context));
    memset(&g_stats, 0, sizeof(g_stats));
    memset(&g_mctl, 0, sizeof(g_mctl));
    memset(g_messages, 0, sizeof(g_messages));
    memset(ignored, 0, sizeof(int) * MAXPLAYER);

    players = g_players;
    planets = g_planets;
    status = &g_status;
    context = &g_context;
    mctl = &g_mctl;
    messages = g_messages;

    if (g_tcp_sv[0] < 0) {
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, g_tcp_sv), 0);
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, g_udp_sv), 0);
    }
    drain_fd(g_tcp_sv[0]);
    drain_fd(g_tcp_sv[1]);
    drain_fd(g_udp_sv[0]);
    drain_fd(g_udp_sv[1]);

    sock = g_tcp_sv[0];
    udpSock = -1;
    clientDead = 0;
    me = NULL;
    mystats = NULL;
    FD_ZERO(&inputMask);
}

static void send_tcp_bytes(const void *buf, size_t len)
{
    ssize_t w = write(g_tcp_sv[1], buf, len);
    (void) w;
    readFromClient();
    drain_fd(g_tcp_sv[1]);
}

static void test_prelogin_null_me_packets(void)
{
    struct updates_cpacket upd;
    struct options_cpacket opt;
    struct resetstats_cpacket rst;
    struct reserved_cpacket rsv;
    struct ping_cpacket png;
    struct planet_cpacket pl;

    setup_net();

    /* CP_UPDATES with usecs == 0 while me == NULL (div-by-zero + NULL deref) */
    memset(&upd, 0, sizeof(upd));
    upd.type = CP_UPDATES;
    upd.usecs = htonl(0);
    send_tcp_bytes(&upd, sizeof(upd));
    ASSERT_EQ(clientDead, 0);

    /* CP_UPDATES with valid usecs while me == NULL */
    upd.usecs = htonl(100000);
    send_tcp_bytes(&upd, sizeof(upd));

    /* CP_OPTIONS while mystats == NULL */
    memset(&opt, 0, sizeof(opt));
    opt.type = CP_OPTIONS;
    send_tcp_bytes(&opt, sizeof(opt));

    /* CP_RESETSTATS while me/mystats == NULL */
    memset(&rst, 0, sizeof(rst));
    rst.type = CP_RESETSTATS;
    rst.verify = 'Y';
    send_tcp_bytes(&rst, sizeof(rst));

    /* CP_RESERVED while me == NULL */
    memset(&rsv, 0, sizeof(rsv));
    rsv.type = CP_RESERVED;
    send_tcp_bytes(&rsv, sizeof(rsv));

    /* CP_PING_RESPONSE while me == NULL */
    ping = 1;
    memset(&png, 0, sizeof(png));
    png.type = CP_PING_RESPONSE;
    png.pingme = 1;
    send_tcp_bytes(&png, sizeof(png));
    ping = 0;

    /* CP_PLANET while me == NULL and out-of-range pnum == MAXPLANETS */
    F_check_planets = 1;
    FD_SET(CP_PLANET, &inputMask);
    memset(&pl, 0, sizeof(pl));
    pl.type = CP_PLANET;
    pl.pnum = MAXPLANETS;
    send_tcp_bytes(&pl, sizeof(pl));

    ASSERT_EQ(clientDead, 0);
}

static void test_loggedin_oob_and_variable_packets(void)
{
    struct planet_cpacket pl;
    struct mesg_cpacket msg;
    struct mesg_s_cpacket smsg;

    setup_net();
    me = &g_players[0];
    mystats = &g_stats;
    me->p_no = 0;
    me->p_team = FED;
    me->p_status = PALIVE;
    me->p_mapchars[0] = 'F';
    me->p_mapchars[1] = '0';
    strcpy(me->p_ip, "127.0.0.1");
    strcpy(me->p_login, "tester");
    strcpy(me->p_name, "Tester");

    /* CP_PLANET pnum == MAXPLANETS off-by-one OOB check */
    F_check_planets = 1;
    FD_SET(CP_PLANET, &inputMask);
    memset(&pl, 0, sizeof(pl));
    pl.type = CP_PLANET;
    pl.pnum = MAXPLANETS;
    send_tcp_bytes(&pl, sizeof(pl));

    /* parseIgnore OOB: indiv == 255 >= MAXPLAYER with ":i" */
    FD_SET(CP_MESSAGE, &inputMask);
    memset(&msg, 0, sizeof(msg));
    msg.type = CP_MESSAGE;
    msg.group = MINDIV;
    msg.indiv = 255;
    strcpy(msg.mesg, ":i");
    send_tcp_bytes(&msg, sizeof(msg));

    /* MINDIV|MCONFIG OOB: indiv == 255 >= MAXPLAYER */
    memset(&msg, 0, sizeof(msg));
    msg.type = CP_MESSAGE;
    msg.group = MINDIV | MCONFIG;
    msg.indiv = 255;
    strcpy(msg.mesg, "cfg");
    send_tcp_bytes(&msg, sizeof(msg));

    /* CP_S_MESSAGE invalid length 0 and 255 must abort read without infinite loop */
    FD_SET(CP_S_MESSAGE, &inputMask);
    memset(&smsg, 0, sizeof(smsg));
    smsg.type = CP_S_MESSAGE;
    smsg.length = 0;
    send_tcp_bytes(&smsg, 4);

    smsg.length = (char) 255;
    send_tcp_bytes(&smsg, 4);

    ASSERT_EQ(clientDead, 0);
}

static void test_truncated_udp_datagram_does_not_block(void)
{
    unsigned char partial[4] = { CP_TORP, 0, 0, 0 }; /* CP_TORP expects 4 bytes or e.g. CP_LOGIN 52 bytes */
    struct login_cpacket trunc_login;

    setup_net();
    udpSock = g_udp_sv[0];
    me = &g_players[0];
    mystats = &g_stats;
    me->p_status = PALIVE;
    FD_SET(CP_LOGIN, &inputMask);

    memset(&trunc_login, 0, sizeof(trunc_login));
    trunc_login.type = CP_LOGIN;
    /* Send only 4 bytes of a 52-byte login_cpacket over UDP; must drop immediately */
    ASSERT_TRUE(write(g_udp_sv[1], &trunc_login, sizeof(partial)) == sizeof(partial));
    readFromClient();
    ASSERT_EQ(clientDead, 0);
}

static void test_ping_loss_extremes(void)
{
    struct ping_cpacket pkt;

    setup_net();
    me = NULL;
    ping = 1;
    packets_sent = 10;
    packets_received = 5;

    memset(&pkt, 0, sizeof(pkt));
    pkt.type = CP_PING_RESPONSE;
    pkt.pingme = 1;
    pkt.number = 0;
    pkt.cp_sent = htonl(0xFFFFFFFFU);
    pkt.cp_recv = htonl(0xFFFFFFFFU);
    pingResponse(&pkt);

    /* With me != NULL, verify multiple responses with small counts don't div-by-zero */
    me = &g_players[0];
    pkt.number = 1;
    pkt.cp_sent = htonl(0);
    pkt.cp_recv = htonl(0);
    pingResponse(&pkt);
    ASSERT_TRUE(me->p_pkls_c_s >= 0 && me->p_pkls_c_s <= 100);
}

static void test_send_motd_line_wrap(void)
{
    char longline[210];

    setup_net();
    memset(longline, 'M', sizeof(longline) - 1);
    longline[sizeof(longline) - 1] = '\0';

    /* Short, boundary, and multi-chunk wrapped MOTD lines under ASan */
    sendMotdLine("Welcome");
    sendMotdLine(longline);
    drain_fd(g_tcp_sv[1]);
    ASSERT_EQ(clientDead, 0);
}

static void test_packet_allowed_in_state(void)
{
    fd_set full_mask;
    struct player p;
    struct stats st;
    int i;

    FD_ZERO(&full_mask);
    for (i = 1; i < 64; i++)
        FD_SET(i, &full_mask);

    /* Pre-login (p == NULL): only handshake/goodbye/feature packets allowed,
       even if all bits of mask are set */
    ASSERT_EQ(packet_allowed_in_state(CP_SOCKET, NULL, NULL, &full_mask), 1);
    ASSERT_EQ(packet_allowed_in_state(CP_BYE, NULL, NULL, &full_mask), 1);
    ASSERT_EQ(packet_allowed_in_state(CP_FEATURE, NULL, NULL, &full_mask), 1);
    ASSERT_EQ(packet_allowed_in_state(CP_SPEED, NULL, NULL, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_DIRECTION, NULL, NULL, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_TORP, NULL, NULL, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_RESETSTATS, NULL, NULL, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_OPTIONS, NULL, NULL, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_PLANET, NULL, NULL, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_PING_RESPONSE, NULL, NULL, &full_mask), 0);

    /* p != NULL, st == NULL: stats-requiring handlers are rejected */
    memset(&p, 0, sizeof(p));
    memset(&st, 0, sizeof(st));
    p.p_status = PALIVE;
    ASSERT_EQ(packet_allowed_in_state(CP_RESETSTATS, &p, NULL, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_OPTIONS, &p, NULL, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_UPDATES, &p, NULL, &full_mask), 1);
    ASSERT_EQ(packet_allowed_in_state(CP_FEATURE, &p, NULL, &full_mask), 1);

    /* PFWAR / PFREFITTING / PFTWARP block gameplay packets but allow PKT_ALWAYS */
    p.p_flags = PFWAR;
    ASSERT_EQ(packet_allowed_in_state(CP_TORP, &p, &st, &full_mask), 0);
    ASSERT_EQ(packet_allowed_in_state(CP_UPDATES, &p, &st, &full_mask), 1);
    ASSERT_EQ(packet_allowed_in_state(CP_PING_RESPONSE, &p, &st, &full_mask), 1);
    ASSERT_EQ(packet_allowed_in_state(CP_RESETSTATS, &p, &st, &full_mask), 1);

    /* Observers respect mask even if PFWAR is set */
    p.p_status = POBSERV;
    ASSERT_EQ(packet_allowed_in_state(CP_TORP, &p, &st, &full_mask), 1);
}

int main(void)
{
    RUN_TEST(test_prelogin_null_me_packets);
    RUN_TEST(test_loggedin_oob_and_variable_packets);
    RUN_TEST(test_truncated_udp_datagram_does_not_block);
    RUN_TEST(test_ping_loss_extremes);
    RUN_TEST(test_send_motd_line_wrap);
    RUN_TEST(test_packet_allowed_in_state);
    TEST_SUMMARY("test_packets");
}
