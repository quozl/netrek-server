/*
 * Netrek player DB maintenance
 *
 * pledit.h - common stuff and tunable constants
 */

#include "config.h"

/* prototypes */
void fatal(int nerrno, const char *fmt, ...);
int get_input(WINDOW *w, int row, int col);
int get_line(WINDOW *w, int row, int col, int width, char *buf);

/* how many seconds to look at a popup (add more for slow terminals) */
#define POP_DELAY	2

/* default players and global files (from ntserv/defs.h) */
#define PLAYERFILE	LOCALSTATEDIR"/players"
#define GLOBAL		LOCALSTATEDIR"/global"

/* main window */
extern WINDOW *mainw;

/*
 * Constants for key bindings.  The names corresponding to entries in the
 * KEYSEQ table must come first and be in order (I quietly rely on the fact
 * that C assigns enums as ints starting from zero).  Perhaps not the best
 * way to go about it.
 */
enum {
	KY_UP, KY_DN, KY_LFT, KY_RGT, KY_PGUP, KY_PGDN, KY_TAB, KY_BKTAB,
	KY_HOME, KY_END, KY_ESCAPE, KY_FWD_SRCH,
	KY_QUIT, KY_HELP, KY_REDRAW, KY_KEYS,
	KY_ADD, KY_EDIT, KY_DELETE, KY_UNDELETE, KY_WRITE 
};

/* Amdahl UTS stuff */
#ifdef _UTS
# define ENHANCED_CURSES
#endif

/* for me, box(w, 0, 0) produces nice ASCII graphics chars */
#ifdef ENHANCED_CURSES
# define VERTCH	0
# define HORCH	0
#else
# define VERTCH	'|'
# define HORCH	'-'
#endif

/* you may need this if vfprintf() is missing */
/*#define NO_VFPRINTF*/

/* a whole bunch of blanks */
#define BLANKS  "                                                                                "
