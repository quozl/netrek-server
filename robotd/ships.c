#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

init_ships()
{
   register			i;
   register struct player	*p;
   if(!players) return;

   for(i=0, p=players; i< MAXPLAYER; i++,p++){
      if(isAlive(p))
	 getship(&p->p_ship, p->p_ship.s_type);
   }
}
