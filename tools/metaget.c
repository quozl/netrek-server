/*
    $Id: metaget.c,v 1.2 2006/04/22 02:16:47 quozl Exp $

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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

int main (int argc, char *argv[])
{
  unsigned short port;
  char *host;
  int sock;
  struct sockaddr_in address;
  int len, stat;
  char buf[BUFSIZ];
  int timeout = 10;
  char *req = "?version=metaget";

  host = "metaserver.us.netrek.org";
  if (argc > 1) host = argv[1];
  port = 3521;
  if (argc > 2) port = atoi(argv[2]);

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) { perror("socket"); return 1; }

  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  if ((address.sin_addr.s_addr = inet_addr(host)) == -1) {
    struct hostent *hp;
    if ((hp = gethostbyname(host)) == NULL) {
      herror("gethostbyname");
      return 2;
    } else {
      address.sin_addr.s_addr = *(long *) hp->h_addr;
    }
  }

  /* send query */
  stat = sendto(sock, req, strlen(req), 0,
                (struct sockaddr *) &address, sizeof(struct sockaddr));
  if (stat < 0) { perror("sendto"); return 3; }

  /* optional premature exit */
  if (argc > 3) {
    if (!strcmp(argv[3], "no-wait")) return 0;
  }

  /* optional alarm suppression */
  if (argc > 3) {
    if (!strcmp(argv[3], "no-timeout")) timeout = 0;
  }
  alarm(timeout);

  /* wait for response */
  len = recvfrom(sock, buf, BUFSIZ, 0, NULL, NULL);
  if (len < 0) { perror("recvfrom"); return 4; }
  if (len == 0) {
    fprintf(stderr, "%s: zero length response received\n", argv[0]);
    return 5;
  }
  
  /* display response */
  stat = write(STDOUT_FILENO, buf, len);
  if (stat < 0) { perror("write"); return 6; }
  
  close(sock);
  return 0;
}
