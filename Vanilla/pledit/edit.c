/*
 * Netrek player DB maintenance
 *
 * edit.c - get user input
 */

#include "config.h"

#ifndef LTD_STATS

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <curses.h>
#include <time.h>
#include <string.h>
#include "pledit.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

/*
 * ---------------------------------------------------------------------------
 *	Static data
 * ---------------------------------------------------------------------------
 */



#define DASHES	"--------------------------------------------------------------------------------"
#define CMDSTR	"Enter command:   (? for help)"
#define CMDPOSN	15


WINDOW *mainw;			/* the interesting window */
WINDOW *glw;			/* window with global data in it */

/*
 * ---------------------------------------------------------------------------
 *	Private variables
 * ---------------------------------------------------------------------------
 */
static char *pl_filename;	/* player file filename */
static char *gl_filename;	/* global file filename */
static int pl_saved;		/* has the players file been saved? */
static int gl_saved;		/* has the global file been saved? */
static int num_players;		/* #of players in the database */
static int num_ent;		/* amount of space in players array */
static int top_player;		/* #of player at top of screen */
static int cursor;		/* where the cursor is (player #) */
static int tilde = 0;		/* if TRUE, then we have tildefied the orig */

#define ENT_QUANTUM	512
typedef enum { E_ALIVE, E_DELETED } SE_STATE;
typedef struct {
    SE_STATE state;
    struct statentry se;
} STATS;
static STATS *player_st = NULL;		/* array of players */

static struct status gl;		/* global stats */

typedef char col80[81];
static col80 *plyr_lines = NULL;	/* display array */


/*
 * ---------------------------------------------------------------------------
 *	The Program
 * ---------------------------------------------------------------------------
 */
static void
init_screen()
{
    char buf[80];

    /*
     * set up the terminal
     */
#ifdef ENHANCED_CURSES
    cbreak();
#else
    crmode();
#endif
    noecho();
#ifdef ENHANCED_CURSES
    keypad(stdscr, FALSE);	/* do it the hard way */
#endif

    /*
     * draw the screen
     */
    if (LINES < 24)
	fatal(0, "Need at least 24 lines");
    mvprintw(0, 0, "PlEdit v1.0 by Andy McFadden");
    sprintf(buf, "Now editing: %s", pl_filename);
    mvprintw(0, (strlen(pl_filename) > 34) ? 32 : 80-strlen(buf), "%.48s", buf);
    mvprintw(1, 0, DASHES);
    mvprintw(1, 1, "#");
    mvprintw(1, 4, "Rank");
    mvprintw(1, 16, "Pseudonym");
    mvprintw(1, 35, "Hours----DI----Last login");
    mvprintw(LINES-2, 0, DASHES);
    mvprintw(LINES-1, 0, CMDSTR);
    move(LINES-1, CMDPOSN);

    top_player = cursor = 0;

    /* this is the window where everything interesting happens */
    if ((mainw = subwin(stdscr, LINES-4, COLS-14, 2, 0)) == NULL)
	fatal(0, "couldn't make mainw");
    /*idlok(mainw, TRUE);*/
    scrollok(mainw, FALSE);

    /* this is used to display the globals off to the side */
    if ((glw = subwin(stdscr, 14, 13, 4, 80-13)) == NULL)
	fatal(0, "couldn't make glw");
    box(glw, VERTCH, HORCH);
    mvwprintw(glw, 0, 2, " global ");
    mvwprintw(glw, 2, 2, "Hours:");
    mvwprintw(glw, 4, 2, "Planets:");
    mvwprintw(glw, 6, 2, "Bombing:");
    mvwprintw(glw, 8, 2, "Offense:");
    mvwprintw(glw, 10, 2, "Defense:");
    overwrite(glw, mainw);

    plyr_lines = NULL;		/* init plyr_lines array to empty */

    refresh();
}


/* pop up a message, refresh, then erase it but don't refresh */
static void
popup(str)
char *str;
{
    WINDOW *w;

    if ((w = newwin(5, strlen(str)+4, (LINES-5)/2,
	    (COLS - (strlen(str)+4)) /2)) == NULL)
	fatal(0, "couldn't make popup w");
    box(w, VERTCH, HORCH);
    mvwprintw(w, 0, (strlen(str)+4 -5) / 2, " FYI ");
    mvwprintw(w, 2, 2, "%s", str);
    wrefresh(w);

    delwin(w);
#ifdef ENHANCED_CURSES
    touchline(mainw, (LINES-5)/2 -2, 5);
#else
    touchwin(mainw);
#endif
    /* don't refresh; this way it'll stick around until wrefresh(mainw) */
}


static void
read_files()
{
    char buf[80];
    int pl_fd, gl_fd;

    popup("Reading players and global files ");

    pl_saved = gl_saved = TRUE;
    num_players = 0;
    num_ent = ENT_QUANTUM;	/* default size is ENT_QUANTUM entries */

    if ((pl_fd = open(pl_filename, O_RDONLY)) < 0)
	fatal(errno, "unable to open %s", pl_filename);
    if ((gl_fd = open(gl_filename, O_RDONLY)) < 0)
	fatal(errno, "unable to open %s", gl_filename);

    player_st = (STATS *) malloc(sizeof(STATS) * num_ent);
    while (1) {
	if (num_players == num_ent) {
	    /* about to run out of space; realloc buffer */
	    num_ent += ENT_QUANTUM;
	    player_st = (STATS *) realloc(player_st, sizeof(STATS) * num_ent);
	}
	if (read(pl_fd, &(player_st[num_players].se), sizeof(struct statentry))
						== sizeof(struct statentry)) {
	    player_st[num_players].state = E_ALIVE;
	    /* fix for Pig borg corrupted scores files */
	    if (!player_st[num_players].se.stats.st_tticks) 
		player_st[num_players].se.stats.st_tticks = 1;
	    num_players++;
	} else
	    break;
    }

    if (!num_players)
	fatal(0, "no entries in player database");

    if (read(gl_fd, &gl, sizeof(struct status)) != sizeof(struct status))
	fatal(errno, "unable to read global file");

    close(pl_fd);
    close(gl_fd);

    wrefresh(mainw);
    sprintf(buf, "Read %d player entries ", num_players);
    popup(buf);
    sleep(POP_DELAY);
    wrefresh(mainw);
}


#define WRITE_HEIGHT	5
#define WRITE_WIDTH	41
static void
write_files(exit_prompt)
int exit_prompt;
{
    WINDOW *w;
    char filename2[256];
    int i, fd, ch, err;

    if ((w = newwin(WRITE_HEIGHT, WRITE_WIDTH, (LINES-WRITE_HEIGHT)/2,
            (COLS - WRITE_WIDTH) /2)) == NULL)
        fatal(0, "couldn't make write screen");

    box(w, VERTCH, HORCH);
    mvwprintw(w, 0, (WRITE_WIDTH - 13)/2, "%s", " write files ");

    if (exit_prompt) {
	mvwprintw(w, 2, 2, "Save changes before leaving (y/n)? ");
    } else {
	mvwprintw(w, 2, 2, "Write players and global (y/n)? ");
    }

    wrefresh(w);
    ch = wgetch(w);

    if ((char) ch == 'y' || (char) ch == 'Y') {
	/* save files */
	strcpy(filename2, pl_filename);
	strcat(filename2, "~");
	if (!tilde) {
	    /* if this is the first time, back up the old file */
	    unlink(filename2);	/* remove existing ~ file */
	    if (link(pl_filename, filename2) < 0) goto failed;
	}
	if (unlink(pl_filename) < 0) goto failed;
	if ((fd = open(pl_filename, O_WRONLY|O_CREAT, 0644)) < 0) goto failed;
	for (i = 0; i < num_players; i++) {
	    if (player_st[i].state != E_ALIVE) continue;
	    if (write(fd, &(player_st[i].se), sizeof(struct statentry))
						!= sizeof(struct statentry)) {
		goto failed;
	    }
	}
	close(fd);

	strcpy(filename2, gl_filename);
	strcat(filename2, "~");
	if (!tilde) {
	    unlink(filename2);
	    if (link(gl_filename, filename2) < 0) goto failed;
	}
	if (unlink(gl_filename) < 0) goto failed;
	if ((fd = open(gl_filename, O_WRONLY | O_CREAT, 0644)) < 0) goto failed;
	if (write(fd, &gl, sizeof(struct status)) != sizeof(struct status))
	    goto failed;
	close(fd);

	popup("Saved ");
	sleep(POP_DELAY);
	/*wrefresh(mainw);*/

	pl_saved = TRUE;
	gl_saved = TRUE;
	tilde = 1;
    }

    delwin(w);
#ifdef ENHANCED_CURSES
    touchline(mainw, (LINES-WRITE_HEIGHT)/2 -2, WRITE_HEIGHT);
#else
    touchwin(mainw);
#endif
    wrefresh(mainw);

    return;

failed:
    err = errno;
    wclear(w);
    box(w, VERTCH, HORCH);
    mvwprintw(w, 0, (WRITE_WIDTH - 13)/2, "%s", " write error ");
    mvwprintw(w, 2, 2, "Error %d during save ", err);
    wgetch(w);

    delwin(w);
#ifdef ENHANCED_CURSES
    touchline(mainw, (LINES-WRITE_HEIGHT)/2 -2, WRITE_HEIGHT);
#else
    touchwin(mainw);
#endif
    wrefresh(mainw);
}


static void
build_line(line)
int line;
{
    struct statentry *sep;
    time_t t;
    float bombing, offense, planets, di;

    sep = &(player_st[line].se);

    strcpy(plyr_lines[line], BLANKS);
    if (player_st[line].state == E_ALIVE) {
	sprintf(plyr_lines[line], "%3d %-10s  %s_", line,
		ranks[sep->stats.st_rank].name, sep->name);
    } else {
	sprintf(plyr_lines[line], "%3d %-10s  %s_", line,
		"(DELETED)", sep->name);
    }
    plyr_lines[line][strlen(plyr_lines[line])] = ' ';	/* nuke the '\0' */
    t = (time_t) sep->stats.st_lastlogin;

    bombing = (float) sep->stats.st_tarmsbomb * (float) gl.timeprod /
	((float) sep->stats.st_tticks * (float) gl.armsbomb);
    planets = (float) sep->stats.st_tplanets * (float) gl.timeprod /
	((float) sep->stats.st_tticks * (float) gl.planets);
    offense = (float) sep->stats.st_tkills * (float) gl.timeprod /
	((float) sep->stats.st_tticks * (float) gl.kills);
    di = (bombing + planets + offense) * sep->stats.st_tticks/36000.0;
    sprintf(plyr_lines[line]+33, "%7.1f  %6.1f  %.16s",
	    (float)(sep->stats.st_ticks + sep->stats.st_tticks)/36000.0,
	    di,
	    ctime(&t));
}


/* call with line to create or (-1) to build the whole thing */
static void
build_display(line)
int line;
{
    int i;

    if (line < 0) {
	/* rebuild whole thing */
	if (plyr_lines != NULL) {
	    free(plyr_lines);
	    plyr_lines = NULL;
	}
	plyr_lines = (col80 *) malloc(num_players * sizeof(col80));

	for (i = 0; i < num_players; i++)
	    build_line(i);
    } else {
	build_line(line);
    }
}


/* redraw the contents of the main window and the gl window */
static void
display()
{
    int i, max;

    max = (num_players < LINES-4) ? num_players : LINES-4;

    for (i = 0; i < max; i++) {
	if (i+top_player == cursor) {
	    /* draw first 3 in inverse */
	    wstandout(mainw);
	    mvwprintw(mainw, i, 0, "%.3s", plyr_lines[i+top_player]);
	    wstandend(mainw);
	    mvwprintw(mainw, i, 3, "%s", plyr_lines[i+top_player]+3);
	} else {
	    mvwprintw(mainw, i, 0, "%s", plyr_lines[i+top_player]);
	}
    }

    mvwprintw(glw, 3, 3, "%-8.1f", gl.timeprod/36000.0);	/* was time */
    mvwprintw(glw, 5, 3, "%-8d", gl.planets);
    mvwprintw(glw, 7, 3, "%-8d", gl.armsbomb);
    mvwprintw(glw, 9, 3, "%-8d", gl.kills);
    mvwprintw(glw, 11,3, "%-8d", gl.losses);

    wrefresh(mainw);
    wrefresh(glw);
}


#define SRCH_HEIGHT	5
#define SRCH_WIDTH	52
static int
search(start, fwd)
int start, fwd;
{
    WINDOW *w;
    int res, posn, incr, match;
    static char buf[16] = "               ";

    if ((w = newwin(SRCH_HEIGHT, SRCH_WIDTH, (LINES-SRCH_HEIGHT)/2,
            (COLS - SRCH_WIDTH) /2)) == NULL)
        fatal(0, "couldn't make search screen");

    box(w, VERTCH, HORCH);
    mvwprintw(w, 0, (SRCH_WIDTH - 18)/2, "%s", " search for entry ");
    mvwprintw(w, 2, 2, "Player name? ");

    match = -1;

    res = get_line(w, 2, 15, 15, buf);

    switch (res) {
    case 1:		/* no change, move ahead */
    case 2:		/* new string, move ahead */
    case -1:		/* no change, move backward */
    case -2:		/* new string, move backward */
	break;
    case 3:		/* no change, escape hit - exit */
	goto escape_hit;
    default:
	fatal(0, "internal error: bad result from get_line");
    }

    if (fwd) incr = 1;
    else     incr = -1;

    posn = start + incr;
    if (posn < 0) posn = num_players-1;
    if (posn >= num_players) posn = 0;

    while (posn != start) {
	if (!strcmp(player_st[posn].se.name, buf)) {
	    match = posn;
	    break;
	}
	posn += incr;
	if (posn < 0) posn = num_players-1;
	if (posn >= num_players) posn = 0;
    }

    if (match < 0) {
	popup("No match found ");
	sleep(POP_DELAY);
    }


escape_hit:
    delwin(w);
#ifdef ENHANCED_CURSES
    touchline(mainw, (LINES-SRCH_HEIGHT)/2 -2, SRCH_HEIGHT);
#else
    touchwin(mainw);
#endif
    wrefresh(mainw);

    return (match);
}


void add_player()
{
    struct statentry *sep;
    int i;

    if (num_players == num_ent) {
	    /* about to run out of space; realloc buffer */
	    num_ent += ENT_QUANTUM;
	    realloc(player_st, sizeof(STATS) * num_ent);
    }

    player_st[num_players].state = E_ALIVE;
    sep = &(player_st[num_players].se);
    memset(sep, 0, sizeof(struct statentry));
    strcpy(sep->name, "New");
    strcpy(sep->password, "foo");
    sep->stats.st_tticks = 1;		/* prevent division errors */
    sep->stats.st_flags = 175;		/* default */
    for (i=0; i<95; i++) {
	sep->stats.st_keymap[i]=i+32;	/* init keymap */
    }
    num_players++;

    build_display(-1);		/* rebuild the whole thing */
    display();			/* only works if num_players < page */
}


static void
print_player(sep, glp)
struct statentry *sep;
struct status *glp;
{
    float bombing, planets, offense, di;
    char buf[32];

    bombing = (float) sep->stats.st_tarmsbomb * (float) glp->timeprod /
	((float) sep->stats.st_tticks * (float) glp->armsbomb);
    planets = (float) sep->stats.st_tplanets * (float) glp->timeprod /
	((float) sep->stats.st_tticks * (float) glp->planets);
    offense = (float) sep->stats.st_tkills * (float) glp->timeprod /
	((float) sep->stats.st_tticks * (float) glp->kills);
    di = (bombing + planets + offense) * sep->stats.st_tticks/36000.0;

/*    wclear(mainw); */		/* use the whole screen */
    mvwprintw(mainw, 0, 24, "Editing player");

    sprintf(buf, "%s_", sep->name);
    mvwprintw(mainw, 2, 0, "Name: %-16s", buf);
    mvwprintw(mainw, 2, 37, "Password: %-13s", sep->password);

    mvwprintw(mainw, 4, 0, "Non-tournament stats:");
    mvwprintw(mainw, 5, 2, "Ticks:  %10d    Kills:   %6d    Losses:    %6d",
	sep->stats.st_ticks, sep->stats.st_kills, sep->stats.st_losses);
    mvwprintw(mainw, 6, 2, "Armies: %10d    Planets: %6d    Maxkills: %7.2f",
	sep->stats.st_armsbomb, sep->stats.st_planets, sep->stats.st_maxkills);

    mvwprintw(mainw, 7, 0, "Tournament stats:");
    mvwprintw(mainw, 8, 2, "Ticks:  %10d    Kills:   %6d    Losses:    %6d",
	sep->stats.st_tticks, sep->stats.st_tkills, sep->stats.st_tlosses);
    mvwprintw(mainw, 9, 2, "Armies: %10d    Planets: %6d    DI:      %8.2f",
	sep->stats.st_tarmsbomb, sep->stats.st_tplanets, di);

    mvwprintw(mainw, 10, 0, "Starbase stats:");
    mvwprintw(mainw, 11, 2, "Ticks:  %10d    Kills:   %6d    Losses:    %6d",
	sep->stats.st_sbticks, sep->stats.st_sbkills, sep->stats.st_sblosses);
    mvwprintw(mainw, 12, 43, "Maxkills: %7.2f",
	sep->stats.st_sbmaxkills);

    mvwprintw(mainw, 14, 0, "Miscellaneous:");
    mvwprintw(mainw, 15, 2, "Last lg:%10ld    Rank:    %6d    Flags:     %6d",
	sep->stats.st_lastlogin, sep->stats.st_rank, sep->stats.st_flags);
    mvwprintw(mainw, 16, 2, "Keymap: %.48s", sep->stats.st_keymap);
    mvwprintw(mainw, 17, 2, "        %s", sep->stats.st_keymap+48);

    mvwprintw(stdscr, LINES-3, 0,
	"You may: e)dit, r)eset keymap, a)bort, q)uit & save changes");

    wrefresh(mainw);
}

/*
 * I whipped up this fancy struct thing to make things semi-general.  I
 * think the only real reason for having this is so that all of the screen
 * coordinates are stored in one place; it doesn't really matter whether
 * stuff gets hard-coded in a static struct or hard-coded in static code.
 */
enum { F_INT, F_FLOAT, F_STRING };
static struct scrn_def_t {
    int x,y;
    int width;
    int format;
} scrn_def[] = {
    {  2,  6, 15, F_STRING },	/* name */
    {  2, 47, 13, F_STRING },	/* password */
    {  5, 10, 10, F_INT },	/* ticks */
    {  5, 33,  6, F_INT },	/* kills */
    {  5, 54,  6, F_INT },	/* losses */
    {  6, 10, 10, F_INT },	/* armies */
    {  6, 33,  6, F_INT },	/* planets */
    {  6, 53,  7, F_FLOAT },	/* maxkills */
    {  8, 10, 10, F_INT },	/* tticks */
    {  8, 33,  6, F_INT },	/* tkills */
    {  8, 54,  6, F_INT },	/* tlosses */
    {  9, 10, 10, F_INT },	/* tarmsbomb */
    {  9, 33,  6, F_INT },	/* tplanets */
    { 11, 10, 10, F_INT },	/* sbticks */
    { 11, 33,  6, F_INT },	/* sbkills */
    { 11, 54,  6, F_INT },	/* sblosses */
    { 12, 53,  7, F_FLOAT },	/* sbmaxkills */
    { 15, 10, 10, F_INT },	/* last login */
    { 15, 33,  6, F_INT },	/* rank */
    { 15, 54,  6, F_INT },	/* flags */
};
#define SCRN_SIZE	(sizeof(scrn_def) / sizeof(struct scrn_def_t))

static void
do_edit(sep, glp)
struct statentry *sep;
struct status *glp;
{
    char *crypt(const char*, const char*);
    int idx, newidx, res, new = FALSE;
    char field[81], tmp[32];
    char oldpw[16];
    int ival = 0;
    float fval = 0.0;
    char *sval = '\0';

    strcpy(oldpw, sep->password);

    for (idx = 0; idx < SCRN_SIZE; ) {
	strcpy(field, BLANKS);

	switch (idx) {
	case  0: sval = sep->name; break;
	case  1: sval = sep->password; break;
	case  2: ival = sep->stats.st_ticks; break;
	case  3: ival = sep->stats.st_kills; break;
	case  4: ival = sep->stats.st_losses; break;
	case  5: ival = sep->stats.st_armsbomb; break;
	case  6: ival = sep->stats.st_planets; break;
	case  7: fval = sep->stats.st_maxkills; break;
	case  8: ival = sep->stats.st_tticks; break;
	case  9: ival = sep->stats.st_tkills; break;
	case 10: ival = sep->stats.st_tlosses; break;
	case 11: ival = sep->stats.st_tarmsbomb; break;
	case 12: ival = sep->stats.st_tplanets; break;
	case 13: ival = sep->stats.st_sbticks; break;
	case 14: ival = sep->stats.st_sbkills; break;
	case 15: ival = sep->stats.st_sblosses; break;
	case 16: fval = sep->stats.st_sbmaxkills; break;
	case 17: ival = sep->stats.st_lastlogin; break;
	case 18: ival = sep->stats.st_rank; break;
	case 19: ival = sep->stats.st_flags; break;
	default:
	    fatal(0, "missing idx in do_edit get");
	}

	/* bad things might happen here if number is wider than field */
	switch (scrn_def[idx].format) {
	case F_INT:
	    sprintf(tmp, "%d", ival);
	    strcpy(field + (scrn_def[idx].width - strlen(tmp)), tmp);
	    break;
	case F_FLOAT:
	    sprintf(tmp, "%.2f", fval);
	    strcpy(field + (scrn_def[idx].width - strlen(tmp)), tmp);
	    break;
	case F_STRING:
	    strcpy(field, sval);
	    break;
	}

	/* remove the "sprintf()" '\0', then put in field null */
	field[strlen(field)] = ' ';
	field[scrn_def[idx].width] = '\0';

	/* result is ASCII string stored in get_line() */
	res = get_line(mainw, scrn_def[idx].x, scrn_def[idx].y,
		scrn_def[idx].width, field);

	newidx = idx;

	switch (res) {
	case 1:		/* no change, move ahead */
	    new = FALSE;
	    newidx++;
	    break;
	case 2:		/* new string, move ahead */
	    new = TRUE;
	    newidx++;
	    break;
	case -1:	/* no change, move backward */
	    new = FALSE;
	    newidx--;
	    break;
	case -2:	/* new string, move backward */
	    new = TRUE;
	    newidx--;
	    break;
	case 3:		/* no change, escape hit - exit */
	    goto escape_hit;
	default:
	    fatal(0, "internal error: bad result from get_line");
	}

	/* if it changed, we have work to do... */
	if (new) {
	    /* convert from ascii to value */
	    switch (scrn_def[idx].format) {
	    case F_INT:
		ival = atoi(field);
		break;
	    case F_FLOAT:
		fval = atof(field);
		break;
	    case F_STRING:
		sval = field;
		break;
	    }

	    switch (idx) {
	    case  0: strcpy(sep->name, sval); break;
	    case  1: strcpy(sep->password, sval); break;
	    case  2: sep->stats.st_ticks = ival; break;
	    case  3: sep->stats.st_kills = ival; break;
	    case  4: sep->stats.st_losses = ival; break;
	    case  5: sep->stats.st_armsbomb = ival; break;
	    case  6: sep->stats.st_planets = ival; break;
	    case  7: sep->stats.st_maxkills = fval; break;
	    case  8: sep->stats.st_tticks = ival; break;
	    case  9: sep->stats.st_tkills = ival; break;
	    case 10: sep->stats.st_tlosses = ival; break;
	    case 11: sep->stats.st_tarmsbomb = ival; break;
	    case 12: sep->stats.st_tplanets = ival; break;
	    case 13: sep->stats.st_sbticks = ival; break;
	    case 14: sep->stats.st_sbkills = ival; break;
	    case 15: sep->stats.st_sblosses = ival; break;
	    case 16: sep->stats.st_sbmaxkills = fval; break;
	    case 17: sep->stats.st_lastlogin = ival; break;
	    case 18: sep->stats.st_rank = ival; break;
	    case 19: sep->stats.st_flags = ival; break;
	    default:
		fatal(0, "missing idx in do_edit put");
	    }
	}
	idx = newidx;
	if (idx < 0) idx = SCRN_SIZE-1;

	print_player(sep, glp);
    }
escape_hit:
    print_player(sep, glp);

    if (strcmp(sep->password, oldpw)) {
	/* password changed, so recompute encrypted version */
	strcpy(sep->password, crypt_player(sep->password, sep->name));
    }
}

#define GLUP_HEIGHT	5
#define GLUP_WIDTH	46
static int
update_global(oldsep, newsep, glp)
struct statentry *oldsep, *newsep;
struct status *glp;
{
    WINDOW *w;
    int ch, res;

    if ((w = newwin(GLUP_HEIGHT, GLUP_WIDTH, (LINES-GLUP_HEIGHT)/2,
            (COLS - GLUP_WIDTH) /2)) == NULL)
        fatal(0, "couldn't make glup screen");

    box(w, VERTCH, HORCH);
    mvwprintw(w, 0, (GLUP_WIDTH - 15)/2, "%s", " global update ");
    mvwprintw(w, 2, 2, "Propagate changes to global stats (y/n)? ");

    wrefresh(w);
    ch = wgetch(w);

    res = FALSE;
    if ((char) ch == 'y' || (char) ch == 'Y') {
	/* update the globals */
	/*glp->time += (newsep->stats.st_tticks - oldsep->stats.st_tticks);*/
	glp->timeprod += (newsep->stats.st_tticks - oldsep->stats.st_tticks);
	glp->kills += (newsep->stats.st_tkills - oldsep->stats.st_tkills);
	glp->losses += (newsep->stats.st_tlosses - oldsep->stats.st_tlosses);
	glp->armsbomb +=(newsep->stats.st_tarmsbomb-oldsep->stats.st_tarmsbomb);
	glp->planets += (newsep->stats.st_tplanets - oldsep->stats.st_tplanets);
	res = TRUE;
    }

    delwin(w);
#ifdef ENHANCED_CURSES
    touchline(mainw, (LINES-GLUP_HEIGHT)/2 -2, GLUP_HEIGHT);
#else
    touchwin(mainw);
#endif
/*    wrefresh(mainw);*/	/* (only called when leaving edit_player) */
    return (res);
}

static void
edit_player(plyr)
int plyr;
{
    static struct statentry newse;
    static struct status newgl;
    struct statentry *sep;
    int i, ch, done;

    wclear(mainw);
    sep = &(player_st[plyr].se);
    memcpy(&newse, sep, sizeof(struct statentry));
    memcpy(&newgl, &gl, sizeof(struct status));

    print_player(&newse, &newgl);

    done = FALSE;
    while (!done) {
	move(LINES-1, CMDPOSN);
	refresh();
	ch = getch();

	switch (ch) {
	case 'e':	/* edit */
	    do_edit(&newse, &newgl);
	    print_player(&newse, &newgl);
	    pl_saved = FALSE;		/* assume the worst, it's easier */
	    gl_saved = FALSE;
	    break;
	case 'r':	/* reset keymap */
	    for (i=0; i<95; i++) {
		newse.stats.st_keymap[i]=i+32;
	    }
	    pl_saved = FALSE;
	    print_player(&newse, &newgl);
	    break;
	case 'a':	/* abort */
	case ' ':	/* super secret key equivalent */
	    done = TRUE;
	    break;
	case 'q':	/* quit & save */
	case 'Q':	/* accept capitals too */
	    /* if tournament stats changed, update global stats on request */
	    if (newse.stats.st_tticks != sep->stats.st_tticks ||
		newse.stats.st_tkills != sep->stats.st_tkills ||
		newse.stats.st_tlosses != sep->stats.st_tlosses ||
		newse.stats.st_tarmsbomb != sep->stats.st_tarmsbomb ||
		newse.stats.st_tplanets != sep->stats.st_tplanets) {

		if (update_global(sep, &newse, &newgl)) {
		    memcpy(&gl, &newgl, sizeof(struct status));
		    build_display(-1);		/* update all (DI) */
		}
	    }
	    /* if nothing changed, this has no effect */
	    memcpy(sep, &newse, sizeof(struct statentry));
	    done = TRUE;
	    break;
	case '\014':	/* ^L */
	case '\022':	/* ^R */
	    wrefresh(curscr);	/* redraw */
	    break;
	case '?':
	    popup("(all available commands are shown below)");
	    sleep(POP_DELAY);
	    wrefresh(mainw);
	    break;
	default:
	    break;
	    /* nada */
	}
    }

    build_display(plyr);	/* rebuild this one in case it got changed */
    wclear(mainw);
    display();
}


#define PAGE	(LINES-4)
static void
main_loop()
{
    int key, oldcursor, change, res;

    build_display(-1);
    display();

    while (1) {
	key = get_input(stdscr, LINES-1, CMDPOSN);

	oldcursor = cursor;
	change = 0;

	switch (key) {
	case KY_UP:
	    cursor--;
	    break;
	case KY_DN:
	    cursor++;
	    break;
	case KY_PGUP:
	    cursor -= PAGE;
	    top_player -= PAGE;		/* move screen a full page too */
	    break;
	case KY_PGDN:
	    cursor += PAGE;
	    top_player += PAGE;
	    break;
	case KY_HOME:
	    cursor = 0;
	    top_player = 0;
	    break;
	case KY_END:
	    cursor = num_players -1;
	    break;
	case KY_FWD_SRCH:
	    if ((res = search(cursor, TRUE)) >= 0) {
		cursor = res;
		if (cursor >= top_player + PAGE)
		    top_player = cursor - 5;
	    }
	    break;
	case KY_ADD:
	    add_player();
	    edit_player(num_players-1);		/* edit the new one */
	    pl_saved = FALSE;
	    break;
	case KY_EDIT:
	    edit_player(cursor);
	    break;
	case KY_DELETE:
	    if (player_st[cursor].state == E_ALIVE) {
		player_st[cursor].state = E_DELETED;
		build_display(cursor);
		change++;
		pl_saved = FALSE;
	    }
	    break;
	case KY_UNDELETE:
	    if (player_st[cursor].state == E_DELETED) {
		player_st[cursor].state = E_ALIVE;
		build_display(cursor);
		change++;
		/* no effect on pl_saved */
	    }
	    break;
	case KY_WRITE:
	    write_files(FALSE);
	    break;

	case KY_LFT:
	case KY_RGT:
	case KY_TAB:
	case KY_BKTAB:
	case KY_ESCAPE:
	    /* these don't do anything here */
	    break;
	case KY_KEYS:
	case KY_HELP:
	case KY_REDRAW:
	    /* these are handled by get_input() */
	    break;

	case KY_QUIT:
	    return;
	default:
	    fatal(0, "Got strange keypress in main_loop from get_input");
	    break;
	}

	/* update the display */
	if (cursor < 0) cursor = 0;
	if (cursor >= num_players) cursor = num_players-1;
	if (num_players > PAGE) {
	    if (top_player < 0) top_player = 0;
	    if (top_player >= (num_players-PAGE)) top_player = num_players-PAGE;
	    if (cursor < top_player)
		top_player = cursor;
	    if (cursor >= top_player + PAGE)
		top_player += cursor - (top_player+PAGE-1);
	    if (top_player >= (num_players-PAGE)) top_player = num_players-PAGE;
	} else {
	    top_player = 0;
	}

	if (cursor != oldcursor || change)
	    display();		/* "oldcursor" check eases load on curses */
    }
}


/*
 * Entry point from main.c
 */
void
edit_file(pfilename, gfilename)
char *pfilename, *gfilename;
{
    pl_filename = pfilename;
    gl_filename = gfilename;

    /* draw the screen, set up windows, etc */
    init_screen();

    /* read the players and global files in */
    read_files();

    /* go to the main input loop */
    main_loop();

    /* time to exit */
    if (!pl_saved || !gl_saved)
	write_files(TRUE);	/* ask before writing */

    move(LINES-1, 0);
    clrtoeol();
    refresh();
    return;
}

#endif /* LTD_STATS */
