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
#include INC_SYS_FCNTL
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "salt.h"

#define streq(a,b) (strcmp((a),(b)) == 0)

/* file scope prototypes */
static void handleLogin(void);
static int lockout(void);
static void savepass(const struct statentry *);


void getname(void)
/* Let person identify themselves from w */
{
    *(me->p_name)=0;
    while (*(me->p_name)==0) {
	handleLogin();
    }
}

static void handleLogin(void)
{
    static struct statentry player;
    static int position= -1;
    int plfd;
    int i;
    int entries;
    off_t file_pos;
    saltbuf sb;
    char newpass[NAME_LEN];

    *namePick='\0';
    FD_SET (CP_LOGIN, &inputMask);
    while (*namePick=='\0') {
	/* Been ghostbusted? */
	if (me->p_status==PFREE) exitGame();
	if (isClientDead()) exitGame();
	socketPause(1);
	readFromClient();
    }

    if ((strcmp(namePick, "Guest")==0 || strcmp(namePick, "guest")==0) &&
	!lockout()) {

        /* all INL games prohibit guest login */
        if (status->gameup & GU_INROBOT) {
	  sendClientLogin(NULL);
	  flushSockBuf();
	  return;
	}
	/* but we don't check for existing players on INL robot entry */

	hourratio=5;
	MZERO(&player.stats, sizeof(struct stats));
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
	MCOPY(&player.stats, &(me->p_stats), sizeof(struct stats));
	return;
    }

#ifdef OBSERVERS
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
#endif

    hourratio=1;

    /* We look for the guy in the stat file */
    if (strcmp(player.name, namePick) != 0) {
	for (;;) {	/* so I can use break; */
	    plfd = open(PlayerFile, O_RDONLY, 0644);
	    if (plfd < 0) {
		ERROR(1,("I cannot open the player file!\n"));
		strcpy(player.name, namePick);
		position= -1;
		ERROR(1,("Error number: %d\n", errno));
		break;
	    }
	    /* sequential search of player file */
	    position=0;
	    while (read(plfd, (char *) &player, sizeof(struct statentry)) ==
		    sizeof(struct statentry)) {
		if (strcmp(namePick, player.name)==0) {
		    close(plfd);
		    break;
		}
		position++;
	    }
	    if (strcmp(namePick, player.name)==0) break;
	    close(plfd);
	    position= -1;
	    strcpy(player.name, namePick);
	    break;
	}
    } 

    /* Was this just a query? */
    if (passPick[15]!=0) {
	if (position== -1) {
	    sendClientLogin(NULL);
	} else {
	    sendClientLogin(&player.stats);
	}
	flushSockBuf();
	return;
    } 

    /* A new guy? */
    if ((position== -1) && !lockout()) {
	strcpy(player.name, namePick);
	/* Linux: compiler warnings with -Wall here, as crypt is in unistd.h
	   but needs _XOPEN_SOURCE defined, which then breaks lots of other
	   things such as u_int. - Quozl */
	strcpy(player.password, (char *) crypt(passPick, salt(namePick, sb)));
	MZERO(&player.stats, sizeof(struct stats));
#ifdef LTD_STATS
        ltd_reset_struct(player.stats.ltd);
#else
	player.stats.st_tticks = 1;
#endif
	player.stats.st_flags=ST_INITIAL;
	for (i=0; i<95; i++) {
	    player.stats.st_keymap[i]=i+32;
	}
	/* race condition: Two new players joining at once
	 * can screw up the database.
	 */
	plfd = open(PlayerFile, O_RDWR|O_CREAT, 0644);
	if (plfd < 0) {
	    sendClientLogin(NULL);
	} else {
	    if ((file_pos = lseek(plfd, 0, SEEK_END)) < 0) {
		sendClientLogin(NULL);
 	    }
	    write(plfd, (char *) &player, sizeof(struct statentry));
	    close(plfd);
	    entries = file_pos / sizeof(struct statentry);
	    me->p_pos = entries;
	    MCOPY(&player.stats, &(me->p_stats), sizeof(struct stats));
	    strcpy(me->p_name, namePick);
	    sendClientLogin(&player.stats);
	}
	flushSockBuf();
	return;
    }
    /* An actual login attempt */
    strcpy(newpass, (char *) crypt(passPick, salt(player.name, sb)));
    if (lockout() ||
	(!streq(player.password, newpass) &&
	 !streq(player.password, (char *) crypt(passPick, player.password)))) {
	    sendClientLogin(NULL);
	    flushSockBuf();
	    return;
    }
    sendClientLogin(&player.stats);
    strcpy(me->p_name, namePick);
    me->p_pos=position;
    MCOPY(&player.stats, &(me->p_stats), sizeof(struct stats));
    if (!streq(player.password, newpass)) {
	/* update db if we were misuing crypt() */
	strcpy(player.password, newpass);
	savepass(&player);
    }
    flushSockBuf();
    return;
}

void changepassword (char *passPick)
{
  saltbuf sb;
  struct statentry se;
  strcpy(se.password, (char *) crypt(passPick, salt(me->p_name, sb)));
  savepass(&se);
}

static void savepass(const struct statentry* se)
{
    int fd;
    if (me->p_pos < 0) return;
    printf("getname.c: updating password for %s\n", se->name);
    fd = open(PlayerFile, O_WRONLY, 0644);
    if (fd >= 0) {
	lseek(fd, me->p_pos * sizeof(struct statentry) +
	      offsetof(struct statentry, password), SEEK_SET);
	write(fd, &se->password, sizeof(se->password));
	close(fd);
    }
}

void savestats(void)
{
    int fd;

    if (me->p_pos < 0) return;

#ifdef OBSERVERS
    /* Do not save stats for observers.  This is corrupting the DB. -da */
    if (Observer) return;
#endif

    fd = open(PlayerFile, O_WRONLY, 0644);
    if (fd >= 0) {
	me->p_stats.st_lastlogin = time(NULL);
	lseek(fd, me->p_pos * sizeof(struct statentry) +
	      offsetof(struct statentry, stats), SEEK_SET);
	write(fd, (char *) &me->p_stats, sizeof(struct stats));
	close(fd);
    }
}

/* return true if we want a lockout */
static int lockout(void)
{
    return ((strncmp(login, "bozo", 4) == 0) || 0);
}
