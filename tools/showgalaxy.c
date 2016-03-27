#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/file.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/select.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include <curses.h>
#include <string.h>
#include "struct.h"
#include "data.h"
#include "ltd_stats.h"

#define HEIGHT 20
#define WIDTH 80
#define MESSAGES 3
#define THEIGHT HEIGHT+MESSAGES
char *ships = "0123456789abcdefghij";
char *teamnames[] = {"---","FED","ROM","???","KLI","???","???","???","ORI"};

extern int openmem(int);
extern void HandleGenDistr (char *message, u_char from, u_char to,
                            struct distress *dist);
extern int makedistress (struct distress *dist, char *cry, char *pm);


void writescreen(int zoom, int x, int y, char *str, int len);
void writechar(int x, int y, char ch);
int isinput(int delay);
void showPlanets(void);
void showPlayers(void);
void usage(char *);
void wspew_mess(struct message *mess);

void reaper()
{
    clear();
    refresh();
    endwin();
    exit(0);
}

WINDOW *mesg_w, *main_w, *command_w;
char lc[]="ifrxkxxxo";

int main(int argc, char **argv)
{
    int i;
    int oldmctl;
    int messageline;
    int command=0;
    int zoom= -1;
    char s[100];
    char *st;
    int delay=10;
    int sendingMessage=0;
    int sendingTo=0;
    int sendMask=0;
    char header[20];
    struct torp *t;
    struct distress dist;
    char buf[MSG_LEN];

    for (i=1; i<argc; i++) {
	if (argv[i][0]=='-') {
	    switch(argv[i][1]) {
	    case 'd':
		delay=atoi(argv[i]+2);
		if (delay==0) delay=10;
		break;
	    default:
		usage(argv[0]);
		break;
	    }
	}
    }

    openmem(0);
    oldmctl=mctl->mc_current;

    initscr();
    signal(SIGINT, reaper);
    signal(SIGTSTP, reaper);	/* I'm lazy, ok? */
    crmode();
    noecho();
    main_w=newwin(HEIGHT, WIDTH, 0, 0);
    mesg_w=newwin(MESSAGES, WIDTH, HEIGHT, 0);
    command_w=newwin(1, WIDTH, THEIGHT, 0);
    wrefresh(mesg_w);
    wrefresh(main_w);
    wrefresh(command_w);
    messageline=0;
    for (;;) {
	while (isinput(delay)) {
	    if (!command) {
		command=getch();
		if (sendingMessage) {
		    if (sendingTo==-1) {
			strcpy(header, "GOD->");
			switch(command) {
			case 27:
			    sendingMessage=0;
			    wclear(command_w);
			    wrefresh(command_w);
			    break;
			case 'A':
			    sendingTo=0;
			    sendMask=MALL;
			    strcat(header,"ALL ");
			    break;
			case 'F':
			    sendingTo=FED;
			    sendMask=MTEAM;
			    strcat(header,"FED ");
			    break;
			case 'R':
			    sendingTo=ROM;
			    sendMask=MTEAM;
			    strcat(header,"ROM ");
			    break;
			case 'K':
			    sendingTo=KLI;
			    sendMask=MTEAM;
			    strcat(header,"KLI ");
			    break;
			case 'O':
			    sendingTo=ORI;
			    sendMask=MTEAM;
			    strcat(header,"ORI ");
			    break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			    sendingTo=command-'0';
			    sendMask=MINDIV;
			    break;
			case 'a': case 'b': case 'c': case 'd': case 'e':
			case 'f': case 'g': case 'h': case 'i': case 'j':
			    sendingTo=command-'a'+10;
			    sendMask=MINDIV;
			    break;
			default:
			    break;
			}
			if (sendMask==MINDIV) {
			    strncat(header, players[sendingTo].p_mapchars, 2);
			    strcat(header, "  ");
			}
			if (sendingTo != -1) {
			    wmove(command_w, 0,0);
			    waddstr(command_w, ": ");
			    waddstr(command_w, header);
			    waddstr(command_w, "_");
			}
		    } else {
			static char theMess[MSG_LEN];
			static char *s=NULL;

			if (s==NULL) {
			    strcpy(theMess,header);
			    s=theMess+strlen(theMess);
			}
			switch(command) {
			case 27:	/* Escape */
			    s=NULL;
			    sendingMessage=0;
			    wclear(command_w);
			    break;
			case '\n':
			    amessage(theMess,sendingTo,sendMask);
			    s=NULL;
			    sendingMessage=0;
			    wclear(command_w);
			    break;
			case '\b':
			    if (s>theMess) {
				s--;
				*s=0;
				wmove(command_w,0,s-theMess+2);
				waddstr(command_w,"_ ");
				break;
			    }
			    break;
			case 21:	/* Ctrl - U */
			    s=theMess;
			    wclear(command_w);
			    wmove(command_w,0,0);
			    waddstr(command_w,": _");
			    break;
			default:
			    if (s<theMess+76) {
				*s=command;
				s++;
				*s=0;
				wmove(command_w,0,s-theMess+1);
				waddch(command_w,command);
				waddch(command_w,'_');
				break;
			    }
			    break;
			}
		    }
		    wrefresh(command_w);
		} else switch(command) {
		case 'm':
		    sendingMessage=1;
		    sendingTo= -1;
		    wmove(command_w,0,0);
		    waddstr(command_w,"?");
		    wrefresh(command_w);
		    break;
		case 'f':
		    zoom= -1;
		    command=0;
		    break;
		case 'z':
		    break;
		case 12:
		    command=0;
		    wclear(mesg_w);
		    wclear(main_w);
		    wclear(command_w);
		    wrefresh(mesg_w);
		    wrefresh(main_w);
		    wrefresh(command_w);
		    printf("\033[;H\033[2J");
		    fflush(stdout);
		    break;
		case 'P':
		    showPlanets();
		    wclear(main_w);
		    break;
		case 'L':
		    showPlayers();
		    wclear(main_w);
		    break;
		default:
		    command=0;
		    break;
		}
	    } else {
		switch(command) {
		case 'z':
		    st=strchr(ships,getch());
		    if (st!=0) {
			zoom=st-ships;
		    }
		    break;
		default:
		    break;
		}
		command=0;
	    }
	}
	if (oldmctl!=mctl->mc_current) {
	    oldmctl++;
	    if (oldmctl==MAXMESSAGE) oldmctl=0;
	    wmove(mesg_w, messageline, 0);
	    waddstr(mesg_w, " ");
	    messageline++;
	    if (messageline==MESSAGES) messageline=0;
	    wmove(mesg_w, messageline, 0);
	    waddstr(mesg_w, ">");
	    wclrtoeol(mesg_w);
	    /* Fix for RCD distresses - 11/18/93 ATH */
	    if (messages[oldmctl].m_flags == (MTEAM | MDISTR | MVALID)) {
		buf[0]='\0';
		messages[oldmctl].m_flags ^= MDISTR; /* get rid of MDISTR flag 
						     so client isn't confused */
	        HandleGenDistr(messages[oldmctl].m_data,
			messages[oldmctl].m_from,
			messages[oldmctl].m_recpt,&dist);
		makedistress(&dist,buf,distmacro[dist.distype].macro);
		/* note that we do NOT send the F0->FED part 
		   so strncat is fine */
		buf[MSG_LEN - strlen(messages[oldmctl].m_data)] = '\0';
	 	strcpy(messages[oldmctl].m_data,buf);
	    }
	    wspew_mess(&(messages[oldmctl]));
	}
	werase(main_w);
	for (i=0; i<MAXPLANETS; i++) {
	    strncpy(s+2,planets[i].pl_name,3);
	    s[5]=0;
	    s[1]='-';
	    s[0]=teamnames[planets[i].pl_owner][0];
	    writescreen(zoom, planets[i].pl_x,planets[i].pl_y,s,5);
	}
	for (i=0; i<MAXPLAYER; i++) {
	    if (players[i].p_status != PFREE) {
	    }
	    if (players[i].p_status != PALIVE) continue;
	    strncpy(s, players[i].p_mapchars,2);
	    s[2]=0;
	    if ((players[i].p_flags & PFSEEN) == 0) {
		if (players[i].p_flags & PFCLOAK) {
		    s[0]='-';
		} else {
		    s[0]=lc[players[i].p_team];
		}
	    } else if (players[i].p_flags & PFCLOAK) {
		s[0]='?';
	    }
	    writescreen(zoom, players[i].p_x, players[i].p_y, 
		s, 2);
	}
	for (t=firstTorp; t<=lastPlasma; t++) {
	  if (t->t_status == TFREE) 
	    continue;
	  if (t->t_status == TEXPLODE) {
	    writescreen(zoom, t->t_x, t->t_y, 
			(t->t_type == TPHOTON) ? "o" : "O", 1);
	  } else {
	    writescreen(zoom, t->t_x, t->t_y, 
			(t->t_type == TPHOTON) ? "." : "*", 1);
	  }
	}
	wrefresh(main_w);
	wrefresh(mesg_w);
	wrefresh(command_w);
    }
}

void writescreen(int zoom, int x, int y, char *str, int len)
{
    int j,k;
    int i;

    if (zoom== -1) {
	if (x==GWIDTH) x=GWIDTH-1;
	if (y==GWIDTH) y=GWIDTH-1;
	j=(WIDTH)*x/GWIDTH;
	k=(HEIGHT)*y/GWIDTH;
    } else {
	j=(WIDTH)*(x-players[zoom].p_x)/(WINSIDE*SCALE)+WIDTH/2;
	k=(HEIGHT)*(y-players[zoom].p_y)/(WINSIDE*SCALE)+HEIGHT/2;
    }
    j=j-len/2;
    for (i=0; i<len; i++) {
	writechar(j+i,k,str[i]);
    }
}

void writechar(int x, int y, char ch)
{
    if (x>=0 && x<WIDTH && y>=0 && y<HEIGHT) {
	wmove(main_w, y,x);
	waddch(main_w, ch);
    }
}

int isinput(int delay)
{
    struct timeval timeout;
    fd_set reads;

    FD_ZERO(&reads);
    FD_SET(1, &reads);
    timeout.tv_sec = delay/10;
    timeout.tv_usec = (delay % 10) * 100000;
    return (select(1, &reads, NULL, NULL,&timeout));
}

void showPlanets(void)
{
    int i,j,k;

    j=0; 
    k=0;
    wclear(main_w);
    for (i=0; i<MAXPLANETS; i++) {
	wmove(main_w,j,k);
	wprintw(main_w,"%-16s %s %4d  %c%c%c  %c%c%c%c",
	    planets[i].pl_name, teamnames[planets[i].pl_owner],
	    planets[i].pl_armies, 
	    (planets[i].pl_flags & PLFUEL) ? 'F' : ' ',
	    (planets[i].pl_flags & PLREPAIR) ? 'R' : ' ',
	    (planets[i].pl_flags & PLAGRI) ? 'A' : ' ',
	    (planets[i].pl_info & FED) ? 'F' : ' ',
	    (planets[i].pl_info & ROM) ? 'R' : ' ',
	    (planets[i].pl_info & KLI) ? 'K' : ' ',
	    (planets[i].pl_info & ORI) ? 'O' : ' ');
	j=j+1;
	if (j==HEIGHT) {
	    j=0;
	    k=k+40;
	    if (k>WIDTH-30) {
		break;
	    }
	}
    }
    wrefresh(main_w);
    getch();
}

void showPlayers(void)
{
    int i,j;
    static int mode=0;

    if (mode) goto mode1;

    for (;;) {
	mode=0;
	j=1;
	wclear(main_w);
	for (i=0; i<MAXPLAYER; i++) {
	    if (players[i].p_status==PFREE) continue;
	    if (j==1) {
		wclear(main_w);
		wmove(main_w,0,0);
		wprintw(main_w," # Player          Display          Type Bombing Planets Offense Defense");
	    }
	    wmove(main_w,j,0);
	    wprintw(main_w,"%2s %-16s%-16.16s   %2s %7.2f %7.2f %7.2f %7.2f",
		players[i].p_mapchars,
		players[i].p_name,
		players[i].p_monitor,
		shiptypes[players[i].p_ship.s_type],
#ifdef LTD_STATS
                ltd_bombing_rating(&(players[i])),
                ltd_planet_rating(&(players[i])),
                ltd_offense_rating(&(players[i])),
                ltd_defense_rating(&(players[i])));
#else
		bombingRating(players+i),
		planetRating(players+i),
		offenseRating(players+i),
		defenseRating(players+i));
#endif
	    j=j+1;
	    if (j==HEIGHT) {
		wrefresh(main_w);
		if (getch() != ' ') break;
		j=1;
	    }
	}
	wrefresh(main_w);
	if (getch() != ' ') break;
mode1:
	mode=1;
	j=1;
	wclear(main_w);
	for (i=0; i<MAXPLAYER; i++) {
	    if (j==1) {
		wclear(main_w);
		wmove(main_w,0,0);
		wprintw(main_w," # Player          Login            Type  Kills Damage Shields Armies  Fuel");
	    }
	    if (players[i].p_status==PFREE) continue;
	    wmove(main_w,j,0);
	    wprintw(main_w,"%2s %-16s%-16.16s   %2s %6.2f %6d %7d %6d %5d",
		players[i].p_mapchars,
		players[i].p_name,
		players[i].p_login,
		shiptypes[players[i].p_ship.s_type],
		players[i].p_kills,
		players[i].p_damage,
		players[i].p_shield,
		players[i].p_armies,
		players[i].p_fuel);
	    j=j+1;
	    if (j==HEIGHT) {
		wrefresh(main_w);
		if (getch() != ' ') break;
		j=1;
	    }
	}
	wrefresh(main_w);
	if (getch() != ' ') break;
    }
}

void usage(char *string)
{
    printf("Usage: %s [-dnnn]\n", string);
    printf("  -dnnn  delay nnn 1/10 seconds between frames\n");
}

 /* NBT 2/20/93 */
char *whydeadmess[] = { "", "[quit]", "[photon]", "[phaser]", "[planet]",
	 "[explosion]", "", "", "[ghostbust]", "[genocide]", "", "[plasma]",
	 "[detted photon]", "[chain explosion]", "[TEAM]","","[team det]",
	 "[team explosion]"};
#define numwhymess 17 /* Wish there was a better way for this - NBT */ 

void wspew_mess(struct message *mess)                /* ATH 10/8/93 */
{
  char *nullstart;
  char temp[150];

  if (mess->args[0] != DMKILL)
    waddstr(mesg_w, mess->m_data);
  else {
    strcpy(temp,mess->m_data);
    nullstart = strstr(temp,"(null)");
    if (nullstart==NULL)
      waddstr(mesg_w, mess->m_data);
    else {
      nullstart[0]='\0';
      strcat(temp,whydeadmess[mess->args[5]]);
      if (strlen(temp)>(MSG_LEN - 1))
      temp[MSG_LEN - 1]='\0';
      waddstr(mesg_w, temp);
    }
  }
}

