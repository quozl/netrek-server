/*
 * smessage.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "shmatomic.h"

/* new code, sends bouncemsg to bounceto from GOD 4/17/92 TC */
/* Moved to smessage and changed to varargs 8/2/93 NBT */
/* Use stdarg 3/18/99 -da */

void do_message(int recip, int group, char *address, u_char from,
                const char *fmt, va_list args);

void godf(int bounceto, const char *fmt, ...) {
  char buf[15];
  va_list args;

  va_start(args, fmt);
  sprintf(buf, "GOD->%2s", players[bounceto].p_mapchars);
  do_message(bounceto, MINDIV, buf, 255, fmt, args);
  va_end(args);
}

void god(int bounceto, const char *msg) {
  godf(bounceto, "%s", msg);
}

/* statistics log message for client stdout or popup */

void lmessage(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  do_message(0, MALL|MCONQ, "", 255, fmt, args);
  va_end(args);
}

void pmessage(int recip, int group, char *address, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  do_message(recip, group, address, 255, fmt, args);
  va_end(args);
}

void pmessage2(int recip, int group, char *address, u_char from,
               const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  do_message(recip, group, address, from, fmt, args);
  va_end(args);
}

/* raw message */
void amessage(char *str, int recip, int group)
{
  pmessage(recip, group, "", "%s", str);
}

/* The message queues are ring buffers in shared memory, each with an
   index in mctl of the entry last published.  A writer fills in the
   entry after the index, then moves the index to it with a release
   store.  A reader loads the index with acquire, and reads entries up
   to it, so it never sees an entry before it is written. */

/* the entry after an index, in a ring buffer of a given size */
static int do_message_after(int current, int maximum)
{
  return current + 1 < maximum ? current + 1 : 0;
}

/* the next entry in the message queue, for the daemon */
static struct message *do_message_next_daemon(int **index, int *next)
{
  *index = &mctl->mc_current;
  *next = do_message_after(mctl->mc_current, MAXMESSAGE);
  return &messages[*next];
}

/* the next entry in the message queue, for a player */
static struct message *do_message_next_player(int **index, int *next)
{
  int j = me->p_no;

  *index = &mctl->mc_uplink[j];
  *next = do_message_after(mctl->mc_uplink[j], MAXUPLINK);
  return &uplink[*next + j * MAXUPLINK];
}

/* the next entry in the message queue, for out-of-band sources, which
   share it, so hold LOCK_OOB_ADD until the entry is published */
static struct message *do_message_next_oob(int **index, int *next)
{
  *index = &mctl->mc_oob;
  *next = do_message_after(mctl->mc_oob, MAXUPLINK);
  return &oob[*next];
}

/* the daemon's indexes of the next uplink entries to read */
static int mc_uplink[MAXPLAYER], mc_oob;

/* callback for preparing struct message entry further, used by daemon */
static void (*do_message_pre)(struct message *message, char *address) = NULL;

/* callback for post message insertion processing */
static int (*do_message_post)(struct message *message) = NULL;

/* message uplink allocation handler, default catch-all out-of-band queue */
static struct message *(*do_message_next)(int **index, int *next) =
  do_message_next_oob;

void do_message_pre_set(void (*proposed)(struct message *message, char *address))
{
  do_message_pre = proposed;
}

void do_message_post_set(int (*proposed)(struct message *message))
{
  do_message_post = proposed;
}

/* force message injection to use daemon uplink */
void do_message_force_daemon()
{
  int i;

  for (i=0;i<MAXPLAYER;i++)
    mc_uplink[i] = do_message_after(mctl->mc_uplink[i], MAXUPLINK);
  mc_oob = do_message_after(mctl->mc_oob, MAXUPLINK);
  do_message_next = do_message_next_daemon;
}

/* force message injection to use per-player uplink */
void do_message_force_player()
{
  do_message_next = do_message_next_player;
}

/* requeue a message into the main message queue */
static void do_message_requeue(struct message *src)
{
  if (src->m_flags & MVALID) {
    int *index, next;
    struct message *dst = do_message_next_daemon(&index, &next);
    memcpy(dst, src, sizeof(struct message));
    store_release(index, next);
    src->m_flags = 0;
  }
}

/* requeue messages from the uplink queues to the main message queue */
void do_message_requeue_all()
{
  int i, end;

  for (i=0; i<MAXPLAYER; i++) {
    end = do_message_after(load_acquire(&mctl->mc_uplink[i]), MAXUPLINK);
    for (; mc_uplink[i] != end;
         mc_uplink[i] = do_message_after(mc_uplink[i], MAXUPLINK)) {
      do_message_requeue(&uplink[mc_uplink[i] + i * MAXUPLINK]);
    }
  }

  end = do_message_after(load_acquire(&mctl->mc_oob), MAXUPLINK);
  for (; mc_oob != end; mc_oob = do_message_after(mc_oob, MAXUPLINK)) {
    do_message_requeue(&oob[mc_oob]);
  }
}

void do_message(int recip, int group, char *address, u_char from,
                const char *fmt, va_list args) {

  struct message msg, *cur;
  int *index, next;
  int oob = (do_message_next == do_message_next_oob);

  if (strlen(fmt) == 0) {
    ERROR(1,("ntserv: bogus pmessage call!!\n"));
    return;
  }

  bzero(&msg, sizeof(struct message));

  msg.m_flags = group;
  msg.m_recpt = recip;
  msg.m_from = from;

  if (address[0] == '\0') {
    vsnprintf(msg.m_data, sizeof(msg.m_data), fmt, args);
  } else {
    snprintf(msg.m_data, sizeof(msg.m_data), "%-9s ", address);
    vsnprintf(msg.m_data + 10, sizeof(msg.m_data) - 10, fmt, args);
  }

  msg.args[0] = DINVALID;

  /* callback for preparing struct message entry further, used by daemon */
  if (do_message_pre != NULL) {
    (*do_message_pre)(&msg, address);
  }

  msg.m_flags |= MVALID;

  /* write the entry, then publish it */
  if (oob) lock_on(LOCK_OOB_ADD);
  cur = (*do_message_next)(&index, &next);
  memcpy(cur, &msg, sizeof(struct message));
  store_release(index, next);
  if (oob) lock_off(LOCK_OOB_ADD);

  /* message insertion post-processor, for command detection; given the
     copy, as the daemon may take the entry from the queue at any time */
  if (do_message_post != NULL) {
    if ((msg.m_flags & MINDIV) &&
        (msg.m_recpt == me->p_no) &&
        (msg.m_from == me->p_no)) {
      (*do_message_post)(&msg);
    }
  }
}
