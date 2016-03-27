/*
 * Netrek player DB maintenance
 *
 * main.c - startup & shutdown, plus some common routines
 */

#include "config.h"

#include <stdio.h>

#ifdef LTD_STATS

int main(void) {

  printf("pledit: This program does not work with LTD_STATS.\n");

}

#else /* LTD_STATS */

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <curses.h>
#include "pledit.h"
#include "defs.h"
#include "struct.h"
#include "data.h"

void usage(char *argv0)
{
    fprintf(stderr, "usage: %s [.players] [.global]\n\n", argv0);
    fprintf(stderr,
    "DO NOT use this or any other score tool while a game is in progress.\n\n");
}


/*
 * fatal(int errno, char *format, arg1, arg2, ...)
 */

void fatal(int nerrno, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    endwin();		/* shut curses down before printing error */

    fflush(stdout);
    fprintf(stderr, "%s: ", "pledit");	/* (or argv[0]) */
#ifdef NO_VFPRINTF
    fprintf(stderr, "a fatal error occurred");	/* lazy */
#else
    vfprintf(stderr, fmt, args);
#endif

    if (nerrno > 0)
        perror(" ");
    else
        putc('\n', stderr);

    va_end(args);

    exit(1);
    /*NOTREACHED*/
}


/*
 * Handle fatal signals (like SIGINT)
 */

void signal_handler(int s)
{
    endwin();		/* shut curses down */
    exit(3);
    /*NOTREACHED*/
}

void getpath(void);
void edit_file(char *, char *);

int main(int argc, char **argv)
{
    char *pl_filename, *gl_filename;

#ifdef NBR
    getpath();
#endif

    if (argc > 3) {
	usage(argv[0]);
	exit(2);
    }

    if (argc >= 2)
	pl_filename = argv[1];
    else
#ifdef NBR
	pl_filename = PlayerFile;
#else
	pl_filename = PLAYERFILE;
#endif
    if (argc == 3)
	gl_filename = argv[2];
    else
#ifdef NBR
	gl_filename = GLOBAL;
#else
	gl_filename = Global;
#endif

    signal(SIGINT, signal_handler);

    /* init curses */
    if (initscr() == NULL) {
	fprintf(stderr, "Unable to initialize curses\n");
	exit(1);
    }

    edit_file(pl_filename, gl_filename);

    /* shut down curses */
    endwin();
    exit(0);
    /*NOTREACHED*/
}

#endif /* LTD_STATS */
