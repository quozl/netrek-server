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
   char path[256];
#ifdef CHECK_ENV
   char *env_path;

   if ((env_path = (char *)(getenv(ENV_NAME))) != NULL) sprintf(path,"%s",env_path);
   else
#endif
   sprintf(path,"%s",LIBDIR);

#ifdef GPPRINT
   printf("Using %s as the path to system files.\n",path);
#endif

   sprintf(Global,"%s/%s",path,N_GLOBAL);

   sprintf(Scores,"%s/%s",path,N_SCORES);

   sprintf(PlFile,"%s/%s",path,N_PLFILE);

   sprintf(Motd_Path,"%s/",path);

   sprintf(Daemon,"%s/%s",path,N_DAEMON);

   sprintf(Robot,"%s/%s",path,N_ROBOT);

   sprintf(LogFileName,"%s/%s",path,N_LOGFILENAME);

   sprintf(PlayerFile,"%s/%s",path,N_PLAYERFILE);

   sprintf(ConqFile,"%s/%s",path,N_CONQFILE);

   sprintf(SysDef_File,"%s/%s",path,N_SYSDEF_FILE);

   sprintf(Time_File,"%s/%s",path,N_TIME_FILE);

   sprintf(Clue_Bypass,"%s/%s",path,N_CLUE_BYPASS);

   sprintf(Banned_File,"%s/%s",path,N_BANNED_FILE);

   sprintf(Scum_File,"%s/%s",path,N_SCUM_FILE);

   sprintf(Error_File,"%s/%s",path,N_ERROR_FILE);

   sprintf(Bypass_File,"%s/%s",path,N_BYPASS_FILE);

#ifdef RSA
   sprintf(RSA_Key_File,"%s/%s",path,N_RSA_KEY_FILE);
#endif

#ifdef AUTOMOTD
   sprintf(MakeMotd,"%s/motd/%s",path,N_MAKEMOTD);
#endif

#ifdef CHECKMESG
   sprintf(MesgLog,"%s/%s",path,N_MESGLOG);
   sprintf(GodLog,"%s/%s",path,N_GODLOG);
#endif

#ifdef FEATURES
   sprintf(Feature_File,"%s/%s",path,N_FEATURE_FILE);
#endif

#ifdef ONCHECK
   sprintf(On_File,"%s/%s",path,N_ON_FILE);
#endif

#ifdef BASEPRACTICE
   sprintf(Basep,"%s/%s",path,N_BASEP);
#endif

#ifdef NEWBIESERVER
   sprintf(Newbie,"%s/%s",path,N_NEWBIE);
#endif

#if defined(BASEPRACTICE) || defined(NEWBIESERVER)
   sprintf(Robodir,"%s/%s",path,N_ROBODIR);
#endif

#ifdef DOGFIGHT
   sprintf(Mars,"%s/%s",path,N_MARS);
#endif
   sprintf(Puck,"%s/%s",path,N_PUCK);

   sprintf(Inl,"%s/%s",path,N_INL);

   sprintf(Access_File,"%s/%s",path,N_ACCESS_FILE);
   sprintf(NoCount_File,"%s/%s",path,N_NOCOUNT_FILE);
   sprintf(Prog,"%s/%s",path,N_PROG);
   sprintf(LogFile,"%s/%s",path,N_LOGFILE);

   sprintf(Cambot,"%s/%s",path,N_CAMBOT);
   sprintf(Cambot_out,"%s/%s",path,N_CAMBOT_OUT);
}
