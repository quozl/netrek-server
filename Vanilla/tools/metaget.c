/*
    $Id: metaget.c,v 1.1 2006/01/14 00:48:08 quozl Exp $

    metaget, query metaserver
    Copyright (C) 2005 James Cameron <quozl@us.netrek.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef lint
static char vcid[] = "$Id: metaget.c,v 1.1 2006/01/14 00:48:08 quozl Exp $";
#endif /* lint */

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

int main (int argc, char *argv[])
{
  unsigned short port;
  char *host;
  int sock;
  struct sockaddr_in address;
  int len, stat;
  char buf[BUFSIZ];

  host = "metaserver.us.netrek.org";
  if (argc > 1) host = argv[1];
  port = 3521;
  if (argc > 2) port = atoi(argv[2]);

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) { perror("socket"); exit(1); }

  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  if ((address.sin_addr.s_addr = inet_addr(host)) == -1) {
    struct hostent *hp;
    if ((hp = gethostbyname(host)) == NULL) {
      herror("gethostbyname");
      exit(2);
    } else {
      address.sin_addr.s_addr = *(long *) hp->h_addr;
    }
  }

  /* send query */
  stat = sendto(sock, "?", 1, 0, (struct sockaddr *) &address, 
		sizeof(struct sockaddr));
  if (stat < 0) { perror("sendto"); exit(3); }
    
  /* wait for response */
  len = recvfrom(sock, buf, BUFSIZ, 0, NULL, NULL);
  if (len < 0) { perror("recvfrom"); exit(4); }
  if (len == 0) {
    fprintf(stderr, "%s: zero length response received\n", argv[0]);
    exit(5);
  }
  
  /* display response */
  stat = write(STDOUT_FILENO, buf, len);
  if (stat < 0) { perror("write"); exit(6); }
  
  close(sock);
}
