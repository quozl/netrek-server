/*
 * db.c
 */
#include "copyright.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#ifdef PLAYER_INDEX
#include <gdbm.h>
#endif
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#include <unistd.h>
#else
#define __USE_XOPEN
#include <unistd.h>
#endif
#include "defs.h"

#include "struct.h"
#include "data.h"
#include "proto.h"
#include "salt.h"

#ifdef PLAYER_INDEX

/* fetch the offset to a player from the index database */
static off_t db_index_fetch(char *namePick, struct statentry *player) {
  GDBM_FILE dbf;
  datum key, content;
  off_t position;

  /* open the player index database */
  dbf = gdbm_open(PlayerIndexFile, 0, GDBM_WRCREAT, 0644, NULL);
  if (dbf == NULL) {
    ERROR(1,("db.c: db_index_fetch: gdbm_open('%s'): '%s', '%s'\n",
	     PlayerIndexFile, gdbm_strerror(gdbm_errno), strerror(errno)));
    return -1;
  }
  ERROR(8,("db.c: db_index_fetch: gdbm_open('%s'): ok\n", 
	   PlayerIndexFile));

  /* fetch the database entry for this player name */
  key.dptr = namePick;
  key.dsize = strlen(namePick);
  content = gdbm_fetch(dbf, key);

  /* player index may not contain this player, that's fine */
  if (content.dptr == NULL) {
    ERROR(8,("db.c: db_index_fetch: gdbm_fetch('%s'): not found in index\n",
	     namePick));
    gdbm_close(dbf);
    return -1;
  }

  if (content.dsize != sizeof(off_t)) {
    ERROR(8,("db.c: db_index_fetch: gdbm_fetch('%s'): dsize [%d] not sizeof(off_t) [%d]\n",
	     namePick, content.dsize, (int)sizeof(off_t)));
    gdbm_close(dbf);
    return -1;
  }

  /* return the position from the database entry */
  position = *((off_t *) content.dptr);
  free(content.dptr);
  gdbm_close(dbf);
  ERROR(8,("db.c: db_index_fetch: gdbm_fetch('%s'): index says position '%d'\n",
	   namePick, (int) position));
  return position;
}

/* store the offset to a player into the index database */
static void db_index_store(struct statentry *player, off_t position) {
  GDBM_FILE dbf;
  datum key, content;

  /* open the player index database */
  dbf = gdbm_open(PlayerIndexFile, 0, GDBM_WRCREAT, 0644, NULL);
  if (dbf == NULL) { return; }

  /* prepare the key and content pair from name and position */
  key.dptr = player->name;
  key.dsize = strlen(player->name);
  content.dptr = (char *) &position;
  content.dsize = sizeof(position);

  /* store this key and position */
  if (gdbm_store(dbf, key, content, GDBM_REPLACE) < 0) {
    ERROR(8,("db.c: db_index_store: gdbm_store('%s' -> '%d'): %s, %s\n", 
	     player->name, (int) position, gdbm_strerror(gdbm_errno), 
	     strerror(errno)));
    gdbm_close(dbf);
    return;
  }

  ERROR(8,("db.c: db_index_store: gdbm_store('%s' -> '%d'): ok\n", 
	   player->name, (int) position));
  gdbm_close(dbf);
}

#endif

/* given a name, find the player in the file, return position */
int findplayer(char *namePick, struct statentry *player, int exhaustive) {
  off_t position;
  int fd;

  /* open the player file */
  fd = open(PlayerFile, O_RDONLY, 0644);
  if (fd < 0) {
    ERROR(1,("db.c: findplayer: open('%s'): '%s'\n", PlayerFile, 
	     strerror(errno)));
    strcpy(player->name, namePick);
    return -1;
  }

#ifdef PLAYER_INDEX
  /* use the index as a hint as to position in the player file */
  position = db_index_fetch(namePick, player);

  /* if an index entry was present, read the entry to check it's right */
  if (position != -1) {
    lseek(fd, position * sizeof(struct statentry), SEEK_SET);
    if (read(fd, (char *) player, sizeof(struct statentry)) < 0) {
      /* read failed for some reason */
      ERROR(1,("db.c: findplayer: read: '%s'\n", strerror(errno)));
      strcpy(player->name, "");
    }
    /* check the entry in the main file matches what the index said */
    if (strcmp(namePick, player->name)==0) {
      close(fd);
      ERROR(8,("db.c: findplayer: ok, '%s' is indeed at position '%d'\n", 
	       namePick, (int) position));
      return position;
    }
    /* otherwise there's an inconsistency that we can recover from */
    ERROR(2,("db.c: findplayer: player index inconsistent with player file, "
	     "entered name '%s', index says position '%d', but file entry name '%s'\n", 
	     namePick, (int) position, player->name));
    /* return file to start for sequential search */
    lseek(fd, 0, SEEK_SET);
  }
#endif

  /* slow sequential search of player file */
  if (exhaustive) {
    position = 0;
    while (read(fd, (char *) player, sizeof(struct statentry)) ==
           sizeof(struct statentry)) {
      if (strcmp(namePick, player->name)==0) {
        close(fd);
#ifdef PLAYER_INDEX
        db_index_store(player, position);
#endif
        ERROR(8,("db.c: findplayer: '%s' found in sequential scan at position '%d'\n", 
                 namePick, (int) position));
        return position;
      }
      position++;
    }
  }

  /* not found, return failure */
  close(fd);
  strcpy(player->name, namePick);
  ERROR(8,("db.c: findplayer: '%s' not found in sequential scan\n", 
	   namePick));
  return -1;
}

void savepass(const struct statentry* se)
{
    int fd;
    if (me->p_pos < 0) return;
    ERROR(8,("db.c: savepass: saving to position '%d'\n", me->p_pos));
    fd = open(PlayerFile, O_WRONLY, 0644);
    if (fd >= 0) {
	lseek(fd, me->p_pos * sizeof(struct statentry) +
	      offsetof(struct statentry, password), SEEK_SET);
	write(fd, &se->password, sizeof(se->password));
	close(fd);
    }
}

char *crypt_player_raw(const char *password, const char *name) {
  return crypt(password, name);
}

char *crypt_player(const char *password, const char *name) {
  saltbuf sb;
  return crypt(password, salt(name, sb));
}

void changepassword (char *passPick)
{
  struct statentry se;
  strcpy(se.password, crypt_player(passPick, me->p_name));
  savepass(&se);
}

void savestats(void)
{
    int fd;

    if (me->p_pos < 0) return;
    /* Do not save stats for observers.  This is corrupting the DB. -da */
    if (Observer) return;
    fd = open(PlayerFile, O_WRONLY, 0644);
    if (fd >= 0) {
	me->p_stats.st_lastlogin = time(NULL);
	lseek(fd, me->p_pos * sizeof(struct statentry) +
	      offsetof(struct statentry, stats), SEEK_SET);
	write(fd, (char *) &me->p_stats, sizeof(struct stats));
	close(fd);
    }
}

int newplayer(struct statentry *player)
{
  int fd, offset, position;

  ERROR(8,("db.c: newplayer: adding '%s'\n", player->name));

  lock_on(LOCK_PLAYER_ADD); /* race: two players might be added at once */
  fd = open(PlayerFile, O_RDWR|O_CREAT, 0644);
  if (fd < 0) goto failed_unlock;
  if ((offset = lseek(fd, 0, SEEK_END)) < 0) goto failed_close;
  ERROR(8,("db.c: newplayer: lseek gave offset '%d'\n", offset));

  position = offset / sizeof(struct statentry);
  if ((offset % sizeof(struct statentry)) != 0) {
    ERROR(1,("db.c: newplayer: SEEK_END not multiple of struct statentry, truncating down\n"));
    offset = position * sizeof(struct statentry);
    ERROR(8,("db.c: newplayer: truncated offset '%d'\n", offset));
    if ((offset = lseek(fd, offset, SEEK_SET)) < 0) goto failed_close;
  }
  write(fd, (char *) player, sizeof(struct statentry));
  close(fd);
  lock_off(LOCK_PLAYER_ADD);

  ERROR(8,("db.c: newplayer: sizeof '%d' offset '%d' position '%d'\n", 
           (int)sizeof(struct statentry), offset, position));

#ifdef PLAYER_INDEX
  /* do not create an index entry until the character name is reused,
     because not all characters are re-used */
#endif

  return position;

failed_close:
  close(fd);
failed_unlock:
  lock_off(LOCK_PLAYER_ADD);
  return -1;
}
