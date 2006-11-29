#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "copyright.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "alarm.h"

pid_t pid;

void ip_lookup(char *ip, char *p_full_hostname)
{
  /* resolve the host name in a new process */
  pid = fork();
  if (pid != 0) return;

  /* ignore alarms */
  alarm_prevent_inheritance();

  /* lower priority */
  nice(1);

  /* convert textual ip address to binary */
  struct in_addr addr;
  if (inet_aton(ip, &addr) == 0) {
    ERROR(2,("ip_to_full_hostname: numeric ip address not valid format %s\n", ip));
    strcpy(p_full_hostname, "unknown");
    _exit(1);
  }

  /* lookup the fully qualified domain name using the address */
  struct hostent *hostent = gethostbyaddr(&addr, sizeof(addr), AF_INET);
  if (hostent == NULL) {
    ERROR(2,("ip_to_full_hostname: gethostbyaddr failed for %s\n", ip));
    strcpy(p_full_hostname, "unknown");
    _exit(1);
  }

  /* set the address in shared memory */
  strcpy(p_full_hostname, hostent->h_name);
  ERROR(3,("ip_to_full_hostname: %s resolved to %s\n", ip, p_full_hostname));
  _exit(0);
}

void ip_waitpid()
{
  waitpid(pid, NULL, 0);
}
