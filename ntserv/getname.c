/* 
 * getname.c
 *
 * Kevin Smith 09/28/88
 *
 */
#include "copyright2.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "defs.h"
#include <sys/fcntl.h>
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "util.h"

#define streq(a,b) (strcmp((a),(b)) == 0)

/* file scope prototypes */
static void handleLogin(void);

#ifdef REGISTERED_USERS
char *registered_users_name;
char *registered_users_pass;
#endif

void getname(void)
/* Let person identify themselves from w */
{
    *(me->p_name)=0;
    while (*(me->p_name)==0) {
	handleLogin();
	/* denial of service attack risk ... if this persists beyond
	the sysdefaults value of LOGINTIME then the daemon will
	ghostbust the slot */
    }
}

static void handleLogin(void)
{
    static struct statentry player;
    static int position= -1;
    int i;
    int entries;
    char newpass[NAME_LEN];

    *namePick='\0';
    FD_SET (CP_LOGIN, &inputMask);
    while (*namePick=='\0') {
	/* Been ghostbusted? */
	if (me->p_status == PFREE) exitGame(0);
	if (isClientDead()) exitGame(0);
	if (me->p_disconnect) exitGame(me->p_disconnect);
	socketPause(1);
	readFromClient();
    }

    ERROR(8,("handleLogin: %s %s %s\n", 
	     passPick[15] == 0 ? "attempt" : "query", namePick, passPick));

    if (is_guest(namePick)) {

	hourratio=5;
	memset(&player.stats, 0, sizeof(struct stats));
#ifdef LTD_STATS
        /* reset both the player entry and stat entry */
        ltd_reset(me);
        ltd_reset_struct(player.stats.ltd);
#else
	player.stats.st_tticks = 1;
#endif
	player.stats.st_flags=ST_INITIAL;
	for (i=0; i<95; i++) {
	    player.stats.st_keymap[i]=i+32;
	}
	player.stats.st_keymap[95]=0;
	/* If this is a query on Guest, the client is screwed, but 
	 * I'll send him some crud anyway. 
	 */
	if (passPick[15]!=0) {
	    sendClientLogin(&player.stats);
	    flushSockBuf();
	    return;
	}
	sendClientLogin(&player.stats);
	flushSockBuf();
	strcpy(me->p_name, namePick);
	me->p_pos= -1;
	memcpy(&(me->p_stats), &player.stats, sizeof(struct stats));
	return;
    }

    /* mark the observer with special flag */
    if (Observer) {
	int x = strlen (namePick);
	if (namePick[x - 1] != 01) {
	    if (x > 13)
		x = 13;
	    namePick[x] = ' ';
	    namePick[x + 1] = 01;
	    namePick[x + 2] = '\0';
	}
    }

    hourratio=1;

    /* We look for the guy in the stat file */
    if (!streq(player.name, namePick)) {
      position = findplayer(namePick, &player, 1);
    } 

    /* Was this just a query? */
    if (passPick[15]!=0) {
	if (position == -1) {
	    sendClientLogin(NULL);
	} else {
	    sendClientLogin(&player.stats);
	}
	flushSockBuf();
	return;
    }
 
#ifdef REGISTERED_USERS
    /* record the username they wanted, but if we don't have it in the
       score file, force them to be a guest */
    registered_users_name = strdup(namePick);
    registered_users_pass = strdup(passPick);
    if (position == -1) {
      /* ERROR(8,("handleLogin: unregistered %s\n", namePick)); */
      strcpy(namePick, "guest");
      goto handlelogin_guest;
    }
    /* ERROR(8,("handleLogin: registered %s\n", namePick)); */
    /* todo: tell this user when they log in to try registering */
    /* todo: turn off certain privs if guest, e.g. eject voting */
    /* todo: replicate code below in the registration response program */
#endif

    /* A new guy? */
    if (position == -1) {
	strcpy(player.name, namePick);
	strcpy(player.password, crypt_player(passPick, namePick));
	memset(&player.stats, 0, sizeof(struct stats));
#ifdef LTD_STATS
        ltd_reset_struct(player.stats.ltd);
#else
	player.stats.st_tticks = 1;
#endif
	player.stats.st_flags=ST_INITIAL;
	for (i=0; i<95; i++) {
	    player.stats.st_keymap[i]=i+32;
	}
	if ((entries = newplayer(&player)) < 0) {
	  sendClientLogin(NULL);
	} else {
	  me->p_pos = entries;
	  memcpy(&(me->p_stats), &player.stats, sizeof(struct stats));
	  strcpy(me->p_name, namePick);
	  sendClientLogin(&player.stats);
	}
	flushSockBuf();
	return;
    }

    /* An actual login attempt */
    strcpy(newpass, crypt_player(passPick, player.name));
    if (!streq(player.password, newpass) &&
	 !streq(player.password, crypt_player_raw(passPick, player.password))) {
	    sendClientLogin(NULL);
	    flushSockBuf();
	    ERROR(8,("handleLogin: password-failure namePick=%s passPick=%s file=%s newstyle=%s oldstyle=%s\n", namePick, passPick, player.password, newpass, crypt_player_raw(passPick, player.password)));
	    return;
    }
    ERROR(8,("handleLogin: password-success namePick=%s passPick=%s file=%s newstyle=%s oldstyle=%s\n", namePick, passPick, player.password, newpass, crypt_player_raw(passPick, player.password)));
    sendClientLogin(&player.stats);
    strcpy(me->p_name, namePick);
    me->p_pos=position;
    memcpy(&(me->p_stats), &player.stats, sizeof(struct stats));
    if (!streq(player.password, newpass)) {
	/* update db if we were misusing crypt() */
	strcpy(player.password, newpass);
	savepass(&player);
    }
    flushSockBuf();
    return;
}
