/* reserved.c
 * 
 * Kevin P. Smith   7/3/89
 */
#include "copyright2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "packets.h"

makeReservedPacket(packet)
struct reserved_spacket *packet;
{
    int i;

    for (i=0; i<16; i++) {
	packet->data[i]=random() % 256;
    }
    packet->type = SP_RESERVED;
}

encryptReservedPacket(spacket, cpacket, server, pno)
struct reserved_spacket *spacket;
struct reserved_cpacket *cpacket;
char *server;
int pno;
{
    struct hostent *hp;
    struct in_addr address;
    unsigned char mixin1, mixin2, mixin3, mixin4, mixin5;
    register int i,j,k;
    char buf[16];
    unsigned char *s;
    /*
    extern int	reserved;
    */
    
    bcopy(spacket->data, cpacket->data, 16);
    bcopy(spacket->data, cpacket->resp, 16);
    cpacket->type=CP_RESERVED;

    if ((address.s_addr = inet_addr(server)) == -1) {
	if ((hp = gethostbyname(server)) == NULL) {
	    fprintf(stderr, "I don't know any %s!\n", server);
	    exit(1);
	} else {
	    address.s_addr = *(long *) hp->h_addr;
	}
    }

/*
    mixin1 = address.s_net;
    mixin2 = pno;
    mixin3 = address.s_host;
    mixin4 = address.s_lh;
    mixin5 = address.s_impno;
*/

    /* Now you've got 5 random bytes to play with (mixin[1-5]), to 
     *  help in coming up with an encryption of your data.
     */

    /* Encryption algorithm goes here.
     * Take the 16 bytes in cpacket->data, and create cpacket->resp,
     *   which you require the client to also do.  If he fails, he
     *   gets kicked out.
     */
   
   /*
   reserved = 1;
   */
   printf("Restricted server.\n");

   return;
}
