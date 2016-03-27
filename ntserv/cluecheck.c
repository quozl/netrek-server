/* $Id: cluecheck.c,v 1.2 2006/05/06 12:06:39 quozl Exp $
 */

#include "copyright.h"
#include <stdlib.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

struct Question {
  char *quest;
  char *help[5];
  char *answer;
} ;

static struct Question clueWords[] = {

  {"What does a++ mean?",
   {"a) Player a is a twink, I killed him two times in a row.",
    "b) Player a carries armies, please kill him now.",
    "c) Player a rewls, don't come close to him.",
    NULL },
   "b" },

  {"How many armies can a Starbase carry?",
   { NULL },
   "25" },

  {"Which ship can carry 3 armies per kill?",
   { "sc, dd, ca, as, bb ", NULL },
   "as" },
    
  {"The most important planets are:",
     {"a) agricultural",
      "b) repair",
      "c) fuel",
      NULL },
     "a" },

  {"You can yank another ship off of a planet using your:",
     {"a) phaser",
      "b) tractor",
      "c) engines",
      NULL },
     "b" },
    
  {"The message \"OGG BASE!!\", that means that your teammates want you to:",
     {"a) attack the enemy starbase",
      "b) go to your home base",
      "c) protect your starbase",
      NULL },
     "a" },

  {"You can find out which players are in assault ships by looking at:",
     {"a) The ALL message window",
      "b) The galactic map",
      "c) The player window",
      NULL },
     "c" },

  {"How many core planets does each team start with?",
     {"1, 5, 10",
      NULL },
     "5" },

  {"To help your starbase dodge, the best place to be is:",
     {"a) In FRONT of the base",
      "b) NEXT TO the base",
      "c) BEHIND the base",
      NULL },
     "b" },

  {"How many armies can be carried by a destroyer with three kills?",
     { NULL },
     "5" },

  {"How long is a standard clue game (2007)?",
     {"a) 30 minutes",
      "b) 60 minutes",
      "c) 90 minutes",
      NULL },
     "b" },

  {"How many armies can be carried by an assault ship with 1.5 kills?",
     { NULL },
     "4" },
    
#ifdef CLUE2
  {"An assault ship can bomb at most how many armies at once?",
     { NULL },
     "4" },
  {"How many points of damage can a cruiser's plasma torpedo do?",
     {"a) 40   b) 85  c) 100",
      NULL },
     "c" },
  {"How many total points of damage can a starbase take before exploding?",
     {"a) 600   b) 850  c) 1100",
      NULL },
     "c" },
  {"A scout's torpedoes move at warp:",
     {"12, 14, 16, 18",
      NULL },
     "16" },

  {"A cruiser's torpedoes move at warp:",
     {"12, 14, 16, 18",
      NULL },
     "12" },
#endif

};

static struct Question *question;

static char *clueWords1[] = {
  "doorstop",
  "bronco",
  "plague",
  "moo",
  "transwarp",
  "guzzler",
  "calvin",
  "base",
  "iggy",
  "lock"
  };

static void save_armies(void)
{
  int i, k;

  k=10*(remap[me->p_team]-1);
  if (k>=0 && k<=30) for (i=0; i<10; i++) {
    if (planets[i+k].pl_owner==me->p_team) {
      planets[i+k].pl_armies += me->p_armies;
      pmessage(0, MALL, "GOD->ALL", "%s's %d armies placed on %s",
                     me->p_name, me->p_armies, planets[k+i].pl_name);
      break;
    }
  }
}

static void do_reminder(void)
{
  char **h;

  new_warning(UNDEF,"You have very important messages, Captain!");
  god(me->p_no,"************* READ THIS ** READ THIS ** READ THIS ******************");
  god(me->p_no,"This is a clue server, you MUST read messages in the game.");
  if (clue>1) {
    god(me->p_no,"To verify, send the answer of the following question to yourself.");
    god(me->p_no,question->quest);
    for(h=question->help; *h; h++) god(me->p_no,*h);
  } else {
    godf(me->p_no,"To verify, send the word '%s' to yourself. Thank you.", clueString);
  }
  god(me->p_no,"********************************************************************");
}

void clue_check(void)
{
  static int flag=0;
  int i;

  if ((me->p_status != PALIVE) && (me->p_status != POBSERV))  return;

  if (!flag) {

    if (me->p_status == POBSERV) {
      god(me->p_no," ");
      god(me->p_no,"Observers don't need cluechecking.");
      god(me->p_no,"Because if they don't read, they'll be very bored anyway.");
      god(me->p_no," ");
      clueVerified=1;
      clueFuse=0;
      clueCount=0;
      return;
    }
    if (!minRank && (me->p_stats.st_rank >= cluerank)){
      god(me->p_no," ");
      god(me->p_no,"Due to your rank, you're automatically verified.");
      god(me->p_no," ");
      clueVerified=1;
      clueFuse=0;
      clueCount=0;
      return;
    }
    if (CheckBypass(login,host,Clue_Bypass)) {
      god(me->p_no," ");
      god(me->p_no,"God has determined that you're not a twink. You've been verified.");
      god(me->p_no," ");
      clueVerified=1;
      clueFuse=0;
      clueCount=0;
      return;
    }
	
    if (clue>1) {
      i = ((int) random() % (sizeof(clueWords) / sizeof(clueWords[0])));
      question = &clueWords[i];
      clueString = clueWords[i].answer;
    } else {
      i = ((int) random() % (sizeof(clueWords1) / sizeof(clueWords1[0])));
      clueString = clueWords1[i];
    }
    do_reminder();
    flag=1;
  }

  /* do not clue check in pickup if no wait queue exists */
  if (!inl_mode)
    if (queues[QU_PICKUP].count == 0) return; 

  clueCount++;

  if ((clueCount % ONEMINUTE)==0) {
    do_reminder();
  }

  if (!clueVerified && (clueCount==THREEMINUTES)){
    new_warning(UNDEF,"You have been nuked as warning to read messages, Captain!");
    do_reminder();
    me->p_ship.s_type = STARBASE; 
    me->p_whydead=KPROVIDENCE;
    me->p_explode=10;
    me->p_status=PEXPLODE;
    me->p_whodead=0;
    pmessage(0, MALL, "GOD->ALL", 
	"%s (%2s) was nuked as a warning to read messages.",
        me->p_name, me->p_mapchars);
    if (!(me->p_flags & PFOBSERV) && (me->p_armies>0)) save_armies();
  }

  if (!clueVerified && (clueCount > FIVEMINUTES)) {
    new_warning(UNDEF,"Sorry, you have failed to verify. Read the MOTD");
    pmessage(me->p_no, MINDIV | MCONQ,addr_mess(me->p_no,MINDIV),"You have been ejected from the game for not reading messages");
    pmessage(me->p_no, MINDIV | MCONQ,addr_mess(me->p_no,MINDIV),"If you have any questions, send mail to the server admin.");
    me->p_whydead=KQUIT;
    me->p_explode=10;
    me->p_status=PEXPLODE;
    me->p_whodead=0;
    pmessage(0, MALL, "GOD->ALL", 
	"%s (%2s) has been ejected from the game for not reading msgs.",
        me->p_name, me->p_mapchars);
    if ((me->p_status != POBSERV) && (me->p_armies>0)) save_armies();
  }
}
