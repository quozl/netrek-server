/* 
 * update.c
 * Modified 23 jul 93 mit:eleazor -- Can use from crontab, not a daemon.
 * This reduces the number of processes running on the server.  Ideally
 * the daemon or newstartd could start this once a day. (-n option)
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "patchlevel.h"
#include "version.h"

/* DEFINABLES. Set these to your enviroment */

#define CAPFILE		"rsa-keyfile"
#define LOCALKEYS	"rsa-local"
#define KEYCOMP		"rsa_keycomp"
#define MOTDLIST	"motd_list"
#define MOTDLOGO	"motd_logo.MASTER"
#define MOTDCLUELOGO	"motd_clue_logo.MASTER"
#define BASESCORES	LIBDIR"/tools/scores s"
#ifdef BASEPRACTICE
#define MOTDBASEPLOGO	"motd_basep_logo.MASTER"
#endif
#define CLASSES		"inl,standard,standard2"
#define EXCLUDE		NULL
#define INFO		"info.MASTER"
#define TOP		50
#define TOPBASE		100
#define METASERVER	"clientkeys.netrek.org"
#define METAPORT	3530
#define FEATUREPORT	3531
#define MAXTRIES	5

#define SLEEPTIME	86400 		/* once a day */
#define INTERVAL	300		/* 5 min. interval between trials */

/* end of DEFINABLES */

/* There is a time bias towards people with at least 4 hours of play... */
#define TIMEBIAS 4

int daysold = 28;

char hostname[20];
struct statentry play_entry;
struct rawdesc *output=NULL;
char mode;
int showall=0;

struct rawdesc {
    struct rawdesc *next;
    char name[NAME_LEN];
    int ttime;
    int rank;
    float bombing, planets, offense, defense;
    int nbomb, nplan, nkill, ndie;
    int sbkills, sblosses, sbtime;
    float sbmaxkills;
    LONG lastlogin;
};

extern int errno;
struct linger my_linger;
char	buf[BUFSIZ],buf1[BUFSIZ],buf2[BUFSIZ];
int	base = FALSE;

void BecomeDaemon(void);
void fixkeymap(char *s);
void usage(void);
void makemotd(char *motd_name, char *logo);
int ordered(struct rawdesc *num1, struct rawdesc *num2, int mode);
void scores(int mode, FILE *out);
extern void getpath(void);

void reaper()
{
#ifdef DEBUG
    printf("UH-OH!!!!! Reaper was called\n");
#endif
    while (wait3(0, WNOHANG, 0) > 0)
        ;
}

void backup(char *f)
{
   char         *backup_f;
   struct stat  sbuf;

#ifdef DEBUG
   printf("Backing up %s\n",f);
#endif

   if(stat(f, &sbuf) < 0)
      return;
   backup_f = (char *)malloc(strlen(f)+2);
   if(!backup_f) { perror("malloc"); exit(1); }
   sprintf(backup_f, "%s~", f);
   (void)link(f, backup_f);
   (void)unlink(f);
   free((char *)backup_f);
}

/* 
    The connection to the metaserver is by Andy McFadden.
    This calls the metaserver and parses the output into something
    useful
*/

static int open_port(char *host, int port, int verbose)
{
    struct sockaddr_in addr;
    struct hostent *hp;
    int sock;

    /* Connect to the metaserver */
    /* get numeric form */
    if ((addr.sin_addr.s_addr = inet_addr(host)) == -1) {
        if ((hp = gethostbyname(host)) == NULL) {
            if (verbose)
                ERROR(1,( "unknown host '%s'\n", host));
            return (-1);
        } else {
            addr.sin_addr.s_addr = *(U_LONG *) hp->h_addr;
        }
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#ifdef DEBUG
    printf("Opening socket at (%s,%d)\n",host,port);
#endif
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        if (verbose)
                perror("socket");
        return (-1);
    }
    if (connect(sock,(struct sockaddr*) &addr, sizeof(addr)) < 0) {
        if (verbose)
                perror("connect");
        close(sock);
        return(-1);
    }
    return (sock);
}

/* prevent child zombie processes -da */

/* some lame OS's don't support SA_NOCLDWAIT */
#ifndef SA_NOCLDWAIT
#define SA_NOCLDWAIT	0
#endif

void no_zombies(void)
{
  struct sigaction action;

  action.sa_flags = SA_NOCLDWAIT;
  action.sa_handler = SIG_DFL;
  sigemptyset(&(action.sa_mask));
  sigaction(SIGCHLD, &action, NULL);
}

int main(int argc, char **argv)
{

    int		tries;
    FILE	*keyfile, *keylocal;
    FILE	*featurefile;
    int		sock;
    int		succeed;
    int	 	num_read, num_written;
    int		c;
    int		no_daemon = FALSE;
    int		no_motd = FALSE;
    int		feature = FALSE;
    char	*classes = CLASSES;
    char	*exclude = EXCLUDE;
    int 	port = METAPORT;

    gethostname(hostname,20);

    for (c=1; c < argc; c++) 
        if (argv[c][0] == '-')
	    switch (argv[c][1]) {
		case 'n' : 
		    no_daemon = TRUE;
		    break;
		case 'm' :
		    no_motd = TRUE;
		    break;
		case 'x' :
		    if (++c < argc)
		        exclude = argv[c];
		    else usage();
		    break;
		case 't' :
		    if (++c < argc)
		        classes = argv[c];
		    else usage();
		    break;
		case 'b' :
		    base = TRUE;
		    break;
		case 'f' :
		    feature = TRUE;
		    break;
		case 's' :
		    showall = TRUE;
		    break;
		case 'a' :
		    if (++c == argc) {
			fprintf(stderr,"alias name needed\n");
			exit(0);
		    }
		    strcpy(hostname, argv[c]);
		    break;
		case 'p' :
		    if (++c == argc) {
                        fprintf(stderr,"port number needed\n");
                        exit(0);
                    }
		    port = atoi (argv[c]);
		    break;
		default : 
		    usage();
		    exit(0);
	    }

    getpath();
#ifndef DEBUG
    if (!no_daemon) BecomeDaemon();
#endif

    /* SysV signals usually create zombie child processes unless SIGCHLD
       is ignored.  -da */

    no_zombies();

    for (;;) {

	succeed=0;
	for (tries=0; (tries<MAXTRIES) && (!succeed); tries++) {

#ifdef DEBUG
	    printf("Try number %d to connect to (%s,%d)\n",tries,METASERVER,port);
#endif

	    if ((sock = open_port(METASERVER,port,1))>0) {

		sprintf(buf,"%s/%s",LOCALSTATEDIR,CAPFILE);
		backup(buf);
		if  ((keyfile=fopen(buf,"w+")) == NULL) {
		    ERROR(1,("Cannot open %s for output\n",buf));
		} else {
		    succeed=TRUE;
		    sprintf(buf,"%s/%s",LOCALSTATEDIR,LOCALKEYS);
		    if ((keylocal=fopen(buf,"r")) != NULL) {
			int c;
			while ((c = getc(keylocal)) != EOF)
			    putc(c, keyfile);
			fclose(keylocal);
		    }
		    while ((num_read = read(sock, buf, BUFSIZ)) != 0) {
			num_written = 0;
			do {
			    num_written += fwrite(buf + num_written, 
					sizeof(*buf),
					num_read - num_written,keyfile); 
#ifdef DEBUG
			    printf("Wrote %d bytes\n",num_written);
#endif
			} while (num_written < num_read);
	            }
		}
		fflush(keyfile);
	 	if (succeed) {
		    succeed=FALSE;
		    rewind(keyfile);
		    while (fgets(buf,BUFSIZ,keyfile))
			if (strstr(buf,"- end of list -")) succeed=TRUE;
		}
	        fclose(keyfile);
	        close(sock);

		if (succeed) {
#ifdef DEBUG
		    printf("Forking....\n");
#endif
	            if (!(fork())) {
		        sprintf(buf,"%s/%s",LIBDIR,KEYCOMP);
			sprintf(buf1,"%s/%s",LOCALSTATEDIR,MOTDLIST);
			sprintf(buf2,"%s/%s",LOCALSTATEDIR,CAPFILE);
#ifdef DEBUG
			printf("Exec'ing %s\n",buf);
#endif
			if (exclude) 
		 	    execl(buf,KEYCOMP,"-c","-m",buf1,"-t",classes,"-x",
				exclude,buf2, (char *) NULL);
 
			else
			    execl(buf,KEYCOMP,"-c","-m",buf1,"-t",classes,
				buf2, (char *) NULL);

		        _exit(1);
		    }
		    else {
			wait(NULL);
			if (!no_motd) {
			    makemotd(N_MOTD,MOTDLOGO);	      /* normal motd */
			    makemotd(N_MOTD_CLUE,MOTDCLUELOGO);	/* clue motd */
#ifdef BASEPRACTICE
			    makemotd(N_MOTD_BASEP,MOTDBASEPLOGO); /* Base prac */
#endif
			}
		    }
		}
	    }	
	    if (!succeed) {
		ERROR(1,("Couldn't connect to metaserver at (%s,%d)\n",METASERVER,port));
	        sleep(INTERVAL);
	    }
	    if (succeed && feature) {
		if ((sock = open_port(METASERVER,FEATUREPORT,1))>0) {
		    if ((featurefile = fopen(Feature_File,"w+")) == NULL) {
			ERROR(1,("Cannot open %s for output\n",Feature_File));
		    } else {
			while ((num_read = read(sock, buf, BUFSIZ)) != 0) {
                            num_written = 0;
                            do {
				num_written += fwrite(buf + num_written,
                                        sizeof(*buf),
                                        num_read - num_written,featurefile);
#ifdef DEBUG
				printf("Wrote %d bytes\n",num_written);
#endif
			    } while (num_written < num_read);
			}
		        fclose(featurefile);
			fprintf(stderr,"Wrote new feature file\n");
                    }
                    close(sock);
		}
		else {
		    ERROR(1, ("Couldn't connect to metaserver at (%s,%d)\n",
			METASERVER,FEATUREPORT));
                    sleep(INTERVAL);
		}
	    }
	}
        if (no_daemon) 
	    exit(0);
	else
	    sleep(SLEEPTIME);
    }

}


void usage(void)
{
	printf("updated [-nmbfs] [-a aliasename] [-x exclude ,...] [-t classes ,...]\n");
	printf("\t-n : No daemon.\n");
	printf("\t-m : Don't make motd(s)\n");
	printf("\t-b : Show base stats instead of top hours\n");
	printf("\t-f : Download feature file\n");
	printf("\t-x : list classes to exclude separated by commas\n");
	printf("\t-t : list classes to include separated by commas\n");
	printf("\t-s : Make motd stats based on everyone\n");
	printf("\t-a : Use alias name for motd stats\n");
}

/*
 * BecomeDaemon.c
 * shamelessly swiped from xdm source code.
 * Appropriate copyrights kept, and hereby follow
 * ERic mehlhaff, 3/11/92
 *
 * xdm - display manager daemon
 *
 * $XConsortium: daemon.c,v 1.5 89/01/20 10:43:49 jim Exp $
 *
 * Copyright 1988 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * Author:  Keith Packard, MIT X Consortium
 */



void BecomeDaemon (void)
{
    int forkresult;

    /*
     * fork so that the process goes into the background automatically. Also
     * has a nice side effect of having the child process get inherited by
     * init (pid 1).
     */

    if ( (forkresult = fork ()) ){	/* if parent */
	  if(forkresult < 0 ){
		perror("Fork error!");
	  }
	  exit(0);			/* then no more work to do */
      }

    /*
     * Close standard file descriptors and get rid of controlling tty
     */

    close (2);

    /*
     * Set up the standard file descriptors.
     */
    errors = open(Error_File, O_WRONLY | O_APPEND | O_CREAT, 0644);
    (void) dup2 (2, 0);
    (void) dup2 (2, 1);

    DETACH
}



/*
 * FOR Making Motd-listing!!! 
 */

#define ESC		'\\'

struct _map {
   char	alias;
   char	actual;
} map[] = {
   { '$',	13 },			/* +-
					   |    */

   { '|',	25 },			/* | */

   { '-',       18 },			/* - */

   { '_',       19 },			/* - lower */

   { '&',       20 },			/* - lowest */

   { '*',       17 },			/* - higher */

   { '=',	16 },			/* - highest */


   { '`',	12 },			/* -+
					    | */

   { '>',	11 },			/*  |
					   -+ */

   { '<',	14 },			/* |
					   +- */

   { '+',	15 },			/* + */

   { '(',	21 },			/* |- */

   { ')',	22 },			/* -| */

   { '^',       23 },                   /* |
					  -+- */
   { '%',	24 },			/* -+- 
					    | */
   { '~',       1 },			/* diamond */

   { '#',       2 },			/* block */
};

void convert (char *in, char *out, char *dir)
{
   int c;
   int i, f;
   FILE		*fi, *fo;

   
   sprintf(buf,"%s/%s",dir,in);
   if ((fi = fopen(buf, "r"))==NULL) {
	perror(buf);
	return;
   }

   if ((fo = fopen(out, "a"))==NULL) {
	perror(out);
	return;
   }
     
   while ((c = getc(fi))!=EOF) {
      if(c == ESC){		/* xx: escape kludge */
	 c = getc(fi);
	 putc(c,fo);
	 continue;
      }

      f = 0;
      for(i=0; i< sizeof(map)/sizeof(struct _map); i++){
	 if(c == map[i].alias){
	    putc(map[i].actual,fo);
	    f = 1;
	    break;
	 }
      }
      if(!f)
	 putc(c,fo);
   }
   fflush(fo);
   fclose(fi);
   fclose(fo);
#ifdef DEBUG 
      printf("Done Converting\n");
#endif
}

void append (char *in, char *out)
{
	FILE *fi, *fo;
	int c;

   if ((fi = fopen(in, "r"))==NULL) {
        perror(in);
        return;
   }

   if ((fo = fopen(out, "a"))==NULL) {
        perror(out);
        return;
   }

   while((c = getc(fi)) != EOF)
	putc(c,fo);

   fflush(fo);
   fclose(fo);
   fclose(fi);
}

void makemotd(char *motd_name, char *logo)
{

    FILE *motd;
    struct tm *today;
    char date[11]; 
    time_t gmtsecs;
    char motd_file[255];

#ifdef DEBUG
    printf("Making MOTD %s\n",motd_file);
#endif

    strcpy(motd_file,Motd_Path);
    strcat(motd_file,motd_name);
    backup(motd_file);	

    convert(logo,motd_file,LOCALSTATEDIR);
    sprintf(buf,"%s/%s",LOCALSTATEDIR,MOTDLIST);
    append(buf,motd_file);
    convert(INFO,motd_file,LOCALSTATEDIR);

    gmtsecs = time(NULL);
    today = localtime(&gmtsecs); 
    sprintf(date,"%04d-%02d-%02d",today->tm_year+1900,(today->tm_mon)+1,
	    today->tm_mday);

    if ((motd=fopen(motd_file,"a"))==NULL) {
	perror(motd_file);
	return;
    }

    fprintf(motd,"\n\n        TOP %d PLAYERS AT %s BY RANK as of %s",
		TOP,hostname,date);
    if (showall) 
	fprintf(motd," (of all time)");
    fprintf(motd,"\n\n");
    fflush(motd);
    scores(0,motd);
    if (!base) {
        fprintf(motd,"\n\n        TOP %d PLAYERS AT %s BY HOURS as of %s",
                TOP,hostname,date);
    	if (showall) 
		fprintf(motd," (of all time)");
	fprintf(motd,"\n\n");
        fflush(motd);
        scores(1,motd);
    } else {
	FILE *pipefp;
	int c;
	fprintf(motd,"\n\n        BASE SCORES AT %s as of %s",
                hostname,date);
    	if (showall) 
		fprintf(motd," (of all time)");
    	fprintf(motd,"\n\n");
	fflush(motd);

	if ((pipefp = popen (BASESCORES, "r")) != NULL){
	    while ((c = getc(pipefp)) != EOF) {
		putc(c, motd);
	    }
	    pclose (pipefp);
	}
	fflush(motd);
    }

    fprintf(motd,"\n-------------------------------------------------------------------------------\n");
    fprintf(motd,"            %s.%d/n", SERVER_VERSION, PATCHLEVEL);
    fclose(motd);
}

void addNewPlayer(char *name, struct player *pl, int mode)
{
    struct rawdesc *temp;
    struct rawdesc **temp2;

    temp=(struct rawdesc *) malloc(sizeof(struct rawdesc));
/*    puts(name);*/
    if (strlen(name) > NAME_LEN) 
	name[NAME_LEN] = '\0';
    strcpy(temp->name, name);
    temp->lastlogin=pl->p_stats.st_lastlogin;
    temp->rank=pl->p_stats.st_rank;
#ifdef LTD_STATS
    temp->ttime		= ltd_ticks(pl, LTD_TOTAL) / 10;
    temp->bombing	= ltd_bombing_rating(pl);
    temp->planets	= ltd_planet_rating(pl);
    temp->offense	= ltd_offense_rating(pl);
    temp->defense	= ltd_defense_rating(pl);
    temp->nbomb		= ltd_armies_bombed(pl, LTD_TOTAL);
    temp->nplan		= ltd_planets_taken(pl, LTD_TOTAL);
    temp->nkill		= ltd_kills(pl, LTD_TOTAL);
    temp->ndie		= ltd_deaths(pl, LTD_TOTAL);
    temp->sbtime	= ltd_ticks(pl, LTD_SB) / 10;
    temp->sbkills	= ltd_kills(pl, LTD_SB);
    temp->sblosses	= ltd_deaths(pl, LTD_SB);
    temp->sbmaxkills	= ltd_kills_max(pl, LTD_SB);
#else
    temp->ttime=pl->p_stats.st_tticks / 10;
    temp->bombing=bombingRating(pl);
    temp->planets=planetRating(pl);
    temp->offense=offenseRating(pl);
    temp->defense=defenseRating(pl);
    temp->nbomb=pl->p_stats.st_tarmsbomb;
    temp->nplan=pl->p_stats.st_tplanets;
    temp->nkill=pl->p_stats.st_tkills;
    temp->ndie=pl->p_stats.st_tlosses;
    temp->sbtime=pl->p_stats.st_sbticks / 10;
    temp->sbkills=pl->p_stats.st_sbkills;
    temp->sblosses=pl->p_stats.st_sblosses;
    temp->sbmaxkills=pl->p_stats.st_sbmaxkills;
#endif
    temp2 = &output;
    for (;;) {
	if (((*temp2)==NULL) || ordered(temp, *temp2, mode)) {
	    temp->next=(*temp2);
	    (*temp2) = temp;
	    break;
	}
	temp2= &((*temp2)->next);
    }
}

int ordered(struct rawdesc *num1, struct rawdesc *num2, int mode)
{
    float temp, temp2;

    if (mode) {
	temp =num1->ttime;
	temp2=num2->ttime;

	if (temp >= temp2) {
	    return(1);
	} else {
	    return(0);
	}
    }
    else {
	if (num1->rank > num2->rank) return(1);
	if (num2->rank > num1->rank) return(0);

	/* 4/2/89  Rank by DI within class.
	 */
    }
    return (0);
}
	 
/* mode=0 is best players in order */ 
/* mode=1 is best by hours */
 
void scores(int mode, FILE *out)
{
    int fd;
    static struct player j;
    struct rawdesc *temp, *temp1;
    int i;
    int lastrank= -1;

    if (out==NULL) {
	perror("Motd File");
	return;
    }

    status=(struct status *) malloc(sizeof(struct status));

    output=NULL;
    fd = open(Global, O_RDONLY, 0777);
    if (fd < 0) {
	ERROR(1,("Cannot open the global file!\n"));
	return;
    }
    read(fd, (char *) status, sizeof(struct status));
    close(fd);
    fd = open(PlayerFile, O_RDONLY, 0777);
    if (fd < 0) {
	perror(PlayerFile);
	return;
    }


    if (!mode) {
	fprintf(out,"    Name               Hours      Planets     Bombing     Offense     Defense\n");
	fprintf(out,"    Totals           %7.2f %11d %11d %11d %11d\n",
	    status->time/36000.0, status->planets, status->armsbomb, 
	    status->kills, status->losses);
    }
    else
	fprintf(out,"    Name               Hours   Ratings   DI    Last login\n");

    i= -1;
    while (read(fd, (char *) &play_entry, sizeof(struct statentry)) == sizeof(struct statentry)) {
	i++;
	j.p_stats = play_entry.stats;
#ifdef LTD_STATS
#ifndef LTD_PER_RACE

        if (j.p_stats.ltd[0][LTD_TOTAL].ticks.total < 1)
          j.p_stats.ltd[0][LTD_TOTAL].ticks.total = 1;

        if (play_entry.stats.ltd[0][LTD_TOTAL].ticks.total < 1)
          play_entry.stats.ltd[0][LTD_TOTAL].ticks.total = 1;

#endif
#else
	if (j.p_stats.st_tticks<1) j.p_stats.st_tticks=1;
	if (play_entry.stats.st_tticks<1) play_entry.stats.st_tticks=1;
#endif

	fixkeymap(play_entry.stats.st_keymap);

	/* Throw away entries with less than 1 hour play time */
#ifdef LTD_STATS
#ifndef LTD_PER_RACE
        if (!showall && play_entry.stats.ltd[0][LTD_TOTAL].ticks.total < 36000)
          continue;
#endif
#else
	if (!showall && play_entry.stats.st_tticks < 36000)
          continue;
#endif

        /* Throw away entries unused for over 4 weeks */
        if (!showall && time(NULL) > play_entry.stats.st_lastlogin + 2419200)
          continue;

        /* Throw away SB's with less than 1 hour play time */
#ifdef LTD_STATS
#ifndef LTD_PER_RACE
        if (!showall && mode=='s' &&
            play_entry.stats.ltd[0][LTD_SB].ticks.total < 36000)
          continue;
#endif
#else
        if (!showall && mode=='s' && play_entry.stats.st_sbticks < 36000)
          continue;
#endif
        addNewPlayer(play_entry.name, &(j), mode);

    }
    close(fd);
    temp=output;
    i=0;
    while ( (temp!=NULL) && (i<TOP) ){
	if (!mode && lastrank != temp->rank) {
		lastrank = temp->rank;
		fprintf(out,"\n%s:\n", ranks[lastrank].name);
	}
	i++;
	if (!mode)
	    fprintf(out,"%3d %-16.16s %7.2f %6d %5.2f %5d %5.2f %5d %5.2f %5d %5.2f\n",
	        i,
	        temp->name,
	        temp->ttime / 3600.0,
	        temp->nplan,
		temp->planets,
		temp->nbomb,
		temp->bombing,
		temp->nkill,
		temp->offense,
		temp->ndie,
		temp->defense);
	else 
	    fprintf(out,"%3d %-16.16s %7.2f %7.2f %8.2f  %s",
		i,
		temp->name,
		temp->ttime / 3600.0,
		temp->bombing+temp->offense+temp->planets,
		(temp->bombing+temp->offense+temp->planets)*temp->ttime/3600.0,
		    ctime((time_t *)&(temp->lastlogin)));

	temp=temp->next;
    }

    /* Now free all of the space that was malloced */
    temp=output;
    while (temp!=NULL) {
	temp1 = temp;
	temp=temp->next;
	free(temp1);
    }
    output = NULL;
    free(status); status = NULL;

    return;
}

void fixkeymap(char *s)
{
    int i;

    for (i=0; i<95; i++) {
	if ((s[i]==0) || (s[i] == 10)) { /* Pig fix (no ^J) 7/8/92 TC */
	    s[i]=i+32;
	}
    }
    s[95]=0;
}
