/* $Id: sysdefaults.c,v 1.3 2006/05/06 14:02:39 quozl Exp $
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "copyright2.h"
#include "sysdefaults.h"
#include "proto.h"

/* file scope prototypes */
static void shipdefs (int set, FILE *f);
static void readstrings(char *type, char *string, char **keys, int *array,
                        int max);

static struct stat oldstat;

static void setarray ( struct sysdef_array *a, int v )
{
  int i;

  for (i=0; i<a->max; i++) a->p[i] = v;
}

void readsysdefaults(void)
{
    int i,j;
    FILE *f;
    char buf[200];
    char *s;

    /* Setup defaults. */
    setarray ( &sysdefplanets, 0 );
    startplanets[0]=startplanets[10]=startplanets[20]=startplanets[30]=1;
    setarray ( &sysdefships, 1 );
    setarray ( &sysdefweapons, 1 );

    testtime= -1;
    topgun=0; /* added 12/9/90 TC */
    newturn=0; /* added 1/20/91 TC */
    hiddenenemy=1;
    binconfirm=0;
    plkills=2;
    ddrank=0;
    garank=0;
    sbrank=3;
    chaosmode=0;
    tournplayers=5;
    udpAllowed=1;		/* UDP */
    top_armies = 30;            /* added 11/27/93 ATH */
    planet_move = 0;		/* added 9/28/92 NBT */
    wrap_galaxy = 0;		/* added 6/28/95 JRP */
    pingpong_plasma = 0;	/* added 7/6/95 JRP */
#ifdef MINES
    mines = 0;			/* added 8/3/95 JRP */
#endif
    twarpMode = 0;
    twarpSpeed = 60;
    distortion = 10;
    fps = 50;
    maxups = fps;
    minups = 1;
    defups = maxups;
    manager_type = NO_ROBOT;
    strcpy(Motd,N_MOTD);
    strcpy(script_tourn_start, "");
    strcpy(script_tourn_end, "");

    getshipdefaults();

    f=fopen(SysDef_File, "r");
    if (f==NULL) {
	ERROR(1,("No system defaults file!  Using coded defaults.\n"));
        return;
    }
    stat(SysDef_File, &oldstat);

    while (fgets(buf, 200, f)!=NULL) {
	if (buf[0] == '#') continue; /* a comment */
	s=strchr(buf, '='); /* index marked as LEGACY in POSIX.1-2001 */
	if (s==NULL) continue;
	*s='\0';
	s++;

	/* check each keyword from file with our list of keywords */
	for (j=0; sysdef_keywords[j].type != SYSDEF_END; j++) {

	    /* check current keyword for a match */
	    if (strcmp(buf,sysdef_keywords[j].key)==0) {
		switch (sysdef_keywords[j].type) {
		case SYSDEF_INT:
		    {
			int *p = (int *) sysdef_keywords[j].p;
			*p = atoi(s);
		    }
		    break;
		case SYSDEF_FLOAT:
		    {
			float *p = (float *) sysdef_keywords[j].p;
			sscanf (s, "%f", p);
		    }
		    break;
		case SYSDEF_CHAR:
		    sscanf(s,"%s", (char *) sysdef_keywords[j].p);
		    break;
		case SYSDEF_ARRAY:
		    {
			struct sysdef_array *a = sysdef_keywords[j].p;
			setarray(a, 0);
			readstrings(a->name, s, a->keys, a->p, a->max);
		    }
		    break;
		case SYSDEF_ROBOT:
		    /* cast from pointer to integer of different size [ok] */
		    if (atoi(s)) manager_type = (int) sysdef_keywords[j].p;
		    break;
		case SYSDEF_SHIP:
		    shipdefs (atoi(s),f);
		    break;
		}
		break;
	    }
	}	
    }

    /* Make sure every team can start at some planet! */
    for (i=0; i<4; i++) { 
	int k = 0;
	for (j=0; j<10; j++) {
	    if (startplanets[i*10+j]) {
		k++;
		break;
	    }
	}
	if (k == 0) {
	    ERROR(1,("No starting planets specified for team %d\n", i));
	    startplanets[i*10]=1;
	}
    }

    /* apply minimum and maximum values here */
    if (tournplayers == 0) tournplayers=5;
    if (ping_freq <= 0) ping_freq = 1;
    if (ping_iloss_interval <= 0) ping_iloss_interval = 1;
    if (ping_ghostbust_interval <= 0) ping_ghostbust_interval = 1;
    if (ghostbust_timer <= 0) ghostbust_timer = 1;

#ifdef BASEPRACTICE
    if (manager_type == BASEP_ROBOT) {
      binconfirm=0;	/* allow robots to join	*/
#ifdef ALLOW_PRACTICE_ROBOT_SB
      sbrank=0;		/* no SB restriction	*/
#endif
      check_scum=0;
    }
#endif

    if (manager_type == INL_ROBOT) {
      tournplayers=1;
      ddrank=0;		/* no DD restriction		*/
      garank=0;		/* no GA restriction		*/
      sbrank=0;		/* no SB restriction		*/
      killer=0;		/* disable iggy for game	*/
      surrenderStart=0;
    }

    fclose(f);
}

static void readstrings(char *type, char *string, char **keys, int *array,
                        int max)
{
  int i;

  while (*string!='\0') { 
    while (*string=='\n' || *string==' ' || *string==',' || *string=='\t') string++;
    if (*string=='\0') break;
    for (i=0; i<max; i++) {
      if (strncmp(keys[i], string, strlen(keys[i]))==0) {
	string+=strlen(keys[i]);
	array[i]=1;
	break;
      }
    }
    if (i==max) {
      ERROR(1,("%s type %s unknown!\n", type, string));
      string++;
    }
  }
}

/* This function will update all of the defaults if the default file 
 * changed.  It is called often, and assuming the OS caches the file
 * inode info well, this isn't a problem.
 */
int update_sys_defaults(void)
{
  struct stat newstat;

  if (stat(SysDef_File, &newstat)==0) {
    if (newstat.st_ino != oldstat.st_ino ||
	newstat.st_mtime != oldstat.st_mtime) {
      readsysdefaults();
      return(1);
    }
  }
  return(0);
}


/*------------------------------------------------------------------------

 Stolen direct from the Paradise 2.2.6 server code 
 This code is used to load shipvals from the .sysdef

 Added 1/9/94 by Steve Sheldon
-------------------------------------------------------------------------*/

/*--------------------------------SHIPDEFS---------------------------------*/
/*    This function gets all of the field values for a ship.  Each line of
input for the ship fields has a single ship field on it.    The name of the
ship field is followed by the value to place in that field.    The function
stops when it encounters a line with "end" on it.     It places the values
into the shipvals so that the next time getship is called, the new values
will be used.    */

static void shipdefs (int set, FILE *f)
{
  struct ship *currship = NULL;
  char buf[256];				/* to get a string from file */
  char field_name[64];				/* to get name of field */
  char value[256];				/* to get name */
  char sdesig[64];				/* to get ship letters */
  int len;					/* hold len of read in string */
  int i;					/* looping var */
  int offset;					/* offset into ship structure */
  
  for (;;) {					/* loop until break */
    if (0 == fgets (buf, sizeof (buf), f))	/* get a string of input */
      return;					/* if end of file then break */
    if (buf[0]=='!' || buf[0]=='#')
      continue;					/* skip line with ! or # */
    len = strlen (buf);
    if (buf[len - 1] == '\n')			/* blast trailing newline */
      buf[--len] = 0;
    if (strncmp (buf, "END", 3) == 0) {		/* if end of ship then break */
      return;
    }
    if (shipvals == 0)
      continue;
    
    /* get field name and value */
    sscanf (buf, "%s %s %s", sdesig, field_name, value);
    
    if (!set) continue;
    
    for (i = 0; i < NUM_TYPES; i++) {
      if (strcasecmp(sdesig,shiptypes[i])==0) 
	currship = shipvals + i;	
    }
    if (!currship) {
      ERROR(1,("Unknown ship type (%s)\n",sdesig));
      return;
    }
    
    for (i = 0; ship_fields[i].name; i++) {	/* loop through field names */
      if (strcasecmp (field_name, ship_fields[i].name) == 0) /* found field?*/
	break;
    }
    if (ship_fields[i].name == 0) {		/* if we did not find name */
      fprintf (stderr, "invalid field name in ship %s description `%s'(%d)\n",
	       sdesig, field_name, i);
      continue;					/* print error, get next */
    }
    
    offset = ship_fields[i].offset;		/* get offset into struct */
    switch (ship_fields[i].type) {		/* parse the right type */
    case FT_CHAR:
      sscanf (value, "%c", (char *) currship + offset);
      break;
    case FT_SHORT:
      sscanf (value, "%hi", (short *) ((char *) currship + offset));
      break;
    case FT_INT:
      sscanf (value, "%i", (int *) ((char *) currship + offset));
      break;
    case FT_LONG:
      sscanf (value, "%li", (long *) ((char *) currship + offset));
      break;
    case FT_FLOAT:
      sscanf (value, "%f", (float *) ((char *) currship + offset));
      break;
    case FT_STRING:
      sscanf (value, "%s", (char *) currship + offset);
      break;
    default:
      fprintf (stderr, "Internal error, unknown field type %d\n",
	       ship_fields[i].type);
    }
  }
}
