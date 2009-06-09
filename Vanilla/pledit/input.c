/*
 * Netrek player DB maintenance
 *
 * input.c - get user input
 */

#include "config.h"

#ifndef LTD_STATS

#include <signal.h>
#include <unistd.h>
#include <curses.h>
#include <string.h>
#include "pledit.h"
#include "defs.h"
#include "struct.h"
#include "data.h"

/*
 * ---------------------------------------------------------------------------
 *	Constants and stuff
 * ---------------------------------------------------------------------------
 */

/*
 * 0: vi keys
 * 1: emacs keys
 * 2: vt100 keys
 * 3: NCD keys
 */
#define DEFAULT_KEY	0		/* vi keys */

static int kbd_type = DEFAULT_KEY;

static /*volatile*/ int alarm_hit;


/*
 * terminal-specific keyboard controls
 *
 * It would be possible to grab definitions for some of this stuff from
 * termcap entries, but that isn't very portable (for example, my system
 * doesn't HAVE termcap entries).  Besides, this lets the user use alternate
 * key bindings, like vi or emacs, instead of PgUp keys.
 *
 * Note that some systems, notably UTS, have extended curses(3x) packages
 * which include handling for keypad keys.  However, in the interests of
 * portability, I'm restraining myself and doing it the hard way.
 *
 * NOTE: the "edit" mode uses the KY_ESC key and the two TAB keys.  Thus
 * it's okay to use the ESC key  for KY_ESC so long as the tab keys don't.
 * (If there IS a conflict, then the handling of KY_ESC will be delayed for
 * a second, which is kind of annoying.)
 *
 * Suggestion for anyone who wants to hack this: set it up as a map rather than
 * as a "0 == up".  That way you can have more than one key to do searches,
 * etc, etc.  You should also merge these with the terminal-independent ones;
 * this will require some duplication of data (i.e. each map will have the
 * same set of terminal-independent stuff in it), but will be better in the
 * long run.  Will require a more complicated setup for the help screen, but
 * will allow one screen showing maps instead of '?' and 'K'.  Might also be
 * nice to include another character string field for keypad key names, so
 * that instead of showing "^[[p" in the 'K' menu, you could show "Home".
 * The "emacs" mode suggests that a more complicated delay feature may be
 * desireable, perhaps one which knows the difference between a keypad prefix
 * and a user key-combination like Esc-V.  Maybe when the keypad delay times
 * out, it would fall into "user input mode", and would recognize the Escape
 * sequence as an abort signal.  Lots of room for improvement.  Learn from my
 * mistakes...
 *
 * 0: up
 * 1: down
 * 2: lft	(unused)
 * 3: rgt	(unused)
 * 4: pgup
 * 5: pgdn
 * 6: tab
 * 7: bktab
 * 8: home
 * 9: end
 * 10: escape
 * 11: fwd_srch
 */
#define KT_SIZE	12	/* #of different keys */
#define LONG_KY	8
typedef struct keyseq_t {
    char keys[KT_SIZE+1][LONG_KY+1];
    char *name;
} KEYSEQ;

KEYSEQ keytab[] = {
    /* vi controls */
    {   {
	"k",		/* k */
	"j",		/* j */
	"h",		/* h */
	"l",		/* l */
	"\002",		/* ^B */
	"\006",		/* ^F */
	"\t",		/* TAB */
	"\027",		/* ^W */	/* or maybe ^D?? */
	"g",		/* g */
	"G",		/* G */
	"\033",		/* ESC */
	"/",		/* / */
	},

	"vi keys",
    },

    /* emacs controls */
    {   {
	"\020",		/* ^P */
	"\016",		/* ^N */
	"\002",		/* ^B */
	"\006",		/* ^F */
	"\033v",	/* ESC-v */
	"\026",		/* ^V */
	"\t",		/* TAB */
	"\033\t",	/* ESC-TAB */
	"\033<",	/* ESC-< */
	"\033>",	/* ESC-> */
	"\007",		/* ^G */
	"\022",		/* ^R (forward search conflicts with XOFF(^S)) */
	},
	"emacs keys",
    },

    /* VT-100 controls */
    {   {
	"\033A",	/* ESC-A */
	"\033B",	/* ESC-B */
	"\033D",	/* ESC-D */
	"\033C",	/* ESC-C */
	"\0335~",	/* ESC-5~ */
	"\0336~",	/* ESC-6~ */
	"\t",		/* TAB */
	"\002",		/* ^B */
	"\033H",	/* ESC-H (Home) */
	"G",		/* G (no end??) */
	"\0334~",	/* ESC-4~ (Select) */
	"\0331~",	/* ESC-1~ (Find) */
	},
	"VT-100 keys",
    },

    /* NCD xterm controls */
    {   {
	"\033[A",	/* ESC-[A */
	"\033[B",	/* ESC-[B */
	"\033[D",	/* ESC-[D */
	"\033[C",	/* ESC-[C */
	"\033[5~",	/* ESC-5~ */
	"\033[6~",	/* ESC-6~ */
	"\t",		/* TAB */
	"\002",		/* ^B */
	"\033[p",	/* ESC-[p (Home) */
	"G",		/* G (end == up_arrow??) */
	"\033",		/* ESC (This can be confusing, but *I* like it) */
	"/",		/* / */
	},
	"NCD xterm keys",
    },
};
#define KT_NSIZE	(sizeof(keytab) / sizeof(KEYSEQ))


/*
 * terminal-independent stuff (command keys)
 *
 * None of the terminal-dependent stuff should start with one of these
 * characters.
 */

typedef struct map_t {
    char *str;
    int val;
} MAP;

MAP keytab2[] = {
    { "Q",	KY_QUIT },
    { "K",	KY_KEYS },
    { "?",	KY_HELP },
    { "a",	KY_ADD },
    { "e",	KY_EDIT },
    { "d",	KY_DELETE },
    { "u",	KY_UNDELETE },
    { "w",	KY_WRITE },
    { "\014",	KY_REDRAW },	/* ^L */
/*    { "\022",	KY_REDRAW }, */	/* ^R */
};
#define KT2_SIZE	(sizeof(keytab2) / sizeof(MAP))

/*
 * ---------------------------------------------------------------------------
 *	The Program
 * ---------------------------------------------------------------------------
 */

#define HELP_WIDTH	52	/* try not to overlap global window */
#define HELP_HEIGHT	17
static void
help_screen()
{
    WINDOW *w;

    if ((w = newwin(HELP_HEIGHT, HELP_WIDTH, (LINES-HELP_HEIGHT)/2,
	    (COLS - HELP_WIDTH) /2)) == NULL)
	fatal(0, "couldn't make help screen");

    box(w, VERTCH, HORCH);
    mvwprintw(w, 0, (HELP_WIDTH - 6)/2, "%s", " help ");
    /* you know, there's no easy way to do this with my structures... oh well */
    mvwprintw(w, 2, 2, "The following are available on all terminals:");
    mvwprintw(w, 4, 2, "%c  add player", 'a');
    mvwprintw(w, 4, HELP_WIDTH/2, "%c  edit player", 'e');
    mvwprintw(w, 5, 2, "%c  delete player", 'd');
    mvwprintw(w, 5, HELP_WIDTH/2, "%c  undelete player", 'u');
    mvwprintw(w, 6, 2, "%c  cursor key config", 'K');
    mvwprintw(w, 6, HELP_WIDTH/2, "%c  help screen", '?');
    mvwprintw(w, 7, 2, "%c  write files", 'w');
    mvwprintw(w, 7, HELP_WIDTH/2, "%c  quit", 'Q');
    mvwprintw(w, 9, 2, "Cursor motion and search are terminal");
    mvwprintw(w, 10, 2, "dependent:");
    mvwprintw(w, 12, 4, "UP, DN, LFT, RGT, PGUP, PGDN, TAB, Back-TAB,");
    mvwprintw(w, 13, 4, "HOME, END, SEARCH, ESCAPE");
    mvwprintw(w, HELP_HEIGHT-2, 2, "Press a key to return: ");

    wrefresh(w);
    wgetch(w);		/* wait for key */

    delwin(w);
#ifdef ENHANCED_CURSES
    touchline(mainw, (LINES-HELP_HEIGHT)/2 -2, HELP_HEIGHT);
#else
    touchwin(mainw);
#endif
    wrefresh(mainw);
}

static char *
visprt(str)
char *str;
{
    static char out[256];
    int i, len;

    if ((len = strlen(str)) > 128)
	fatal(0, "internal error: huge visprt req (%d)", len);
    out[0] = '\0';
    for (i = 0; i < strlen(str); i++)
	strcat(out, unctrl(str[i]));
    
    return (out);
}

#define KEYM_WIDTH	52	/* try not to overlap global window */
#define KEYM_HEIGHT	18
static void
choose_map()
{
    WINDOW *w;
    int i, ch;

    if ((w = newwin(KEYM_HEIGHT, KEYM_WIDTH, (LINES-KEYM_HEIGHT)/2,
	    (COLS - KEYM_WIDTH) /2)) == NULL)
	fatal(0, "couldn't make keymap screen");

    box(w, VERTCH, HORCH);
    mvwprintw(w, 0, (HELP_WIDTH - 16)/2, "%s", " cursor key map ");
    mvwprintw(w, 2, 2, "Currently map %d is in use (%s):",
	kbd_type, keytab[kbd_type].name);
    /* there must be a better way.  but hey, this whole program is a hack. */
    mvwprintw(w, 4, 4,  "UP     : %s", visprt(keytab[kbd_type].keys[KY_UP]));
    mvwprintw(w, 4, KEYM_WIDTH/2 +2,
			"DN     : %s", visprt(keytab[kbd_type].keys[KY_DN]));
    mvwprintw(w, 5, 4,  "LFT    : %s", visprt(keytab[kbd_type].keys[KY_LFT]));
    mvwprintw(w, 5, KEYM_WIDTH/2 +2,
			"RGT    : %s", visprt(keytab[kbd_type].keys[KY_RGT]));
    mvwprintw(w, 6, 4,  "PGUP   : %s", visprt(keytab[kbd_type].keys[KY_PGUP]));
    mvwprintw(w, 6, KEYM_WIDTH/2 +2,
			"PGDN   : %s", visprt(keytab[kbd_type].keys[KY_PGDN]));
    mvwprintw(w, 7, 4,  "TAB    : %s", visprt(keytab[kbd_type].keys[KY_TAB]));
    mvwprintw(w, 7, KEYM_WIDTH/2 +2,
			"BKTAB  : %s", visprt(keytab[kbd_type].keys[KY_BKTAB]));
    mvwprintw(w, 8, 4,  "HOME   : %s", visprt(keytab[kbd_type].keys[KY_HOME]));
    mvwprintw(w, 8, KEYM_WIDTH/2 +2,
			"END    : %s", visprt(keytab[kbd_type].keys[KY_END]));
    mvwprintw(w, 9, 4,  "ESCAPE : %s",visprt(keytab[kbd_type].keys[KY_ESCAPE]));
    mvwprintw(w, 9, KEYM_WIDTH/2 +2,
		    "SEARCH : %s", visprt(keytab[kbd_type].keys[KY_FWD_SRCH]));
    mvwprintw(w, 11, 2, "Available cursor key maps:");
    for (i = 0; i < KT_NSIZE; i++) {
	wmove(w, 12 +(i/2), (i % 2) ? KEYM_WIDTH/2 +2 : 4);
	wprintw(w, "%d  %s", i, keytab[i].name);
    }

    mvwprintw(w, KEYM_HEIGHT-2, 2, "Press [0-%d] to change or SPACE to exit: ",
	KT_NSIZE-1);


    wrefresh(w);
    ch = wgetch(w);		/* wait for key */
    if (ch - '0' >= 0 && ch - '0' < KT_NSIZE)
	kbd_type = ch - '0';

    delwin(w);
#ifdef ENHANCED_CURSES
    touchline(mainw, (LINES-KEYM_HEIGHT)/2 -2, KEYM_HEIGHT);
#else
    touchwin(mainw);
#endif
    wrefresh(mainw);
}


void
alarm_handler()
{
    alarm_hit = TRUE;
}


/*
 * Given a prefix, wait for the rest of the string to arrive.
 *
 * After 1 second (2 seconds for emacs mode), we time out.  If we matched
 * anything up to this point, we return it.  If not, we return (-1).  This
 * allows "Esc" and "Esc-5~" to both match, though "Esc" alone will have
 * a delay between the time it's hit and the time the response gets through.
 *
 * Fix: while this scheme sounds nice, in practice it sucks.  We want to
 * stop waiting once we get a sequence which is a complete match on exactly
 * one string.  If we can't get an exact match, THEN we wait for the whole
 * delay before we do anything.  If somebody wants to make ESC meaningful
 * and then use the keypad keys, that's their problem...
 */
static int
match_prefix(w, ich)
WINDOW *w;
int ich;
{
    int i, ch, idx, len, alrm;
    int match, last_match;
    char matchstr[LONG_KY+1];

    alrm = (kbd_type == 1) ? 2 : 1;		/* ick! */
    alarm_hit = FALSE;
    signal(SIGALRM, alarm_handler);
    alarm(alrm);

    matchstr[0] = (char) ich;
    matchstr[1] = '\0';
    idx = 1;

    while (!alarm_hit) {
	ch = wgetch(w);
	if (idx < LONG_KY && ch != ERR) {
	    matchstr[idx++] = (char) ch;
	    matchstr[idx] = '\0';
	}

	/* do we have a single, complete match? */
	len = strlen(matchstr);
	for (i = 0, match = 0, last_match = -1; i < KT_SIZE; i++) {
	    if (!strncmp(keytab[kbd_type].keys[i], matchstr, len)) {
		match++;
		last_match = i;
	    }
	}
	if (match == 1) {
	    /* we have a single prefix match, but is it a complete match? */
	    if (!strcmp(keytab[kbd_type].keys[last_match], matchstr)) {
		alarm(0);
		signal(SIGALRM, SIG_DFL);
		return (last_match);		/* yes!! */
	    }
	}
    }
    signal(SIGALRM, SIG_DFL);

    /* okay, we've got the whole thing, so take the first EXACT match */
    for (i = 0; i < KT_SIZE; i++) {
	if (!strcmp(keytab[kbd_type].keys[i], matchstr))
	    return (i);
    }
    return (-1);
}


/*
 * Basic theory of operation: if the key hit matches something in keytab2
 * (the terminal independent stuff), then we return immediately.  If not,
 * then we go searching for a match in keytab.
 *
 * If we find an exact match, we return.  If not, or we matched more than
 * one prefix, we set a timer and wait for the rest.  This is what
 * the "keypad()" function does on spiffy versions of curses.
 */
static int
get_key(w, row, col)
WINDOW *w;
{
    int i, ch;
    int match, last_match;

    wmove(w, row, col);
    wrefresh(w);
    if ((ch = wgetch(w)) == ERR)
	fatal(0, "bad getch in get_key");

    /* look for easy terminal-independent match */
    for (i = 0; i < KT2_SIZE; i++) {
	if (keytab2[i].str[0] == (char) ch)
	    return (keytab2[i].val);
    }

    /* okay, it didn't match; see if it's a prefix for anything amusing */
    for (i = 0, match = 0, last_match = -1; i < KT_SIZE; i++) {
	if (keytab[kbd_type].keys[i][0] == (char) ch) {
	    match++;
	    last_match = i;
	}
    }

    if (match) {
	/* we have a match! */
	if (match > 1) {
	    /* in fact, we have several... */
	    return (match_prefix(w, ch));
	} else {
	    /* exactly one match; special case for single-key strings */
	    if (strlen(keytab[kbd_type].keys[last_match]) == 1) {
		return (last_match);
	    } else {
		return (match_prefix(w, ch));
	    }
	}
    }

    /* we have failed... oh dear */
    return (-1);
}


/* called during editing */
/* (result: 1 = no change, 2 = new string, -1/-2 = same but BKTAB, 3 = ESC) */
int get_line(WINDOW *w, int row, int col, int width, char *buf)
{
    enum { NOTHING, EDITING } mode;
    int changed, posn;
    int ch, res;
    int match, last_match = 0;
    char new[81], blanks[81];

    strcpy(new, buf);
    mode = NOTHING;
    changed = 0;
    posn = 0;
    strncpy(blanks, BLANKS, width);
    blanks[width] = '\0';

    while (1) {
	wmove(w, row, col);
	wstandout(w);
	wprintw(w, "%s", new);
	wstandend(w);

	wrefresh(w);
	ch = mvwgetch(w, row, col + ((mode == EDITING) ? posn : width));

	/* see if it's a TAB, BKTAB, or ESCAPE */
	/* (don't do general search or we could get conflicts) */
	match = 0;
	if (keytab[kbd_type].keys[KY_TAB][0] == (char) ch) {
	    match++;
	    last_match = KY_TAB;
	}
	if (keytab[kbd_type].keys[KY_BKTAB][0] == (char) ch) {
	    match++;
	    last_match = KY_BKTAB;
	}
	if (keytab[kbd_type].keys[KY_ESCAPE][0] == (char) ch) {
	    match++;
	    last_match = KY_ESCAPE;
	}

	if (match) {
	    /* we have a match! */
	    if (match > 1) {
		/* must've matched both :-( */
		ch = match_prefix(w, ch);
	    } else {
		/* exactly one match; special case for single-key strings */
		if (strlen(keytab[kbd_type].keys[last_match]) == 1) {
		    ch = last_match;
		} else {
		    ch = match_prefix(w, ch);
		}
	    }
	} else {
	    ch += 256;		/* indicate it's a standard char */
	}

	switch (ch) {
	case KY_TAB:
	case KY_BKTAB:
	case '\012'+256:	/* LF (CR) */
	    goto accept;
	case KY_ESCAPE:
	    if (mode == EDITING) {
		/* if editing, restore original entry */
		mode = NOTHING;
		strcpy(new, buf);
		changed = 0;
	    } else {
		/* if not, we're all done */
		res = 3;
		goto done;
	    }
	    break;
	case '\014'+256:	/* ^L */
	case '\022'+256:	/* ^R */
	    wrefresh(curscr);	/* redraw */
	    break;
	case (-1):	/* illegal keypad key? */
	    break;
	default:
	    ch &= 0xff;		/* strip off 9th bit */
#ifdef ENHANCED_CURSES
	    if (ch == erasechar()) {
#else
	    if (ch == '\010' || ch == '\177') {
#endif
		if (mode == NOTHING) {
		    /* nothing to erase! */
		} else if (mode == EDITING) {
		    posn--;
		    if (posn < 0) {
			/* equivalent to ESC */
			mode = NOTHING;
			strcpy(new, buf);
			changed = 0;
		    } else {
			new[posn] = ' ';
		    }
		}
	    } else {
		if (ch < 32) break;		/* assumes ASCII; oh well */
		changed = 1;
		if (mode == NOTHING) {
		    mode = EDITING;
		    strcpy(new, blanks);
		    posn = 0;
		}
		/* guess he wants to type... */
		if (posn < width)
		    new[posn++] = (char) ch;
	    }
	    break;
	}
    }

accept:
    /* accept "new" as the new string, replacing "buf", if changed==1 */
    if (changed) {
	strcpy(buf, new);	/* copy changes into "buf" */
	buf[posn] = '\0';	/* very important for player name & pw */
	res = 2;
    } else {
	res = 1;		/* nothing changed; leave "buf" alone */
    }
    if (ch == KY_BKTAB) res = -res;

done:
    /* redisplay "buf", which may or may not have been changed */
    wmove(w, row, col);
    wprintw(w, new);
    wrefresh(w);
    return (res);
}


/* called from main loop */
int get_input(WINDOW *w, int row, int col)
{
    int key;

    while (1) {
	key = get_key(w, row, col);
	switch (key) {
	case KY_UP:
	case KY_DN:
	case KY_LFT:
	case KY_RGT:
	case KY_PGUP:
	case KY_PGDN:
	case KY_TAB:
	case KY_BKTAB:
	case KY_HOME:
	case KY_END:
	case KY_FWD_SRCH:
	case KY_ESCAPE:

	case KY_ADD:
	case KY_EDIT:
	case KY_DELETE:
	case KY_UNDELETE:
	case KY_QUIT:
	case KY_WRITE:
	    return (key);	/* let the caller handle these */

	case KY_KEYS:
	    choose_map();	/* pick which keyboard style we want */
	    break;

	case KY_HELP:
	    help_screen();	/* show a friendly help screen */
	    break;

	case KY_REDRAW:
	    wrefresh(curscr);	/* redraw */
	    break;

	default:		/* say what? (handles -1) */
#ifdef ENHANCED_CURSES
	    beep();
#else
	    printw("\007");
#endif
	    move(row, col);
	    refresh();
	    break;
	}
    }
}

#endif /* LTD_STATS */
