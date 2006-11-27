/* 	$Id: newaccess.c,v 1.4 2006/05/08 08:50:21 quozl Exp $	 */

#ifdef lint
static char vcid[] = "$Id: newaccess.c,v 1.4 2006/05/08 08:50:21 quozl Exp $";
#endif /* lint */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <signal.h>

#include "defs.h"
#include INC_STRINGS
#include "struct.h"
#include "data.h"

/*#define	SUBNET*/
#define	LOG
#undef	SYSLOG

/*
 * host_access -- determine if the client has permission to play.
 *
 *	Parameters:	host a name of a host to check, or NULL
 *			if we should do a getpeername.
 *
 *	Returns:	1 if host can play.
 *			0 otherwise.
 *
 *	Side effects:	None.
 */

#ifdef LOG
char	peerhostname[256];
#endif

extern int fd;

static int str1in2(char *, char *);

int host_access(char *host)
{
    char		hostornet[256];
    char		playperm[256];
    char		host_name[256], net_name[256];
    char		line[256];
    register char	*cp;
    int			canplay;
    int			ncanplay = 0;
    int			netmatch = 0;
    int			count, sockt;
    in_addr_t		net_addr;
    struct netent	*np;
    struct sockaddr_in	addr;
    socklen_t		addrlen;
    struct hostent	*hp;
    FILE		*acs_fp;
#ifdef SUBNET
    char		snet_name[256];
    int			snetmatch;
    int			sncanplay;
    in_addr_t		snet_addr;
    static int		snet_gotconf = 0;
#endif

    /* added 11/24/90 TC */

    char            host2[32];
    char            pipebuf[100];
    FILE            *pipefp;
    int             connCount;
    void	    reaper();
    
    canplay = 0;


#ifdef SUBNET
    if (!snet_gotconf) {
	if (getifconf() < 0) {
	    return (1);
	}
	snet_gotconf = 1;
    }
#endif

    sockt = fileno(stdin);
    addrlen = sizeof (struct sockaddr_in);
    
#ifdef DEBUG
    return 1;
#endif
    
    if (getpeername(sockt, (struct sockaddr *) &addr, &addrlen) < 0) {
	if (isatty(sockt)) {
#ifdef LOG
	    (void) strcpy(peerhostname, "stdin");
#endif
	    canplay = 1;
	} else {
#ifdef LOG
	    (void) strcpy(peerhostname, "unknown");
#endif
	    canplay = 1;
	}
	return (canplay);
    }
    
    /* TODO: reduce the time delay caused by this */
    hp = gethostbyaddr((char *) &addr.sin_addr.s_addr, sizeof(U_LONG),
		       AF_INET);
    if (hp != NULL)
	(void) strcpy(host_name, hp->h_name);
    else
	(void) strcpy(host_name, inet_ntoa(addr.sin_addr));
    
#ifdef LOG
#ifdef notdef
    syslog(LOG_INFO, "%s connect\n", host_name);
#endif
    (void) strcpy(peerhostname, host_name);
#endif
    
    if (host) {
	hp = gethostbyname(host);
	if (hp) {
	    MCOPY(hp->h_addr, &addr.sin_addr, sizeof (hp->h_length));
	    (void) strcpy(host_name, host);
	}
    }
    
    for (cp = host_name; *cp; cp++)
	if (isupper(*cp))
	    *cp = tolower(*cp);
    
    /*
     * At this point, addr.sin_addr.s_addr is the address of
     * the host in network order.
     */
    
    net_addr = inet_netof(addr.sin_addr);	/* net_addr in host order */
    
    np = getnetbyaddr(net_addr, AF_INET);
    if (np != NULL)
	strcpy(net_name, np->n_name);
    else
	strcpy(net_name,inet_ntoa(*(struct in_addr *)&net_addr));
    
#ifdef SUBNET
    snet_addr = inet_snetof(addr.sin_addr.s_addr);
    if (snet_addr == 0)
	snet_name[0] = '\0';
    else {
	np = getnetbyaddr(snet_addr, AF_INET);
	if (np != NULL)
	    (void) strcpy(snet_name, np->n_name);
	else
	    (void) strcpy(snet_name,
			  inet_ntoa(*(struct in_addr *)&snet_addr));
    }
#endif
    
	/*
	 * So, now we have host_name and net_name.
	 * Our strategy at this point is:
	 *
	 * for each line, get the first word
	 *
	 *	If it matches "host_name", we have a direct
	 *		match; parse and return.
	 *
	 *	If it matches "snet_name", we have a subnet match;
	 *		parse and set flags.
	 *
	 *	If it matches "net_name", we have a net match;
	 *		parse and set flags.
	 *
	 *	If it matches the literal "default", note we have
	 *		a net match; parse.
	 */

    strcpy(host2, "2592  ");
    strncat(host2, host_name, 16);

    if ((pipefp = fopen(NoCount_File, "r")) != NULL) {
	fclose(pipefp);
    }
    else {
	strcpy(pipebuf, NETSTAT);
	
	SIGNAL(SIGCHLD, SIG_DFL);
	if ((pipefp = popen(pipebuf,"r"))!=NULL) {
	    connCount = 0;
	    while (fgets(pipebuf, 80, pipefp) != NULL) {
		if (str1in2(host2, pipebuf))
		    connCount++;
	    }
	    pclose(pipefp);
	    SIGNAL(SIGCHLD, reaper);
	    if (connCount > 1) {
		sprintf(pipebuf," Host %s has %d connections.\n", host_name, connCount);
		write(fd, pipebuf, strlen(pipebuf));
		return 0; /* You're outta here! */
	    }
	}
	else {
	    SIGNAL(SIGCHLD, reaper);
	    strcpy(pipebuf, " Warning: could not open pipe.\n");
	    write(fd, pipebuf, strlen(pipebuf));
	}
    }
    
    acs_fp = fopen(Access_File, "r");
    if (acs_fp == NULL) {
	return (1);
    }
    
    while (fgets(line, sizeof(line), acs_fp) != NULL) {
	if ((cp = INDEX(line, '\n')) != NULL)
	    *cp = '\0';
	if ((cp = INDEX(line, '#')) != NULL)
	    *cp = '\0';
	if (*line == '\0')
	    continue;
	
	count = sscanf(line, "%s %s", hostornet, playperm);
	
	if (count < 2)
	    continue;
	
	if (strcmp(hostornet, host_name) == 0) {
	    (void) fclose(acs_fp);
	    canplay = (playperm[0] == 'y' || playperm[0] == 'Y');
	    return (canplay);
	}
	    
#ifdef SUBNET
	if (strcmp(hostornet, snet_name) == 0) {
	    snetmatch = 1;
	    sncanplay = (playperm[0] == 'y' || playperm[0] == 'y');
	}
#endif
	
	if (strcmp(hostornet, net_name) == 0  ||
	    strcmp(hostornet, "default") == 0) {
	    netmatch = 1;
	    ncanplay = (playperm[0] == 'y' || playperm[0] == 'Y');
	}
    }
    
    (void) fclose(acs_fp);

    if (netmatch) {
	canplay = ncanplay;
    }
    
#ifdef SUBNET
    if (snetmatch) {
	canplay = sncanplay;
    }
#endif
    
    return (canplay);
}

static int str1in2(char *target, char *source)
{
    int len;
    char *fix;
    
    fix = source;
    while (*fix) {
	if (isupper(*fix)) *fix = tolower(*fix);
	fix++;
    }
    
    len = strlen(target);
    while (*source) {
	if (strncmp(source++, target, len) == 0)
	    return 1;
    }
    return 0;
}
