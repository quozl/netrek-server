/*
 * $Id: ltd_convert.c,v 1.1 2005/03/21 05:23:47 jerub Exp $
 *
 * ahn@users.sourceforge.net
 *
 * Convert from non-LTD player DB to LTD player DB.
 * USE WITH CAUTION!
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "defs.h"
#include "ltd_stats.h"
#include "struct.h"

#if !defined(LTD_STATS) || defined(LTD_PER_RACE)

int main(void) {

# if !defined(LTD_STATS)
  printf("ltd_convert: this program requires LTD_STATS\n");
# endif
# if defined(LTD_PER_RACE)
  printf("ltd_convert: LTD_PER_RACE mode is not supported.\n");
# endif
  return 1;

}

#else

const char rcsid[] = "$Id: ltd_convert.c,v 1.1 2005/03/21 05:23:47 jerub Exp $";

void error(const char *fmt, ...) {

  va_list args;
  va_start(args, fmt);

  fprintf(stderr, "ltd_convert: ");

  vfprintf(stderr, fmt, args);

  fprintf(stderr, "\n");

  va_end(args);

  exit(-3);

}

void usage(void) {

  printf("usage: ltd_convert <old_player_file> <new_player_file>\n");
  exit(-1);

}

void confirm(void) {

  char buf[11];

  printf("*************************************************************\n"
         "* WARNING: MAKE SURE YOU KNOW WHAT YOU ARE DOING!!!!        *\n"
         "* READ docs/README.LTD before continuing!                   *\n"
         "* Your Netrek server player database may be irreversibly    *\n"
         "* corrupted if you proceed.  Type 'yes' to continue         *\n"
         "*************************************************************\n"
         );

  printf("Continue? ");

  fgets(buf, 10, stdin);
  buf[10] = '\0';

  if (strcmp(buf, "yes\n")) {

    printf("\nAborting.\n");
    exit(-2);

  }

}

/*
 * this struct should be identical to struct statentry in struct.h
 * with LTD_STATS undefined
 */

struct oldstats {

    double st_maxkills;         /* max kills ever */
    int st_kills;               /* how many kills */
    int st_losses;              /* times killed */
    int st_armsbomb;            /* armies bombed */
    int st_planets;             /* planets conquered */
    int st_ticks;               /* Ticks I've been in game */
    int st_tkills;              /* Kills in tournament play */
    int st_tlosses;             /* Losses in tournament play */
    int st_tarmsbomb;           /* Tournament armies bombed */
    int st_tplanets;            /* Tournament planets conquered */
    int st_tticks;              /* Tournament ticks */
                                /* SB stats are entirely separate */
    int st_sbkills;             /* Kills as starbase */
    int st_sblosses;            /* Losses as starbase */
    int st_sbticks;             /* Time as starbase */
    double st_sbmaxkills;       /* Max kills as starbase */

    LONG st_lastlogin;          /* Last time this player was played */
    int st_flags;               /* Misc option flags */
    char st_keymap[96];         /* keymap for this player */
    int st_rank;                /* Ranking of the player */
#ifdef GENO_COUNT
    int st_genos;               /* # of winning genos */
#endif

};

struct oldstatentry {
    char name[NAME_LEN], password[NAME_LEN];
    struct oldstats stats;
};

int main(const int argc, const char *argv[]) {

  int ifd, ofd;
  int count = 0;

  struct oldstatentry old_entry;
  struct statentry new_entry;

  if (argc != 3)
    usage();

  printf("%s\n\n", rcsid);

  confirm();

  ifd = open(argv[1], O_RDONLY, 0744);

  if (ifd == -1)
    error("can't read from %s", argv[1]);

  ofd = open(argv[2], O_RDONLY, 0744);

  if (ofd != -1)
    error("out file %s already exists", argv[2]);

  ofd = open(argv[2], O_WRONLY|O_EXCL|O_CREAT|O_APPEND, 0644);

  if (ofd == -1)
    error("can't write to %s", argv[2]);

  memset(&old_entry, 0, sizeof(struct oldstatentry));
  memset(&new_entry, 0, sizeof(struct statentry));

  while (read(ifd, &old_entry, sizeof(struct oldstatentry)) > 1) {

    printf("  Converting [%s]\n", old_entry.name);

    new_entry.name[0] = '\0';
    new_entry.password[0] = '\0';

    ltd_reset_struct(new_entry.stats.ltd);

    assert(old_entry.name);
    assert(old_entry.password);
    assert(strlen(old_entry.name) < NAME_LEN);
    assert(strlen(old_entry.password) < NAME_LEN);

    strcpy(new_entry.name, old_entry.name);
    strcpy(new_entry.password, old_entry.password);

#   define add_stat(XXX, YYY) \
      new_entry.stats.ltd[0][LTD_GA].XXX += old_entry.stats.YYY
#   define copy_stat(XXX, YYY) \
      new_entry.stats.ltd[0][LTD_GA].XXX = old_entry.stats.YYY

    copy_stat(kills.max,	st_maxkills);

    /* no st_kills */
    /* no st_losses */
    /* no st_armsbomb */
    /* no st_planets */
    /* no st_ticks */

    copy_stat(kills.total,	st_tkills);
    copy_stat(deaths.total,	st_tlosses);
    copy_stat(bomb.armies,	st_tarmsbomb);
    copy_stat(planets.taken,	st_tplanets);
    add_stat(ticks.total,	st_tticks);

    /* SB stats are not converted */

    new_entry.stats.st_lastlogin	= old_entry.stats.st_lastlogin;
    new_entry.stats.st_flags		= old_entry.stats.st_flags;

    memcpy(new_entry.stats.st_keymap, old_entry.stats.st_keymap, 96);

    new_entry.stats.st_rank		= old_entry.stats.st_rank;

#   ifdef GENO_COUNT
    new_entry.stats.st_genos		= old_entry.stats.st_genos;
#   endif

    if (write(ofd, &new_entry, sizeof(struct statentry)) == -1)
      error("write failed at count %d\n", count);

    memset(&old_entry, 0, sizeof(struct oldstatentry));

    count++;

  }

  close(ifd);
  close(ofd);

  printf("Total players: %d\n", count);
  printf("Old player file: %s\n", argv[1]);
  printf("New LTD enabled player file: %s\n", argv[2]);

  return 0;

}

#endif /* LTD_STATS */
