#include <stdio.h>
#include <ctype.h>

#include "copyright.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* ban functions */

/* add a ban given a player slot number */
int bans_add_temporary_by_player(int who)
{
  int i;
  struct player *j = &players[who];
  for (i=0; i<MAXBANS; i++) {
    struct ban *b = &bans[i];
    if (b->b_remain == 0) {
      strcpy(b->b_ip, j->p_ip);
      b->b_remain = ban_vote_duration;
      ERROR(2,( "ban of %s was voted\n", b->b_ip));
      return 1;
    }
  }
  return 0;
}

/* ages all temporary bans by the seconds that have elapsed, game time */
void bans_age_temporary(int elapsed) {
  int i;
  for (i=0; i<MAXBANS; i++) {
    struct ban *b = &bans[i];
    if (b->b_remain == 0) continue;
    if (b->b_remain < elapsed) {
      b->b_remain = 0;
    } else {
      b->b_remain -= elapsed;
    }
  }
}

/* identified ban relevant to caller */
struct ban *bans_check_temporary_identified;

/* check temporary ban again */
int bans_check_temporary_remaining()
{
  return bans_check_temporary_identified->b_remain;
}

/* check temporary bans voted */
int bans_check_temporary(char *ip)
{
  int i;
  for(i=0; i<MAXBANS; i++) {
    struct ban *b = &bans[i];
    if (b->b_remain) {
      if (!strcmp(b->b_ip, ip)) {
	ERROR(2,( "ban of %s is still current\n", b->b_ip));
	bans_check_temporary_identified = b;
	return TRUE;
      }
    }
  }
  return FALSE;
}

/* check permanent bans in file (previously in main.c) */
int bans_check_permanent(char *login, char *host)
{
  FILE  *bannedfile;
  char  log_buf[64], host_buf[64], line_buf[160];
  char  *position;
  int   num1;
  int   Hits=0;                 /* Hits==2 means we're banned */

  if ((bannedfile = fopen(Banned_File, "r")) == NULL) {
    ERROR(1,( "No banned file %s\n", Banned_File));
    fflush(stderr);
    return(FALSE);
  }
  while(fgets(line_buf, 160, bannedfile) != NULL) {
    /* Split line up */
    if((*line_buf=='#')||(*line_buf=='\n'))
      continue;
    if ((position = (char *) RINDEX(line_buf, '@')) == 0) {
      ERROR(1,( "Bad line in banned file\n"));
      fflush(stderr);
      continue;
    }
    num1 = position - line_buf;
    STRNCPY(log_buf, line_buf, num1); /* copy login name into log_buf */
    log_buf[num1] = '\0';
    STRNCPY(host_buf, position + 1, 64); /* copy host name into host_buf */
    /* Cut off any extra spaces on the host buffer */
    position = host_buf;
    while (!isspace(*position))
      position++;
    *position = '\0';

    /*
      ERROR(1,( "Login: <%s>; host: <%s>\n", login, host));
    ERROR(1,("    Checking Banned <%s> and <%s>.\n",log_buf,host_buf));
    */
    if(*log_buf=='*')

      Hits=1;
    else if (!strcmp(login, log_buf))
      Hits=1;
    if(Hits==1)
      {
        if (*host_buf == '*'){  /* Lock out any host */
          Hits++;
          break;                /* break out now. otherwise Hits will get reset
                                   to one */
        }
        else if(strstr(host,host_buf)!=NULL){ /* Lock out subdomains (eg, "*@usc
.edu" */
          Hits++;
          break;                /* break out now. otherwise Hits will get reset
                                   to one */
        }
        else if (!strcmp(host, host_buf)){ /* Lock out specific host */
          Hits++;
          break;                /* break out now. otherwise Hits will get reset
                                   to one */
        }
      }
  }
  fclose(bannedfile);
  if(Hits>=2)
    return(TRUE);
  else
    return(FALSE);
}
