/*
 * warning.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"


/* 
  All warning messages should use new_warning() from now on. The index
  is a define found in defs.h that is specific to a particular message. If
  the warning doesn't have a define then use UNDEF.
*/

void new_warning(int index, const char *fmt, ...) {

  char temp[150];

  va_list args;
  va_start(args, fmt);

  vsprintf(temp, fmt, args);

  va_end(args);

  if (index == UNDEF) {
    warning(temp);
  }
  else {
    spwarning(temp, index);
  }

  if (eventlog) {
    char from_str[9]="WRN->\0\0\0";

    strcat(from_str, me->p_mapchars);
    pmessage2(0, 0, from_str, me->p_no, "%s", temp);
  }
}


/*
** The warning in text will be printed in the warning window.
** The message will last WARNTIME/10 seconds unless another message
** comes through and overwrites it.
*/

extern int sizes[];

void warning(char *text) {

  struct warning_spacket warn;

  if (!send_short) {

    warn.type = SP_WARNING;
    strncpy(warn.mesg, text, MSG_LEN);
    warn.mesg[MSG_LEN-1]='\0';
    sendClientPacket((CVOID) &warn);

  }
  else {  /* send it like a CP_S_MESSAGE */

    int size;
    strncpy(warn.mesg, text, MSG_LEN);
    warn.mesg[MSG_LEN-1]='\0';
    size = strlen(warn.mesg);
    size += 5; /* 1 for '\0', 4 packetheader */

    if((size % 4) != 0)
      size += (4 - (size % 4));

    warn.pad3 = (char) size;
    sizes[SP_S_WARNING] = size;
    warn.type = SP_S_WARNING;
    warn.pad1 = SHORT_WARNING;
    warn.pad2 = 0;
    sendClientPacket((CVOID) &warn);
    sizes[SP_S_WARNING] = sizeof(struct warning_s_spacket);
    /* Back to default */

  }

}


/* This is the interface to the new warning scheme. If send_short = 1 all
    warnings are send with SP_S_WARNING */

/* index = The index into the warnings array in client */
void spwarning(char *text, int index) {

  struct warning_spacket warn;
  struct warning_s_spacket swarn;

  if (!send_short) {

    warn.type = SP_WARNING;
    strncpy(warn.mesg, text, MSG_LEN);
    warn.mesg[MSG_LEN-1]='\0';
    sendClientPacket((CVOID) &warn);

  }
  else {  /* These are only 'TEXTE' warnings */

    swarn.type = SP_S_WARNING;
    swarn.whichmessage = TEXTE;
    swarn.argument = (u_char)index & 0xff;
    swarn.argument2 = ((u_char) index >> 8) & 0xff;
    sendClientPacket((CVOID) &swarn);

  }

}

/* This function is for new warnings that are not compiled in the client */
/* index = The index into the warnings array in client */
void s_warning(char *text, int index) {

  struct warning_spacket warn;
  struct warning_s_spacket swarn;
  int size;

  /* This is a (sortof) boolean arry to remind the server that he has sent */
  static char sent_to_client[256];


  /* a warning to the client */

  if (!send_short) {

    warn.type=SP_WARNING;
    strncpy(warn.mesg, text, MSG_LEN);
    warn.mesg[MSG_LEN-1]='\0';
    sendClientPacket((CVOID) &warn);

  }
  else if (sent_to_client[index]) { /* The client has that warning */

    swarn.type = SP_S_WARNING;
    swarn.whichmessage = STEXTE;
    swarn.argument = (u_char) index;
    swarn.argument2 = 0;
    sendClientPacket((CVOID) &swarn);

  }
  else  { /* first time */

    strncpy(warn.mesg, text, MSG_LEN);
    warn.mesg[MSG_LEN-1]='\0';
    size = strlen(warn.mesg);
    size += 5; /* 1 for '\0', 4 packetheader */
    if((size % 4) != 0)
      size += (4 - (size % 4));

    warn.pad3 = (char) size;
    sizes[SP_S_WARNING] = size;
    warn.type = SP_S_WARNING;
    warn.pad1 = STEXTE_STRING;
    warn.pad2 = (u_char)index; /* This is for future reference */
    sent_to_client[index] = 1;
    sendClientPacket((CVOID) &warn);
    sizes[SP_S_WARNING] = sizeof(struct warning_s_spacket);
    /* Back to default */

  }

}

void swarning(u_char whichmessage, u_char argument, u_char argument2) {

  struct warning_s_spacket swarn;

  swarn.type = SP_S_WARNING;
  swarn.whichmessage = whichmessage;
  swarn.argument = argument;
  swarn.argument2 = argument2;
  sendClientPacket((CVOID) &swarn);

}
