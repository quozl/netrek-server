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

void ip_lookup(char *ip, char *p_full_hostname, char *p_dns_hostname, int len)
{
  /* resolve the host name in a new process */
  pid = fork();
  if (pid != 0) return;

  /* ignore alarms */
  alarm_prevent_inheritance();

  /* convert textual ip address to binary */
  struct in_addr addr;
  if (inet_aton(ip, &addr) == 0) {
    ERROR(2,("ip_to_full_hostname: numeric ip address not valid format %s\n", ip));
    strcpy(p_full_hostname, ip);
    strcpy(p_dns_hostname, ip);
    _exit(1);
  }

  /* lookup the fully qualified domain name using the address */
  struct hostent *hostent = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
  if (hostent == NULL) {
    ERROR(2,("ip_to_full_hostname: gethostbyaddr failed for %s\n", ip));
    strcpy(p_full_hostname, ip);
    strcpy(p_dns_hostname, ip);
    _exit(1);
  }

  /* set the address in shared memory */
  /* Display the IP if the forward and reverse DNS do not match */
  strcpy(p_dns_hostname, hostent->h_name);
  hostent = gethostbyname(p_dns_hostname);
  if (!hostent || strcmp(p_dns_hostname, hostent->h_name))
    strcpy(p_full_hostname, ip);
  else
    strcpy(p_full_hostname, p_dns_hostname);
  ERROR(3,("ip_to_full_hostname: %s resolved to %s\n", ip, p_full_hostname));
  _exit(0);
}

void ip_waitpid()
{
  waitpid(pid, NULL, 0);
}

static void flag_set(char *name)
{
  FILE *file = fopen(name, "w");
  if (file == NULL) return;
  fclose(file);
}

static void flag_clear(char *name)
{
  unlink(name);
}

static int flag_test(char *name)
{
  return (access(name, F_OK) == 0);
}

static char name[256];

static char *name_etc(char *prefix, char *ip)
{
  snprintf(name, 255, "%s/ip/%s/%s", SYSCONFDIR, prefix, ip);
  return name;
}

static char *name_var(char *prefix, char *ip)
{
  snprintf(name, 255, "%s/ip/%s/%s", LOCALSTATEDIR, prefix, ip);
  return name;
}

/* whitelist, whether to trust user more */
int ip_whitelisted(char *ip) {
  return flag_test(name_etc("whitelist", ip));
}

/* hide, whether to hide ip address and host name */
int ip_hide(char *ip) {
  return flag_test(name_etc("hide", ip));
}

/* global mute, default others :ita to individual, team and all ignore */
int ip_mute(char *ip) {
  if (ip == NULL) return 0;
  return flag_test(name_etc("mute", ip));
}

/* global ignore, default others :ita to individual ignore */
int ip_ignore(char *ip) {
  if (ip == NULL) return 0;
  return flag_test(name_etc("ignore", ip));
}

/* saved doosh message ignore state */

int ip_ignore_doosh(char *ip) {
  return flag_test(name_var("ignore/doosh", ip));
}

void ip_ignore_doosh_set(char *ip) {
  return flag_set(name_var("ignore/doosh", ip));
}

void ip_ignore_doosh_clear(char *ip) {
  return flag_clear(name_var("ignore/doosh", ip));
}

/* saved multiline macro ignore state */

int ip_ignore_multi(char *ip) {
  return flag_test(name_var("ignore/multi", ip));
}

void ip_ignore_multi_set(char *ip) {
  return flag_set(name_var("ignore/multi", ip));
}

void ip_ignore_multi_clear(char *ip) {
  return flag_clear(name_var("ignore/multi", ip));
}

/* saved ignore state by ip address of both parties */

static char *ip_ignore_ip_name(char *us, char *them) {
  snprintf(name, 255, "%s/ip/%s/%s-%s", 
           LOCALSTATEDIR, "ignore/by-ip", us, them);
  return name;
}

/* check saved ignore state */
int ip_ignore_ip(char *us, char *them) {
  if (us == NULL) return 0;
  if (them == NULL) return 0;
  char *name = ip_ignore_ip_name(us, them);
  FILE *file = fopen(name, "r");
  int mask = 0;
  if (file == NULL) return mask;
  fscanf(file, "%d\n", &mask);
  fclose(file);
  return mask;
}

/* update saved ignore state */
void ip_ignore_ip_update(char *us, char *them, int mask) {
  char *name = ip_ignore_ip_name(us, them);
  FILE *file = fopen(name, "w");
  fprintf(file, "%d\n", mask);
  fclose(file);
}

extern int ignored[];

/* called on login by me, to recall saved state */
void ip_ignore_initial(struct player *me)
{
  int i;
  for (i = 0; i < MAXPLAYER; i++) {
    if (players[i].p_status == PFREE) continue;
    if (i == me->p_no) continue;
    int mode = ip_ignore_ip(me->p_ip, players[i].p_ip);
    if (ip_ignore(players[i].p_ip)) { mode |= MALL; }
    if (ip_mute(players[i].p_ip)) { mode |= MALL | MTEAM | MINDIV; }
    ignored[i] = mode;
  }
}

/* called on login by others, to recall saved state */
void ip_ignore_login(struct player *me, struct player *pl)
{
  ignored[pl->p_no] = ip_ignore_ip(me->p_ip, pl->p_ip);

  if (ip_ignore(pl->p_ip)) {
    if (!(ignored[pl->p_no] & MALL)) {
      ignored[pl->p_no] |= MALL;
    }
  }

  if (ip_mute(pl->p_ip)) {
    if (!(ignored[pl->p_no] & (MALL | MTEAM | MINDIV))) {
      ignored[pl->p_no] |= MALL | MTEAM | MINDIV;
    }
  }
}
