#ifndef SALT_H
#define SALT_H

typedef char saltbuf[3];

char* salt(const char*, saltbuf);

#endif /* SALT_H */
