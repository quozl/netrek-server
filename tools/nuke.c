#include <stdio.h>
#include "defs.h"
#include "struct.h"
#include "proto.h"

int main(int argc, char **argv)
{
 char c;
 
 if(openmem(-1) == 0)
 {
  printf("No shared memory defined...\n");
  return 0;
 }
 c='y';
 
 if(setupmem() != 1)
 {
  printf("setupmem return an error... Should I try to remove the segment (y/N): ");
  scanf("%c", &c);
 }

 if(( c == 'y') || ( c == 'Y'))
  printf("removemem() : %s.\n", removemem()?"success":"failure");

 printf(" If you still encounter problems that might be related to shared memory, try to use.\n");
 printf("ipcs(8) and ipcrm(8).\n");

 return 0;
}
