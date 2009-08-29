#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "util.h"

static void usage(void)
{
  fprintf(stderr, "\
Usage: setteam dump\n\
       setteam TEAM COMMAND [COMMAND ...]\n\
\n\
TEAM is a team name prefix.\n\
COMMAND is one of:\n\
\n\
get           dump team data for one team in setteam format\n\
verbose       send messages to all in game for certain changes\n\
\n\
surrender N             set the surrender countdown\n\
reconstruction N        set the starbase reconstruction countdown\n\
");
}

/* send message to players */
static void say(const char *fmt, ...)
{
  va_list args;
  char buf[80];

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  amessage(buf, 0, MALL);
  va_end(args);
}

/* display everything known about a planet in command line format */
void get(char *us, char *name, struct team *te)
{
  printf("%s %s", us, name);
  printf(" reconstruction %d", te->te_turns);
  printf(" surrender %d", te->te_surrender);
  printf("\n");
}

/* display everything about every team */
static void dump(char *us) {
  get(us, team_name(FED), &teams[FED]);
  get(us, team_name(ROM), &teams[ROM]);
  get(us, team_name(KLI), &teams[KLI]);
  get(us, team_name(ORI), &teams[ORI]);
}

int main(int argc, char **argv)
{
    int i, team, verbose = 0;

    if (argc == 1) { usage(); return 1; }
    openmem(0);

    i = 1;

 state_0:
    /* dump - perform a get for each team */
    if (!strcmp(argv[i], "dump")) {
      dump(argv[0]);
      if (++i == argc) return 0;
      goto state_0;
    }

    /* check for team identifier */
    team = team_find(argv[i]);
    if (team == 0) {
      fprintf(stderr, "team %s not known\n", argv[i]);
      return 1;
    }

 state_1:
    if (++i == argc) return 0;
    
    if (!strcmp(argv[i], "get")) {
      get(argv[0], team_name(team), &teams[team]);
      goto state_1;
    }

    if (!strcmp(argv[i], "verbose")) {
      verbose++;
      goto state_1;
    }

    if (!strcmp(argv[i], "reconstruction")) {
      if (++i == argc) return 0;
      if (verbose) say("%s starbase reconstruction timer reset to %s", 
		       team_name(team), argv[i]);
      teams[team].te_turns = atoi(argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "reconstruct")) {
      if (++i == argc) return 0;
      teams[team].te_turns = -1;
      if (verbose) say("%s starbase reconstruction timer reset", 
		       team_name(team));
      goto state_1;
    }

    if (!strcmp(argv[i], "surrender")) {
      if (++i == argc) return 0;
      if (verbose) say("%s surrender timer reset to %s", 
		       team_name(team), argv[i]);
      teams[team].te_surrender = atoi(argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "bust")) {
      if (teams[team].te_surrender > 0) {
	if (verbose) say("%s suffer an economic downturn, surrender timer lowered", 
		       team_name(team));
	teams[team].te_surrender = teams[team].te_surrender / 2;
      }
      goto state_1;
    }

    if (!strcmp(argv[i], "boom")) {
      if (teams[team].te_surrender > 0) {
	if (verbose) say("%s economic boom, surrender timer raised", 
		       team_name(team));
	teams[team].te_surrender = teams[team].te_surrender * 2;
      }
      goto state_1;
    }

    goto state_0;
}
