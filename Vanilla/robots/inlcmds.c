/*
 *  inlcmds.c
 */

/*
   Command interface routines specific to the INL robot.  This is a
   replacement for the overloaded compile method used previously in
   commands.c .	 By seperating the code it will be possible to list
   only those commands relevant at any given moment.  Something that
   would be a horror to implement in the old scheme..
*/

#include "defs.h"
#include "struct.h"
#include "data.h"
#include "gencmds.h"
#include "inldefs.h"

const char myname[] = {"INL"};

extern int do_home();
extern int do_away();
extern int do_start();
extern int do_register();
extern int do_captain();
extern int do_uncaptain();
extern int do_tname();
extern int do_pickside();
extern int do_resetgalaxy();
extern int do_switchside();
extern int do_pause();
extern int do_restart();
extern int do_free();
extern int do_gametime();
extern int do_army();
extern int do_trade();
extern int do_timeout();
extern int do_confine();
extern int do_cscore();
extern int do_scoremode();
extern int do_draft();

extern int cleanup();

int
do_nothing()
{
  return 0;
}

/********* COMMANDS LIST ********
	Note: - The commands *must* be in upper case. All commands are
		converted to upper case before they are checked.
	The first field is the command,
		second is the command flags,
		the third is a description string (up to 50 chars.),
		fourth is the function to call.
	There may be more options depending on the command flags.
*/

struct command_handler_2 inl_commands[] = {
  { "Possible INL commands are:", C_DESC },
  { "HELP",
    0,
    NULL,
    (void (*)()) do_help },				/* HELP */
  { "TIME",
    C_PR_INGAME,
    "Shows the current game time.",
    (void (*)()) do_gametime },			/* GAMETIME */
  { "CAPTAIN",
    0,
    "Make yourself captain of team.",
    (void (*)()) do_captain },			/* CAPTAIN */
  { "|Pregame commands:", C_DESC | C_PR_PREGAME },
  { "HOME",
    C_PR_PREGAME,
    "Switch to home team.",
    (void (*)()) do_switchside },			/* HOME */
  { "AWAY",
    C_PR_PREGAME,
    "Switch to away team.",
    (void (*)()) do_switchside },			/* AWAY */
  { "GAMETIME",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Sets regulation/overtime length. Ex: 'GAMETIME 90 30'",
    (void (*)()) do_gametime },			/* GAMETIME */
  { "TRADE",
    C_PR_CAPTAIN,
    "Trade one/two slots. Ex: 'TRADE a 3', 'TRADE RESET'",
    (void (*)()) do_trade },				/* TRADE */
  { "ARMY",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Set starting armies.  Ex: 'ARMY 17'",
    (void (*)()) do_army },				/* ARMY */
  { "TNAME",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Change the name of your team.",
    (void (*)()) do_tname },				/* TNAME */
  { "NEWGALAXY",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Reset the galaxy.",
    (void (*)()) do_resetgalaxy },			/* RESETGALAXY */
  { "CONFINE",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Confine teams from other races cores.",
    (void (*)()) do_confine },			/* CONFINE */
  { "START",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Start tournament.",
    (void (*)()) do_start },				/* START */
  { "DRAFT",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Pool all players and let captains choose teams.",
    (void (*)()) do_draft },			/* DRAFT */
  { "REGISTER",
    C_PR_CAPTAIN,
    "Toggle official/unofficial state of game.",
    (void (*)()) do_register },			/* REGISTER */
  { "|Team selection commands:", C_DESC | C_PR_CAPTAIN | C_PR_PREGAME},
  { "FED",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Change your side to Federation.",
    (void (*)()) do_pickside },			/* FED */
  { "ROM",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Change your side to Romulan.",
    (void (*)()) do_pickside },			/* ROM */
  { "KLI",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Change your side to Klingon.",
    (void (*)()) do_pickside },			/* KLI */
  { "ORI",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Change your side to Orion.",
    (void (*)()) do_pickside },			/* ORI */
  { "PASS",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Pass the first choice of sides to the home team.",
    (void (*)()) do_pickside },			/* PASS */
  { "|Captain commands:", C_DESC | C_PR_CAPTAIN},
  { "UNCAPTAIN",
    C_PR_CAPTAIN,
    "Release captain duties.",
    (void (*)()) do_uncaptain },			/* UNCAPTAIN */
  { "FREE",
    C_PR_CAPTAIN,
    "Frees a slot. Ex: 'FREE 0'",
    (void (*)()) do_free },				/* FREE */
  { "PAUSE",
    C_PR_CAPTAIN | C_PR_INGAME,
    "Requests a pause.",
    (void (*)()) do_pause },				/* PAUSE */
  { "PAUSENOW",
    C_PR_CAPTAIN | C_PR_INGAME,
    "Imediately pauses the game.",
    (void (*)()) do_pause },				/* PAUSENOW */
  { "TIMEOUT",
    C_PR_CAPTAIN | C_PR_INGAME,
    "Call a timeout.",
    (void (*)()) do_timeout },			/* TIMEOUT */
  { "CONTINUE",
    C_PR_CAPTAIN | C_PR_INGAME,
    "Requests that the game continue.",
    (void (*)()) do_pause },				/* CONTINUE */
  { "RESTART",
    C_PR_CAPTAIN | C_PR_INGAME,
    "Restart, cancel game in progress.",
    (void (*)()) do_restart },			/* RESTART */
  { "SCORE",
    C_PR_INGAME,
    "Shows the INL continuous score.",
    (void (*)()) do_cscore },			/* SCORE */
  { "SCOREMODE",
    C_PR_CAPTAIN | C_PR_PREGAME,
    "Switch scoring mode for winning condition.",
    (void (*)()) do_scoremode },
#if defined(AUTO_INL)
  { "The following votes can be used:	 (M=Majority, T=Team vote)", C_DESC },
  { "EXIT",
    C_VC_ALL,
    "Terminate robot by majority vote.",
    cleanup,
    1, 20, 0 },
#endif /* AUTO_INL */

  /* The following commands do nothing - they are here to
     fool inl.c into thinking a real command was found */
  { "CLIENT", 0, NULL, (void (*)()) do_nothing },
  { "PING", 0, NULL, (void (*)()) do_nothing },
  { "STATS", 0, NULL, (void (*)()) do_nothing },
  { "SBSTATS", 0, NULL, (void (*)()) do_nothing },
  { "QUEUE", 0, NULL, (void (*)()) do_nothing },
  { "WHOIS", 0, NULL, (void (*)()) do_nothing },
  { NULL }
};

extern Inl_stats inl_stat;
extern Team	 inl_teams[];

int
check_command(mess)
     struct message *mess;
{
  int flags = 0;
  int c;
  int num = -1;

  /* Ignore commands that the standard pickup server
     accepts */
  c = mess->m_data[10];
  if (c == '?' || c == '!' || c == '#'
      || c == '^' || c == ':' || c == '@')
    return 1;

  for (c=0; c < INLTEAM; c++)
    {
      if (players[mess->m_from].p_team == inl_teams[c].side)
	{
	  num = c;
	  break;
	}
    }

  if (num != -1)
    {
      if (inl_teams[num].captain == mess->m_from)
	flags |= C_PR_CAPTAIN;
      if (inl_stat.flags & S_PREGAME)
	flags |= C_PR_PREGAME;
      if (inl_stat.flags & (S_TOURNEY | S_OVERTIME))
	flags |= C_PR_INGAME;
    }

  return check_2_command(mess, inl_commands, flags);
}
