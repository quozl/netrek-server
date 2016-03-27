#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

static void spew_mess(struct message *mess);
static void logmessage(struct message *m);

int main(int argc, char **argv)
{
    int i; 
    int oldmctl;

    int filter;			/* 7/29/91 TC */

    int size;
    struct distress dist;
    char buf[MSG_LEN];
    struct message msg;

    openmem(0);

    filter = argc > 1;		/* special logging */

    oldmctl=mctl->mc_current;
    if (!filter)
	for (i=0; i<=oldmctl; i++) {
            memcpy(&msg, &messages[i], sizeof(struct message));
	    /* Fix for RCD distresses - 11/18/93 ATH */
	    if (msg.m_flags == (MTEAM | MDISTR | MVALID)) {
		buf[0]='\0';
		msg.m_flags ^= MDISTR;
		HandleGenDistr(msg.m_data,msg.m_from,msg.m_recpt,&dist);
		size = makedistress(&dist,buf,distmacro[dist.distype].macro);
		strncpy(msg.m_data,buf,size+1);
	    }

	    spew_mess(&msg);
	}
    for (;;) {
	sleep(1);
	while (oldmctl!=mctl->mc_current) {
	    oldmctl++;
	    if (oldmctl==MAXMESSAGE) oldmctl=0;
	    if (filter)
		logmessage(&(messages[oldmctl]));
	    else {
                memcpy(&msg, &messages[oldmctl], sizeof(struct message));
		/* Fix for RCD distresses - 11/18/93 ATH */
		if (msg.m_flags == (MTEAM | MDISTR | MVALID)) {
		    buf[0]='\0';
		    msg.m_flags ^= MDISTR;
		    HandleGenDistr(msg.m_data,msg.m_from,msg.m_recpt,&dist);
		    size= makedistress(&dist,buf,distmacro[dist.distype].macro);
		    strncpy(msg.m_data,buf,size+1);
		}
		spew_mess(&msg);
		fflush(stdout);
	    }
	}
    }
}

/* NBT 2/20/93 */
char *whydeadmess[] = { "", "[quit]", "[photon]", "[phaser]", "[planet]", 
      "[explosion]", "", "", "[ghostbust]", "[genocide]", "", "[plasma]",
      "[detted photon]", "[chain explosion]",
      "[TEAM]","","[team det]","[team explosion]"};
#define numwhymess 17 /* Wish there was a better way for this - NBT */ 

static void spew_mess(struct message *mess)                /* ATH 10/8/93 */
{
  char *nullstart;
  char temp[150];

#ifdef COLOUR
  char *colour = "\033[1;37m";

  if (!strncmp(mess->m_data, " F", 2)) colour = "\033[1;33m";
  if (!strncmp(mess->m_data, " R", 2)) colour = "\033[1;31m";
  if (!strncmp(mess->m_data, " K", 2)) colour = "\033[1;32m";
  if (!strncmp(mess->m_data, " O", 2)) colour = "\033[1;36m";
  if (!strncmp(mess->m_data, "FED", 3)) colour = "\033[1;33m";
  if (!strncmp(mess->m_data, "ROM", 3)) colour = "\033[1;31m";
  if (!strncmp(mess->m_data, "KLI", 3)) colour = "\033[1;32m";
  if (!strncmp(mess->m_data, "ORI", 3)) colour = "\033[1;36m";
  if (!strncmp(mess->m_data, "IND", 3)) colour = "\033[0;37m";

  printf("%s", colour);
#endif

  if (mess->args[0] != DMKILL)
    printf("%s\n", mess->m_data);
  else {
    strcpy(temp,mess->m_data);
    nullstart = strstr(temp,"(null)");
    if (nullstart == NULL || mess->args[5] > numwhymess)
      printf("%s\n", mess->m_data);
    else {
      nullstart[0]='\0';
      strcat(temp,whydeadmess[mess->args[5]]);
      if (strlen(temp)>(MSG_LEN - 1))
	temp[MSG_LEN - 1]='\0';
      printf("%s\n",temp);
    }
  }
}


static void logmessage(struct message *m)
{
    time_t curtime;
    struct tm *tmstruct;
    int hour;
    int least = MBOMB;

    /* decide whether or not to log this message */

    if (m->m_flags & MINDIV) return; /* individual message */

    if (!(m->m_flags & MGOD)) return;
    if ((m->m_flags & MGOD) > least) return;

    time(&curtime);
    tmstruct = localtime(&curtime);
    if (!(hour = tmstruct->tm_hour%12)) hour = 12;
    printf("%02d:%02d %-73.73s\n", hour, tmstruct->tm_min,
	   m->m_data);
    fflush(stdout);
}
