/* features.c
 * 
 * Nick Trown  7/93
 */
#include "copyright2.h"
#include <stdio.h>
#include "defs.h"
#include INC_STRINGS
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"


/* NBT
	Just a bunch of strstr(s) to find any of the options that change
	the server's environment. 
*/

static void config_features(char *features)
{
	/* Only one option right now that depends on version */

#ifdef CHAIN_REACTION
	if (!strcmp(features,"WHY_DEAD")) {
	    why_dead = 1;
	}
#endif

#ifdef RCD
	if (!strcmp(features,"RC_DISTRESS")) {
            F_rc_distress = 1;
        }
#endif
	if (!strcmp(features,"SBHOURS")) {
	    SBhours = 1;
	}
}

/*
 * Send the corresponding line in the .features file to the client
 */
void TellClient(char *typ)
{
        FILE *f;
        char buf[BUFSIZ];
        char *features;
	char *min = NULL;
	u_char order = 0;
	u_char warning=0;

	
        if((f = fopen(Feature_File, "r")) == (FILE *) NULL) {
            perror(Feature_File);
            return;
        }

        while(!feof(f)) {
            fgets(buf, sizeof(buf), f);

            if (feof(f))
                continue;

	    if (buf[0]!='#') {				/* comment line */
                if(strstr(buf, typ)) {			/* possible match */
		    if ((buf[strlen(typ)]!='!') && (buf[strlen(typ)]!='*') &&
			(buf[strlen(typ)]!=':')) 
			continue;
		    order = 0;
	 	    min = (char *) strstr(buf, "*confirm:");
		    if (!min) {
			if ((min = (char *) strstr(buf, "!confirm:")))
				order = 1;
		    }		
		    features = (char *)RINDEX(buf, ':') +2;
		    features[strlen(features) - 1] = '\0';
		    if (min) {
			min = (char *)INDEX(min,':')+1;    		/* second ':' */
			min[(char *)RINDEX(min,':') - min] = '\0';
			if (version == 0) {
			    if (!warning) 
				pmessage(me->p_no,MINDIV,"Server: ","%s","Your client is truly ancient! It doesn't even identify itself properly.");
			    warning = 1;
			    if (!order) {
			        fclose(f);
			        return;
			    }
			}
			else if (strcmp(version,min)<0) {
			    if (!warning) 
				pmessage(me->p_no,MINDIV,"Server: ","%s","What an old client you have!! It is missing some features.");
			    warning = 1;
			    if (!order) {
			        fclose(f);
			        return;
			    }
			}
			if ((!order) || ((order) && (strcmp(version,min)<0))) {
			  config_features(features);
			  pmessage(me->p_no,MINDIV|MCONFIG,"SRV:","%s",features);
			}
		    }
		    else {
			config_features(features);
			pmessage(me->p_no,MINDIV|MCONFIG,"SRV:","%s",features);
		    }
		}
	    }
        }
        fclose(f);
}

