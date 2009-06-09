/******************************************************************************

How to use this program :

If you've got an established server, and have saved your .conquer file, this 
is what you need to run to convert your player DB over to one which keeps 
track of the number of times each character has been on the winning side of a 
genocide.

Compile up this program.  It lives in the ./tools directory, and doesn't need 
to get installed (it's a one-shot conversion program).  "make conq_vert" from 
the ./tools directory should do it.  

Make sure your server is down (one should _always_ have the server off when 
fooling with the scores DB, or Weird Things will happen).  

run the program with three parameters - .conquer file, .players file, and the 
new.players file.  For example, from the server home directory :

        ./tools/conq_vert ./lib/.conquer ./lib/.players new.players

Warning - stdout is an unordered dump of the players and their genos, useful 
but annoying unless you > it to a file.

Then, save your old .players file (in case of weird foo!), and mv new.players
(or whatever you called it) to .lib/.players.  In your config.h.in, make sure
you've set the #define "GENO_COUNT" on.  If this is a 2.02pl1 server or lower,
apply the patches needed.  (Contact Alec Habig,
ahabig@bigbang.astro.indiana.edu).  Make clean (to ensure that everything gets
recompiled with the right structures), configure, recompile, reinstall, and
start up the server.
******************************************************************************/
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include "defs.h"
#include "struct.h"

struct node {              /* A structure for counting up genocides */
  int count;
  char data[21];
  struct node *next;
};

void count(char *item, struct node **pointer)
        /* FILO stack of nodes, this creates it */
/* char *item;                =  A stack of names with #'s of geno's. */
/* struct node **pointer;     =  Not efficient, but Oh Well. */
{
  struct node *temp;

  temp = (*pointer);
  if (temp==NULL) {
    temp=(struct node *) malloc(sizeof(struct node));
    temp->count = 1;
    temp->next = NULL;
    strcpy(temp->data,item);
    (*pointer)=temp;
    return;
  } else if (!strcmp(item,temp->data)) {
    temp->count++;
    return;
  } else {
    count(item,&(temp->next));
  }
  return;
}

void spew(struct node *pointer)
				/* This routine cleans up the names, and */
				/* and prints them to stdout, given a root. */
{
  int i;
  int offset;
  char temp [NAME_LEN];

  if(pointer==NULL) return;
  else {
    offset = 0;
    while(pointer->data[offset]<'A') offset++;
    for (i=offset; (pointer->data[i]!='\0');i++)
      temp[i-offset]=pointer->data[i];
    temp[i-offset]='\0';
    strcpy(pointer->data,temp);
    printf("%d %s\n",pointer->count,pointer->data);
    spew(pointer->next);
  }
  return;
}

int find_genos(struct node *pointer, char name[NAME_LEN])
				/* This routine returns the number of */
				/* genos given a name and a stack root.*/
{
  if(pointer==NULL) return(0);
  else if (!strcmp(name,pointer->data)) return(pointer->count);
  else {
    return find_genos(pointer->next,name);
  }
}

void clean(char *item)        /* This routine cleans up the .conquer file */
                              /* records, for use with this program.  */
{
  int i;
  static char losers[17];
  static int skip;

/* Clean out \n's */

  for(i=0;((i<80)&&(item[i]!='\0')); i++)
    if ((item[i]=='\n')||(item[i]=='\0')) {
      item[i]='\0';
      item[i+1]='\0';
    }

/* Skip losing side in "Genocide" messages */

  if (item[0]=='\0') return;

  if ((!strncmp(item,"Genocide!",9)) ||
      (!strncmp(item,"Conquer!",8))){
    item[0] = '\0';
    skip = 0;
    return;
  }

  if ((losers[0]!='\0')&&(!strncmp(losers,item,16))) {
    losers[0] = '\0';
    skip = 1;
  }

  if (skip) {
    item[0] = '\0';
    return;
  }

  if (strstr(item,"been genocided")!=NULL) {
    strncpy(losers,item,16);
    losers[16] = '\0';
  }

/* Clean out other .conquer headers */

  if (((strlen(item)<5)) ||
      (!strncmp(item,"Surrender!",10)) ||
      (!strncmp(item,"  The ",6)))  {
    item[0]='\0';
    return;
  }

/* It passed all these tests, must be a valid name.  Truncate it at 20. */

  item[20] = '\0';
}

int main(int argc, char **argv)
{
  struct statentry {
    char name[NAME_LEN], password[NAME_LEN];
    struct stats stats;
  };

  struct new_stats {
    double st_maxkills;		/* max kills ever */
    int st_kills;		/* how many kills */
    int st_losses;		/* times killed */
    int st_armsbomb;		/* armies bombed */
    int st_planets;		/* planets conquered */
    int st_ticks;		/* Ticks I've been in game */
    int st_tkills;		/* Kills in tournament play */
    int st_tlosses;		/* Losses in tournament play */
    int st_tarmsbomb;		/* Tournament armies bombed */
    int st_tplanets;		/* Tournament planets conquered */
    int st_tticks;		/* Tournament ticks */
				/* SB stats are entirely separate */
    int st_sbkills;		/* Kills as starbase */
    int st_sblosses;		/* Losses as starbase */
    int st_sbticks;		/* Time as starbase */
    double st_sbmaxkills;       /* Max kills as starbase */
    LONG st_lastlogin;		/* Last time this player was played */
    int st_flags;		/* Misc option flags */
    char st_keymap[96];		/* keymap for this player */
    int st_rank;		/* Ranking of the player */
    int st_genos;               /* # of genos player has */
  };

  struct new_statentry {
    char name[NAME_LEN], password[NAME_LEN];
    struct new_stats stats;
  };

  FILE *infile;
  int inLUN;
  int outLUN;
  char buff[81];
  struct node *stack = NULL;
  struct statentry inrecord;
  struct new_statentry outrecord;

/* Deal with arguments */

  if (argc>1) {
    if((infile=fopen(argv[1],"r"))==NULL) {
      printf("Cannot open %s\n",argv[1]);
      exit(1);
    } }
  else {
    printf("usage - conq_vert [.conquer_name] [.players_name] [.new_name]\n");
    exit(1);
  }

/* Accumulate .conquer stats */

  while(fgets(buff,81,infile)) {
    buff[35]='\0';
    clean(buff);
    if (buff[0]!='\0') count(buff,&stack);
  }
  fclose(infile);
  spew(stack);

/* Read in scores A (stdin), check for .conquers, write out new .players */

  inLUN = open(argv[2],O_RDONLY,0777);
  if (inLUN < 0) { 
    perror(argv[2]);
    exit(1);
  }

  outLUN = open(argv[3],O_WRONLY|O_CREAT|O_TRUNC,0600);
  if (outLUN < 0) { 
    perror(argv[3]);
    exit(1);
  }

    while (read(inLUN, (char *) &inrecord, sizeof(struct statentry)) == sizeof(struct statentry)) {
      memcpy(&outrecord,&inrecord,sizeof(struct statentry));
      outrecord.stats.st_genos = find_genos(stack,inrecord.name);
      write(outLUN, (char *) &outrecord, sizeof(struct new_statentry));  
    }

  close(inLUN);
  close(outLUN);
  return (1);			/* satisfy lint */
}

