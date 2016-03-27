#include "copyright.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "alarm.h"
#include "ip.h"

pid_t pid;

#ifdef DNSBL_CHECK
#define XBLDOMAIN "xbl.spamhaus.org"
#define SORBSDOMAIN "dnsbl.sorbs.net"
#define NJABLDOMAIN "dnsbl.njabl.org"

static int xbltest(char *ip)
{
    char *ipbuf = strdup(ip);
    char **ipptr = &ipbuf;
    int ipf[4], i;
    char dnshost[256];

    for (i = 0; i < 4; i++)
        ipf[i] = atoi(strsep(ipptr, "."));
    sprintf(dnshost, "%d.%d.%d.%d.%s",
            ipf[3], ipf[2], ipf[1], ipf[0], XBLDOMAIN);
    free(ipbuf);

    return (int)(gethostbyname(dnshost) != 0);
}

static int njabltest(char *ip)
{
    char *ipbuf = strdup(ip);
    char **ipptr = &ipbuf;
    int ipf[4], i;
    char dnshost[256];
    struct hostent *njabl;

    for (i = 0; i < 4; i++)
        ipf[i] = atoi(strsep(ipptr, "."));
    sprintf(dnshost, "%d.%d.%d.%d.%s",
            ipf[3], ipf[2], ipf[1], ipf[0], NJABLDOMAIN);
    free(ipbuf);

    if ((njabl = gethostbyname(dnshost)) && (njabl->h_addr_list[0][3] == 9))
        return 1;
    else
        return 0;
}

static int sorbstest(char *ip)
{
    char *ipbuf = strdup(ip);
    char **ipptr = &ipbuf;
    int ipf[4], i;
    char dnshost[256];
    struct hostent *sorbs;
    int proxy = 0, proxyadd = 1;

    for (i = 0; i < 4; i++)
        ipf[i] = atoi(strsep(ipptr, "."));
    sprintf(dnshost, "%d.%d.%d.%d.%s",
            ipf[3], ipf[2], ipf[1], ipf[0], SORBSDOMAIN);
    free(ipbuf);

    if ((sorbs = gethostbyname(dnshost)))
        for (i = 0; sorbs->h_addr_list[i]; i++) {
            if ((sorbs->h_addr_list[i][3] >= 2) &&
                (sorbs->h_addr_list[i][3] <= 4)) {
                proxy |= proxyadd;
                proxyadd *= 2;
            }
            if (sorbs->h_addr_list[i][3] == 7)
                proxy |= 8;
        }
    return proxy;
}

static void dnsbl_lookup(char *ip, int *xblproxy, int *sorbsproxy,
                         int *njablproxy)
{
    if (xblproxy) {
        if ((*xblproxy = xbltest(ip)))
            ERROR(2,("DNSBL Hit (XBL): %s\n", ip));
    }
    
    if (sorbsproxy) {
        if ((*sorbsproxy = sorbstest(ip)))
            ERROR(2,("DNSBL Hit (SORBS): %s [%d]\n", ip, *sorbsproxy));
    }
    
    if (njablproxy) {
        if ((*njablproxy = njabltest(ip)))
            ERROR(2,("DNSBL Hit (NJABL): %s\n", ip));
    }    
}
#endif

void ip_lookup(char *ip, char *p_full_hostname, char *p_dns_hostname,
               int *xblproxy, int *sorbsproxy, int *njablproxy, int len)
{
    struct in_addr addr;
    struct hostent *reverse;
    struct hostent *forward;
    char fwdhost[MAXHOSTNAMESIZE];

    /* resolve the host name in a new process */
    pid = fork();
    if (pid != 0) return;

    /* ignore alarms */
    alarm_prevent_inheritance();

#ifdef DNSBL_CHECK
    dnsbl_lookup(ip, xblproxy, sorbsproxy, njablproxy);
#endif

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

    /* Resolve the IP from the FQDN resolved above and store the text
       string in fwdhost */
    if (ip_check_dns && (!(forward = gethostbyname(reverse->h_name)) ||
        !inet_ntop(AF_INET, forward->h_addr_list[0], fwdhost, MAXHOSTNAMESIZE)
        || strncmp(ip, fwdhost, len - 1))) {
        /* Display the IP in full_hostname and the resolved reverse in
            dns_hostname if the reverse does not forward resolve to an
            IP or if the forward resolution does not match the
            original IP */
        ERROR(3,("ip_to_full_hostname: forward lookup failed for %s (%s)\n",
                 reverse->h_name, ip));
        strncpy(p_full_hostname, ip, len - 1);
        strncpy(p_dns_hostname, reverse->h_name, MAXHOSTNAMESIZE - 1);
        _exit(0);
    }
    /* if this test works, then DNS is set up correctly */

    /* Display the resolved reverse in both full_hostname and
       dns_hostname if the resolved forward matches or if IP_CHECK_DNS
       is disabled */
    strncpy(p_full_hostname, reverse->h_name, MAXHOSTNAMESIZE - 1);
    strncpy(p_dns_hostname, reverse->h_name, MAXHOSTNAMESIZE - 1);
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

int flag_test_cidr(char *name, char *ip)
{
    DIR *dp;
    struct dirent *ep;
    char *str;
    in_addr_t host, entry;
    int mask;
    
    if (!(dp = opendir(name)))
        return 0;
    
    host = inet_addr(ip);
    while ((ep = readdir(dp)))
    {
        if (!strcmp(ep->d_name, ip))
        {
            closedir(dp);
            return 1;
        }
        if (!(str = strtok(ep->d_name, "-")))
            continue;
        if (!(str = strtok(NULL, "-")))
            continue;
        if ((entry = inet_addr(ep->d_name)) == -1)
            continue;
        mask = atoi(str);
        if ((mask < 0) || (mask > 32))
            continue;
        if ((host << (32 - mask)) == (entry << (32 - mask)))
        {
            closedir(dp);
            return 1;
        }
    }
    closedir(dp);
    return 0;
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

/* denied parties list, whether to drop connection in newstartd */
int ip_deny(char *ip) {
    return flag_test_cidr(name_etc("deny", ""), ip);
}

/* denied parties list, add to, as a critical defense measure */
void ip_deny_set(char *ip) {
  flag_set(name_etc("deny", ip));
}

/* whitelist, whether to trust user more */
int ip_whitelisted(char *ip) {
  return flag_test_cidr(name_etc("whitelist", ""), ip);
}

/* hide, whether to hide ip address and host name */
int ip_hide(char *ip) {
  return flag_test_cidr(name_etc("hide", ""), ip);
}

/* duplicates, whether to allow multiple connections from this ip */
int ip_duplicates(char *ip) {
  return flag_test_cidr(name_etc("duplicates", ""), ip);
}

/* duplicates, allow them */
void ip_duplicates_set(char *ip) {
  flag_set(name_etc("duplicates", ip));
}

/* duplicates, deny them */
void ip_duplicates_clear(char *ip) {
  flag_clear(name_etc("duplicates", ip));
}

/* global mute, default others :ita to individual, team and all ignore */
int ip_mute(char *ip) {
  if (ip == NULL || ip[0] == '\0') return 0;
  return flag_test_cidr(name_etc("mute", ""), ip);
}

/* prevent user from sending messages to all, aka global :a */
int ip_muteall(char *ip) {
    if (ip == NULL || ip[0] == '\0') return 0;
    return flag_test_cidr(name_etc("muteall", ""), ip);
}

/* global ignore, default others :ita to individual ignore */
int ip_ignore(char *ip) {
  if (ip == NULL || ip[0] == '\0') return 0;
  return flag_test_cidr(name_etc("ignore", ""), ip);
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
    ignored[i] = mode;
  }
  if (ip_mute(me->p_ip))
      mute = 1;
  if (ip_muteall(me->p_ip))
      muteall = 1;
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
