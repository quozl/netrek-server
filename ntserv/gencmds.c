/*
 *  gencmds.c		
 */

/*
   Command interface routines general to ntserv/daemon/robot code.  This
   is a replacement for the overloaded compile method used previously in
   commands.c .  Instead of compiling this file several times with modified
   defines, the generalized routines found here can be linked into other
   executables.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "gencmds.h"
#include "proto.h"

/* file scope prototypes */
int do_vote(char *comm, struct message *mess, struct command_handler_2 *votes,
            int num);
int getplayer(int from, char *line);

char *addr_mess(int who, int type)
{
    static char addrbuf[10];
    char* team_names[MAXTEAM+1] = { "", "FED", "ROM", "", "KLI",
                                 "", "", "", "ORI" };

    if (type == MALL) 
      sprintf(addrbuf,"%s->ALL", myname);
    else if (type == MTEAM)
      sprintf(addrbuf,"%s->%3s", myname, team_names[who]);
    else /* Assume MINDIV */
      sprintf(addrbuf, "%s->%2s", myname, players[who].p_mapchars);

    return (addrbuf);
}

/* Check a command list to see if a message is a command */
int check_2_command(struct message *mess, struct command_handler_2 *cmds,
                    int prereqs)
{
  int i, len;
  char *comm;

  if (!(mess->m_flags & MINDIV)) return 0;	/* double-check */

  len = strlen(mess->m_data);

  /* Check if a null command ( First 8 characters are XXX->XXX ) */
  if (len <= 8)
    return 0;

  /* skip header/space */
  for (i = 8; i < len && isspace(mess->m_data[i]); i++) continue; 

  comm = strdup(&mess->m_data[i]);

  len = strlen(comm);

  /* Convert first word in msg to uppercase */
  for (i=0; (i < len && !isspace(comm[i])); i++)
  {
    comm[i] = toupper(comm[i]);
  }

  len = i;

  for (i=0; cmds[i].command != NULL; i++)
  {
    /* Dont check dummy descriptions.. */
    if (cmds[i].tag & C_DESC)
      continue;
    if ((cmds[i].tag & C_PR_MASK) & ~(prereqs))
      /* Dont meet the prereqs */
      continue;
    if (!(voting) && (cmds[i].tag & C_PR_VOTE))
      continue;
    if (len == strlen(cmds[i].command) &&
	strncmp(cmds[i].command, comm, strlen(cmds[i].command)) == 0)
    {

      /* Record vote in GodLog if C_GLOG is set */
      if (cmds[i].tag & C_GLOG )
      {
	if (glog_open() == 0) {	/* Log votes for God to see */
	  time_t    t = time(NULL);
	  char      tbuf[40];
	  struct tm *tmv = localtime(&t);

#ifdef STRFTIME
	  sprintf(tbuf, "%02d/%02d %d/%d", tmv->tm_hour, tmv->tm_min,
		  tmv->tm_mday, tmv->tm_mon);
#else
	  strftime(tbuf, 39, "%R %D", tmv);
#endif
	  glog_printf("VOTE: %s %s@%s \"%s\"\n", tbuf,
		  players[mess->m_from].p_login, 
		  players[mess->m_from].p_monitor, comm);
	}
      } /* !C_GLOG */

      if ((voting) && (cmds[i].tag & (C_VC_ALL | C_VC_TEAM))) {
	if (do_vote(comm, mess, cmds, i)) {
	  if ( cmds[i].tag & C_GLOG ) {
	    if (glog_open() == 0) {	/* Log pass for God to see */
	      glog_printf("VOTE: The motion %s passes\n", comm);
	    }
          }
	}
      } else {
	if (cmds[i].tag & C_PLAYER) {
	  int who;

	  /* This is really not that useful - If the calling
	     function needs a player field, it should check it
	     on it's own.  I include it here for compatibility */
	  who = getplayer(mess->m_from, comm);
	  if (who >= 0)
	    (*cmds[i].handler)(comm, mess, who, cmds, i, prereqs);
	  else {
	    free(comm);
	    return 0;
	  }
	} else
	  (*cmds[i].handler)(comm, mess, cmds, i, prereqs);
      }
      free(comm);
      return 1;
    }
  }
  free(comm);
  return 0;
}

int getplayer(int from, char *line)
{
  char id;
  char temp[MSG_LEN];
  int what;
  int numargs;

  numargs = sscanf(line,"%s %c", temp, &id);
  id = toupper(id);

  if (numargs==1)
    what = from;
  else if ((id >= '0') && (id <='9'))
    what = id - '0';
  else if ((id >= 'A') && (id <= ('A' + MAXPLAYER - 10)))
    what = id - 'A' + 10;
  else
  {
    pmessage(from, MINDIV, addr_mess(from, MINDIV),
	     "Invalid player number.");
    return -1;
  }
  if (players[what].p_status == PFREE)
  {
    pmessage(from, MINDIV, addr_mess(from, MINDIV),
	     "Player not in the game.");
    return -1;
  }
  return what;
}

int do_vote(char *comm, struct message *mess, struct command_handler_2 *votes,
            int num)
{
  int what = 0;
  int player = -1;
  register int i;
  register struct player *j;
  int pcount=0;  /* Total players in game */
  int vcount=0;  /* Number who've voted recently */
  int mflag = 0;
  int sendto = 0;
  int who;
  char nv[PV_TOTAL+1];

  who = mess->m_from;

  if (players[who].p_status == POBSERV)
  {
    god(who, "Sorry, observers can't vote");
    return 0;
  }

  if (votes[num].tag & C_PLAYER)
  {
    if ((what = getplayer(who, comm)) < 0)
      return 0;
    player = what;
  }

  what += votes[num].start;

  if (what >= PV_TOTAL) return 0;

  j = &players[who];
  if ((j->voting[what] != 0) && (votes[num].frequency != 0))
  {
    if ( (j->voting[what] + votes[num].frequency) > time(NULL) )
    {
      godf(who,"Sorry, you can only use %s every %1.1f minutes",
           votes[num].command, votes[num].frequency / 60.0);
      return 0;
    }
  }
  j->voting[what] = time(NULL);  /* Enter newest vote */

  if (votes[num].tag & C_VC_TEAM)
  {
    sendto = players[who].p_team;
    mflag = MTEAM;
  }
  else if (votes[num].tag & C_VC_ALL)
  {
    sendto = 0;
    mflag = MALL;
  }
  else
  {
    ERROR(1,("Unrecognized message flag in do_vote() for %s\n",
	     comm));
    return 0;
  }

  if (!strncmp(comm, "EJECT", 5) && (who == player))
  {
      god(who, "Sorry, you can't eject yourself.");
      return 0;
  }

  nv[0] = '\0';
  for (i=0, j = &players[i]; i < MAXPLAYER; i++, j++)
  {
    /*Skip free slots and robots */
    if ( (j->p_status == PFREE) || (j->p_flags & PFROBOT))
      continue;

    /*Also skip players that are not on the same team if team flag is set*/
    if ((votes[num].tag & C_VC_TEAM) && (j->p_team != players[who].p_team))
      continue; 

    /* Also skip observers */
    if (j->p_status == POBSERV)
      continue;

    pcount++;

    /* Did a player vote expire? */
    if ((j->voting[what] > 0)
	&& ((votes[num].expiretime == 0)
	    || (j->voting[what] + votes[num].expiretime > time(NULL))))
    {
      vcount++;
    }
    else
    {
      strcat(nv, j->p_mapchars);
      strcat(nv, " ");
    }
  }
  pmessage(sendto, mflag, addr_mess(sendto,mflag),
	   "%s has voted to %s.  %d player%s (of %d) %s voted.",
	   players[who].p_mapchars, comm, vcount, vcount==1 ? "":"s",
	   pcount, vcount==1 ? "has":"have");

  /* The Votes Passes */
  if ( (vcount >= votes[num].minpass)
      && ( ((pcount < (2*vcount)) && (votes[num].tag & C_VC_ALL))
	  || ((votes[num].tag & C_VC_TEAM) && (pcount/2 + 1 <= vcount)) ))
  {
    pmessage(sendto, mflag, addr_mess(sendto,mflag), 
	     "The motion %s passes", comm);

    /* Reset the Votes for this item since it passed */
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) 
      j->voting[what] = -1;

    if (votes[num].tag & C_PLAYER)
      (*votes[num].handler)(who, player, mflag, sendto);
    else
      (*votes[num].handler)(who, mflag, sendto);

    return 1;
  }
  else
  {
    /*
    if (votes[num].tag & C_VC_TEAM) 
      i = pcount - vcount - 1;
    else */
      i = pcount/2 + 1 - vcount;

    if (i < (votes[num].minpass - vcount))
      i = votes[num].minpass - vcount;

    pmessage(sendto, mflag, addr_mess(sendto, mflag),
	     "Still need %d more vote%s, from %s", i, (i==1 ? "":"s"), nv);
    return 0;
  }
}

/* ARGSUSED */
int do_help(char *comm, struct message *mess, struct command_handler_2 *cmds,
            int cmdnum, int prereqs)
{
  int who;
  int i;
  char *addr;

  who = mess->m_from;
  addr = addr_mess(who,MINDIV); 

  if (cmds[0].command != NULL)
  { 
    for (i=0; cmds[i].command != NULL; i++)
    {
      if ((cmds[i].tag & C_PR_MASK) & ~(prereqs))
	/* Dont meet the prereqs */
	continue;
      if (cmds[i].tag & C_DESC)
	/* Command is just a dummy description */
	pmessage(who,MINDIV,addr, cmds[i].command);
      if (!(cmds[i].desc))
	/* Command has no description
	   (its description has a value of NULL, not "")
	   Use this hack to make a command hidden */
	continue;
      if (!(voting) && (cmds[i].tag & C_PR_VOTE))
	continue;
      else if (cmds[i].tag & (C_VC_TEAM | C_VC_ALL))
      {
	char ch;

	if (cmds[i].tag & C_VC_TEAM)
	  ch = 'T';
	else if (cmds[i].tag & C_VC_ALL)
	  ch = 'M';
	else
	  ch = '?';
	pmessage(who,MINDIV,addr, "|%10s - %c: %s",
		 cmds[i].command, ch, cmds[i].desc);
      }
      else
	pmessage(who,MINDIV,addr, "|%10s - %s",
		 cmds[i].command, cmds[i].desc);
    }
  }
  return 1;
}
