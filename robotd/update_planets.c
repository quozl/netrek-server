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

update_planets()
{
   register			k;
   register struct planet	*pl;
   PlanetRec			*pls = _state.planets;
   register			d;

   pls->total_tarmies 		= 0;
   pls->total_textra_armies	= 0;
   pls->total_warmies		= 0;
   pls->total_wextra_armies	= 0;
   pls->num_teamsp		= 0;
   pls->num_safesp		= 0;
   pls->num_agrisp		= 0;
   pls->num_indsp		= 0;
   pls->num_unknownsp		= 0;
   pls->num_warteamsp		= 0;

   for(k=0, pl=planets; k < MAXPLANETS; k++,pl++){
      d = mydist_to_planet(pl);
      pl->pl_mydist = d;

      if(me->p_team == pl->pl_owner){
	 pls->team_planets[pls->num_teamsp] = pl;
	 pls->num_teamsp ++;
	 pls->total_tarmies += pl->pl_armies;
	 if(pl->pl_armies > 4)
	    pls->total_textra_armies += pl->pl_armies - 4;

	 if(pl->pl_flags & PLAGRI){
	    pls->agri_planets[pls->num_agrisp] = pl;
	    pls->num_agrisp ++;
	 }

	 pls->safe_planets[pls->num_safesp] = pl;
	 pls->num_safesp ++;

      }
      else if(unknownpl(pl)){
	 pls->unknown_planets[pls->num_unknownsp] = pl;
	 pls->num_unknownsp ++;

	 if(guess_team(pl) == _state.warteam){
	    pls->warteam_planets[pls->num_warteamsp] = pl;
	    pls->num_warteamsp ++;
	    /* CONFIGURE */
	    pls->total_warmies += 15;
	    pls->total_wextra_armies += 11;
	 }
	 else {
	    pls->safe_planets[pls->num_safesp] = pl;
	    pls->num_safesp ++;
	 }
      }
      else if(pl->pl_armies == 0){
	 pls->ind_planets[pls->num_indsp] = pl;
	 pls->num_indsp ++;

	 pls->safe_planets[pls->num_safesp] = pl;
	 pls->num_safesp ++;
      }
      else if(pl->pl_owner == _state.warteam){
	 pls->warteam_planets[pls->num_warteamsp] = pl;
	 pls->num_warteamsp ++;
	 pls->total_warmies += pl->pl_armies;
	 if(pl->pl_armies > 4)
	    pls->total_wextra_armies += pl->pl_armies - 4;
      }
      else{
	 pls->safe_planets[pls->num_safesp] = pl;
	 pls->num_safesp ++;
      }
   }

   sort_planets_by_distance();

#ifdef nodef
   /*
   if(DEBUG & DEBUG_UPDATE)
   */
      printf("total armies: %d\n", pls->total_tarmies);
      printf("total war armies: %d\n", pls->total_warmies);
      printf("total team planets: %d\n", pls->num_teamsp); 
      printf("total team agris: %d\n", pls->num_agrisp);
      printf("total independents: %d\n", pls->num_indsp);
      printf("total unknowns: %d\n", pls->num_unknownsp);
      printf("total war planets: %d\n", pls->num_warteamsp);
#endif
}

_plcmp(el1, el2)

   struct planet **el1, **el2;
{
   return (*el1)->pl_mydist - (*el2)->pl_mydist;
}

sort_planets_by_distance()
{
   PlanetRec			*pls = _state.planets;

   if(pls->num_teamsp)
      qsort(pls->team_planets, pls->num_teamsp, sizeof(struct planet *), 
	 _plcmp);
   if(pls->num_safesp)
      qsort(pls->safe_planets, pls->num_safesp, sizeof(struct planet *), 
	 _plcmp);
   if(pls->num_agrisp)
      qsort(pls->agri_planets, pls->num_agrisp, sizeof(struct planet *), 
	 _plcmp);
   if(pls->num_indsp)
      qsort(pls->ind_planets, pls->num_indsp, sizeof(struct planet *), 
	 _plcmp);
   if(pls->num_unknownsp)
      qsort(pls->unknown_planets, pls->num_unknownsp, sizeof(struct planet *), 
	 _plcmp);
   if(pls->num_warteamsp)
      qsort(pls->warteam_planets, pls->num_warteamsp, sizeof(struct planet *), 
	 _plcmp);
}
