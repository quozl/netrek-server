#include "config.h"
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

static void usage(void)
{
  fprintf(stderr, "\
Usage: ban PLAYER\n\
       ban COMMAND\n\
\n\
PLAYER is a player slot number, 0-9, a-z\n\
COMMAND is one of:\n\
\n\
list          list temporary bans\n\
expire-all    expire all temporary bans\n\
erase-all     erase all temporary bans\n\
dump          dump temporary ban data in ban format\n\
");
}

static struct player *player_by_number(char *name)
{
  int i = atoi(name);
  if ((i == 0) && (name[0] != '0')) {
    char c = name[0];
    if (c >= 'a' && c <= 'z')
      i = c - 'a' + 10;
    else
      return NULL;
  }
  if (i >= MAXPLAYER) return NULL;
  return &players[i];
}

/* display everything known about a ban in command line format */
void get(char *us, struct ban *b)
{
  printf("add");
  printf(" remain %d", b->b_remain);
  printf(" ip %s", b->b_ip);
  printf("\\ \n");
}

/* display everything about every ban */
static void dump(char *us) {
  int j;
  printf("%s erase-all \\\n", us);
  for(j=0; j<MAXBANS; j++) {
    if (bans[j].b_remain != 0 || bans[j].b_ip[0] != 0)
      get(us, &bans[j]);
  }
}

int main(int argc, char **argv)
{
    int i, j;

    openmem(0);
    if (argc == 1) { usage(); return 1; }

    i = 1;

 state_0:
    if (!strcmp(argv[i], "list")) {
      if (++i == argc) return 0;
      for(j=0; j<MAXBANS; j++) {
	if (strlen(bans[j].b_ip) == 0) continue;
	printf("%s\t%d\n", bans[j].b_ip, bans[j].b_remain);
      }
      goto state_0;
    }

    if (!strcmp(argv[i], "expire-all")) {
      if (++i == argc) return 0;
      for(j=0; j<MAXBANS; j++) {
	bans[j].b_remain = 0;
      }
      goto state_0;
    }

    if (!strcmp(argv[i], "clear-all")) {
      if (++i == argc) return 0;
      for(j=0; j<MAXBANS; j++) {
	bans[j].b_remain = 0;
	bans[j].b_ip[0] = '\0';
      }
      goto state_0;
    }

    /* dump - show all bans */
    if (!strcmp(argv[i], "dump")) {
      dump(argv[0]);
      if (++i == argc) return 0;
      goto state_0;
    }

    /* check for player number */
    if (strlen(argv[i]) == 1) {
      struct player *p = player_by_number(argv[i]);
      if (p == NULL) {
        fprintf(stderr, "player %s not found\n", argv[i]);
        return 1;
      }
      bans_add_temporary_by_player(p->p_no, " by the administrator");
    }

    int remain = 3600;
    int n = 0;
    if (!strcmp(argv[i], "add")) goto state_add;
    if (++i == argc) return 0;
    goto state_0;

 state_add:
    if (++i == argc) return 0;
    
    if (!strcmp(argv[i], "remain")) {
      if (++i == argc) return 0;
      remain = atoi(argv[i]);
      goto state_add;
    }

    if (!strcmp(argv[i], "ip")) {
      if (++i == argc) return 0;
      strcpy(bans[n].b_ip, argv[i]);
      bans[n].b_remain = remain;
      n++;
      if (n == MAXBANS) {
	fprintf(stderr, "error, reached MAXBANS %d\n", MAXBANS);
	exit(1);
      }
      goto state_add;
    }

    goto state_0;
}
