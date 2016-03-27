#include <string.h>
#include <stdlib.h>
#include "copyright.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "util.h"

#define START_ARMIES	17	

struct planet virginal[MAXPLANETS] = {
   {  0, FED|PLHOME|PLCORE|PLREPAIR|PLFUEL, FED, 20000, 80000, "Earth", 5, START_ARMIES, FED },
    {  1, FED, FED, 10000, 60000, "Rigel", 5, START_ARMIES, FED },
    {  2, FED, FED, 25000, 60000, "Canopus", 7, START_ARMIES, FED },
    {  3, FED, FED, 44000, 81000, "Beta Crucis", 11, START_ARMIES, FED },
    {  4, FED, FED, 39000, 55000, "Organia", 7, START_ARMIES, FED },
    {  5, FED|PLCORE, FED, 30000, 90000, "Deneb", 5, START_ARMIES, FED },
    {  6, FED, FED, 45000, 66000, "Ceti Alpha V", 12, START_ARMIES, FED },
    {  7, FED|PLCORE, FED, 11000, 75000, "Altair", 6, START_ARMIES, FED },
    {  8, FED|PLCORE, FED, 8000, 93000, "Vega", 4, START_ARMIES, FED },
    {  9, FED, FED, 32000, 74000, "Alpha Centauri", 14, START_ARMIES, FED },
    { 10, ROM|PLHOME|PLCORE|PLREPAIR|PLFUEL, ROM, 20000, 20000, "Romulus", 7, START_ARMIES, ROM },
    { 11, ROM, ROM, 45000, 7000, "Eridani", 7, START_ARMIES, ROM },
    { 12, ROM|PLCORE, ROM, 4000, 12000, "Aldeberan", 9, START_ARMIES, ROM },
    { 13, ROM, ROM, 42000, 44000, "Regulus", 7, START_ARMIES, ROM },
    { 14, ROM, ROM, 13000, 45000, "Capella", 7, START_ARMIES, ROM },
    { 15, ROM|PLCORE, ROM, 28000, 8000, "Tauri", 5, START_ARMIES, ROM },
    { 16, ROM|PLCORE, ROM, 28000, 23000, "Draconis", 8, START_ARMIES, ROM },
    { 17, ROM, ROM, 40000, 25000, "Sirius", 6, START_ARMIES, ROM },
    { 18, ROM, ROM, 25000, 44000, "Indi", 4, START_ARMIES, ROM },
    { 19, ROM, ROM, 8000, 29000, "Hydrae", 6, START_ARMIES, ROM },
    { 20, KLI|PLHOME|PLCORE|PLREPAIR|PLFUEL, KLI, 80000, 20000, "Klingus", 7, START_ARMIES, KLI },
    { 21, KLI, KLI, 70000, 40000, "Pliedes V", 9, START_ARMIES, KLI },
    { 22, KLI, KLI, 60000, 10000, "Andromeda", 9, START_ARMIES, KLI },
    { 23, KLI, KLI, 56400, 38200, "Lalande", 7, START_ARMIES, KLI },
    { 24, KLI|PLCORE, KLI, 91120, 9320, "Praxis", 6, START_ARMIES, KLI },
    { 25, KLI, KLI, 89960, 31760, "Lyrae", 5, START_ARMIES, KLI },
    { 26, KLI|PLCORE, KLI, 70720, 26320, "Scorpii", 7, START_ARMIES, KLI },
    { 27, KLI, KLI, 83600, 45400, "Mira", 4, START_ARMIES, KLI },
    { 28, KLI, KLI, 54600, 22600, "Cygni", 5, START_ARMIES, KLI },
    { 29, KLI|PLCORE, KLI, 73080, 6640, "Achernar", 8, START_ARMIES, KLI },
    { 30, ORI|PLHOME|PLCORE|PLREPAIR|PLFUEL, ORI, 80000, 80000, "Orion", 5, START_ARMIES, ORI },
    { 31, ORI, ORI, 91200, 56600, "Cassiopeia", 10, START_ARMIES, ORI },
    { 32, ORI, ORI, 70800, 54200, "El Nath", 7, START_ARMIES, ORI },
    { 33, ORI, ORI, 57400, 62600, "Spica", 5, START_ARMIES, ORI },
    { 34, ORI|PLCORE, ORI, 72720, 70880, "Procyon", 7, START_ARMIES, ORI },
    { 35, ORI, ORI, 61400, 77000, "Polaris", 7, START_ARMIES, ORI },
    { 36, ORI, ORI, 55600, 89000, "Arcturus", 8, START_ARMIES, ORI },
    { 37, ORI|PLCORE, ORI, 91000, 94000, "Ursae Majoris", 13, START_ARMIES, ORI },
    { 38, ORI, ORI, 70000, 93000, "Herculis", 8, START_ARMIES, ORI },
    { 39, ORI|PLCORE, ORI, 86920, 68920, "Antares", 7, START_ARMIES, ORI }
};


/* find a planet by name */
struct planet *planet_find(char *name)
{
  int i, count = 0, match = 0;

  for(i=0; i<MAXPLANETS; i++) {
    if (!strncasecmp(name, planets[i].pl_name, strlen(name))) {
      match = i;
      count++;
    }
  }
  if (count == 1) return &planets[match];
  return NULL;
}

/* find a planet by number */
struct planet *planet_by_number(char *name)
{
  int i = atoi(name);
  if (i < 0) return NULL;
  if (i > (MAXPLANETS-1)) return NULL;
  return &planets[i];
}

struct planet *pl_pick_home(int p_team)
{
    int j, i, tno = team_no(p_team);
    for (j=0; j<GWIDTH; j++) {
        i = tno * 10 + random() % 10;
        if (startplanets[i]) return &planets[i];
    }
    return &planets[13]; /* Regulus */
}

void pl_pick_home_offset(int p_team, int *x, int *y)
{
    struct planet *pl = pl_pick_home(p_team);
    *x = pl->pl_x + (random() % 10000) - 5000;
    *y = pl->pl_y + (random() % 10000) - 5000;
}

/* maximum number of agris that can exist in one quadrant */

#define AGRI_LIMIT 3

/* the four close planets to the home planet */
static int core_planets[4][4] =
{{ 7, 9, 5, 8,},
 { 12, 19, 15, 16,},
 { 24, 29, 25, 26,},
 { 34, 39, 38, 37,},
};
/* the outside edge, going around in order */
static int front_planets[4][5] =
{{ 1, 2, 4, 6, 3,},
 { 14, 18, 13, 17, 11,},
 { 22, 28, 23, 21, 27,},
 { 31, 32, 33, 35, 36,},
};

struct planet *pl_virgin()
{
  return virginal;
}

int pl_virgin_size()
{
  return sizeof(virginal);
}

void pl_reset(void)
{
  int i, j, k, which, sanity;
  memcpy(planets, pl_virgin(), pl_virgin_size());
/* if we are generating a df-friendly galaxy do that first and return */
  if (recreational_dogfight_mode) {
    for (i = 0; i < MAXPLANETS; i++) {
      planets[i].pl_armies = 0;
      planets[i].pl_owner = NOBODY;
      planets[i].pl_flags |= PLREPAIR;
      planets[i].pl_flags |= PLFUEL;
      planets[i].pl_info = ALLTEAM;
    }
    return;
  }

  for (i = 0; i < MAXPLANETS; i++) {
    planets[i].pl_armies = top_armies;
  }
  for (i = 0; i < 4; i++) {
    /* one core AGRI */
    planets[core_planets[i][random() % 4]].pl_flags |= PLAGRI;

    /* one front AGRI */
    which = random() % 2;
    if (which) {
      planets[front_planets[i][random() % 2]].pl_flags |= PLAGRI;

      /* place one repair on the other front */
      planets[front_planets[i][(random() % 3) + 2]].pl_flags |= PLREPAIR;

      /* place 2 FUEL on the other front */
      for (j = 0; j < 2; j++) {
        sanity = 0;
        do {
          k = random() % 3;
          if (sanity++ > GWIDTH) break;
        } while (planets[front_planets[i][k + 2]].pl_flags & PLFUEL) ;
        planets[front_planets[i][k + 2]].pl_flags |= PLFUEL;
      }
    } else {
      planets[front_planets[i][(random() % 2) + 3]].pl_flags |= PLAGRI;

      /* place one repair on the other front */
      planets[front_planets[i][random() % 3]].pl_flags |= PLREPAIR;

      /* place 2 FUEL on the other front */
      for (j = 0; j < 2; j++) {
        sanity = 0;
        do {
          k = random() % 3;
          if (sanity++ > GWIDTH) break;
        } while (planets[front_planets[i][k]].pl_flags & PLFUEL);
        planets[front_planets[i][k]].pl_flags |= PLFUEL;
      }
    }

    /* drop one more repair in the core (home + 1 front + 1 core = 3 Repair)*/
    planets[core_planets[i][random() % 4]].pl_flags |= PLREPAIR;

    /* now we need to put down 2 fuel (home + 2 front + 2 = 5 fuel) */
    for (j = 0; j < 2; j++) {
      sanity = 0;
      do {
        k = random() % 4;
        if (sanity++ > GWIDTH) break;
      } while (planets[core_planets[i][k]].pl_flags & PLFUEL);
      planets[core_planets[i][k]].pl_flags |= PLFUEL;
    }
  }
}
