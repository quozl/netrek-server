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
#include "ip.h"

pid_t pid;

// TODO: set an alarm and die if no response in gethostbyaddr

void ip_lookup(char *ip, char *p_full_hostname, int len)
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
    strcpy(p_full_hostname, ip);
    _exit(1);
  }

  /* lookup the fully qualified domain name using the address */
  struct hostent *hostent = gethostbyaddr(&addr, sizeof(addr), AF_INET);
  if (hostent == NULL) {
    ERROR(2,("ip_to_full_hostname: gethostbyaddr failed for %s\n", ip));
    strcpy(p_full_hostname, ip);
    _exit(1);
  }

  /* set the address in shared memory */
  p_full_hostname[len-1] = '\0';
  strncpy(p_full_hostname, hostent->h_name, len-1);
  ERROR(3,("ip_to_full_hostname: %s resolved to %s\n", ip, p_full_hostname));
  _exit(0);
}

void ip_waitpid()
{
  waitpid(pid, NULL, 0);
}

static int ip_test(char *prefix, char *ip)
{
  char name[128];
  snprintf(name, 127, "%s/%s/%s", SYSCONFDIR, prefix, ip);
  return (access(name, F_OK) == 0);
}

/* whitelist, whether to trust user more */
int ip_whitelisted(char *ip) {
  return ip_test("whitelist", ip);
}

/* hide, whether to hide ip address and host name */
int ip_hide(char *ip) {
  return ip_test("hide", ip);
}

/* mute, default :ita to individual, team and all ignore */
int ip_mute(char *ip) {
  return ip_test("mute", ip);
}

/* ignore, default :ita to individual ignore */
int ip_ignore(char *ip) {
  return ip_test("ignore", ip);
}
