/* 
 * getpath.c
 *
 * David Gosselin 11/6/92
 *
 */

#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "data.h"

void getpath()
{
#define MAXPATH 256
   char libdir[MAXPATH], sysconfdir[MAXPATH], localstatedir[MAXPATH];

   snprintf(libdir, MAXPATH-1, "%s", LIBDIR);
   snprintf(sysconfdir, MAXPATH-1, "%s", SYSCONFDIR);
   snprintf(localstatedir, MAXPATH-1, "%s", LOCALSTATEDIR);

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
   sprintf(Cambot_out,"%s/%s",localstatedir,N_CAMBOT_OUT);
}
