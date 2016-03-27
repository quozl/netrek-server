#include <ctype.h>
#include "salt.h"

#define DEFAULT 'X'

/* Valid salt chars are [A-Za-z0-9/.] */
static char saltchar(char ch)
{
    switch (ch) {
    case '/':
    case '.':
	return ch;
    default:
	if (isalnum(ch)) return ch;
	else return DEFAULT;
    }
}

/* Sets sb to a valid salt argument for crypt(), based on s. */
char* salt(const char* s, saltbuf sb)
{
    int i;
    int end = 0;
    int saltlen = sizeof(sb) - 2;
    
    for (i=0; i<saltlen; i++) {
	if (!end) end = (*s == '\0');
	if (end) {
	    sb[i] = DEFAULT;
	} else {
	    sb[i] = saltchar(*s);
	    s++;
	}
    }
    sb[saltlen] = '\0';
    return sb;
}
