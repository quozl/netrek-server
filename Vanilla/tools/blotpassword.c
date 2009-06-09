/* Takes the output of scores A and generates an equivalent scores
 *  data file which has all the passwords blotted out with X.  */

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

#include "proto.h"

#define MAXBUFFER 512

int
main(int argc, char **argv)
{
    struct statentry play_entry;
    const unsigned PASSWORD_START = 17;
    const unsigned PASSWORD_LEN = sizeof (play_entry.password);
    char buf[MAXBUFFER];
    unsigned killpass;
    int is_first_line = 1;
    
    while (fgets (buf, sizeof (buf), stdin)) {
	buf[sizeof (buf) - 1] = '\0';
	if (is_first_line)
	    is_first_line = 0;
	else
	    for (killpass = 0; killpass < PASSWORD_LEN; killpass++)
		buf[PASSWORD_START + killpass] = 'X';
	fputs (buf, stdout);
    }

    return 0;
}
