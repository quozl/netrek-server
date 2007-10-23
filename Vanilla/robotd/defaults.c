/* defaults.c
 * 
 * Kevin P. Smith  6/11/89
 */
#include "copyright2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct stringlist {
    char *string;
    char *value;
    struct stringlist *next;
};

struct stringlist *defaults=NULL;

#ifndef __FreeBSD__
char *getenv();
#endif

initDefaults(deffile)
char *deffile;		/* As opposed to defile? */
{
    FILE *fp;
    char file[100];
    char *home;
    char *v;
    struct stringlist *new;

    if (!deffile) {
	deffile=file;
	home=getenv("HOME");
	if (home) {
	    sprintf(file, "%s/.xtrekrc", home);
	} else {
	    strcpy(file, ".xtrekrc");
	}
    }
    fp=fopen(deffile, "r");
    if (!fp) return;
    while (fgets(file, 99, fp)) {
	if (*file=='#') continue;
	if (*file!=0) 
	    file[strlen(file)-1]=0;
	v=file;
	while (*v!=':' && *v!=0) {
	    v++;
	}
	if (*v==0) continue;
	*v=0;
	v++;
	while (*v==' ' || *v=='\t') {
	    v++;
	}
	if (*v!=0) {
	    new=(struct stringlist *) malloc(sizeof(struct stringlist));
	    new->next=defaults;
	    new->string=strdup(file);
	    new->value=strdup(v);
	    defaults=new;
	}
    }
    fclose(fp);
}

char *getdefault(str)
char *str;
{
    struct stringlist *sl;

    sl=defaults;
    while (sl!=NULL) {
	if (strcmpi(sl->string, str)==0) {
	    return(sl->value);
	}
	sl=sl->next;
    }
    return(NULL);
}

strcmpi(const char * str1, const char * str2)
{
/*#ifdef strcasecmp */
    return (strcasecmp(str1,str2));
/*#else
    char chr1, chr2;
    for(;;) {
        chr1 = isupper(*str1) ? *str1 : toupper(*str1);
        chr2 = isupper(*str2) ? *str2 : toupper(*str2);
        if (chr1 != chr2) return(chr2 - chr1);
        if (chr1==0 || chr2==0) return(1);
        if (chr1==0 && chr2==0) return(0);
        str1++;
        str2++;
    }
    return (0);
#endif
*/
}

booleanDefault(def, preferred)
char *def;
int preferred;
{
    char *str;

    str=getdefault(def);
    if (str==NULL) return(preferred);
    if (strcmpi(str,"on")==0) {
	return(1);
    } else {
	return(0);
    }
}
