#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "gencmds.h"
#include "proto.h"
#include "util.h"

#ifdef TRIPLE_PLANET_MAYHEM
/*
**  16-Jul-1994 James Cameron
**
**  Balances teams according to player statistics.
**  Intended for use with Triple Planet Mayhem
*/

/*
**  Tell all that player will be moved
*/
static void moveallmsg(int p_no, int ours, int theirs, int p_value)
{
    struct player *k = &players[p_no];
    pmessage(0, MALL, addr_mess(p_no, MALL),
       "Balance: %16s (slot %c, rating %.2f) is to join the %s",
       k->p_name, shipnos[p_no], 
       (float) ( p_value / 100.0 ),
       team_name(ours));
}

/*
**  Move a player to the specified team, if they are not yet there.
**  Make them peaceful with the new team, and hostile/at war with the
**  other team.
*/
static void move(int p_no, int ours, int theirs)
{
  struct player *k = &players[p_no];
    
  if ( k->p_team != ours ) {
    pmessage(k->p_no, MINDIV, addr_mess(k->p_no,MINDIV),
	     "%s: please SWAP SIDES to the --> %s <--", k->p_name, team_name(ours));
  }
  else {
    pmessage(k->p_no, MINDIV, addr_mess(k->p_no,MINDIV),
	     "%s: please remain with the --> %s <--", k->p_name, team_name(ours));
  }
  
  printf("Balance: %16s (%s) is to join the %s\n", 
	 k->p_name, k->p_mapchars, team_name(ours));
  
  change_team(p_no, ours, theirs);
}

/*
**  Return two team masks corresponding to the teams of the first two
**  teams found in the player list.
*/
static void sides (int *one, int *two)
{
    struct player *k;
    int i;
    int unseen;

    unseen = (FED | ROM | ORI | KLI);
    *one = 0;
    *two = 0;
    k = &players[0];
    for(i=0;i<MAXPLAYER;i++)
    {
        if ( (k->p_status != PFREE) && (!(k->p_flags & PFROBOT)))
        {
            if ( ( unseen & k->p_team ) != 0 )
            {
                if ( *one == 0 )
                {
                    *one = k->p_team;
                    unseen &= ~k->p_team;
                    k++;
                    continue;
                }
                *two = k->p_team;
                return;
            }
        }
        k++;
    }
}

/*
**  Calculate a player value
*/
static int value (struct player *k)
{
    return
    (int)
    (
        (float)
        (
#ifdef LTD_STATS
            ltd_bombing_rating(k) * BALANCE_BOMBING +
            ltd_planet_rating(k)  * BALANCE_PLANET  +
            ltd_defense_rating(k) * BALANCE_DEFENSE +
            ltd_offense_rating(k) * BALANCE_OFFENSE
#else
            bombingRating(k) * BALANCE_BOMBING +
            planetRating(k)  * BALANCE_PLANET  +
            defenseRating(k) * BALANCE_DEFENSE +
            offenseRating(k) * BALANCE_OFFENSE
#endif /* LTD_STATS */
        )
    );
}

/*
**  Balance the teams
**
**  Uses an exhaustive algorithm (I'm exhausted!) to find the best combination
**  of the current players that balances the teams in terms of statistics.
**  The algorithm will support only the number of players that fits into the
**  number of bits in an int.
**
**  If there are multiple "best" combinations, then the combination
**  involving the least number of team swaps will be chosen.
*/
void do_balance(void)
{
    int i, j;                   /* miscellaneous counters    */
    int records;                /* number of players in game */
    int one;                    /* team number one mask      */
    int two;                    /* team number two mask      */

    struct player *k;           /* pointer to current player */

    struct item
    {
        int p_no;               /* player number             */
        int p_value;            /* calculated player value   */
        int p_team;             /* team player on previously */
    } list[MAXPLAYER];          /* working array             */

    struct
    {
        int combination;        /* combination number        */
        int value;              /* team balance difference   */
        int one;                /* team one total value      */
        int two;                /* team two total value      */
        int swaps;              /* number of swaps involved  */
    } best;                     /* best team combination     */

    /* which teams are playing?  give up if only one found */
    sides ( &one, &two );
    if ( two == 0 )
    {
	/* addr_mess shouldn't be called with 0 as first arg
         * for MALL, but we don't have a player numer here.
         * Let addr_mess catch it. -da
         */

        pmessage ( 0, MALL, addr_mess(0 ,MALL),
          "Can't balance only one team!" );
        pmessage ( 0, MALL, addr_mess(0 ,MALL),
          "Please could somebody move to another team, then all vote again?" );
        return;
    }

    /* initialise best to worst case */
    best.combination = -1;
    best.value = 1<<30;
    best.one = 0;
    best.two = 0;
    best.swaps = 1<<30;

    /* reset working array */
    for(i=0;i<MAXPLAYER;i++)
    {
        list[i].p_no    = 0;
        list[i].p_value = 0;
    }

    /* insert players in working array */
    records = 0;
    k = &players[0];
    for(i=0;i<MAXPLAYER;i++)
    {
        if ( (k->p_status != PFREE) && (!(k->p_flags & PFROBOT)))
        {
            list[records].p_no    = k->p_no;
            list[records].p_value = value ( k );
            list[records].p_team  = k->p_team;
            records++;
        }
        k++;
    }

    /* randomise the working array; may cause different team mixes */
    for(i=0;i<records;i++)
    {
        int a, b;
        struct item swapper;

        a = random() % records;
        b = random() % records;

        swapper = list[a];
        list[a] = list[b];
        list[b] = swapper;
    }

    /* loop for every _possible_ combination to find the best */
    for(i=0;i<(1<<records);i++)
    {
        int difference;         /* difference in team total       */
        int value_a, value_b;   /* total stats per team           */
        int count_a, count_b;   /* total count of players on team */
        int swaps;              /* number of swaps involved       */

        /* if this a shadow combination already considered, ignore it */
        /* if ( ( i ^ ( ( 1<<records ) - 1 ) ) < i ) continue; */
        /* disabled - it will interfere with swap minimisation goal */
        
        /* is this combination an equal number of players each side? */
        count_a = 0;
        count_b = 0;

        for(j=0;j<records;j++)
            if((1<<j)&i)
                count_a++;
            else
                count_b++;

        /* skip this combination if teams are significantly unequal */
        if ( abs ( count_a - count_b ) > 1 ) continue;

        /* reset team total for attempt */
        value_a = 0;
        value_b = 0;

        /* calculate team total stats */
        for(j=0;j<records;j++)
            if((1<<j)&i)
                value_a += list[j].p_value;
            else
                value_b += list[j].p_value;

        /* calculate number of swaps this combination produces */
        swaps = 0;
        for(j=0;j<records;j++)
            if((1<<j)&i)
            {
                if ( list[j].p_team != one ) swaps++;
	    }
            else
	    {
                if ( list[j].p_team != two ) swaps++;
	    }

        /* calculate difference in team total stats */
        difference = abs ( value_a - value_b );

        /* if this combo is better than the previous one we had,
           or the combo is the same and the number of swaps is lower...  */
        if ( ( difference < best.value )
            || ( ( difference == best.value )
                && ( swaps < best.swaps ) ) )
        {
            /* remember it */
            best.value = difference;
            best.combination = i;
            best.one = value_a;
            best.two = value_b;
            best.swaps = swaps;
        }
    }

    /* don't do balance if not worth it */
    if ( best.swaps != 0 )
    {
        /* move players to their teams */
        for(j=0;j<records;j++)
            if ( (1<<j)&best.combination )
                move ( list[j].p_no, one, two );
            else
                move ( list[j].p_no, two, one );
        
        /* build messages to ALL about the results */
        for(j=0;j<records;j++)
            if ( (1<<j)&best.combination )
                moveallmsg ( list[j].p_no, one, two, list[j].p_value );
    
	/* addr_mess ignores first arg if second is MALL. -da
         */

        pmessage(0, MALL, addr_mess(0, MALL),
            "Balance:                           total rating %.2f",
            (float) ( best.one / 100.0 ) );
        
        for(j=0;j<records;j++)
            if ( !((1<<j)&best.combination) )
                moveallmsg ( list[j].p_no, two, one, list[j].p_value );
    
        pmessage(0, MALL, addr_mess(0, MALL),
            "Balance:                           total rating %.2f",
            (float) ( best.two / 100.0 ) );
    }
    else
    {
        pmessage ( 0, MALL, addr_mess(0, MALL),
            "No balance performed, this is the best: %-3s %.2f, %-3s %.2f",
            team_name(one), (float) ( best.one / 100.0 ),
            team_name(two), (float) ( best.two / 100.0 ) );
    }
    
}

void do_triple_planet_mayhem(void)
{
    int i;

    /* balance the teams */
    do_balance();

    /* move all planets off the galaxy */
    for (i=0; i<MAXPLANETS; i++)
    {
        planets[i].pl_flags = 0;
        planets[i].pl_owner = 0;
        planets[i].pl_x = -10000;
        planets[i].pl_y = -10000;
        planets[i].pl_info = 0;
        planets[i].pl_armies = 0;
        strcpy ( planets[i].pl_name, "" );
    }

    /* disable Klingon and Orion teams; stop people from joining them */
    planets[20].pl_couptime = 999999; /* no Klingons */
    planets[30].pl_couptime = 999999; /* no Orions */

    /* initialise earth */
    i = 0;
    planets[i].pl_flags |= FED | PLHOME | PLCORE | PLAGRI | PLFUEL | PLREPAIR;
    planets[i].pl_x = 40000;
    planets[i].pl_y = 65000;
    planets[i].pl_armies = 40;
    planets[i].pl_info = FED;
    planets[i].pl_owner = FED;
    strcpy ( planets[i].pl_name, "Earth" );

    /* initialise romulus */
    i = 10;
    planets[i].pl_flags |= ROM | PLHOME | PLCORE | PLAGRI | PLFUEL | PLREPAIR;
    planets[i].pl_x = 40000;
    planets[i].pl_y = 35000;
    planets[i].pl_armies = 40;
    planets[i].pl_info = ROM;
    planets[i].pl_owner = ROM;
    strcpy ( planets[i].pl_name, "Romulus" );

    /* initialise indi */
    i = 18;
    planets[i].pl_flags |= PLFUEL | PLREPAIR;
    planets[i].pl_flags &= ~PLAGRI;
    planets[i].pl_x = 15980;
    planets[i].pl_y = 50000;
    planets[i].pl_armies = 4;
    planets[i].pl_info &= ~ALLTEAM;
    strcpy ( planets[i].pl_name, "Indi" );

    /* fix all planet name lengths */
    for (i=0; i<MAXPLANETS; i++)
    {
        planets[i].pl_namelen = strlen(planets[i].pl_name);
    }

    /* advise players */
    {
        char *list[] =
        {
	    "Galaxy reset for triple planet mayhem!",
	    "Rule 1: they take Indi, they win,",
	    "Rule 2: they take your home planet, they win,",
	    "Rule 3: you can't bomb Indi,",
	    "Rule 4: you may bomb their home planet, and;",
	    "Rule 5: all planets are FUEL & REPAIR, home planets are AGRI.",
	    ""
	};

	for ( i=0; strlen(list[i])!=0; i++ )

            /* addr_mess ignores first arg for MALL. -da
             */

            pmessage ( 0, MALL, addr_mess(0,MALL), list[i] );
    }
}
#endif /* TRIPLE_PLANET_MAYHEM */

