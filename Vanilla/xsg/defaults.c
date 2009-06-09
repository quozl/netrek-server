/* defaults.c
 * 
 * Kevin P. Smith  6/11/89
 */
#include "copyright2.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct stringlist {
    char *string;
    char *value;
    struct stringlist *next;
};

struct stringlist *defaults=NULL;

char *getenv();

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
	    sprintf(file, "%s/.xsgrc", home);
	} else {
	    strcpy(file, ".xsgrc");
	}
    }
    fp=fopen(deffile, "r");
    if (!fp) return;
    while (fgets(file, 100, fp)) {
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

int strcmpi(str1, str2) 		/* fixed by jwh 2/93 */
char *str1, *str2;
{
  char one, two;
    for(;;) {
        one = *str1;
        two = *str2;
        if (one<='z' && one>='a') one+='A'-'a';
        if (two<='z' && two>='a') two+='A'-'a';
        if (one != two) return(two - one);
        if (one==0 || two==0) return(0);
        str1++;
        str2++;
    }
}

booleanDefault(def, preferred)
char *def;
int preferred;
{
    char *str;
    char strbuf[3];

    str=getdefault(def);
    if (str==NULL) return(preferred);
    if (strcmpi(str,"on")==0) {
	return(1);
    } else {
	return(0);
    }
}
