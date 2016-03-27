#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

extern int clue;

static char hours[7][24];
static time_t hr_loaded=0;

/* NBT - I didn't like the old way this was done so I rewrote it. It now
 * 	 checks a file that has a entry for every hour of every day.
	 It also allows for clue code checking.
	 X = not playable
	 C = clue hour
	 anything else = playable
 * returns a 1 if the time is acceptable for play, 0 otherwise
 */
int time_access(void)
{
    struct tm *tm;
    time_t secs;
    FILE *tfd;
    int day;
    struct stat tstat;

    time(&secs);
    tm = localtime(&secs);

    if (stat(Time_File,&tstat) != 0)	{
	ERROR(1,("Time file is missing! (%s)\n",Time_File));
	return 0;
    }

    if (tstat.st_mtime > hr_loaded) {
	hr_loaded = tstat.st_mtime;

        tfd=fopen(Time_File,"r");

        for (day=0; day<=6; day++) {
	  fscanf(tfd,"%s",hours[day]);
	}
    
        fclose(tfd); 
    }

    if (hours[tm->tm_wday][tm->tm_hour]=='X') return 0;
    else if (hours[tm->tm_wday][tm->tm_hour]=='C') {
/*
	if (!clue) {
	    char from_str[10];
	   
	    sprintf(from_str,"GOD->%2s", me->p_mapchars);
	    pmessage(me->p_no, MINDIV, from_str,
		"######## Clue Checking has been enabled! ########");
	    clueVerified=0;
	    clueFuse=0;
	    clueCount=0;
	}
*/
	clue++;
	return 1;
    }
    else return 1;
}
