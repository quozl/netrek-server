#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

/* Search for player by name, leaving f positioned at start of
 * player's entry.  Returns 0 on error.
 */
static int find(FILE* f, const char* name, struct statentry* player)
{
	for(;;) {
		if (fread(player, sizeof(struct statentry), 1, f) != 1) {
			if (feof(f)) break;
			perror("fread");
			return 0;
		}
		if (strcmp(player->name, name) == 0) {
			if (fseek(f, -sizeof(struct statentry),
				  SEEK_CUR) == -1) {
				perror("fseek");
				return 0;
			}
			return 1;
		}
	}
	printf("%s not found in database\n", name);
	return 0;
}

static const char* check(const char* s)
{
	if (strlen(s) < NAME_LEN)
		return s;
	else
		return NULL;
}

int main(int argc, const char** argv)
{
	FILE* f;
	struct statentry player;
	const char* name;
	const char* password;
	
	if (argc != 3) {
		fprintf(stderr, "usage: %s player password\n",
			argv[0]);
		return 1;
	}
	name = check(argv[1]);
	password = check(argv[2]);
	if (name == NULL || password == NULL) {
		fprintf(stderr, "%s: player name and password can be "
			"at most %d characters\n", argv[0], NAME_LEN-1);
		return 1;
	}
	getpath();
	f = fopen(PlayerFile, "r+b");
	if (f == NULL) {
		perror(PlayerFile);
		return 1;
	}
	if (! find(f, name, &player)) {
		fclose(f);
		return 1;
	}
	/* we could be zeroing the password... */
	memset(player.password, 0, sizeof(player.password));
	strcpy(player.password, crypt_player(password, name));
	if (fseek(f, offsetof(struct statentry, password), SEEK_CUR) == -1) {
		perror("fseek");
		fclose(f);
		return 1;
	}
	if (fwrite(player.password, sizeof(player.password), 1, f) != 1) {
		perror("fwrite");
		fclose(f);
		return 1;
	}
	fclose(f);
	printf("password for %s changed\n", name);
	return 0;
}
