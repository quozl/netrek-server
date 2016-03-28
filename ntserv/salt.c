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
    char *out_p = &sb[0];
    char *out_end = &sb[SALT_LEN];

    while (out_p < out_end) {
        char c = *s++;

        if (c == '\0') {
            /* String ended early. Pad with DEFAULT. */
            do {
                *out_p++ = DEFAULT;
            } while (out_p < out_end);
            break;
        }

        *out_p++ = saltchar(c);
    }

    *out_end = '\0';

    return sb;
}
