#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/file.h>
#include <math.h>
#include <sys/ipc.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

/* file scope prototypes */
static void pmessage(char *, int, int);

/* external prototypes */
extern int openmem(int);	/* from openmem.c */


int main()
{
    char message[MSG_LEN + 20];
    char buf[MSG_LEN + 20];
    char *to;

    openmem(0);
    printf("Enter Message:\n");
    fgets(message, MSG_LEN + 20, stdin);
    if (strlen(message) > 0) message[strlen(message)-1] = '\0';
    message[79]='\0';
    printf("Who are you sending the message to? ");
    fgets(buf, MSG_LEN + 20, stdin);
    if (strlen(buf) > 0) buf[strlen(buf)-1] = '\0';
    to=buf;
    while (*to!='\0') {
	switch(*to) {
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
	    printf("Message goes to player %c\n", *to);
	    pmessage(message, *to-'0', MINDIV);
	    break;
	case 'a': case 'b': case 'c': case 'd':
	case 'e': case 'f': case 'g': case 'h':
	case 'i': case 'j': case 'k': case 'l':
	case 'm': case 'n': case 'o': case 'p':
	case 'q': case 'r': case 's': case 't':
	case 'u': case 'v': case 'w': case 'x':
	case 'y': case 'z':
	    printf("Message goes to player %c\n", *to);
	    pmessage(message, *to-'a'+10, MINDIV);
	    break;
	case 'A':
	    printf("Message goes to ALL\n");
	    pmessage(message, 0, MALL);
	    break;
	case 'K':
	    printf("Message goes to KLI\n");
	    pmessage(message, KLI, MTEAM);
	    break;
	case 'R':
	    printf("Message goes to ROM\n");
	    pmessage(message, ROM, MTEAM);
	    break;
	case 'O':
	    printf("Message goes to ORI\n");
	    pmessage(message, ORI, MTEAM);
	    break;
	case 'F':
	    printf("Message goes to FED\n");
	    pmessage(message, FED, MTEAM);
	    break;
	default: 
	    break;
	}
	to++;
    }
    return 1;
}

void pmessage(char *str, int recip, int group)
{
    struct message *cur;
    if (++(mctl->mc_current) >= MAXMESSAGE)
	mctl->mc_current = 0;
    cur = &messages[mctl->mc_current];
    cur->m_no = mctl->mc_current;
    cur->m_flags = group;
    cur->m_time = 0;
    cur->m_recpt = recip;
    cur->m_from = 255; /* change 12/11/90 TC */
    (void) sprintf(cur->m_data, "%s", str);
    cur->m_flags |= MVALID;
}
