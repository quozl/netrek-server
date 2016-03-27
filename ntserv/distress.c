/*
 * distress.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

/* #$!@$#% length of address field of messages */
#define ADDRLEN 10

#define MAXMACLEN 85
extern char *shiptypes[];
#define warning(x)	ERROR(1,(x))
#ifndef new_warning
#define new_warning(x,y)        warning(y)
#endif

/* file scope prototypes */
static void testmacro (char *bufa, char *bufb, int *inda, int *indb);
static int condmacro (char *bufa, char *bufb, int *inda, int *indb, int flag);
static int solvetest (char *bufa, int *inda);
static int skipmacro (char buf[], int index);
/* static void Dist2Mesg (struct distress *dist, char *buf); */

char mbuf[MSG_LEN];
char *getaddr (), *getaddr2 ();
char *pappend;

/*
 * Like APPEND, and APPEND_CAP, APPEND_INT is an in-line function that
 * stops us from calling sprintf over and over again.
 */
char *pappend;
#define APPEND_INT(ptr, i) \
    ptr += itoa2(i, ptr);

/*
 * The two in-line defs that follow enable us to avoid calling strcat over
 * and over again.
 */
#define APPEND(ptr,str)     \
   pappend = str;           \
   while(*pappend)          \
       *ptr++ = *pappend++;

#define APPEND_CAP(ptr,cap,str) \
   pappend = str;               \
   while(*pappend)              \
   {                            \
       *ptr++ = (cap ? toupper(*pappend) : *pappend); \
       pappend++;               \
   }


/*
 * This is a hacked version from the K&R book.  Basically it puts <n> into
 * <s> in reverse and then reverses the string...
 * MH.  10-18-93
 */
static int itoa2(int n, char s[])
{
    int i, c, j, len;

    if ((c = n) < 0)
        n = -n;

    i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (c < 0)
        s[i++] = '-';

    s[i] = '\0';

    len = i--;

    for (j = 0; i > j; j++, i--)
    {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }

    return len;
}

/* ARGSUSED */
static void dist_error(char *str, char *macro, char ch)
{
              new_warning(UNDEF,"Bad Macro character in distress!");
              ERROR(1,("Unrecognizable special character in distress\n"));
              ERROR(1,("From: %s@%s. %s: %s (%c)\n",
                        me->p_name,
                        me->p_monitor,
                        str,
                        macro,
                        ch));
}




/* This takes an MDISTR flagged message and makes it into a dist struct */
/*ARGSUSED*/
void HandleGenDistr (char *message, u_char from, u_char to,
                     struct distress *dist)
{

  char *mtext;
  u_char i;

  mtext = &message[ADDRLEN];

  memset((char *) dist, 0, sizeof (dist));

  dist->sender = from;
  dist->distype = mtext[0] & 0x1f;
  dist->macroflag = ((mtext[0] & 0x20) > 0);
  dist->fuelp = mtext[1] & 0x7f;
  dist->dam = mtext[2] & 0x7f;
  dist->shld = mtext[3] & 0x7f;
  dist->etmp = mtext[4] & 0x7f;
  dist->wtmp = mtext[5] & 0x7f;
  dist->arms = mtext[6] & 0x1f;
  dist->sts = mtext[7] & 0x7f;
  dist->wtmpflag = (u_char)((dist->sts & PFWEP) > 0) ? 1 : 0;
  dist->etempflag = (u_char)((dist->sts & PFENG) > 0) ? 1 : 0;
  dist->cloakflag = (u_char)((dist->sts & PFCLOAK) > 0) ? 1 : 0;
  dist->close_pl = mtext[8] & 0x7f;
  dist->close_en = mtext[9] & 0x7f;
  dist->tclose_pl = mtext[10] & 0x7f;
  dist->tclose_en = mtext[11] & 0x7f;
  dist->tclose_j = mtext[12] & 0x7f;
  dist->close_j = mtext[13] & 0x7f;
  dist->tclose_fr = mtext[14] & 0x7f;
  dist->close_fr = mtext[15] & 0x7f;
  i=0;
  while ((mtext[16+i] & 0xc0) == 0xc0 && (i < 6))
	{
	  dist->cclist[i]=mtext[16+i] & 0x1f;
	  i++;
	}
  dist->cclist[i]=mtext[16+i];
  if (dist->cclist[i] == 0x80)
	dist->pre_app = 1;
  else
	dist->pre_app = 0;
  dist->preappend[0] = '\0';

  if (mtext[16+i+1] != '\0') 
  {
    strncpy (dist->preappend, &mtext[16+i+1], MSG_LEN - 1);
    dist->preappend[MSG_LEN - 1] = '\0';
  }
}

/* this converts a dist struct to the appropriate text
   (excludes F1->FED text bit).. sorry if this is not what we said 
   earlier jeff.. but I lost the paper towel I wrote it all down on */

#ifdef nodef
static void Dist2Mesg (struct distress *dist, char *buf)
{
  int len, i;

  sprintf (buf, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
	   (dist->macroflag << 5) + (dist->distype),
	   dist->fuelp | 0x80,
	   dist->dam | 0x80,
	   dist->shld | 0x80,
	   dist->etmp | 0x80,
	   dist->wtmp | 0x80,
	   dist->arms | 0x80,
	   dist->sts | 0x80,
	   dist->close_pl | 0x80,
	   dist->close_en | 0x80,
	   dist->tclose_pl | 0x80,
	   dist->tclose_en | 0x80,
	   dist->tclose_j | 0x80,
	   dist->close_j | 0x80,
	   dist->tclose_fr | 0x80,
           dist->close_fr | 0x80);
	   
      /* cclist better be terminated properly otherwise we hose here */
      i=0;
      while (((dist->cclist[i] & 0xc0) == 0xc0)) 
	{
	  buf[16+i]=dist->cclist[i];
	  i++;
	}
  /* get the pre/append cclist terminator in there */
  buf[16+i]=dist->cclist[i];
  buf[16+i+1]='\0';

  len = 16+i+1;
  if (dist->preappend[0] != '\0') 
  {
    strncat (buf, dist->preappend, MSG_LEN - len);      /* false sense of security? */
    buf[MSG_LEN - 1] = '\0';
  }
}
#endif

/* small permutation on the newmacro code... this takes a pointer to
   ** a distress structure and a pointer to a macro syntax string,
   ** and converts it into a line of text.
   **  9/1/93 - jn
 */
int makedistress (struct distress *dist, /* the info */
                   char *cry, 	/* the call for help! (output) -
                                   should be array */
                   char *pm)	/* macro to parse, used for distress and
                                   macro */
{
  char buf1[10 * MAXMACLEN];
  char *pbuf1;
  char buf2[10 * MAXMACLEN];
  char buf3[10 * MAXMACLEN];
  char tmp[10 * MAXMACLEN];
  int index = 0;
  int index2 = 0;
  int index3 = 0;
  int cap = 0;
  struct player *sender;
  struct player *j;
  struct planet *l;
  char c;
  char *macro;

  sender = &players[dist->sender];

  if (!(*pm))
    {
      cry[0] = '\0';
      return (0);
    }

  buf1[0] = '\0';
  pbuf1 = buf1;

  macro = pm;

  /* first step is to substitute variables */
  while (*pm)
    {
      if (*pm == '%')
	{
	  pm++;

	  if (!pm)
	      continue;

	  switch (c = *(pm++))
	    {
	    case ' ':
	      APPEND (pbuf1, " \0");
	      break;
	    case 'O':		/* push a 3 character team name into buf */
	      cap = 1;
	    case 'o':		/* push a 3 character team name into buf */
	      if (sender->p_team != ALLTEAM)
		APPEND_CAP (pbuf1, cap, teamshort[sender->p_team]);
	      cap = 0;
	      break;
	    case 'a':		/* push army number into buf */
	      APPEND_INT(pbuf1, dist->arms);
	      break;
	    case 'd':		/* push damage into buf */
	      APPEND_INT (pbuf1, dist->dam);
	      break;
	    case 's':		/* push shields into buf */
	      APPEND_INT (pbuf1, dist->shld);
	      break;
	    case 'f':		/* push fuel into buf */
	      APPEND_INT (pbuf1, dist->fuelp);
	      break;
	    case 'w':		/* push wtemp into buf */
	      APPEND_INT (pbuf1, dist->wtmp);
	      break;
	    case 'e':		/* push etemp into buf */
	      APPEND_INT (pbuf1, dist->etmp);
	      break;

	    case 'P':		/* push player id into buf */
	    case 'G':		/* push friendly player id into buf */
	    case 'H':		/* push enemy target player id into buf */

	    case 'p':		/* push player id into buf */
	    case 'g':		/* push friendly player id into buf */
	    case 'h':		/* push enemy target player id into buf */

	      switch (c)
		{
		case 'p':
		  j = &players[dist->tclose_j];
		  break;
		case 'g':
		  j = &players[dist->tclose_fr];
		  break;
		case 'h':
		  j = &players[dist->tclose_en];
		  break;
		case 'P':
		  j = &players[dist->close_j];
		  break;
		case 'G':
		  j = &players[dist->close_fr];
		  break;
		default:
		  j = &players[dist->close_en];
		  break;
		}
	      tmp[0] = j->p_mapchars[1];
	      tmp[1] = '\0';
	      APPEND (pbuf1, tmp);
	      break;

	    case 'n':		/* push planet armies into buf */
	      l = &planets[dist->tclose_pl];
	      APPEND_INT (pbuf1,
			  ((l->pl_info & sender->p_team) ? l->pl_armies : -1));
              break;
            case 'B':
              cap = 1;
	    case 'b':		/* push planet into buf */
	      l = &planets[dist->close_pl];
	      tmp[0] = l->pl_name[0] - 'A' + 'a';
	      tmp[1] = l->pl_name[1];
              tmp[2] = l->pl_name[2];
	      tmp[3] = '\0';
	      APPEND_CAP (pbuf1, cap, tmp);
	      cap = 0;
	      break;
	    case 'L':
	      cap = 1;
	    case 'l':		/* push planet into buf */
	      l = &planets[dist->tclose_pl];
	      tmp[0] = l->pl_name[0] - 'A' + 'a';
	      tmp[1] = l->pl_name[1];
	      tmp[2] = l->pl_name[2];
	      tmp[3] = '\0';
	      APPEND_CAP (pbuf1, cap, tmp);
	      cap = 0;
	      break;
	    case 'Z':		/* push a 3 character team name into buf */
	      cap = 1;
	    case 'z':		/* push a 3 character team name into buf */
	      l = &planets[dist->tclose_pl];
	      APPEND_CAP (pbuf1, cap, teamshort[l->pl_owner]);
	      cap = 0;
	      break;
	    case 't':		/* push a team character into buf */
	      l = &planets[dist->tclose_pl];
	      tmp[0] = teamlet[l->pl_owner];
	      tmp[1] = '\0';
	      APPEND (pbuf1, tmp);
	      break;
	    case 'T':		/* push my team into buf */
	      tmp[0] = teamlet[sender->p_team];
	      tmp[1] = '\0';
	      APPEND (pbuf1, tmp);
	      break;
	    case 'c':		/* push my id char into buf */
	      tmp[0] = sender->p_mapchars[1];
	      tmp[1] = '\0';
	      APPEND (pbuf1, tmp);
	      break;
	    case 'W':		/* push WTEMP flag into buf */
	      if (dist->wtmpflag)
		tmp[0] = '1';
	      else
		tmp[0] = '0';
	      tmp[1] = '\0';
	      APPEND (pbuf1, tmp);
	      break;
	    case 'E':		/* push ETEMP flag into buf */
	      if (dist->etempflag)
		tmp[0] = '1';
	      else
		tmp[0] = '0';
	      tmp[1] = '\0';
	      APPEND (pbuf1, tmp);
	      break;
            case 'K':
              cap = 1;
            case 'k':
              if (cap)
                j = &players[dist->tclose_en];
              else
                j = sender;
 
              if (j->p_ship.s_type == STARBASE)
#ifdef LTD_STATS
                sprintf (tmp, "%5.2f", (float) ltd_kills(j, LTD_SB));
#else
                sprintf (tmp, "%5.2f", (float) j->p_stats.st_sbkills);
#endif
              else
#ifdef LTD_STATS
                sprintf (tmp, "%5.2f", (float) ltd_kills(j, LTD_TOTAL));
#else
                sprintf (tmp, "%5.2f", (float) j->p_stats.st_kills +
                         j->p_stats.st_tkills);
#endif
              APPEND (pbuf1, tmp);
              break;

	    case 'U':		/* push player name into buf */
	      cap = 1;
	    case 'u':		/* push player name into buf */
	      j = &players[dist->tclose_en];
	      APPEND_CAP (pbuf1, cap, j->p_name);
	      cap = 0;
	      break;
	    case 'I':		/* my player name into buf */
	      cap = 1;
	    case 'i':		/* my player name into buf */
	      APPEND_CAP (pbuf1, cap, sender->p_name);
	      cap = 0;
	      break;
	    case 'S':		/* push ship type into buf */
              APPEND (pbuf1, shiptypes[sender->p_ship.s_type]);
	      break;
                 case 'v':      /* push average ping round trip time into buf */
                 case 'V':      /* push ping stdev into buf */
                 case 'y':      /* push packet loss into buf */
                   APPEND (pbuf1, "0\0");
            case 'M':           /* push capitalized lastMessage into buf */
            case 'm':           /* push lastMessage into buf */
                   break;
	    case '*':		/* push %} into buf */
	      APPEND (pbuf1, "%*\0");
	      break;
	    case '}':		/* push %} into buf */
	      APPEND (pbuf1, "%}\0");
	      break;
	    case '{':		/* push %{ into buf */
	      APPEND (pbuf1, "%{\0");
	      break;
	    case '!':		/* push %! into buf */
	      APPEND (pbuf1, "%!\0");
	      break;
	    case '?':		/* push %? into buf */
	      APPEND (pbuf1, "%?\0");
	      break;
	    case '%':		/* push %% into buf */
	      APPEND (pbuf1, "%%\0");
	      break;
	    default:
/* try to continue
** bad macro character is skipped entirely,
** the message will be parsed without whatever %. has occurred. - jn
*/
	      dist_error("Pass 1",macro,*pm-1);
	      break;     
	    }
	}
	else
	{
            tmp[0] = *pm;
            tmp[1] = '\0';
            APPEND (pbuf1, tmp);
            pm++;
	}

    }

  *pbuf1 = '\0';

  /* second step is to evaluate tests, buf1->buf2 */
  testmacro (buf1, buf2, &index, &index2);
  buf2[index2] = '\0';

  if (index2 <= 0)
    {
      cry[0] = '\0';
      return (0);
    }

  index2 = 0;

  /* third step is to include conditional text, buf2->buf3 */
  condmacro (buf2, buf3, &index2, &index3, 1);

  if (index3 <= 0)
    {
      cry[0] = '\0';
      return (0);
    }

  if (index3 > MSG_LEN)
	index3 = MSG_LEN;

  buf3[index3] = '\0';

  strcpy(cry,buf3);

  return (index3);
}

static void testmacro (char *bufa, char *bufb, int *inda, int *indb)
{
  int state = 0;

  if (*indb >= 10 * MAXMACLEN)
    return;	
/* maybe we should do something more "safe" here (and at other returns)? */


  while (bufa[*inda] && (*indb < 10 * MAXMACLEN))
    {
      if (state)
	{
	  switch (bufa[(*inda)++])
	    {
            case '*':           /* push %* into buf */
              if (*indb < 10 * MAXMACLEN - 2)
                {
                  bufb[*indb] = '%';
                  (*indb)++;
                  bufb[*indb] = '*';
                  (*indb)++;
                }
              else
                return;     /* we are full, so we are done */
              state = 0;
              continue;

	    case '%':		/* push %% into buf */
	      if (*indb < 10 * MAXMACLEN - 2)
		{
		  bufb[*indb] = '%';
		  (*indb)++;
		  bufb[*indb] = '%';
		  (*indb)++;
		}
	      else
		return;	/* we are full, so we are done */
	      state = 0;
	      continue;

	    case '{':		/* push %{ into buf */
	      if (*indb < 10 * MAXMACLEN - 2)
		{
		  bufb[*indb] = '%';
		  (*indb)++;
		  bufb[*indb] = '{';
		  (*indb)++;
		}
	      else
		return;	/* we are full, so we are done */
	      state = 0;
	      continue;

	    case '}':		/* push %} into buf */
	      if (*indb < 10 * MAXMACLEN - 2)
		{
		  bufb[*indb] = '%';
		  (*indb)++;
		  bufb[*indb] = '}';
		  (*indb)++;
		}
	      else
		return;	/* we are full, so we are done */
	      state = 0;
	      continue;

	    case '!':		/* push %! into buf */
	      if (*indb < 10 * MAXMACLEN - 2)
		{
		  bufb[*indb] = '%';
		  (*indb)++;
		  bufb[*indb] = '!';
		  (*indb)++;
		}
	      else
		return;	/* we are full, so we are done */
	      state = 0;
	      continue;

	    case '?':		/* the dreaded conditional, evaluate it */
	      bufb[*indb] = '0' + solvetest (bufa, inda);
	      (*indb)++;
	      state = 0;
	      continue;

	    default:
	      dist_error("Pass2",bufa,bufa[(*inda) -1]);
	      state = 0;
	      continue;
	    }
	}

      if (bufa[*inda] == '%')
	{
	  state++;
	  (*inda)++;
	  continue;
	}

      state = 0;


      if (*indb < 10 * MAXMACLEN)
	{
	  bufb[*indb] = bufa[*inda];
	  (*inda)++;
	  (*indb)++;
	}
      else
	return;
    }

    return;
}

static int solvetest (char *bufa, int *inda)
{
  int state = 0;
  char bufh[10 * MAXMACLEN];
  char bufc[10 * MAXMACLEN];
  int indh = 0, indc = 0, i;
  char operation;


  while (bufa[*inda] &&
	 bufa[*inda] != '<' &&
	 bufa[*inda] != '>' &&
	 bufa[*inda] != '=')
    {

      bufh[indh++] = bufa[(*inda)++];
    }
  bufh[indh] = '\0';

  operation = bufa[(*inda)++];

  while (bufa[*inda] &&
	 !(state &&
	   ((bufa[*inda] == '?') ||
	    (bufa[*inda] == '{'))))
    {

      if (state && (bufa[*inda] == '%' ||
		    bufa[*inda] == '!' ||
		    bufa[*inda] == '}'))
	{
	  bufc[indc++] = '%';
	}
      else if (bufa[*inda] == '%')
	{
	  state = 1;
	  (*inda)++;
	  continue;
	}

      state = 0;
      bufc[indc++] = bufa[(*inda)++];
    }
  bufc[indc] = '\0';

  if (bufa[*inda])
    (*inda)--;

  if (!operation)		/* incomplete is truth, just ask Godel */
    return (1);

  switch (operation)
    {
    case '=':			/* character by character equality */
      if (indc != indh)
	return (0);
      for (i = 0; i < indc; i++)
	{
	  if (bufc[i] != bufh[i])
	    return (0);
	}
      return (1);

    case '<':
      if (atoi (bufh) < atoi (bufc))
	return (1);
      else
	return (0);

    case '>':
      if (atoi (bufh) > atoi (bufc))
	return (1);
      else
	return (0);

    default:
      dist_error("Pass3", bufa, operation);
      return (1);		/* don't know what happened, pretend we do */
    }
/*    return (0);*/
}

static int condmacro (char *bufa, char *bufb, int *inda, int *indb, int flag)
{
  int newflag, include;
  int state = 0;


  if (*indb >= MAXMACLEN)
    return (0);

  include = flag;

  while (bufa[*inda] && (*indb < MAXMACLEN))
    {
      if (state)
	{
	  switch (bufa[(*inda)++])
	    {
	    case '}':		/* done with this conditional, return */
	      return (0);

	    case '{':		/* handle new conditional */
	      if (*indb > 0)
		{
		  (*indb)--;
		  if (bufb[*indb] == '0')
		    newflag = 0;
		  else
		    newflag = 1;
		}
	      else		/* moron starting with cond, assume true */
		newflag = 1;

	      if (include)
		condmacro (bufa, bufb, inda, indb, newflag);
	      else
		{
		  (*indb)++;
		  *inda = skipmacro (bufa, *inda);
		}

	      state = 0;
	      continue;

	    case '!':		/* handle not indicator */
	      if (flag)
		include = 0;
	      else
		include = 1;

	      state = 0;
	      continue;

            case '*':           /* test for abort */
              if (include)
                {
                  /* abort this macro */
                  bufb[0] = '\0';
                  *indb = 0;
                  return (0);
                }
 
              state = 0;
              continue;

	    case '%':		/* push % into buf */
	      if (include)
		{
		  if (*indb < MAXMACLEN)
		    {
		      bufb[*indb] = '%';
		      (*indb)++;
		    }
		  else
		    return (0);
		}
	      state = 0;
	      continue;

	    default:
	      dist_error("Pass4",bufa,bufa[(*inda) -1]);
	    }
	}

      if (bufa[*inda] == '%')
	{
	  state++;
	  (*inda)++;
	  continue;
	}

      state = 0;


      if (include)
	{
	  if (*indb < MAXMACLEN)
	    {
	      bufb[*indb] = bufa[*inda];
	      (*inda)++;
	      (*indb)++;
	    }
	  else
	    return (0);
	}
      else
	(*inda)++;
    }
    return (0);
}

static int skipmacro (char buf[], int index)
{
  int state = 0;
  int end = 0;

  if (index == 0)
    index++;

  while (buf[index] && !end)
    {
      if (state)
	{
	  switch (buf[index++])
	    {
	    case '{':
	      index = skipmacro (buf, index);
	      continue;
	    case '}':
	      end = 1;
	      continue;
	    case '!':
	    case '%':
	      state = 0;
	      continue;
	    default:
	      dist_error("Pass5",buf,buf[index]);
	    }
	}

      if (buf[index] == '%')
	{
	  state++;
	  index++;
	  continue;
	}

      state = 0;
      index++;
    }

  return (index);
}
