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

void ip_lookup(char *ip, char *p_full_hostname, char *p_dns_hostname, int len)
{
    struct in_addr addr;
    struct hostent *reverse;
#ifdef IP_CHECK_DNS
    struct hostent *forward;
    char fwdhost[MAXHOSTNAMESIZE];
#endif

    /* resolve the host name in a new process */
    pid = fork();
    if (pid != 0) return;

    /* ignore alarms */
    alarm_prevent_inheritance();

    /* Pack IP text string into binary */
    if (inet_aton(ip, &addr) == 0) {
        /* Display the IP in both full_hostname and dns_hostname on failure */
        ERROR(2,("ip_to_full_hostname: numeric ip address not valid format %s\n", ip));
        strncpy(p_full_hostname, ip, len - 1);
        strncpy(p_dns_hostname, ip, len - 1);
        _exit(1);
    }

    /* Resolve the FQDN from the IP address */
    reverse = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
    if (reverse == NULL) {
        /* Display the IP in both full_hostname and dns_hostname on failure */
        ERROR(2,("ip_to_full_hostname: reverse lookup failed for %s\n", ip));
        strncpy(p_full_hostname, ip, len - 1);
        strncpy(p_dns_hostname, ip, len - 1);
        _exit(1);
    }

#ifdef IP_CHECK_DNS
    /* Resolve the IP from the FQDN resolved above and store the text string in fwdhost */
    if (!(forward = gethostbyname(reverse->h_name)) || !inet_ntop(AF_INET, forward->h_addr_list[0], fwdhost, MAXHOSTNAMESIZE)
        || strncmp(ip, fwdhost, len - 1)) {
        /* Display the IP in full_hostname and the resolved reverse in dns_hostname if the reverse does not forward resolve to an IP
            or if the forward resolution does not match the original IP */
        ERROR(3,("ip_to_full_hostname: forward lookup failed for %s (%s)\n", reverse->h_name, ip));
        strncpy(p_full_hostname, ip, len - 1);
        strncpy(p_dns_hostname, reverse->h_name, MAXHOSTNAMESIZE - 1);
        /* This is a soft error */
        _exit(0);
    }
#endif

    /* Display the resolved reverse in both full_hostname and dns_hostname if the resolved forward matches
       or if IP_CHECK_DNS is disabled */
    strncpy(p_full_hostname, reverse->h_name, MAXHOSTNAMESIZE - 1);
    strncpy(p_dns_hostname, reverse->h_name, MAXHOSTNAMESIZE - 1);
    ERROR(3,("ip_to_full_hostname: %s resolved to %s\n", ip, p_full_hostname));
    /* DNS is actually set up correctly! (If we're using IP_CHECK_DNS, otherwise it may not be.) */
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
  if (ip == NULL || ip[0] == '\0') return 0;
  return flag_test(name_etc("mute", ip));
}

/* global ignore, default others :ita to individual ignore */
int ip_ignore(char *ip) {
  if (ip == NULL || ip[0] == '\0') return 0;
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
