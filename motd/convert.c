#include <stdio.h>

/*
 * Filter.  Usage: 'convert < motd > /home/calvin/ntacct/Netrek/.motd'
 */

#define ESC		'\\'

struct _map {
   char	alias;
   char	actual;
} map[] = {
   { '$',	13 },			/* +-
					   |    */

   { '|',	25 },			/* | */

   { '-',       18 },			/* - */

   { '_',       19 },			/* - lower */

   { '&',       20 },			/* - lowest */

   { '*',       17 },			/* - higher */

   { '=',	16 },			/* - highest */


   { '`',	12 },			/* -+
					    | */

   { '>',	11 },			/*  |
					   -+ */

   { '<',	14 },			/* |
					   +- */

   { '+',	15 },			/* + */

   { '(',	21 },			/* |- */

   { ')',	22 },			/* -| */

   { '^',       23 },                   /* |
					  -+- */
   { '%',	24 },			/* -+- 
					    | */
   { '~',       1 },			/* diamond */

   { '#',       2 },			/* block */
};

main(argc, argv)
   
   int	argc;
   char	**argv;
{
   register	c;
   register	i, f;
   int		esc;
   FILE		*fi = stdin;

   if(argc > 1){
      fi = fopen(argv[1], "r");
      if(!fi) {
	 perror(argv[1]);
	 exit(0);
      }
   }

   while((c = getc(fi)) != EOF){
      
      if(c == ESC){		/* xx: escape kludge */
	 c = getc(fi);
	 putchar(c);
	 continue;
      }

      f = 0;
      for(i=0; i< sizeof(map)/sizeof(struct _map); i++){
	 if(c == map[i].alias){
	    putchar(map[i].actual);
	    f = 1;
	    break;
	 }
      }
      if(!f)
	 putchar(c);
   }
}
