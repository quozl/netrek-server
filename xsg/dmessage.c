/*
 * dmessage.c 
 *
 * for the client of a socket based protocol.
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

char *whydeadmess[] = { "", "[quit]", "[photon]", "[phaser]", "[planet]", 
	"[explosion]", "", "", "[ghostbust]", "[genocide]", "", "[plasma]",
	"[detted photon]", "[chain explosion]", "[TEAM]","","[team det]",
	"[team explosion]"};
#define numwhymess 17   /* Wish there was a better way for this - NBT */


dmessage(mess)
int mess;
{
    register int len;
    W_Color color;

    struct distress dist;

    char message[MSG_LEN];
    unsigned char flags = (unsigned char)messages[mess].m_flags;
    unsigned char from  = (unsigned char)messages[mess].m_from;
    unsigned char recpt = (unsigned char)messages[mess].m_recpt;

    int templen;
    char *nullstart;
    char tempmsg[150];

    strcpy(message,messages[mess].m_data);
    color=textColor;

    if (from==255) {
	/* From God */
	color=textColor;
    } else if(from < MAXPLAYER){
	color=playerColor(&(players[from]));
    }
    else
#ifdef NBR
        if (from == 254) 
		from = 255;
	else {	
#else
        {
#endif
	    fprintf(stderr, "Bad 'from' field (%d) for message \"%s\"\n",
	 		from, message);
            return;
	}

        /* kludge for clients that can't handle RCD's */
	if (from < MAXPLAYER) {
          if (flags == (MTEAM | MDISTR | MVALID)) {
                char buf[MSG_LEN];
		flags ^= MDISTR;
                HandleGenDistr(message,from,recpt,&dist);
                makedistress(&dist,buf,distmacro[dist.distype].macro);
                /* note that we do NOT send the F0->FED part
                   so strncat is fine */
                buf[MSG_LEN - strlen(message)] = '\0';
                strcpy(message,buf);
	  }
        }

    /* Kludge stuff for report kills... 
     */

    message[79] = '\0';		/* make sure the message isn't garbage */
    len = strlen(message);

    if ((messages[mess].args[0]==DMKILL)||(messages[mess].args[0]==DMBOMB)) {
	if (!reportKills) return;
	strcpy(tempmsg,message);
	nullstart = strstr(tempmsg,"(null)");
	if (nullstart!=NULL) {
	    nullstart[0]='\0';
	    strcat(tempmsg,whydeadmess[messages[mess].args[5]]);
	}
	templen = strlen(tempmsg);
	if (templen>(MSG_LEN-1)) tempmsg[MSG_LEN-1]='\0';
	W_WriteText(reviewWin, 0, 0, color, tempmsg, templen, 0);
	return;
    }
    if (!strncmp(message+5, "GOD", 3) && msgBeep)	/* msg to GOD? */
	W_Beep();

    /* suppress warnings */
    if (!strncmp(message, "WRN->", 5)) return;
    
    W_WriteText(reviewWin, 0, 0, color, message, len, 0);

/*    update_sys_defaults();*/		/* after "rules of game" message */
}


instr(string1, string2)
char *string1, *string2;
{
    char *s;
    int length;

    length=strlen(string2);
    for (s=string1; *s!=0; s++) {
	if (*s == *string2 && strncmp(s, string2, length)==0) return(1);
    }
    return(0);
}

checkMsgs()
{
    while (oldmctl != mctl->mc_current) {
	oldmctl++;
	if (oldmctl>=MAXMESSAGE) oldmctl=0;
	dmessage(oldmctl);
    }
}

