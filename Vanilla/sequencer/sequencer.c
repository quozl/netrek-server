#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "planets.h"
#include "proto.h"
#include INC_STRINGS

#define KTOURNSTART     0x0e

int debug=0;

/* the four close planets to the home planet */
static int core_planets[4][4] =
{
  {7, 9, 5, 8,},
  {12, 19, 15, 16,},
  {24, 29, 25, 26,},
  {34, 39, 38, 37,},
};
/* the outside edge, going around in order */
static int front_planets[4][5] =
{
  {1, 2, 4, 6, 3,},
  {14, 18, 13, 17, 11,},
  {22, 28, 23, 21, 27,},
  {31, 32, 33, 35, 36,},
};

/* from inl.c */
void obliterate(int wflag, char kreason)
{
  /* 0 = do nothing to war status, 1= make war with all, 2= make peace with all */
  struct player *j;

  /* clear torps and plasmas out */
  MZERO(torps, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
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

void doResources(int startup)
{
  int i, j, k, which;

  /* this is all over the place :-) */
  for (i = 0; i <= MAXTEAM; i++)
    {
      teams[i].s_turns = 0;
    }
  if (startup)
    {
      MCOPY (pdata, planets, sizeof (pdata));

/*
      for (i = 0; i < MAXPLANETS; i++)
        {
          planets[i].pl_info = ALLTEAM;
        }
*/

      for (i = 0; i < 4; i++)
        {
          /* one core AGRI */
          planets[core_planets[i][random () % 4]].pl_flags |= PLAGRI;

          /* one front AGRI */
          which = random () % 2;
          if (which)
            {
              j = random () % 2;
              planets[front_planets[i][j]].pl_flags |= PLAGRI;

              /* give fuel to planet next to agri (hde) */
              planets[front_planets[i][!j]].pl_flags |= PLFUEL;

              /* place one repair on the other front */
              planets[front_planets[i][(random () % 3) + 2]].pl_flags |= PLREPAIR;

              /* place 2 FUEL on the other front */
              for (j = 0; j < 2; j++)
                {
                  do
                    {
                      k = random () % 3;
                    }
                  while (planets[front_planets[i][k + 2]].pl_flags & PLFUEL);
                  planets[front_planets[i][k + 2]].pl_flags |= PLFUEL;
                }
            }
          else
            {
              j = random () % 2;
              planets[front_planets[i][j + 3]].pl_flags |= PLAGRI;
              /* give fuel to planet next to agri (hde) */
              planets[front_planets[i][(!j) + 3]].pl_flags |= PLFUEL;

              /* place one repair on the other front */
              planets[front_planets[i][random () % 3]].pl_flags |= PLREPAIR;

              /* place 2 FUEL on the other front */
              for (j = 0; j < 2; j++)
                {
                  do
                    {
                      k = random () % 3;
                    }
                  while (planets[front_planets[i][k]].pl_flags & PLFUEL);
                  planets[front_planets[i][k]].pl_flags |= PLFUEL;
                }
            }

          /* drop one more repair in the core 
		(home + 1 front + 1 core = 3 Repair) */

          planets[core_planets[i][random () % 4]].pl_flags |= PLREPAIR;

          /* now we need to put down 2 fuel (home + 2 front + 2 = 5 fuel) */

          for (j = 0; j < 2; j++)
            {
              do
                {
                  k = random () % 4;
                }
              while (planets[core_planets[i][k]].pl_flags & PLFUEL);
              planets[core_planets[i][k]].pl_flags |= PLFUEL;
            }
        }
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

    /* get rid of annoying compiler warning, -O will remove this code */
    if (nothing) return;
}

/*
**  Balance the teams by statistics.
*/
extern void do_balance(char *);

/*
**  Reset planets, save planet state and player state for later scoring
*/
void
do_reset ( void *nothing )
{
    doResources(1);
    /* save planet/player state ? */

    /* get rid of annoying compiler warning, -O will remove this code */
    if (nothing) return;
}

/*
**  Freeze everything
*/
void
do_pause ( void *nothing )
{
    status->gameup |= (GU_PRACTICE | GU_PAUSED);

    /* get rid of annoying compiler warning, -O will remove this code */
    if (nothing) return;
}

/*
**  Continue after freeze
*/
void
do_continue ( void *nothing )
{
    status->gameup &= ~(GU_PRACTICE | GU_PAUSED);

    /* get rid of annoying compiler warning, -O will remove this code */
    if (nothing) return;
}

/*
**  Examine planets and players, display results to standard output and
**  to the players.
*/
void
do_score ( char *nothing )
{
    printf ( "Score...\n" );

    /* get rid of annoying compiler warning, -O will remove this code */
    if (nothing) return;
}

/*
**  Eject all players from game.
*/
void
do_eject ( void *nothing )
{
    obliterate ( 0, KQUIT );

    /* get rid of annoying compiler warning, -O will remove this code */
    if (nothing) return;
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

    /* get rid of annoying compiler warning, -O will remove this code */
    if (nothing) return;
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
        COMMAND ( "balance",  do_balance  ); /* see commands.c */
#endif
        COMMAND ( "reset",    do_reset    );
        COMMAND ( "pause",    do_pause    );
        COMMAND ( "continue", do_continue );
        COMMAND ( "score",    do_score    );
        COMMAND ( "eject",    do_eject    );
    }

    return 0;
}
