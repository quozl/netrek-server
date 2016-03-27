#include "config.h"
#include <stdio.h>
#include <string.h>
#include "defs.h"

int main(int argc, char **argv)
{
  if (argc == 1) {
    char *dir = strdup(LIBDIR);
    char *end = strrchr(dir, '/');
    *end = '\0';
    printf("%s\n", dir);
    return 0;
  }
  if (!strcmp(argv[1], "--libdir")) {
    printf("%s\n", LIBDIR);
    return 0;
  }
  if (!strcmp(argv[1], "--sysconfdir")) {
    printf("%s\n", SYSCONFDIR);
    return 0;
  }
  if (!strcmp(argv[1], "--localstatedir")) {
    printf("%s\n", LOCALSTATEDIR);
    return 0;
  }
  if (!strcmp(argv[1], "--help")) {
    printf("Usage: %s [--libdir] [--sysconfdir] [--localstatedir]\n", argv[0]);
    return 0;
  }
  return 1;
}
