#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "util.h"

int prior()
{
    char message[MSG_LEN + 20];
    char buf[MSG_LEN + 20];
    char *to;

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
	    amessage(message, *to-'0', MINDIV);
	    break;
	case 'a': case 'b': case 'c': case 'd':
	case 'e': case 'f': case 'g': case 'h':
	case 'i': case 'j': case 'k': case 'l':
	case 'm': case 'n': case 'o': case 'p':
	case 'q': case 'r': case 's': case 't':
	case 'u': case 'v': case 'w': case 'x':
	case 'y': case 'z':
	    printf("Message goes to player %c\n", *to);
	    amessage(message, *to-'a'+10, MINDIV);
	    break;
	case 'A':
	    printf("Message goes to ALL\n");
	    amessage(message, 0, MALL);
	    break;
	case 'K':
	    printf("Message goes to KLI\n");
	    amessage(message, KLI, MTEAM);
	    break;
	case 'R':
	    printf("Message goes to ROM\n");
	    amessage(message, ROM, MTEAM);
	    break;
	case 'O':
	    printf("Message goes to ORI\n");
	    amessage(message, ORI, MTEAM);
	    break;
	case 'F':
	    printf("Message goes to FED\n");
	    amessage(message, FED, MTEAM);
	    break;
	default: 
	    break;
	}
	to++;
    }
    return 1;
}

int main(int argc, char **argv)
{
    int i = 1;

    openmem(0);
    if (argc == 1) return prior();

    if (strcmp(argv[i], "forge-to-self")) return 1;
    if (++i == argc) return 1;

    struct player *me = player_by_number(argv[i]);
    if (me == NULL) return 1;
    if (++i == argc) return 1;

    pmessage2(me->p_no, MINDIV, "", me->p_no,
              " %s->%s   %s", me->p_mapchars, me->p_mapchars, argv[i]);
    return 0;
}
