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

/* point to the next entry in the ring buffer message queue, the old tail */
static int do_message_kick(int *current, int maximum)
{
  int i;

  if ((i = ++(*current)) >= maximum) {
    *current = 0;
    i = 0;
  }

  return i;
}

/* point to the next entry in the message queue, for the daemon */
static struct message *do_message_next_daemon()
{
  int mesgnum = do_message_kick(&mctl->mc_current, MAXMESSAGE);
  struct message *cur = &messages[mesgnum];
  cur->m_no = mesgnum;
  return cur;
}

/* point to the next entry in the message queue, for a player */
static struct message *do_message_next_player()
{
  int i, j;

  j = me->p_no;
  i = do_message_kick(&mctl->mc_uplink[j], MAXUPLINK);
  return &uplink[i + j * MAXUPLINK];
}

/* point to the next entry in the message queue, for out-of-band sources */
static struct message *do_message_next_oob()
{
  return &oob[do_message_kick(&mctl->mc_oob, MAXUPLINK)];
}

static int mc_uplink[MAXPLAYER], mc_oob;

/* callback for preparing struct message entry further, used by daemon */
static void (*do_message_pre)(struct message *message, char *address) = NULL;

/* callback for post message insertion processing */
static int (*do_message_post)(struct message *message) = NULL;

/* message uplink allocation handler, default catch-all out-of-band queue */
static struct message *(*do_message_next)() = do_message_next_oob;

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

  for (i=0;i<MAXPLAYER;i++) mc_uplink[i] = mctl->mc_uplink[i] + 1;
  mc_oob = mctl->mc_oob + 1;
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
    struct message *dst = do_message_next_daemon();
    memcpy(dst, src, sizeof(struct message));
    src->m_flags = 0;
  }
}

/* requeue messages from the uplink queues to the main message queue */
void do_message_requeue_all()
{
  int i;

  for (i=0; i<MAXPLAYER; i++) {
    for (; mc_uplink[i] != (mctl->mc_uplink[i]+1) % MAXUPLINK;
         mc_uplink[i] = (mc_uplink[i]+1) % MAXUPLINK) {
      do_message_requeue(&uplink[mc_uplink[i] + i * MAXUPLINK]);
    }
  }

  for (; mc_oob != (mctl->mc_oob+1) % MAXUPLINK;
       mc_oob = (mc_oob+1) % MAXUPLINK) {
    do_message_requeue(&oob[mc_oob]);
  }
}

void do_message(int recip, int group, char *address, u_char from,
                const char *fmt, va_list args) {

  struct message *cur;
  char temp[150];
  char temp2[150];

  if (strlen(fmt) == 0) {
    ERROR(1,("ntserv: bogus pmessage call!!\n"));
    return;
  }

  vsnprintf(temp, 150, fmt, args);

  if (address[0] == '\0') {
    strncpy(temp2, temp, 150);
  } else {
    snprintf(temp2, 150, "%-9s %s", address, temp);
  }
  temp2[MSG_LEN - 1] = '\0';      /* added 3/8/93  NBT */

  cur = (*do_message_next)();
  memset(cur, 0, sizeof(struct message));
  cur->m_flags = group;
  cur->m_recpt = recip;
  cur->m_from = from;

  strncpy(cur->m_data, temp2, MSG_LEN);
  cur->args[0] = DINVALID;

  /* callback for preparing struct message entry further, used by daemon */
  if (do_message_pre != NULL) {
    (*do_message_pre)(cur, address);
  }

  cur->m_flags |= MVALID;

  /* message insertion post-processor, for command detection */
  if (do_message_post != NULL) {
    if ((cur->m_flags & MINDIV) &&
        (cur->m_recpt == me->p_no) &&
        (cur->m_from == me->p_no)) {
      (*do_message_post)(cur);
    }
  }
}
