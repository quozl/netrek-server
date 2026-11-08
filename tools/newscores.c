/*
 *  Kevin P. Smith      12/05/88
 *
 *  Takes the output of scores A and generates the player file.
 *  (Also creates the .GLOBAL file)
 *  This program reads stdin for its data.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

/* the numbers on a line start after the name, password and keymap */
#define KEYMAP_END 130

struct statentry play_entry;

extern void getpath(void);

static void usage(void);

static void trimblanks2(char *str)
{
    *str=0;
    str--;
    while (*str==' ') {
	*str=0;
	str--;
    }
    if (*str=='_') *str=0;
}

static void trimblanks(char *str)
{
    *str=0;
    str--;
    while (*str==' ') {
	*str=0;
	str--;
    }
}

static int lineno;		/* line of input being read */

#ifdef LTD_STATS

#ifdef GENO_COUNT
#define FIXED_FIELDS 4		/* rank, last login, flags, genos */
#else
#define FIXED_FIELDS 3		/* rank, last login, flags */
#endif
#define COUNT_FIELD(f) + 1
#define LTD_FIELDS (0 LTD_STATS_FIELDS(COUNT_FIELD, COUNT_FIELD))
#define LINE_FIELDS (FIXED_FIELDS + LTD_NUM_RACES * LTD_NUM_SHIPS * LTD_FIELDS)

static char *field;		/* rest of the line not yet read */
static int nfield;		/* number of fields read from the line */

static int count_fields(char *p)
{
    int count = 0;

    for (;;) {
	while (*p == ' ') p++;
	if (*p == '\0') return count;
	count++;
	while (*p != ' ' && *p != '\0') p++;
    }
}

static char *next_field(void)
{
    char *start;

    while (*field == ' ') field++;
    start = field;
    while (*field != ' ' && *field != '\0') field++;
    if (*field == ' ') *field++ = '\0';
    nfield++;
    return start;
}

static void bad_field(char *text, char *what)
{
    fprintf(stderr, "newscores: line %d, field %d after the keymap, "
	    "\"%s\" is not %s\n", lineno, nfield, text, what);
    exit(1);
}

static long load_long(long min, long max)
{
    char *text = next_field(), *end;
    long v;

    errno = 0;
    v = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || v < min || v > max)
	bad_field(text, "an integer in range");
    return v;
}

static unsigned int load_unsigned(void)
{
    char *text = next_field(), *end;
    unsigned long v;

    errno = 0;
    v = strtoul(text, &end, 10);
    if (*text == '-' || end == text || *end != '\0' || errno == ERANGE ||
	v > UINT_MAX)
	bad_field(text, "an unsigned integer in range");
    return v;
}

static double load_double(void)
{
    char *text = next_field(), *end;
    double v;

    v = strtod(text, &end);
    if (end == text || *end != '\0')
	bad_field(text, "a number");
    return v;
}

#define LOAD_U(f) s->f = load_unsigned();
#define LOAD_D(f) s->f = load_double();

static void load_ltd_stats(struct ltd_stats *s)
{
    LTD_STATS_FIELDS(LOAD_U, LOAD_D)
}

/* the numbers after the keymap, in the order scores A writes them */
static void load_numbers(char *numbers)
{
    int count, race, ship;

    count = count_fields(numbers);
    if (count != LINE_FIELDS) {
	fprintf(stderr, "newscores: line %d has %d fields after the keymap, "
		"expected %d; check that LTD_STATS, LTD_PER_RACE and "
		"GENO_COUNT are the same as on the server that wrote it\n",
		lineno, count, LINE_FIELDS);
	exit(1);
    }

    field = numbers;
    nfield = 0;
    play_entry.stats.st_rank = load_long(INT_MIN, INT_MAX);
    /* LONG is 32 bits on 32 bit and on 64 bit systems */
    play_entry.stats.st_lastlogin = load_long(INT_MIN, INT_MAX);
    play_entry.stats.st_flags = load_long(INT_MIN, INT_MAX);
#ifdef GENO_COUNT
    play_entry.stats.st_genos = load_long(INT_MIN, INT_MAX);
#endif
    for (race = 0; race < LTD_NUM_RACES; race++)
	for (ship = 0; ship < LTD_NUM_SHIPS; ship++)
	    load_ltd_stats(&play_entry.stats.ltd[race][ship]);
}

#else /* LTD_STATS */

static void load_numbers(char *numbers)
{
#ifdef GENO_COUNT
    sscanf(numbers, " %d %lf %d %d %d %d %d %d %d %d %d %d %d %d %d %lf %ld %d %d\n",
#else
    sscanf(numbers, " %d %lf %d %d %d %d %d %d %d %d %d %d %d %d %d %lf %ld %d\n",
#endif
	&play_entry.stats.st_rank,
	&play_entry.stats.st_maxkills,
	&play_entry.stats.st_kills,
	&play_entry.stats.st_losses,
	&play_entry.stats.st_armsbomb,
	&play_entry.stats.st_planets,
	&play_entry.stats.st_ticks,
	&play_entry.stats.st_tkills,
	&play_entry.stats.st_tlosses,
	&play_entry.stats.st_tarmsbomb,
	&play_entry.stats.st_tplanets,
	&play_entry.stats.st_tticks,
	&play_entry.stats.st_sbkills,
	&play_entry.stats.st_sblosses,
	&play_entry.stats.st_sbticks,
	&play_entry.stats.st_sbmaxkills,
	&play_entry.stats.st_lastlogin,
#ifdef GENO_COUNT
	&play_entry.stats.st_flags,
	&play_entry.stats.st_genos);
#else
	&play_entry.stats.st_flags);
#endif

#ifdef CONVERT
    play_entry.stats.st_planets*=3;
    play_entry.stats.st_tplanets*=3;
#endif
}

#endif /* LTD_STATS */

/* ARGSUSED */
int main(int argc, char **argv)
{
    int fd;
    char *buf = NULL;
    size_t size = 0;
    ssize_t len;

    if (argc>1) usage();
    getpath();
    printf("Warning:  If you do not know how to use this program, break it now!\n");
    status=(struct status *) calloc(1, sizeof(struct status));
    if (scanf("%d %d %d %d %d %lf\n",
	&status->time,
	&status->planets,
	&status->armsbomb,
	&status->kills,
	&status->losses,
	&status->timeprod) != 6) {
	fprintf(stderr, "newscores: line 1 is not the totals line\n");
	exit(1);
    }
    lineno = 1;

#ifdef CONVERT
    status->planets*=3;
#endif

    fd = open(Global, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
	perror(Global);
	exit(1);
    }
    if (write(fd, (char *) status, sizeof(struct status)) !=
	sizeof(struct status)) {
	perror(Global);
	exit(1);
    }
    close(fd);

    fd = open(PlayerFile, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
	perror(PlayerFile);
	exit(1);
    }

    while ((len = getline(&buf, &size, stdin)) != -1) {
	lineno++;
	if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
	if (len == 0) continue;
	if (len < KEYMAP_END) {
	    fprintf(stderr, "newscores: line %d is too short\n", lineno);
	    exit(1);
	}
	memset(&play_entry, 0, sizeof(play_entry));
	trimblanks2(buf+16);
	trimblanks(buf+33);
	trimblanks(buf+KEYMAP_END-1);
	strcpy(play_entry.name, buf);
	strcpy(play_entry.password, buf+17);
	strcpy(play_entry.stats.st_keymap, buf+34);
	load_numbers(buf+KEYMAP_END);

	if (write(fd, (char *) &play_entry, sizeof(struct statentry)) !=
	    sizeof(struct statentry)) {
	    perror(PlayerFile);
	    exit(1);
	}
    }
    free(buf);
    close(fd);
    return 0;
}

static void usage(void)
{
    printf("Usage: newscores < scoredb\n");
    printf("This program takes input of the form generated by the 'scores A'\n");
    printf(" command.  This allows you to create an ascii form of the database\n");
    printf(" with 'scores A > scoredb', then you can edit it as needed, then you\n");
    printf(" can issue a 'newscores < scoredb' command to restore the database.\n");
    printf("Note:  The form of the output from the 'scores A' command is very\n");
    printf(" specific.  Be sure that the input stream matches it very accurately\n");
    exit(0);
}
