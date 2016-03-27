/* 
 * getpath.c
 *
 * David Gosselin 11/6/92
 *
 */

#include "config.h"
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

struct getpath_directories {
  char *bindir, *libdir, *sysconfdir, *localstatedir;
};

struct getpath_directories *getpath_getenv()
{
  static struct getpath_directories g;

  /* inherit directory names from environment */
  g.bindir = getenv("BINDIR");
  g.libdir = getenv("LIBDIR");
  g.sysconfdir = getenv("SYSCONFDIR");
  g.localstatedir = getenv("LOCALSTATEDIR");

  /* default them to source configured values */
  if (g.bindir == NULL) g.bindir = BINDIR;
  if (g.libdir == NULL) g.libdir = LIBDIR;
  if (g.sysconfdir == NULL) g.sysconfdir = SYSCONFDIR;
  if (g.localstatedir == NULL) g.localstatedir = LOCALSTATEDIR;

  return &g;
}

void getpath()
{
  struct getpath_directories *g = getpath_getenv();
  int x = FNAMESIZE - 1;

  /* define the file names */
  snprintf(Global,x,"%s/%s",g->localstatedir,N_GLOBAL);
  snprintf(Scores,x,"%s/%s",g->localstatedir,N_SCORES);
  snprintf(PlFile,x,"%s/%s",g->localstatedir,N_PLFILE);
  snprintf(Motd_Path,x,"%s/",g->sysconfdir);
  snprintf(Daemon,x,"%s/%s",g->libdir,N_DAEMON);
  snprintf(Robot,x,"%s/%s",g->libdir,N_ROBOT);
  snprintf(LogFileName,x,"%s/%s",g->localstatedir,N_LOGFILENAME);
  snprintf(PlayerFile,x,"%s/%s",g->localstatedir,N_PLAYERFILE);
  snprintf(PlayerIndexFile,x,"%s/%s",g->localstatedir,N_PLAYERINDEXFILE);
  snprintf(ConqFile,x,"%s/%s",g->localstatedir,N_CONQFILE);
  snprintf(SysDef_File,x,"%s/%s",g->sysconfdir,N_SYSDEF_FILE);
  snprintf(Time_File,x,"%s/%s",g->sysconfdir,N_TIME_FILE);
  snprintf(Clue_Bypass,x,"%s/%s",g->sysconfdir,N_CLUE_BYPASS);
  snprintf(Banned_File,x,"%s/%s",g->sysconfdir,N_BANNED_FILE);
  snprintf(Scum_File,x,"%s/%s",g->localstatedir,N_SCUM_FILE);
  snprintf(Error_File,x,"%s/%s",g->localstatedir,N_ERROR_FILE);
  snprintf(Bypass_File,x,"%s/%s",g->sysconfdir,N_BYPASS_FILE);

#ifdef RSA
  snprintf(RSA_Key_File,x,"%s/%s",g->sysconfdir,N_RSA_KEY_FILE);
#endif

#ifdef AUTOMOTD
  snprintf(MakeMotd,x,"%s/motd/%s",g->localstatedir,N_MAKEMOTD);
#endif

  snprintf(MesgLog,x,"%s/%s",g->localstatedir,N_MESGLOG);
  snprintf(GodLog,x,"%s/%s",g->localstatedir,N_GODLOG);
  snprintf(Feature_File,x,"%s/%s",g->sysconfdir,N_FEATURE_FILE);

#ifdef BASEPRACTICE
  snprintf(Basep,x,"%s/%s",g->libdir,N_BASEP);
#endif

#ifdef NEWBIESERVER
  snprintf(Newbie,x,"%s/%s",g->libdir,N_NEWBIE);
#endif

#ifdef PRETSERVER
  snprintf(PreT,x,"%s/%s",g->libdir,N_PRET);
#endif

#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
  snprintf(Robodir,x,"%s/%s",g->libdir,N_ROBODIR);
#endif

#ifdef DOGFIGHT
  snprintf(Mars,x,"%s/%s",g->libdir,N_MARS);
#endif
  snprintf(Puck,x,"%s/%s",g->libdir,N_PUCK);
  snprintf(Inl,x,"%s/%s",g->libdir,N_INL);
  snprintf(NoCount_File,x,"%s/%s",g->sysconfdir,N_NOCOUNT_FILE);
  snprintf(Prog,x,"%s/%s",g->libdir,N_PROG);
  snprintf(LogFile,x,"%s/%s",g->localstatedir,N_LOGFILE);
  snprintf(Cambot,x,"%s/%s",g->libdir,N_CAMBOT);
  snprintf(Cambot_out,x,"%s/%s",g->libdir,N_CAMBOT_OUT);
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
  
  struct getpath_directories *g = getpath_getenv();

  /* export the directory paths for use by external scripts */
  setenv("BINDIR", g->bindir, 1);
  setenv("LIBDIR", g->libdir, 1);
  setenv("SYSCONFDIR", g->sysconfdir, 1);
  setenv("LOCALSTATEDIR", g->localstatedir, 1);
}
