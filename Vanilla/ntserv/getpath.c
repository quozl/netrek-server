/* 
 * getpath.c
 *
 * David Gosselin 11/6/92
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

/*

getpath() initialises all the file name global variables, by
concatenating one or more directory names and a file name to make a
relative path to a file.

The file name global variables are in data.c and data.h, but are not
initialised there.  They are to be CamelCase.

The file name macros are in defs.h.  They are to begin with N_.

When adding a new file, you must edit each of data.c, data.h, and defs.h.

When using a file from other code, you must include "data.h", and your
main() must call getpath().

*/

void getpath()
{
#define MAXPATH 256
   char libdir[MAXPATH], sysconfdir[MAXPATH], localstatedir[MAXPATH];

   /* define the directory names */
   snprintf(libdir, MAXPATH-1, "%s", LIBDIR);
   snprintf(sysconfdir, MAXPATH-1, "%s", SYSCONFDIR);
   snprintf(localstatedir, MAXPATH-1, "%s", LOCALSTATEDIR);

   /* define the file names */
   sprintf(Global,"%s/%s",localstatedir,N_GLOBAL);
   sprintf(Scores,"%s/%s",localstatedir,N_SCORES);
   sprintf(PlFile,"%s/%s",localstatedir,N_PLFILE);
   sprintf(Motd_Path,"%s/",sysconfdir);
   sprintf(Daemon,"%s/%s",libdir,N_DAEMON);
   sprintf(Robot,"%s/%s",libdir,N_ROBOT);
   sprintf(LogFileName,"%s/%s",localstatedir,N_LOGFILENAME);
   sprintf(PlayerFile,"%s/%s",localstatedir,N_PLAYERFILE);
   sprintf(PlayerIndexFile,"%s/%s",localstatedir,N_PLAYERINDEXFILE);
   sprintf(ConqFile,"%s/%s",localstatedir,N_CONQFILE);
   sprintf(SysDef_File,"%s/%s",sysconfdir,N_SYSDEF_FILE);
   sprintf(Time_File,"%s/%s",sysconfdir,N_TIME_FILE);
   sprintf(Clue_Bypass,"%s/%s",sysconfdir,N_CLUE_BYPASS);
   sprintf(Banned_File,"%s/%s",sysconfdir,N_BANNED_FILE);
   sprintf(Scum_File,"%s/%s",localstatedir,N_SCUM_FILE);
   sprintf(Error_File,"%s/%s",localstatedir,N_ERROR_FILE);
   sprintf(Bypass_File,"%s/%s",sysconfdir,N_BYPASS_FILE);

#ifdef RSA
   sprintf(RSA_Key_File,"%s/%s",sysconfdir,N_RSA_KEY_FILE);
#endif

#ifdef AUTOMOTD
   sprintf(MakeMotd,"%s/motd/%s",localstatedir,N_MAKEMOTD);
#endif

#ifdef CHECKMESG
   sprintf(MesgLog,"%s/%s",localstatedir,N_MESGLOG);
   sprintf(GodLog,"%s/%s",localstatedir,N_GODLOG);
#endif

#ifdef FEATURES
   sprintf(Feature_File,"%s/%s",sysconfdir,N_FEATURE_FILE);
#endif

#ifdef ONCHECK
   sprintf(On_File,"%s/%s",sysconfdir,N_ON_FILE);
#endif

#ifdef BASEPRACTICE
   sprintf(Basep,"%s/%s",libdir,N_BASEP);
#endif

#ifdef NEWBIESERVER
   sprintf(Newbie,"%s/%s",libdir,N_NEWBIE);
#endif

#ifdef PRETSERVER
   sprintf(PreT,"%s/%s",libdir,N_PRET);
#endif

#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
   sprintf(Robodir,"%s/%s",libdir,N_ROBODIR);
#endif

#ifdef DOGFIGHT
   sprintf(Mars,"%s/%s",libdir,N_MARS);
#endif
   sprintf(Puck,"%s/%s",libdir,N_PUCK);
   sprintf(Inl,"%s/%s",libdir,N_INL);
   sprintf(Access_File,"%s/%s",sysconfdir,N_ACCESS_FILE);
   sprintf(NoCount_File,"%s/%s",sysconfdir,N_NOCOUNT_FILE);
   sprintf(Prog,"%s/%s",libdir,N_PROG);
   sprintf(LogFile,"%s/%s",localstatedir,N_LOGFILE);
   sprintf(Cambot,"%s/%s",libdir,N_CAMBOT);
   sprintf(Cambot_out,"%s/%s",libdir,N_CAMBOT_OUT);
}

void setpath()
{
  char *old, *new;
  int len, siz;

  /* get the current PATH */
  old = getenv("PATH");
  if (old == NULL) {
    fprintf(stderr, "setpath: the PATH environment variable has no value\n");
    exit(1);
  }

  /* do not set PATH if PATH already contains our LIBDIR and BINDIR */
  if (strstr(old, LIBDIR) != NULL && strstr(old, BINDIR) != NULL) {
    /* fprintf(stderr, "setpath: PATH has what we need\n%s\n", old); */
    return;
  }

  /* calculate and allocate space for a new PATH */
  len = strlen(BINDIR) + strlen(LIBDIR) * 2 + 64 + strlen(old);
  new = malloc(len+1);
  if (new == NULL) {
    fprintf(stderr, "setpath: unable to allocate memory for new PATH\n");
    exit(1);
  }

  /* compose the new PATH by prefixing existing PATH with LIBDIR references */
  siz = snprintf(new, len, "%s:%s:%s/tools:%s", 
                 BINDIR, LIBDIR, LIBDIR, old);
  if (siz > len) {
    fprintf(stderr, "setpath: snprintf: exceeded allocated buffer\n");
    exit(1);
  }
  if (siz < 0) {
    fprintf(stderr, "setpath: snprintf: output error\n");
    exit(1);
  }

  /* set the new PATH */
  if (setenv("PATH", new, 1) < 0) {
    fprintf(stderr, "setpath: unable to set new PATH, insufficient space\n");
    exit(1);
  }
  free(new);
  
  /* export the directory paths for use by external scripts */
  setenv("BINDIR", BINDIR, 1);
  setenv("LIBDIR", LIBDIR, 1);
  setenv("SYSCONFDIR", SYSCONFDIR, 1);
  setenv("LOCALSTATEDIR", LOCALSTATEDIR, 1);
}
