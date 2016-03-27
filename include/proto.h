/* $Id: proto.h,v 1.8 2006/05/06 14:02:37 quozl Exp $
 *
 * Function prototypes for externally accessed functions.
 */

#ifndef __INCLUDED_proto_h__
#define __INCLUDED_proto_h__

/* balance.c */
#if defined (TRIPLE_PLANET_MAYHEM)
void do_balance(void);
void do_triple_planet_mayhem(void);
#endif

/* bans.c */
int bans_add_temporary_by_player(int who, char *by);
void bans_age_temporary(int elapsed);
int bans_check_temporary_remaining();
int bans_check_temporary(char *ip);
int bans_check_permanent(char *login, char *host);

/* bay.c */
void bay_consistency_check(struct player *base);
struct player *bay_owner(struct player *me);
void bay_claim(struct player *base, struct player *me, int bay_no);
void bay_release(struct player *me);
void bay_release_all(struct player *base);
void bay_init(struct player *me);
int bay_closest(struct player *base, LONG dx, LONG dy);

/* cluecheck.c */
void clue_check(void);

/* coup.c */
void coup(void);

/* db.c */
int findplayer(char *namePick, struct statentry *player, int exhaustive);
void savestats(void);
int newplayer(struct statentry *player);
char *crypt_player_raw(const char *password, const char *name);
char *crypt_player(const char *password, const char *name);
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
int findslot(int w_queue);

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
void addSequenceFlags(void *buf);
void sendQueuePacket(short pos);
void sendPickokPacket(int state);
void sendClientLogin(struct stats *stats);
void sendMotdLine(char *line);
void sendMaskPacket(int mask);
int sndShipCap(void);

/* getentry.c */
void getEntry(int *team, int *stype);

/* getname.c */
void getname(void);

/* getpath.c */
void getpath(void);
void setpath(void);

/* getship.c */
void getshipdefaults(void);
void getship(struct ship *shipp, int s_type);

/* glog.c */
void glog_close(void);
int glog_open();
void glog_printf(const char *fmt, ...);
void glog_flush();

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
void declare_war(int mask, int refitdelay);
void do_refit(int type);
int numPlanets(int owner);

/* main.c */
void exitGame(int why);
int CheckBypass(char *login, char *host, char *file);

/* ntscmds.c */
int bounceSessionStats(int from);
int bounceSBStats(int from);
int bouncePingStats(int from);
int bounceUDPStats(int from);
#ifdef RSA
int bounceRSAClientType(int from);
#endif
int bounceWhois(int from);
int do_check_command(struct message *mess);

/* openmem.c */
int openmem(int trystart);
int setupmem(void);
int removemem(void);
void lock_show(int lock);
void lock_off(int lock);
void lock_on(int lock);
void lock_on_nowait(int lock);
int forgotten(void);

/* orbit.c */
void orbit(void);
void orbit_release_by_planet(struct planet *pl);

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

/* rsa_key.c */
void makeRSAPacket(void *packet);
int decryptRSAPacket (void *spacket,
		      void *cpacket,
		      char *serverName);

/* redraw.c */
void intrupt(void);

/* reserved.c */
struct reserved_spacket;
struct reserved_cpacket;

void makeReservedPacket(struct reserved_spacket *packet);
void encryptReservedPacket(struct reserved_spacket *spacket,
                           struct reserved_cpacket *cpacket,
                           char *server, int pno);

/* smessage.c */
void godf(int bounceto, const char *, ...);
void god(int bounceto, const char *);
void lmessage(const char *fmt, ...);
void pmessage(int recip, int group, char *address, const char *, ...);
void pmessage2(int recip, int group, char *address, u_char from,
               const char *, ...);
void amessage(char *str, int recip, int group);
void do_message_pre_set(void (*proposed)(struct message *message,
                                         char *address));
void do_message_post_set(int (*proposed)(struct message *message));
void do_message_force_daemon();
void do_message_force_player();
void do_message_requeue_all();

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
int practice_robo(void);

/* sysdefaults.c */
void readsysdefaults(void);
int update_sys_defaults(void);

/* timecheck.c */
int time_access(void);

/* torp.c */
int torpGetVectorSpeed(u_char pdir, int pspeed, u_char tdir, int speed);
void ntorp(u_char course, int attributes);
int getAdjTorpCost(u_char torpdir, int adjType);

/* transwarp.c */
int handleTranswarp(void);

/* warning.c */
void new_warning(int index, const char *fmt, ...);
void warning(char *text);
void spwarning(char *text, int index);
void s_warning(char *text, int index);
void swarning(u_char, u_char, u_char);

#endif /* __INCLUDED_proto_h__ */
