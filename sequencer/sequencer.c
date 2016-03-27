#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "planet.h"
#include "util.h"

#define KTOURNSTART     0x0e

int debug=0;

/* from inl.c */
void obliterate(int wflag, char kreason)
{
  /* 0 = do nothing to war status, 1= make war with all, 2= make peace with all */
  struct player *j;

  /* clear torps and plasmas out */
  memset(torps, 0, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
  for (j = firstPlayer; j<=lastPlayer; j++) {
    if (j->p_status == PFREE)
      continue;
    j->p_status = PEXPLODE;
    j->p_whydead = kreason;
    if (j->p_ship.s_type == STARBASE)
      j->p_explode = 2 * SBEXPVIEWS ;
    else
      j->p_explode = 10 ;
    j->p_ntorp = 0;
    j->p_nplasmatorp = 0;
    if (wflag == 1)
      j->p_hostile = (FED | ROM | ORI | KLI);         /* angry */
    else if (wflag == 2)
      j->p_hostile = 0;       /* otherwise make all peaceful */
    j->p_war = (j->p_swar | j->p_hostile);
  }
}

/*
**  Commands
*/

/*
**  Sleep for specified number of seconds
*/
void
do_seconds ( char *delay )
{
    int seconds = atoi ( delay );
    if ( seconds > 0 ) sleep ( seconds );
}

/*
**  Sleep for specified number of minutes
*/
void
do_minutes ( char *delay )
{
    int seconds = atoi ( delay ) * 60;
    if ( seconds > 0 ) sleep ( seconds );
}

/*
**  Send a message to all players
*/
void
do_say ( char *message )
{
    if ( message == NULL ) return;
    pmessage ( 0, MALL, "GOD->ALL", message );
}

/*
**  Nuke all players.  Make them explode in order to place them back at the
**  starting line.  They can come back.  Balance does this too.
*/
void
do_nuke ( void *nothing )
{
    obliterate ( 0, KPROVIDENCE );
}

/*
**  Balance the teams.
*/
#if defined (TRIPLE_PLANET_MAYHEM)
void
do_local_balance ( void *nothing )
{
    do_balance();
}
#endif

/*
**  Reset planets, save planet state and player state for later scoring
*/
void
do_reset ( void *nothing )
{
    pl_reset();
    /* save planet/player state ? */
}

/*
**  Freeze everything
*/
void
do_pause ( void *nothing )
{
    status->gameup |= (GU_PRACTICE | GU_PAUSED);
}

/*
**  Continue after freeze
*/
void
do_continue ( void *nothing )
{
    status->gameup &= ~(GU_PRACTICE | GU_PAUSED);
}

/*
**  Examine planets and players, display results to standard output and
**  to the players.
*/
void
do_score ( char *nothing )
{
    printf ( "Score...\n" );
}

/*
**  Eject all players from game.
*/
void
do_eject ( void *nothing )
{
    obliterate ( 0, KQUIT );
}

/*
**  Execute a shell command.
*/
void
do_shell ( char *command )
{
    if ( command == NULL ) return;
    system ( command );
}

/*
**  Exit the sequencer.
*/
void
do_exit ( void *nothing )
{
    exit(0);
}

/*
**  Mainline
*/
int
main(int argc, char *argv[])
{
    int running = openmem(-1);
    
    for ( ;; )
    {
        char buffer[512];
        char *line, *command, *argument;
        
        line = fgets ( buffer, 512, stdin );
        if ( line == NULL ) break;
        
        command = strtok ( line, " \t" );
        argument = strtok ( NULL, "\001" );
        
#define COMMAND(a,b) if ( !strcmp(command,a) ) { b(argument); continue; }
            
        /* commands which do not require game to be up */
        COMMAND ( "shell",    do_shell    );
        COMMAND ( "exit",     do_exit     );
        COMMAND ( "sleep",    do_seconds  );
        COMMAND ( "seconds",  do_seconds  );
        COMMAND ( "minutes",  do_minutes  );
    
        if ( !running ) { running = openmem(-1); }
        if ( !running ) continue;

        /* commands which _do_ require the game to be up */
        COMMAND ( "say",      do_say      );
        COMMAND ( "nuke",     do_nuke     );
#if defined (TRIPLE_PLANET_MAYHEM)
        COMMAND ( "balance",  do_local_balance );
#endif
        COMMAND ( "reset",    do_reset    );
        COMMAND ( "pause",    do_pause    );
        COMMAND ( "continue", do_continue );
        COMMAND ( "score",    do_score    );
        COMMAND ( "eject",    do_eject    );
    }

    return 0;
}
