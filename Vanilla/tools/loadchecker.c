#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/file.h>
#include <math.h>
#include <sys/ipc.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

#define HIGH_USAGE       60 /* percentages.  high means report usage at */
#define VERY_HIGH_USAGE  80 /* every interval. 'very' and 'ultra' are */
#define ULTRA_HIGH_USAGE 90 /* just for cute comments. */

#define INTERVAL 300 /* how many seconds between usage samples, report */
                     /* usage every 3 intervals if usage is not high */

#define STAT_CMD "/usr/ucb/vmstat 5 2"

/*

   This program requires that /usr/ucb/vmstat exists, and that the
   output of "/usr/ucb/vmstat 5 2" should look like:

 procs      faults    cpu      memory              page             disk  
 r b w   in  sy  cs us sy id  avm  fre  re at  pi  po  fr  de  sr s0 
 1 0 0  415 457 314 10 19 71  16k 2076   0  0   0   0   0   0   0  0
 1 0 0  232 353 238 11 25 63  16k 2076   0  2   0   0   0   0   0  0
....5....0....5....0....5...

   In particular, the idle stats must occur directly under the letters
   id which occur in the second line of output.  Furthermore, id must
   occur only once in the second line (the program find only the first
   instance).

*/

/* file scope prototypes */
static void pmessage(char *, int, int);

/* external prototypes */
extern int openmem(int);	/* from openmem.c */


int gotmem = 0;

int main()
{
  FILE *fp;
  char buf[100];
  int idle;
  int i = -1;
  int displayCount = 0;
  
  for (;;) {
    if ( !(gotmem = openmem( -1 )) ){   /* try to open shmem */
        sleep(INTERVAL); /* if we don't have it, wait */
        continue;
    }
    fp=popen(STAT_CMD, "r");
    if (fp==NULL) {
      sleep(INTERVAL);
      continue;
    }
    sleep(10);
    
    fgets(buf, 100, fp); /* First line is some header info */
    fgets(buf, 100, fp); /* Second line shows where idle is */
    
    if (i==-1){  /* We haven't found the id location yet */
      i=0;
      while ( !( (buf[i]=='i') && (buf[i+1]=='d')) && (i<99) ){
	i++;
      }
      if (i==99){ /* can't find it */
	fprintf(stderr,"loadchecker:  id not in /usr/ucb/vmstat\n");
	pclose(fp);
	exit(1);
      }
    }
    
    fgets(buf, 100, fp); /* don't want the third line */
    fgets(buf, 100, fp); /* only want the 4th line */
    pclose(fp);

    buf[i+2] = '\0';
    if (sscanf(&buf[i], "%d", &idle) >= 1) {  /* >= is VERY important */
      sprintf(buf, "GOD->ALL  Current CPU usage is %d%%.", 100-idle);
      if (idle < 100-ULTRA_HIGH_USAGE)
	strcat(buf, "  Ack!");
      else if (idle < 100-VERY_HIGH_USAGE)
	strcat(buf, "  I can live with it.");

      /* print out usage every 3 intervals or more often if usage is high */
      
      if (((displayCount % 3) == 0) || (idle < 100-HIGH_USAGE)) {
	pmessage(buf, 0, MALL);
      }
      if ( ((++displayCount) % 3) == 0) displayCount =0;
    }
    else{ /* something weird happened, so we couldn't read idle */
      i = -1;  /* redo the entire process */
    }
    sleep(INTERVAL);	/* wait 5 minutes */
  }
}


static void pmessage(char *str, int recip, int group)
{
  struct message *cur;
  if (++(mctl->mc_current) >= MAXMESSAGE)
    mctl->mc_current = 0;
  cur = &messages[mctl->mc_current];
  cur->m_no = mctl->mc_current;
  cur->m_flags = group;
  cur->m_time = 0;
  cur->m_recpt = recip;
  cur->m_from = 255; /* change 12/11/90 TC */
  (void) sprintf(cur->m_data, "%s", str);
  cur->m_flags |= MVALID;
}
