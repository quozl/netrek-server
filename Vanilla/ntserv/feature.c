#include "copyright2.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "defs.h"
#include INC_STRINGS
#include "struct.h"
#include "data.h"
#include "packets.h"

#ifdef FEATURE_PACKETS
/*
 * Feature.c
 *
 * March, 1994.    Joe Rumsey, Tedd Hadley
 *
 * most of the functions needed to handle SP_FEATURE/CP_FEATURE
 * packets.  fill in the features list below for your server.
 *
 * feature packets look like:
struct feature_spacket {
   char                 type;
   char                 feature_type;
   char                 arg1,
                        arg2;
   int                  value;
   char                 name[80];
};

 *  type is SP_FEATURE, which is 60.  feature_cpacket is identical.
 */


/* not the actual packets: this holds a list of features to report for */
/* this server. */
struct feature_var {
    char   *name;
    int    *var; /* local variable */
};

struct feature_var feature_vars[] = {
   { "FEATURE_PACKETS",		&F_client_feature_packets },
#ifdef CHAIN_REACTION
   { "WHY_DEAD",		&why_dead },
#endif
   { "SBHOURS",			&SBhours },
   { "SELF_8FLAGS",		&F_self_8flags},
   { "19FLAGS", 		&F_19flags},
#ifdef RCD
   { "RC_DISTRESS",		&F_rc_distress}, /* xx */
#endif
   { "CLOAK_MAXWARP",		&F_cloak_maxwarp},
   { "SHIP_CAP",		&F_ship_cap},
   { "DEAD_WARP",		&dead_warp },
   { "SHOW_ALL_TRACTORS",	&F_show_all_tractors},
   { "SHOW_ARMY_COUNT",		&F_show_army_count},
   { "SHOW_OTHER_SPEED",	&F_show_other_speed},
   { "SHOW_CLOAKERS",		&F_show_cloakers},
   { "SP_GENERIC_32",		&F_sp_generic_32},
   { "FULL_DIRECTION_RESOLUTION",	&F_full_direction_resolution},
   { "FULL_WEAPON_RESOLUTION",	&F_full_weapon_resolution},
   { "CHECK_PLANETS",		&F_check_planets},
   { NULL, NULL },
};


/* file scope prototypes */
static int feature_cmp(char *f, char *s);

/*
 * ------------------- NEW FEATURE STUFF
 */

static int                    num_features;
static struct feature_spacket *features;

static void freeFeatures(void)
{
  free(features);
}

/*
 * Called once to read in feature strings
 */

void readFeatures(void)
{
   register int               i, sv, line=0;
   int                        a1,a2;
   char                       buf[512], value_string[64];
   FILE                       *f;

   f = fopen(Feature_File, "r");
   if(!f) return;

   num_features = 0;
   while(fgets(buf, 512, f)){
      if(buf[0] == '#' || buf[0] == '\n') continue;
      /* old format. Ignore */
      if(INDEX(buf, ':')) continue;
      num_features++;
   }
   fclose(f);

   features = (struct feature_spacket *) calloc(sizeof(struct feature_spacket)
                                              * num_features, 1);
   if(!features) {    /* failsafe */
      perror("calloc");
      num_features = 0;
      return;
   }
   atexit(freeFeatures);

   f = fopen(Feature_File, "r");
   i=0;
   while(fgets(buf, 511, f)){
      line++;

      /* comment */
      if(buf[0] == '#' || buf[0] == '\n') continue;
      /* old format. Ignore */
      if(INDEX(buf, ':')) continue;

      *value_string = 0;
      a1 = 0;
      a2 = 0;

      sv = sscanf(buf, "%s %c %s %d %d",
       features[i].name,
       &features[i].feature_type,
       value_string,
       &a1, &a2);
      features[i].arg1 = a1;
      features[i].arg2 = a2;

      if(!sv) {
       fprintf(stderr, "%s:%d: syntax error on string %s",
          Feature_File, line, buf);
       continue;
      }
      switch(features[i].feature_type){
       case 'C':
       case 'S':
          break;
       default:
          fprintf(stderr, "%s:%d: bad feature type in string %s",
             Feature_File, line, buf);
          continue;
      }
      /* value_string needs to be converted to an integer */
      if(feature_cmp(value_string, "ON"))
       features[i].value = 1;
      else if(feature_cmp(value_string, "OFF"))
       features[i].value = 0;
      /* this probably won't be needed, but just in case ... */
      else if(feature_cmp(value_string, "UNKNOWN"))
       features[i].value = 2;
      else if(!sscanf(value_string, "%d", &features[i].value)){
       fprintf(stderr, "%s:%d: unknown value in string %s",
          Feature_File, line, buf);
       continue;
      }
      i++;
   }
   fclose(f);

   num_features = i;
}

void
getFeature(struct feature_cpacket *cpack, /* client request */
           struct feature_spacket *spack) /* server response */
{
   register int                       i;
   register struct feature_spacket    *f;

   strcpy(spack->name, cpack->name);

   /* default is unknown feature */
   spack->feature_type		= 0;
   spack->value			= -1;
   spack->arg1			= 0;
   spack->arg2			= 0;

    /* find the server's record of the feature */
    for(i=0,f=features; i<num_features; i++,f++) {
	if(feature_cmp(cpack->name, f->name)) {
	    /* This is an evil hack, turn SAT on in obs mode */
	    if(Observer && !strcmp(f->name,"SHOW_ALL_TRACTORS")) f->value = 1;
	    spack->feature_type	= f->feature_type;
	    spack->value	= f->value;
	    spack->arg1		= f->arg1;
	    spack->arg2		= f->arg2;
	    break;
	}
    }

   /* unknown feature name, return */
   if(spack->value < 0) return;

   /* if it's a client option, no further processing needed.
      THe value and args specified in the feature file will already
      be in 'spack' */
   if(spack->feature_type == 'C') return;

   /* server/client options that might need special variables */
   for(i=0; feature_vars[i].name ; i++){
      if(feature_cmp(cpack->name, feature_vars[i].name)) {
       if(feature_vars[i].var){
          if(cpack->value == 1)
             *feature_vars[i].var = (spack->value == 1);
          else {
             spack->value = 0;
             *feature_vars[i].var = 0;
          }
       }
       else {
          /* xx: special features that don't have a single variable
             associated: */
	  /* client values mean nothing here */
	  spack->value = 1;
       }
       return;
      }
   }
   /* unimplemented server feature.  We could be impolite and give a
      warning message here to get the server administrator to add it.. */
   spack->value = -1;
}

/* this is exactly the same as (strcasecmp(f, s)==0), but some OS's don't have
   it */
static int feature_cmp(char *f, char *s)
{
   while(*f && *s){
      if((islower(*f)?toupper(*f):*f) != (islower(*s)?toupper(*s):*s))
       return 0;
      f++; s++;
   }
   return !*f && !*s;
}

#endif


