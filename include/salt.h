#ifndef SALT_H
#define SALT_H

/* Salt is a two-character string. */
#define SALT_LEN 2

/* Salt buffers contain the two-character string followed by '\0'. */
typedef char saltbuf[SALT_LEN + 1];

char* salt(const char*, saltbuf);

#endif /* SALT_H */
