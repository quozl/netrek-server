/* $Id: proto.h,v 1.2 2005/03/21 10:17:16 quozl Exp $
 *
 * Function prototypes for externally accessed functions.
 */

#ifndef __INCLUDED_proto_h__
#define __INCLUDED_proto_h__

/* cluecheck.c */
void clue_check(void);

/* coup.c */
void coup(void);

/* db.c */
int findplayer(char *namePick, struct statentry *player);
void savestats(void);
void changepassword(char *passPick);
void savepass(const struct statentry *);

/* death.c */
void death(void);

/* detonate.c */
void detothers(void);

/* distress.c */
struct distress;

void HandleGenDistr (char *message, u_char from, u_char to,
                     struct distress *dist);
int makedistress (struct distress *dist, char *cry, char *pm);

/* enter.c */
void enter(int tno, int disp, int pno, int s_type, char *name);

/* feature.c */
struct feature_cpacket;
struct feature_spacket;

void readFeatures(void);
void getFeature(struct feature_cpacket *cpack, struct feature_spacket *spack);

/* features.c */
void TellClient(char *typ);

/* findslot.c */
int findslot(int w_queue, char *host);

/* gencmds.c */
char *addr_mess(int who, int type);

/* genspkt.c */
void updateStatus(int force);
void updateSelf(int force);
void updateShips(void);
void updateTorps(void);
void updatePlasmas(void);
void updatePhasers(void);
void updatePlanets(void);
void updateMessages(void);
void SupdateTorps(void);
void updatePlayerStats(void);
void initSPackets(void);
void clearSPackets(int update_all, int update_sall);
void sendFeature(struct feature_spacket *packet);
int addSequence(char *outbuf, LONG *seq_no);
void sendQueuePacket(short pos);
void sendPickokPacket(int state);
void sendClientLogin(struct stats *stats);
void sendMotdLine(char *line);
void sendMaskPacket(int mask);

/* getentry.c */
void getEntry(int *team, int *stype);

/* getname.c */
void getname(void);

/* getpath.c */
void getpath(void);

/* getship.c */
void getshipdefaults(void);
void getship(struct ship *shipp, int s_type);

/* input.c */
void input(void);

/* interface.c */
void set_speed(int speed);
void set_course(u_char dir);
void shield_up(void);
void shield_down(void);
void shield_tog(void);
void bomb_planet(void);
void beam_up(void);
void beam_down(void);
void repair(void);
void repair_off(void);
void repeat_message(void);
void cloak(void);
void cloak_on(void);
void cloak_off(void);
void lock_planet(int planet);
void lock_player(int player);
void tractor_player(int player);
void pressor_player(int player);
void declare_war(int mask);
void do_refit(int type);
int numPlanets(int owner);

/* main.c */
void exitGame(void);
int CheckBypass(char *login, char *host, char *file);
void message_flag(struct message *cur, char *address);

/* ntscmds.c */
int bounceSessionStats(int from);
int bounceSBStats(int from);
#ifdef PING
int bouncePingStats(int from);
#endif
#ifdef RSA
int bounceRSAClientType(int from);
#endif
int bounceWhois(int from);
int check_command(struct message *mess);

/* openmem.c */
int openmem(int trystart);
int setupmem(void);
int removemem(void);

/* orbit.c */
void orbit(void);

/* phaser.c */
void phaser(u_char course);

/* ping.c */
struct ping_cpacket;

void pingResponse(struct ping_cpacket *packet);
void sendClientPing(void);

/* plasma.c */
void nplasmatorp(u_char course, int attributes);

/* queue.c */
int queues_init(void);
int queue_add(int w_queue);
int queue_exit(int waitindex);
int queue_update(int w_queue);
int queues_purge(void);
int queue_setname(int w_queue, char *name);

/* redraw.c */
void intrupt(void);

/* reserved.c */
struct reserved_spacket;
struct reserved_cpacket;

void makeReservedPacket(struct reserved_spacket *packet);
void encryptReservedPacket(struct reserved_spacket *spacket,
                           struct reserved_cpacket *cpacket,
                           char *server, int pno);

/* slotmaint.c */
int freeslot(struct player *who);
int pickslot(int w_queue);

/* smessage.c */
void bounce(int bounceto, const char *, ...);
void pmessage(int recip, int group, char *address, const char *, ...);
void pmessage2(int recip, int group, char *address, u_char from,
               const char *, ...);

/* socket.c */
struct player_spacket;
void setNoDelay(int fd);
int connectToClient(char *machine, int port);
void checkSocket(void);
void initClientData(void);
int isClientDead(void);
void updateClient(void);
void sendClientPacket(void *);
void flushSockBuf(void);
void socketPause(int);
int readFromClient(void);
int checkVersion(void);
void logEntry(void);
int closeUdpConn(void);
void forceShutdown(int s);

/* startrobot.c */
void practice_robo(void);

/* sysdefaults.c */
void readsysdefaults(void);
int update_sys_defaults(void);

/* timecheck.c */
int time_access(void);

/* torp.c */
int torpGetVectorSpeed(u_char pdir, int pspeed, u_char tdir, int speed);
void ntorp(u_char course, int attributes);

/* transwarp.c */
#ifdef SB_TRANSWARP
int handleTranswarp(void);
#endif

/* util.c */
int angdist(u_char x, u_char y);
#ifdef DEFINE_NINT
int nint(double x);
#endif
int realNumShips(int owner);
#ifdef LTD_STATS
void setEnemy(int myteam, struct player *me);
#endif /* LTD_STATS */
int find_slot_by_host(char *host, int j);

/* warning.c */
void new_warning(int index, const char *fmt, ...);
void warning(char *text);
void spwarning(char *text, int index);
void s_warning(char *text, int index);
void swarning(u_char, u_char, u_char);

#endif /* __INCLUDED_proto_h__ */
