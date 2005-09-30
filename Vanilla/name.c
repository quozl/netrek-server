#define MVERS 1
#include "version.h"
#include "patchlevel.h"

main()
{
  int i = 0;

  printf("%s.%d",mvers,PATCHLEVEL);
  while (patchname[i] != NULL)
    printf("+%s", patchname[i++]);
  printf("\n");
  exit(0);
}
