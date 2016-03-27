#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
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
#include "proto.h"

#define MAXLINELEN 128
#define MAXLINES 400
#define MAXRECIP 40

/* file scope prototypes */
static char *read_line(FILE *fp);

typedef int Boolean;

int main(argc, argv)
int argc;
char *argv[];
{
    char buf[MSG_LEN + 20], *messages[MAXLINES];
    char *to;
    int i=0, j=0, nmessages=0, nrecipients=0;
    int recipients[MAXRECIP], groups[MAXRECIP];
    Boolean endinput = 0;
    FILE *input=NULL;

    if (argc > 1) {
	if ((input = fopen(argv[1], "r")) == NULL) {
	    perror(argv[0]);
	    exit(1);
	}
    } else {
	if (stdin) {
	    input = stdin;
	} else {
	    fprintf(stderr, "No input!\n");
	    exit(1);
	}
    }

    openmem(0);
    printf("Who are you sending the message to? ");
    fgets(buf, MSG_LEN + 20, stdin);
    if (strlen(buf) > 0) buf[strlen(buf)-1] = '\0';
    to=buf;
    while ((*to!='\0') && j<MAXRECIP) {
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
	    recipients[j] = *to-'0';
	    groups[j] = MINDIV;
	    j++;
	    break;
	case 'a': case 'b': case 'c': case 'd':
	case 'e': case 'f': case 'g': case 'h':
	case 'i': case 'j': case 'k': case 'l':
	case 'm': case 'n': case 'o': case 'p':
	case 'q': case 'r': case 's': case 't':
	case 'u': case 'v': case 'w': case 'x':
	case 'y': case 'z':
	    printf("Message goes to player %c\n", *to);
	    recipients[j] = *to-'a'+10;
	    groups[j] = MINDIV;
	    j++;
	    break;
	case 'A':
	    printf("Message goes to ALL\n");
	    recipients[j] = 0;
	    groups[j] = MALL;
	    j++;
	    break;
	case 'K':
	    printf("Message goes to KLI\n");
	    recipients[j] = KLI;
	    groups[j] = MTEAM;
	    j++;
	    break;
	case 'R':
	    printf("Message goes to ROM\n");
	    recipients[j] = ROM;
	    groups[j] = MTEAM;
	    j++;
	    break;
	case 'O':
	    printf("Message goes to ORI\n");
	    recipients[j] = ORI;
	    groups[j] = MTEAM;
	    j++;
	    break;
	case 'F':
	    printf("Message goes to FED\n");
	    recipients[j] = FED;
	    groups[j] = MTEAM;
	    j++;
	    break;
	default: 
	    break;
	}
	to++;
    }
    nrecipients = j;
    j = 0;
    printf("Enter Message:\n");
    while (i<MAXLINES && !endinput) {
	messages[i] = read_line(input);
	if (!messages[i]) {
	    endinput = 1;
	    i--;
	} else {
	    i++;
	}
    }
    nmessages = i;
    i = 0;
    while (j < nrecipients) {
	while (i < nmessages) {
	    printf("%d: %s\n", i, messages[i]);
	    amessage(messages[i++], recipients[j], groups[j]);
	    sleep(1);
	}
	j++;
    }
    return 1;		/* satisfy lint */
}

/********************************************************************************
 * MAKE_STR()                                                                   *
 *                                                                              *
 * Summary:     Returns a pointer to a static memory rendition of the input     *
 *                 string.  If size is 0, only enough space to fit the string   *
 *                 is allocated.  The input size MUST be at least equal to      *
 *                 strlen(string).  The space for the terminating '\0' is       *
 *                 automatically added in both cases.                           *
 *                                                                              *
 ********************************************************************************/
char *make_str(string, size)
     char *string;
     int size;
{
    char *new_str = NULL;

    if ((size > 3000) || (size < 0)) {
	fprintf(stderr, "make_str:  Rediculous string size: %d for {%s}\n",
		size, string);
	return(NULL);
    } else {
	if (size == 0)
	   size = strlen(string);
	new_str = (char *) calloc(size+1, 1);
	return(strncpy(new_str, string, size));
    }
}

static char *read_line(FILE *fp)
{
    int i=0;
    char *buf=NULL;

    if ((fp != NULL) && (!feof(fp))) {
	buf = (char *) calloc(MAXLINELEN+1, 1);
	buf[i++] = fgetc(fp);
	while(!feof(fp) && (buf[i-1] != '\n') && (i<MAXLINELEN)) {
	    buf[i++] = fgetc(fp);
	}
	if (i == MAXLINELEN)
	    fprintf(stderr, "");
	if (i<MAXLINELEN && i>0) i--;
	buf[i] = '\0';  /* buf[i] == '\n', or EOF, or last char in buf */
	buf = (char *) realloc(buf, i+1);
    }
    return(buf);
}


