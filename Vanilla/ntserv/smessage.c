/*
 * smessage.c
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <ctype.h>
#include <stdarg.h>
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
  /* Message from God */

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

static void (*do_message_pre)(struct message *message, char *address) = NULL;
static int (*do_message_post)(struct message *message) = NULL;

void do_message_pre_set(void (*proposed)(struct message *message, char *address))
{
  do_message_pre = proposed;
}

void do_message_post_set(int (*proposed)(struct message *message))
{
  do_message_post = proposed;
}

void do_message(int recip, int group, char *address, u_char from,
                const char *fmt, va_list args) {

  struct message *cur;
  char temp[150];
  char temp2[150];
  int mesgnum;
/*  va_list args;*/

  if (strlen(fmt) == 0) {
    ERROR(1,("ntserv: bogus pmessage call!!\n"));
    return;
  }

/*  va_start(args, fmt);*/
  vsprintf(temp, fmt, args);
/*  va_end(args);*/

  if (address[0] == '\0') {
    strcpy(temp2, temp);
  }
  else {
    sprintf(temp2,"%-9s %s",address,temp);
  }
  temp2[MSG_LEN - 1]='\0';      /* added 3/8/93  NBT */

  if ((mesgnum = ++(mctl->mc_current)) >= MAXMESSAGE) {
    mctl->mc_current = 0;
    mesgnum = 0;
  }

  cur = &messages[mesgnum];
  cur->m_flags = group;
  cur->m_no = mesgnum;
  cur->m_recpt = recip;
  cur->m_from = from;

  strcpy(cur->m_data,temp2);

  /* message insertion pre-processor, for generalised adjustment */
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
