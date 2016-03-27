#ifndef lint
#ifdef RSA

/* rsa_key.c
 * 
 * Mike Polek   11/92
 */
#include "copyright2.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#include <signal.h>
#include <string.h>
#include <sys/fcntl.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"

/* from res-rsa/rsa_encode.c */
void rsa_encode(unsigned char *out, unsigned char *message,
                unsigned char *key, unsigned char *global, const int digits);

void makeRSAPacket(void *p)
{
    struct rsa_key_spacket *packet = (struct rsa_key_spacket *) p;
    int i;

    for (i=0; i<KEY_SIZE; i++)
	packet->data[i]=random() % 256;
    /* make sure it is less than the global key */
    for (i=KEY_SIZE-10; i < KEY_SIZE; i++)
	packet->data[i] = 0; 
    packet->type = SP_RSA_KEY;
}

/* returns 1 if the user verifies incorrectly */
int decryptRSAPacket (void *s,
		      void *c,
		      char *serverName)
{
    struct rsa_key_spacket *spacket = (struct rsa_key_spacket *) s;
    struct rsa_key_cpacket *cpacket = (struct rsa_key_cpacket *) c;
    struct rsa_key key;
    struct sockaddr_in saddr;
    socklen_t addrlen;
    u_char temp[KEY_SIZE], *data;
#ifdef SHOW_RSA
    char format[MSG_LEN];
#endif
    int fd;
    FILE *logfile;
    int done, found;
    time_t curtime;

/*    signal(SIGALRM, SIG_IGN);*/

    addrlen = sizeof(saddr);
    if (getsockname(sock, (struct sockaddr *) &saddr, &addrlen) < 0) {
        perror("getsockname(sock)");
        exit(1);
    }
 
    /* replace the first few bytes of the message */
    /* will be the low order bytes of the number */
    data = spacket->data;
    memcpy(data, &saddr.sin_addr.s_addr, sizeof(saddr.sin_addr.s_addr));
    data += sizeof(saddr.sin_addr.s_addr);
    memcpy(data, &saddr.sin_port, sizeof(saddr.sin_port));

    fd = open(RSA_Key_File, O_RDONLY);
    if (fd < 0)
    {
	ERROR(1,("Can't open the RSA key file %s\n", RSA_Key_File));
	exit(1);
    }

    done = found = 0;
    do 
    {
	if (read(fd, &key, sizeof(struct rsa_key)) != sizeof(struct rsa_key))
	    done = 1;
	if (! (memcmp(key.global, cpacket->global, KEY_SIZE) ||
	       memcmp(key.public, cpacket->public, KEY_SIZE)))
	    done = found = 1;
    } while (! done);
    
    close(fd);

/*    signal(SIGALRM, SIG_IGN);*/

    /* If he wasn't in the file, kick him out */

    if (! found) {
	return 1;
    }

    rsa_encode(temp, cpacket->resp, key.public, key.global, KEY_SIZE);

    /* If we don't get the right answer, kick him out */
    if (memcmp(temp, spacket->data, KEY_SIZE))
	return 1;
    logfile=fopen(LogFileName, "a");
    if (!logfile) return 0;

    curtime=time(NULL);

#ifdef SHOW_RSA
    sprintf (format, "Client: %%.%ds. Arch: %%.%ds. Player: %%s.\n\tLogin: %%s. at %%s",
                       KEY_SIZE, KEY_SIZE);
#endif
    fprintf (logfile, format,
             key.client_type,
             key.architecture,
             me->p_name,
             me->p_login,
	     ctime(&curtime));

    fclose(logfile);

#ifdef SHOW_RSA
    /* add rsa type to message log */
    sprintf(format,"%%s --> %%.%ds.--- %%.%ds.",
                KEY_SIZE, KEY_SIZE);

    pmessage(0,MALL | MJOIN," GOD->ALL",format,
	me->p_name,key.client_type,key.architecture);

#endif

    sprintf(RSA_client_type, "%s / %s",         /* LAB 4/1/93 */
            key.client_type, key.architecture);
    RSA_client_type[KEY_SIZE * 2] = '\0';

    return 0;
}
#endif	/* RSA */
#endif  /* lint */
