#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "solicit.h"

int main(int argc, char **argv)
{
  if (argc != 3) {
    fprintf(stderr, "Usage: %s metaserver_ip host_name_on_metaserver\n"
            "\n"
            "Attempts to delist your server by flooding the metaserver.\n"
            "Bans your server from metaserver.  Does not harm metaserver.\n"
            "Only do this if you do not plan to run your server here again.\n",
            argv[0]);
    exit(1);
  }

  solicit_delist(argv[1], argv[2]);
  exit(0);
}
