#define MVERS
#include "version.h"
#include "patchlevel.h"

main()
{
  int i = 0;

  printf("%spl%d",mvers,PATCHLEVEL);
  while (patchname[i] != NULL)
    printf("+%s", patchname[i++]);
  printf("\n");
  exit(0);
}

