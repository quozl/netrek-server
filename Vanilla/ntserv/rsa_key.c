#ifndef lint
#ifdef RSA

/* rsa_key.c
 * 
 * Mike Polek   11/92
 */
#include "copyright2.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#include <signal.h>
#include "defs.h"
#include INC_STRINGS
#include INC_SYS_FCNTL
#include "struct.h"
#include "data.h"
#include "packets.h"

makeRSAPacket(packet)
struct rsa_key_spacket *packet;
{
    int i;

    for (i=0; i<KEY_SIZE; i++)
	packet->data[i]=random() % 256;
    /* make sure it is less than the global key */
    for (i=KEY_SIZE-10; i < KEY_SIZE; i++)
	packet->data[i] = 0; 
    packet->type = SP_RSA_KEY;
}

/* returns 1 if the user verifies incorrectly */
decryptRSAPacket(spacket, cpacket, serverName)
struct rsa_key_spacket *spacket;
struct rsa_key_cpacket *cpacket;
char *serverName;
{
    struct rsa_key key;
    struct sockaddr_in saddr;
    u_char temp[KEY_SIZE], *data;
#ifdef SHOW_RSA
    char format[MSG_LEN];
#endif
    int fd;
    FILE *logfile;
    int done, found, curtime, len;
    int foo;
    int total;

/*    SIGNAL(SIGALRM, SIG_IGN);*/

    len = sizeof(saddr);
    if (getsockname(sock, &saddr, &len) < 0) {
        perror("getsockname(sock)");
        exit(1);
    }
 
    /* replace the first few bytes of the message */
    /* will be the low order bytes of the number */
    data = spacket->data;
    MCOPY (&saddr.sin_addr.s_addr, data, sizeof(saddr.sin_addr.s_addr));
    data += sizeof(saddr.sin_addr.s_addr);
    MCOPY (&saddr.sin_port, data, sizeof(saddr.sin_port));

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
	if (! (MCMP(key.global, cpacket->global, KEY_SIZE) ||
	       MCMP(key.public, cpacket->public, KEY_SIZE)))
	    done = found = 1;
    } while (! done);
    
    close(fd);

/*    SIGNAL(SIGALRM, SIG_IGN);*/

    /* If he wasn't in the file, kick him out */

    if (! found) {
	return 1;
    }

    rsa_encode(temp, cpacket->resp, key.public, key.global, KEY_SIZE);

    /* If we don't get the right answer, kick him out */
    if (MCMP(temp, spacket->data, KEY_SIZE))
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

#ifdef FEATURES
#ifdef FEATURE_PACKETS
    if (!F_client_feature_packets)
#endif
      TellClient(key.client_type); /* 6/12/93 LAB */
#endif

    return 0;
}
#endif	/* RSA */
#endif  /* lint */
