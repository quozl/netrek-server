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
#include <ctype.h>
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

static int format_dnsbl_host(const char *ip, const char *domain,
                             char *dnshost, size_t size)
{
    struct in_addr in;
    unsigned char *b;

    if (!inet_aton(ip, &in))
        return 0;
    b = (unsigned char *) &in.s_addr;
    snprintf(dnshost, size, "%u.%u.%u.%u.%s",
             b[3], b[2], b[1], b[0], domain);
    return 1;
}

static int xbltest(char *ip)
{
    char dnshost[256];

    if (!format_dnsbl_host(ip, XBLDOMAIN, dnshost, sizeof(dnshost)))
        return 0;

    return (int)(gethostbyname(dnshost) != 0);
}

static int njabltest(char *ip)
{
    char dnshost[256];
    struct hostent *njabl;

    if (!format_dnsbl_host(ip, NJABLDOMAIN, dnshost, sizeof(dnshost)))
        return 0;

    if ((njabl = gethostbyname(dnshost)) && njabl->h_addr_list &&
        njabl->h_addr_list[0] && (unsigned char) njabl->h_addr_list[0][3] == 9)
        return 1;
    else
        return 0;
}

static int sorbstest(char *ip)
{
    int i;
    char dnshost[256];
    struct hostent *sorbs;
    int proxy = 0, proxyadd = 1;

    if (!format_dnsbl_host(ip, SORBSDOMAIN, dnshost, sizeof(dnshost)))
        return 0;

    if ((sorbs = gethostbyname(dnshost)) && sorbs->h_addr_list)
        for (i = 0; sorbs->h_addr_list[i]; i++) {
            unsigned char last = (unsigned char) sorbs->h_addr_list[i][3];
            if ((last >= 2) && (last <= 4)) {
                proxy |= proxyadd;
                proxyadd *= 2;
            }
            if (last == 7)
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

/* Copy a host name into dst, truncating to the buffer and replacing any
   character that is not valid in a host name.  Reverse DNS names are
   controlled by the client, so this keeps them safe to display and to pass
   to helper programs, and guarantees NUL termination even when the source
   is longer than the buffer. */
static void copy_hostname(char *dst, const char *src, size_t size)
{
    size_t i;

    if (size == 0)
        return;
    for (i = 0; i + 1 < size && src[i] != '\0'; i++) {
        unsigned char c = (unsigned char) src[i];
        if (isalnum(c) || c == '.' || c == '-' || c == '_' || c == ':')
            dst[i] = c;
        else
            dst[i] = '_';
    }
    dst[i] = '\0';
}

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
        copy_hostname(p_full_hostname, ip, len);
        copy_hostname(p_dns_hostname, ip, len);
        _exit(1);
    }

    /* Resolve the FQDN from the IP address */
    reverse = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
    if (reverse == NULL) {
        /* Display the IP in both full_hostname and dns_hostname on failure */
        ERROR(2,("ip_to_full_hostname: reverse lookup failed for %s\n", ip));
        copy_hostname(p_full_hostname, ip, len);
        copy_hostname(p_dns_hostname, ip, len);
        _exit(1);
    }

    /* Resolve the IP from the FQDN resolved above and store the text
       string in fwdhost */
    if (ip_check_dns && (!(forward = gethostbyname(reverse->h_name)) ||
        !forward->h_addr_list || !forward->h_addr_list[0] ||
        !inet_ntop(AF_INET, forward->h_addr_list[0], fwdhost, sizeof(fwdhost))
        || strncmp(ip, fwdhost, len > 0 ? len - 1 : 0))) {
        /* Display the IP in full_hostname and the resolved reverse in
            dns_hostname if the reverse does not forward resolve to an
            IP or if the forward resolution does not match the
            original IP */
        ERROR(3,("ip_to_full_hostname: forward lookup failed for %s (%s)\n",
                 reverse->h_name, ip));
        copy_hostname(p_full_hostname, ip, len);
        copy_hostname(p_dns_hostname, reverse->h_name, len);
        _exit(0);
    }
    /* if this test works, then DNS is set up correctly */

    /* Display the resolved reverse in both full_hostname and
       dns_hostname if the resolved forward matches or if IP_CHECK_DNS
       is disabled */
    copy_hostname(p_full_hostname, reverse->h_name, len);
    copy_hostname(p_dns_hostname, reverse->h_name, len);
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
    struct in_addr host_in, entry_in;
    uint32_t host, entry;
    char entry_buf[256];
    char *dash, *endptr;
    long mask;

    if (ip == NULL || !inet_aton(ip, &host_in))
        return 0;
    if (!(dp = opendir(name)))
        return 0;

    host = ntohl(host_in.s_addr);
    while ((ep = readdir(dp)))
    {
        if (!strcmp(ep->d_name, ip))
        {
            closedir(dp);
            return 1;
        }
        strncpy(entry_buf, ep->d_name, sizeof(entry_buf) - 1);
        entry_buf[sizeof(entry_buf) - 1] = '\0';
        dash = strchr(entry_buf, '-');
        if (!dash || dash[1] == '\0')
            continue;
        *dash = '\0';
        if (!inet_aton(entry_buf, &entry_in))
            continue;
        mask = strtol(dash + 1, &endptr, 10);
        if (*endptr != '\0' || mask < 0 || mask > 32)
            continue;
        entry = ntohl(entry_in.s_addr);
        if (mask == 0 || (host >> (32 - mask)) == (entry >> (32 - mask)))
        {
            closedir(dp);
            return 1;
        }
    }
    closedir(dp);
    return 0;
}

static int valid_path_component(const char *s)
{
  if (s == NULL)
    return 0;
  if (strchr(s, '/') != NULL)
    return 0;
  if (strstr(s, "..") != NULL)
    return 0;
  return 1;
}

static int build_etc_path(char *dst, size_t size,
                          const char *prefix, const char *ip)
{
  int n;
  if (!valid_path_component(ip))
    return 0;
  n = snprintf(dst, size, "%s/ip/%s/%s", SYSCONFDIR, prefix, ip);
  return n >= 0 && (size_t) n < size;
}

static int build_var_path(char *dst, size_t size,
                          const char *prefix, const char *ip)
{
  int n;
  if (!valid_path_component(ip))
    return 0;
  n = snprintf(dst, size, "%s/ip/%s/%s", LOCALSTATEDIR, prefix, ip);
  return n >= 0 && (size_t) n < size;
}

/* denied parties list, whether to drop connection in newstartd */
int ip_deny(char *ip) {
    char path[256];
    if (!build_etc_path(path, sizeof(path), "deny", "")) return 0;
    return flag_test_cidr(path, ip);
}

/* denied parties list, add to, as a critical defense measure */
void ip_deny_set(char *ip) {
  char path[256];
  if (!build_etc_path(path, sizeof(path), "deny", ip)) return;
  flag_set(path);
}

/* whitelist, whether to trust user more */
int ip_whitelisted(char *ip) {
  char path[256];
  if (!build_etc_path(path, sizeof(path), "whitelist", "")) return 0;
  return flag_test_cidr(path, ip);
}

/* hide, whether to hide ip address and host name */
int ip_hide(char *ip) {
  char path[256];
  if (!build_etc_path(path, sizeof(path), "hide", "")) return 0;
  return flag_test_cidr(path, ip);
}

/* duplicates, whether to allow multiple connections from this ip */
int ip_duplicates(char *ip) {
  char path[256];
  if (!build_etc_path(path, sizeof(path), "duplicates", "")) return 0;
  return flag_test_cidr(path, ip);
}

/* duplicates, allow them */
void ip_duplicates_set(char *ip) {
  char path[256];
  if (!build_etc_path(path, sizeof(path), "duplicates", ip)) return;
  flag_set(path);
}

/* duplicates, deny them */
void ip_duplicates_clear(char *ip) {
  char path[256];
  if (!build_etc_path(path, sizeof(path), "duplicates", ip)) return;
  flag_clear(path);
}

/* global mute, default others :ita to individual, team and all ignore */
int ip_mute(char *ip) {
  char path[256];
  if (ip == NULL || ip[0] == '\0') return 0;
  if (!build_etc_path(path, sizeof(path), "mute", "")) return 0;
  return flag_test_cidr(path, ip);
}

/* prevent user from sending messages to all, aka global :a */
int ip_muteall(char *ip) {
    char path[256];
    if (ip == NULL || ip[0] == '\0') return 0;
    if (!build_etc_path(path, sizeof(path), "muteall", "")) return 0;
    return flag_test_cidr(path, ip);
}

/* global ignore, default others :ita to individual ignore */
int ip_ignore(char *ip) {
  char path[256];
  if (ip == NULL || ip[0] == '\0') return 0;
  if (!build_etc_path(path, sizeof(path), "ignore", "")) return 0;
  return flag_test_cidr(path, ip);
}

/* saved doosh message ignore state */

int ip_ignore_doosh(char *ip) {
  char path[256];
  if (!build_var_path(path, sizeof(path), "ignore/doosh", ip)) return 0;
  return flag_test(path);
}

void ip_ignore_doosh_set(char *ip) {
  char path[256];
  if (!build_var_path(path, sizeof(path), "ignore/doosh", ip)) return;
  flag_set(path);
}

void ip_ignore_doosh_clear(char *ip) {
  char path[256];
  if (!build_var_path(path, sizeof(path), "ignore/doosh", ip)) return;
  flag_clear(path);
}

/* saved multiline macro ignore state */

int ip_ignore_multi(char *ip) {
  char path[256];
  if (!build_var_path(path, sizeof(path), "ignore/multi", ip)) return 0;
  return flag_test(path);
}

void ip_ignore_multi_set(char *ip) {
  char path[256];
  if (!build_var_path(path, sizeof(path), "ignore/multi", ip)) return;
  flag_set(path);
}

void ip_ignore_multi_clear(char *ip) {
  char path[256];
  if (!build_var_path(path, sizeof(path), "ignore/multi", ip)) return;
  flag_clear(path);
}

/* saved ignore state by ip address of both parties */

static int build_ignore_ip_path(char *dst, size_t size,
                                const char *us, const char *them)
{
  int n;
  if (!valid_path_component(us) || !valid_path_component(them) ||
      us[0] == '\0' || them[0] == '\0')
    return 0;
  n = snprintf(dst, size, "%s/ip/%s/%s-%s",
               LOCALSTATEDIR, "ignore/by-ip", us, them);
  return n >= 0 && (size_t) n < size;
}

/* check saved ignore state */
int ip_ignore_ip(char *us, char *them) {
  char path[256];
  if (us == NULL) return 0;
  if (them == NULL) return 0;
  if (!build_ignore_ip_path(path, sizeof(path), us, them)) return 0;
  FILE *file = fopen(path, "r");
  int mask = 0;
  if (file == NULL) return mask;
  if (fscanf(file, "%d\n", &mask) == EOF)
    if (ferror(file))
      perror("ip_ignore_ip");
  fclose(file);
  return mask;
}

/* update saved ignore state */
void ip_ignore_ip_update(char *us, char *them, int mask) {
  char path[256];
  if (us == NULL || them == NULL) return;
  if (!build_ignore_ip_path(path, sizeof(path), us, them)) return;
  FILE *file = fopen(path, "w");
  if (file == NULL) return;
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
