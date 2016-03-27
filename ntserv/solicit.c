#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>

#include "defs.h"
#include "struct.h"
#include "data.h"
#include "solicit.h"
#include "config.h"
#include "util.h"
#include "slotmaint.h"

/* our copy of metaservers file */
static struct metaserver metaservers[MAXMETASERVERS];

/* initialisation done flag */
static int initialised = 0;

/* remove non-printable chars in a string.  Lifted from tools/players.c */
/* with a minor addition:  error checking on the strdup */
static char *name_fix(char *name)
{
  char *new = strdup(name);                    /* xx, never freed */
  register
    char *r = new;

  if(!new) return new;                         /* don't play with null ptr */

  while(*name) {
    *r++ = (*name <= 32)?'_':*name;
    name++;
  }
  *r = 0;
  return new;
}

/* attach to a metaserver, i.e. prepare the socket */
static int udp_attach(struct metaserver *m)
{
  char *ours = m->ours;

  /* create the socket structure */
  m->sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (m->sock < 0) {
    perror("solicit: udp_attach: socket");
    return 0;
  }

  /* bind the local socket */
  /* bind to interface attached to this.host.name */

  /* first check if perhaps it is a fake INL server address */
  if ((tolower(m->type[0]) == 'i') &&
      (strncasecmp(m->ours, "home.", 5) == 0 ||
       strncasecmp(m->ours, "away.", 5) == 0)) {
    ours += 5;
  }

#ifdef SOLICIT_BIND_ANY
  m->address.sin_addr.s_addr = inet_addr("0.0.0.0");
#else
  /* numeric first */
  if ((m->address.sin_addr.s_addr = inet_addr(ours)) == -1) {
    struct hostent *hp;
    /* then name */
    if ((hp = gethostbyname(ours)) == NULL) {
      /* bad hostname or unable to get ip address */
      ERROR(1,("Bad host name field in metaservers file.\n"));
      return 0;
    } else {
      memcpy(&(m->address.sin_addr.s_addr), hp->h_addr, 4);
    }
  }
#endif

  m->address.sin_family      = AF_INET;
  m->address.sin_port        = 0;
  if (bind(m->sock,(struct sockaddr *)&m->address, sizeof(m->address)) < 0) {
    perror("solicit: udp_attach: unable to bind to desired interface (this.host.name field)");
    return 0;
  }

  /* build the destination address */
  m->address.sin_family = AF_INET;
  m->address.sin_port = htons(m->port);

  /* attempt numeric translation first */
  if ((m->address.sin_addr.s_addr = inet_addr(m->host)) == -1) {
    struct hostent *hp;

    /* then translation by name */
    if ((hp = gethostbyname(m->host)) == NULL) {
      /* if it didn't work, return failure and warning */
      ERROR(1,("solicit: udp_attach: metaserver %s not resolved\n", m->host));
      return 0;
    }
    memcpy(&(m->address.sin_addr.s_addr), hp->h_addr, 4);
  }

  return 1;
}

static void udp_detach(struct metaserver *m)
{
  close(m->sock);
}

/* transmit a packet to the metaserver */
static int udp_tx(struct metaserver *m, char *buffer, int length)
{
  /* send the packet */
  ERROR(7,("solicit: udp_tx: sendto (size:%d)\n", length));
  if (sendto(m->sock, buffer, length, 0, (struct sockaddr *)&m->address,
             sizeof(m->address)) < 0) {
    perror("solicit: udp_tx: sendto");
    return 0;
  }

  return 1;
}

static struct stat oldstat;

static void initialise()
{
  int i;
  FILE *file;

  /* clear metaserver socket list */
  for (i=0; i<MAXMETASERVERS; i++) metaservers[i].sock = -1;

  /* open the metaserver list file */
  file = fopen(SYSCONFDIR"/metaservers", "r");
  if (file == NULL) file = fopen(SYSCONFDIR"/.metaservers", "r");
  if (file == NULL) {
    initialised++;
    return;
  }
  stat(SYSCONFDIR"/metaservers", &oldstat);

  /* read the metaserver list file */
  for (i=0; i<MAXMETASERVERS; i++) {
    struct metaserver *m = &metaservers[i];
    char buffer[256];         /* where to hold the metaservers line */
    char *line;               /* return from fgets() */
    char *token;              /* current line token */

    /* read a line */
    line = fgets(buffer, 256, file);

    /* if end of file reached, stop */
    if (line == NULL) break;
    if (feof(file)) break;

    /* ignore comments */
    if (line[0] == '#') continue;

    /* parse each field, ignore the line if insufficient fields found */

    token = strtok(line, " ");        /* meta host name */
    if (token == NULL) continue;
    strncpy(m->host, token, 32);

    token = strtok(NULL, " ");        /* meta port */
    if (token == NULL) continue;
    m->port = atoi(token);

    token = strtok(NULL, " ");        /* min solicit time */
    if (token == NULL) continue;
    m->minimum = atoi(token);

    token = strtok(NULL, " ");        /* max solicit time */
    if (token == NULL) continue;
    m->maximum = atoi(token);

    token = strtok(NULL, " ");        /* our host name */
    if (token == NULL) continue;
    strncpy(m->ours, token, 32);

    token = strtok(NULL, " ");        /* server type */
    if (token == NULL) continue;
    strncpy(m->type, token, 2);

    token = strtok(NULL, " ");        /* player port */
    if (token == NULL) continue;
    m->pport = atoi(token);

    token = strtok(NULL, " ");        /* observer port */
    if (token == NULL) continue;
    m->oport = atoi(token);

    token = strtok(NULL, "\n");       /* comment text */
    if (token == NULL) continue;
    strncpy(m->comment, token, 32);

    /* force minimum and maximum delays (see note on #define) */
    if (m->minimum < META_MINIMUM_DELAY)
      m->minimum = META_MINIMUM_DELAY;
    if (m->maximum > META_MAXIMUM_DELAY)
      m->maximum = META_MAXIMUM_DELAY;

    /* attach to the metaserver (DNS lookup is only delay) */
    udp_attach(m);
    /* place metaserver addresses in /etc/hosts to speed this */
    /* use numeric metaserver address to speed this */

    /* initialise the other parts of the structure */
    m->sent = 0;
    strcpy(m->prior, "");
  }
  initialised++;
  fclose(file);
}

static void terminate()
{
  int i;

  for (i=0; i<MAXMETASERVERS; i++) {
    struct metaserver *m = &metaservers[i];
    if (m->sock != -1) {
      udp_detach(m);
      m->sock = -1;
    }
  }
}

static void reload()
{
  terminate();
  initialise();
}

static void recheck()
{
  struct stat newstat;

  if (stat(SYSCONFDIR"/metaservers", &newstat) == 0) {
    if (newstat.st_ino != oldstat.st_ino ||
        newstat.st_mtime != oldstat.st_mtime) {
      ERROR(1,("solicit: reloading %s\n", SYSCONFDIR"/metaservers"));
      reload();
    }
  }
}

void solicit(int force)
{
  int i, nplayers=0, nfree=0, nplayersall=0;
  char packet[MAXMETABYTES];
  char *fixed_name, *fixed_login; /* name/login stripped of unprintables */
  char *name, *login;             /* name and login guaranteed not blank */
  char unknown[] = "unknown";     /* unknown player/login string */
  char *here = packet;
  time_t now = time(NULL);
  int gamefull = 0;               /* is the game full? */
  int isrsa = 0;                  /* is this server RSA? */

  /* perform first time initialisation or recheck */
  if (initialised == 0) {
    initialise();
  } else {
    recheck();
  }

  /* Don't solicit in INL mode unless both sides have captains;
     this fixes the problem of the server sticking around post-game
     due to player slots remaining in the game */
  if (inl_mode) {
    int h;
    struct player *j;
    int captains = 0;

    for (h = 0, j = &players[0]; h < MAXPLAYER; h++, j++) {
      if (j->p_inl_captain && (j->p_status != PFREE))
        captains++;
    }
    if (captains < 2)
      return;
  }

  /* update each metaserver */
  for (i=0; i<MAXMETASERVERS; i++) {
    struct metaserver *m = &metaservers[i];
    int j;

    /* skip empty metaserver entries */
    if (m->sock == -1) continue;

    ERROR(9,("solicit[%d](ours:'%s' type='%s' pport=%d oport=%d meta='%s' mport=%d)\n", i, m->ours, m->type, m->pport, m->oport, m->host, m->port));

    /* only process entries with the correct server type */
    if (inl_mode && tolower(m->type[0]) != 'i') {
      ERROR(7,("  skip metaserver entries with non-INL type during INL mode\n"));
      continue;
    } else if (!(inl_mode) && (tolower(m->type[0]) == 'i')) {
      ERROR(7,("  skip metaserver entries with INL type during non-INL mode\n"));
      continue;
    }

    /* if we told metaserver recently, don't speak yet */
    if (!force)
      if ((now-m->sent) < m->minimum) continue;

    /* don't remake the packet unless necessary */
    /* for INL the always recreate because we have multiple ports */
    if (inl_mode) {
      here = packet; nplayers=0; nfree=0; gamefull=0; isrsa=0;
    }

    if (here == packet) {
      int queue;
      if (newbie_mode)
        queue = QU_NEWBIE_PLR;
      else if (inl_mode && strncasecmp(m->ours, "home", 4 ) == 0 )
        queue = QU_HOME;
      else if (inl_mode && strncasecmp(m->ours, "away", 4 ) == 0 )
        queue = QU_AWAY;
      else
        queue = QU_PICKUP;

      /* count the slots free to new logins, and the slots taken */
      nfree = slots_free(queue);
      nplayers = slots_playing(queue, 0);
      nplayersall = slots_playing(queue, 1);

      ERROR(7,("before: nfree=%d nplayers=%d gamefull=%d\n", nfree, nplayers, gamefull));

      /* Special case: do *not* report anything for INL servers if nplayers=0,
         except one last time when players are leaving. Actually i just want to
         delist the server. is there a better way?  ehb  */
      if (nplayers == 0 &&
          inl_mode &&
          (strcmp(packet, m->prior) == 0 || strcmp(m->prior, "") == 0)) {
        ERROR(7,("  skip metaserver entries during INL mode if zero players\n"));
        continue;
      }

      /* If the free slots are zero but the game is not actually full,
         don't report a queue if there are 4 or more entering slots.
         Workaround to not show a queue if there are many entering slots. */
      if ((nfree == 0) && ((nplayersall - nplayers) < 4)) {
        nfree = -queues[queue].count;
        gamefull++;
      }
      ERROR(7,("  nfree=%d nplayers=%d gamefull=%d\n", nfree, nplayers, gamefull));

#ifdef RSA
      isrsa++;     /* this is an RSA server */
#endif

      /* build start of the packet, the server information */
      if (report_users) {
        sprintf(here, "%s\n%s\n%s\n%d\n%d\n%d\n%d\n%s\n%s\n%s\n%s\n",
                /* version */   "b",
                /* address */   m->ours,
                /* type    */   m->type,
                /* port    */   m->pport,
                /* observe */   m->oport,
                /* players */   nplayers,
                /* free    */   nfree,
                /* t-mode  */   status->tourn ? "y" : "n",
                /* RSA     */   isrsa ? "y" : "n",
                /* full    */   gamefull ? "y" : "n",
                /* comment */   m->comment
                );
      } else {
        sprintf(here, "%s\n%s\n%s\n%d\n%d\n%d\n%d\n%s\n%s\n%s\n%s\n",
                /* version */   "b",
                /* address */   m->ours,
                /* type    */   m->type,
                /* port    */   m->pport,
                /* observe */   m->oport,
                /* players */   0,
                /* free    */   16,
                /* t-mode  */   status->tourn ? "y" : "n",
                /* RSA     */   isrsa ? "y" : "n",
                /* full    */   "n",
                /* comment */   m->comment
                );
      }
      here += strlen(here);

      /* now append per-player information to the packet */
      for (j=0; j<MAXPLAYER; j++) {
        /* ignore free slots and local players */
        if (players[j].p_status == PFREE ||
            is_local(&players[j]) ||
#ifdef LTD_STATS
            ltd_ticks(&(players[j]), LTD_TOTAL) == 0
#else
            players[j].p_stats.st_tticks == 0
#endif
            )
          continue;
        fixed_name = name_fix(players[j].p_name);  /*get rid of non-printables*/
        fixed_login = name_fix(players[j].p_login);

        /* make sure name_fix() doesn't return NULL */
        name  = (fixed_name != NULL)  ? fixed_name : players[j].p_name;
        login = (fixed_login != NULL) ? fixed_login : players[j].p_login;

        /* if string is empty, report "unknown" */
        name  = (name[0] == 0)  ? unknown : name;
        login = (login[0] == 0) ? unknown : login;

        sprintf(here, "%c\n%c\n%d\n%d\n%s\n%s@%s\n",
                /* number */   players[j].p_mapchars[1],
                /* team   */   players[j].p_mapchars[0],
                /* class  */   players[j].p_ship.s_type,
                /* ??? note change from design, ship type number not string */
                /* rank   */   players[j].p_stats.st_rank,
                /* ??? note change from design, rank number not string */
                /* name   */   name,
                /* user   */   login,
                /* host   */   players[j].p_monitor );
        here += strlen(here);
        free(fixed_name);      /*because name_fix malloc()s a string */
        free(fixed_login);
      }
    }

    ERROR(7,("  packet created for sending:\n%s\n", packet));

    /* if we have exceeded the maximum time, force an update */
    if ((now-m->sent) > m->maximum) force=1;

    /* if we are not forcing an update, and nothing has changed, drop */
    if (!force)
      if (!strcmp(packet, m->prior)) {
        ERROR(7,("  No change in packet since last time. dont send.\n"));
        continue;
      }

    /* send the packet */
    if (udp_tx(m, packet, here-packet)) {
      m->sent=time(NULL);
      strcpy(m->prior, packet);
    }
  }
}

void solicit_delist(char *host, char *ours)
{
  int i;
  char packet[MAXMETABYTES];
  char *here = packet;
  struct metaserver *m = (struct metaserver *) calloc(1, sizeof(struct metaserver));

  strcpy(m->host, host);
  m->port = 3521;
  strcpy(m->ours, ours);
  udp_attach(m);

  sprintf(here, "%s\n%s\n%s\n%d\n%d\n%d\n%d\n%s\n%s\n%s\n%s\n",
          /* version */   "b",
          /* address */   m->ours,
          /* type    */   "B",
          /* port    */   2592,
          /* observe */   2593,
          /* players */   -1,
          /* free    */   0,
          /* t-mode  */   "n",
          /* RSA     */   "n",
          /* full    */   "y",
          /* comment */   "delist"
          );
  here += strlen(here);
  for (i=0; i<8; i++) {
    udp_tx(m, packet, here - packet);
    fprintf(stderr, "tx ");
    sleep(1);
  }
  fprintf(stderr, "\n");
}
