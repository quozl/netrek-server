/*
 * smessage.c
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <ctype.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

#ifndef MGOD
#define MGOD   0x10             /* TSH 3/10/93 */
#endif

#define ADDRLEN 10

#define BLANKCHAR(col, n) W_ClearArea(messagew, 5+W_Textwidth*(col), 5, \
    W_Textwidth * (n), W_Textheight, backColor);
#define DRAWCURSOR(col) W_WriteText(messagew, 5+W_Textwidth*(col), 5, \
    textColor, &cursor, 1, W_RegularFont);

static int lcount;
static char buf[80];
static char cursor = '_';
static int counterfeit = 0;
static int counterfeitFrom = -1;
static int addr_len;

char *getaddr(), *getaddr2();
static char addr, *addr_str;

smessage(ichar)
    char ichar;
{
    register int i;
    char *getaddr();
    char twochar[2];

    if (messpend == 0) {
	messpend = 1;
	if (mdisplayed) {
	    BLANKCHAR(0, lastcount);
	    mdisplayed = 0;
	}
	/* Put the proper recipient in the window */
	addr = ichar;
	addr_str = getaddr(addr);
	if (!counterfeit && (addr_str == 0)) {
	    /* print error message */
	    messpend = 0;
	    counterfeit = 0;
	    counterfeitFrom = -1;
	    BLANKCHAR(0, lcount + 1);
	    W_WarpPointer(NULL);
	    return;
	}
	if (counterfeit && !addr_str)
	    return;

	addr_len = strlen(addr_str);
	W_WriteText(messagew, 5, 5, textColor, addr_str, addr_len,
	    W_RegularFont);
	addr_len += 2;
	lcount = addr_len;
	if (!messpend)
	    lcount -= 5;
	DRAWCURSOR(lcount);
	return;
    }
    switch ((unsigned char)ichar & ~(0x80)) {
	case '\b':	/* character erase */
	case '\177':
	    if (--lcount < addr_len) {
		lcount = addr_len;
		break;
	    }
	    BLANKCHAR(lcount + 1, 1);
	    DRAWCURSOR(lcount);
	    break;

	case '\027':	/* word erase */
	    i = 0;
	    /* back up over blanks */
	    while (--lcount >= addr_len &&
		isspace((unsigned char)buf[lcount - addr_len] & ~(0x80)))
		i++;
	    lcount++;
	    /* back up over non-blanks */
	    while (--lcount >= addr_len &&
		!isspace((unsigned char)buf[lcount - addr_len] & ~(0x80)))
		i++;
	    lcount++;

	    if (i > 0) {
		BLANKCHAR(lcount, i + 1);
		DRAWCURSOR(lcount);
	    }
	    break;

	case '\025':	/* kill line */
	case '\030':
	    if (lcount > addr_len) {
		BLANKCHAR(addr_len, lcount - addr_len + 1);
		lcount = addr_len;
		DRAWCURSOR(addr_len);
	    }
	    break;

	case '\033':	/* abort message */
	    BLANKCHAR(0, lcount + 1);
	    mdisplayed = 0;
	    messpend = 0;
	    counterfeit = 0;
	    counterfeitFrom = -1;
	    W_WarpPointer(NULL);
	    break;

	case '\r':	/* send message */
	    buf[lcount - addr_len] = '\0';
	    messpend = 0;
	    switch (addr) {
		case 'A':
		    pmessage(buf, 0, MALL);
		    break;
		case 'G':
		    pmessage(buf, me->p_mapchars[1] - '0', MGOD);
		    break;
		case 'F':
		    pmessage(buf, FED, MTEAM);
		    break;
		case 'R':
		    pmessage(buf, ROM, MTEAM);
		    break;
		case 'K':
		    pmessage(buf, KLI, MTEAM);
		    break;
		case 'O':
		    pmessage(buf, ORI, MTEAM);
		    break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		    pmessage(buf, addr - '0', MINDIV);
		    break;
		case 'a': case 'b': case 'c': case 'd':
		case 'e': case 'f': case 'g': case 'h':
		case 'i': case 'j': case 'k': case 'l':
		case 'm': case 'n': case 'o': case 'p':
		case 'q': case 'r': case 's': case 't': 
		case 'u': case 'v': case 'w': case 'x': 
		case 'y': case 'z':
		    pmessage(buf, addr - 'a' + 10, MINDIV);
		    break;
		default:
		    warning("Not legal recipient");
	    }
	    BLANKCHAR(0, lcount + 1);
	    mdisplayed = 0;
	    lcount = 0;
	    break;

	default:	/* add character */
	    if (lcount >= 80) {
		W_Beep();
		break;
	    }
	    if (iscntrl((unsigned char )ichar & ~(0x80)))
		break;
	    twochar[0] = ichar;
	    twochar[1] = cursor;
	    W_WriteText(messagew, 5 + W_Textwidth * lcount, 5, textColor,
		twochar, 2, W_RegularFont);
	    buf[(lcount++) - addr_len] = ichar;
	    break;
    }
}

pmessage(str, recip, group)
char *str;
int recip;
int group;
{
    char newbuf[132];		/* increased, TSH 2/93 */
    struct message *cur;

    strcpy(newbuf, getaddr2(group, recip));
    strcat(newbuf, "  ");
    strcat(newbuf, str);

    if (++(mctl->mc_current) >= MAXMESSAGE)
        mctl->mc_current = 0;
    cur = &messages[mctl->mc_current];
    cur->m_no = mctl->mc_current;
    cur->m_flags = group;
    cur->m_time = 0;
    cur->m_recpt = recip;
    if (counterfeit && (addr_str[0] == ' '))
	cur->m_from = counterfeitFrom;
    else
	cur->m_from = 255;	/* TSH 5/93 (was 19 (?)) */
    strncpy(cur->m_data, newbuf, 79);
    cur->m_data[79] = '\0';

    cur->m_flags |= MVALID;
    cur->args[0] = DINVALID; /* to prevent false sending with SPS_S_WARNING*/
    counterfeit = 0;
    counterfeitFrom = -1;
    W_WarpPointer(NULL);
}

char *
getaddr(who)
char who;
{
    switch (who) {
    case 'A':
	return(getaddr2(MALL, 0));
    case 'F':
	return(getaddr2(MTEAM, FED));
    case 'R':
	return(getaddr2(MTEAM, ROM));
    case 'K':
	return(getaddr2(MTEAM, KLI));
    case 'O':
	return(getaddr2(MTEAM, ORI));
    case 'G':
	return(getaddr2(MGOD, 0));
    case 'C':
	if (counterfeit) {
	    BLANKCHAR(0, lcount + 1);
	    warning("Not legal recipient");
	    counterfeit = 0;
	    counterfeitFrom = 0;
	    return 0;
	}
	else {
	    warning("Select who message is from");
	    counterfeit = 1;
	    messpend = 0;
	}
	return 0;
	break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	if (isAlive(&players[who - '0'])
#ifdef POBSERV
	    || players[who-'0'].p_status == POBSERV
#endif
	) {
	    return(getaddr2(MINDIV, who-'0'));
	}
	else {
	    warning("Player is not in game");
	    counterfeit = 0;
	    counterfeitFrom = 0;
	    return(0);
	}
	break;
    case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p':
    case 'q': case 'r': case 's': case 't': 
    case 'u': case 'v': case 'w': case 'x': 
    case 'y': case 'z':
	if (who-'a'+10 > MAXPLAYER) {
	    warning("Player is not in game");
	    counterfeit = 0;
	    counterfeitFrom = 0;
	    return(0);
	}
	if (isAlive(&players[who - 'a' + 10])
#ifdef POBSERV
	    || players[who- 'a' + 10].p_status == POBSERV
#endif
	) {
	    return(getaddr2(MINDIV, who-'a'+10));
	}
	else {
	    warning("Player is not in game");
	    counterfeit = 0;
	    counterfeitFrom = 0;
	    return(0);
	}
	break;
    default:
	warning("Not legal recipient");
	counterfeit = 0;
	counterfeitFrom = 0;
	return(0);
    }
}

char *getaddr2(flags, recip) 
int flags;
int recip;
{
    static char addrmesg[32];		/* increased, TSH 2/93 */
    int		i;

    if (!counterfeit)
	sprintf(addrmesg, "%s->", godsname);	/* TSH 2/93 */
    else if (counterfeitFrom < 0) {
	messpend = 0;
	counterfeitFrom = recip;
	switch(flags) {
	case MALL:
	    (void) sprintf(addrmesg, "ALL->");
	    break;
	case MGOD:
	    (void) sprintf(addrmesg, "GOD->");
	    break;
	case MTEAM:
	    (void) sprintf(addrmesg, "%s->", teamshort[recip]);
	    break;
	case MINDIV:
	    (void) sprintf(addrmesg, " %c%c->",
			   teamlet[players[recip].p_team], shipnos[recip]);
	    break;
	}
	return addrmesg;
    }
    i = strlen(addrmesg);
    switch(flags) {
    case MALL:
	(void) sprintf(&addrmesg[i], "ALL");
	break;
    case MGOD:
	(void) sprintf(&addrmesg[i], "GOD");
	break;
    case MTEAM:
	(void) sprintf(&addrmesg[i], teamshort[recip]);
	break;
    case MINDIV:
	(void) sprintf(&addrmesg[i], "%c%c ",
	    teamlet[players[recip].p_team], shipnos[recip]);
	break;
    }
    return(addrmesg);
}
