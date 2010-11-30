/*! @file bans.c
    Player banning.
    Provides temporary and permanent banning by IP address.
    */

#include <stdio.h>
#include <ctype.h>

#include "copyright.h"
#include "config.h"
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "blog.h"

/*! @brief Add a ban given a player slot number.
    @details Adds a temporary ban to the next empty slot in the list
    of bans, reports the action to the error log, and blogs about it.
    @param who player slot number to be banned.
    @param by description of who is making the ban.
    @return status 0 if ban list full, 1 if ban added.
    */
int bans_add_temporary_by_player(int who, char *by)
{
  int i;
  struct player *j = &players[who];
  for (i=0; i<MAXBANS; i++) {
    struct ban *b = &bans[i];
    if (b->b_remain == 0) {
      strcpy(b->b_ip, j->p_ip);
      b->b_remain = ban_vote_duration;
      ERROR(2,( "ban of %s was voted\n", b->b_ip));
      blog_printf("bans", "%s banned\n\nThe player on IP address %s "
                  "has been temporarily banned%s.\n"
                  "slot=%s character name=%s user name=%s "
                  "translated host name=%s ban duration=%d\n",
                  b->b_ip, b->b_ip, by, j->p_mapchars, j->p_name, j->p_login,
                  j->p_full_hostname, b->b_remain);
      return 1;
    }
  }
  return 0;
}

/*! @brief Age temporary bans.
    @details Ages all temporary bans by the game time that has
    elapsed, expiring them as needed.  Blogs about expired bans.
    @param elapsed time elapsed in seconds.
    @return none.
*/
void bans_age_temporary(int elapsed) {
  int i;

  if (elapsed == 0) {
    ERROR(2,("bans_age_temporary: elapsed zero\n"));
    return;
  }

  for (i=0; i<MAXBANS; i++) {
    struct ban *b = &bans[i];
    if (b->b_remain == 0) continue;
    if (b->b_remain < elapsed) {
      b->b_remain = 0;
      blog_printf("bans", "Temporary ban of %s expired\n\n"
                  "The temporary ban of %s which was voted earlier, "
                  "or set by the administrator, has now expired.",
                  b->b_ip, b->b_ip);
    } else {
      b->b_remain -= elapsed;
    }
  }
}

/*! @brief Identified ban relevant to caller.
    @details A local pointer to the last chosen temporary ban, so that
    it can be checked rapidly by the wait queue logic without causing
    a ban list lookup.
 */
struct ban *bans_check_temporary_identified;

/*! @brief Check temporary ban again.
    @details Return the remaining time on the ban most recently
    identified by bans_check_temporary().  Used to provide a minute by
    minute countdown to a player connection that was given a wait
    queue response as a result of their ban.
    @return remaining time remaining in seconds */
int bans_check_temporary_remaining()
{
  return bans_check_temporary_identified->b_remain;
}

/*! @brief Check for a temporary ban.
    @details Given an IP address, check for a temporary ban.  Must be
    called before bans_check_temporary_remaining() is called.
    @param ip the IP address in text form, dotted quad.
    @return result TRUE if banned, FALSE if not.
*/
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

/*! @brief Check for a permanent ban.
    @details Check for permanent bans in the banned file.
    @bug unnecessary action, main.c calls this twice, and so the
    banned file is opened and closed twice.
    @param login the login name sent by the client.
    @param host the host name or IP address of the client.
    @return result TRUE if banned, FALSE if not.
    */
int bans_check_permanent(char *login, char *host)
{
  FILE  *bannedfile;
  char  log_buf[64], host_buf[64], line_buf[160];
  char  *position;
  int   num1;
  int   hits = 0; /* (hits == 2) means we're banned */

  if ((bannedfile = fopen(Banned_File, "r")) == NULL) {
    ERROR(1,("No banned file %s\n", Banned_File));
    fflush(stderr);
    return FALSE;
  }
  while (fgets(line_buf, 160, bannedfile) != NULL) {
    /* Split line up */
    if ((*line_buf == '#') || (*line_buf == '\n'))
      continue;
    if ((position = (char *) strrchr(line_buf, '@')) == 0) {
      ERROR(1,("Bad line in banned file\n"));
      fflush(stderr);
      continue;
    }
    num1 = position - line_buf;
    strncpy(log_buf, line_buf, num1); /* copy login name into log_buf */
    log_buf[num1] = '\0';
    strncpy(host_buf, position + 1, 64); /* copy host name into host_buf */
    /* Cut off any extra spaces on the host buffer */
    position = host_buf;
    while (!isspace(*position))
      position++;
    *position = '\0';

    /*
    ERROR(1,( "Login: <%s>; host: <%s>\n", login, host));
    ERROR(1,("    Checking Banned <%s> and <%s>.\n",log_buf,host_buf));
    */
    if (*log_buf == '*')
      hits = 1;
    else if (!strcmp(login, log_buf))
      hits = 1;

    if (hits == 1) {
      if (*host_buf == '*'){  /* Lock out any host */
	hits++;
	break; /* break out now. otherwise hits will get reset to one */
      } else if (strstr(host, host_buf) != NULL) {
	/* Lock out subdomains (eg, "*@usc.edu" */
	hits++;
	break; /* break out now. otherwise hits will get reset to one */
      } else if (!strcmp(host, host_buf)) {
	/* Lock out specific host */
	hits++;
	break; /* break out now. otherwise hits will get reset to one */
      }
    }
  }
  fclose(bannedfile);
  if (hits >= 2)
    return TRUE;
  else
    return FALSE;
}
