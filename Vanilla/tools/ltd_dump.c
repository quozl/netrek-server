/* $Id: ltd_dump.c,v 1.1 2005/03/21 05:23:47 jerub Exp $
 *
 * Dave Ahn
 *
 * LTD stats dump utility program.
 *
 * */

#include "config.h"

#include <stdio.h>

#ifdef LTD_STATS

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "defs.h"
#include "ltd_stats.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

#define LEN_ABBR	4
#define LEN_NAME	30

static const char rcsid [] = "$Id: ltd_dump.c,v 1.1 2005/03/21 05:23:47 jerub Exp $";

static FILE *fp;

static void pad(char *buf, const int len) {

  int i;

  i = strlen(buf);

  while (i < len) {
    buf[i] = ' ';
    i++;
  }
  buf[i] = '\0';

}

static char *fmt_abbr(const char *abbr) {

  static char _abbr[LEN_ABBR + 3];

  assert(strlen(abbr) <= LEN_ABBR);

  sprintf(_abbr, "(%s)", abbr);

  return _abbr;

}

static char *fmt_name(const char *name) {

  static char _name[LEN_NAME + 3];

  assert(strlen(name) <= LEN_NAME);

  sprintf(_name, "[%s]", name);

  return _name;

}

static void dump_prefix(const char *abbr, const char *name) {

  char buf[1024];

  strcpy(buf, fmt_abbr(abbr));
  pad(buf, 7);

  strcat(buf, fmt_name(name));
  pad(buf, 38);

  fprintf(fp, buf);

}

#define dump_stat(STAT) { \
  int xxx; \
  unsigned int ttt; \
  ttt = 0; \
  for (xxx=1; xxx<8; xxx++) { \
    fprintf(fp, ":%4u", (unsigned int) ltd[xxx].STAT); \
    if (xxx != LTD_SB) ttt += ltd[xxx].STAT; \
  } \
  fprintf(fp, ":%4u:\n", (unsigned int) ttt); \
}

#define dump_max(STAT) { \
  int xxx; \
  unsigned int ttt; \
  ttt = 0; \
  for (xxx=1; xxx<8; xxx++) { \
    fprintf(fp, ":%4u", (unsigned int) ltd[xxx].STAT); \
    if (xxx != LTD_SB) \
      if (ttt < ltd[xxx].STAT) \
        ttt = ltd[xxx].STAT; \
  } \
  fprintf(fp, ":%4u:\n", (unsigned int) ttt); \
}

static void dump_full(const char *name, const char *race,
                      struct ltd_stats *ltd) {

  int i, t;

  t = 0;
  for (i=1; i<8; i++)
    t += ltd[i].ticks.total;

  if (t < 10) return;
 
  fprintf(fp, "++ LTD statistics for player [%s] (%s)\n\n", name, race);
  fprintf(fp,
          "Abbr   Name                           "
          ": SC : DD : CA : BB : AS : SB : GA : TL :\n"
          "----   ----                           "
          ":----:----:----:----:----:----:----:----:\n");

  
  dump_prefix("kt",   "kills total");			dump_stat(kills.total);
  dump_prefix("kmax", "kills max");			dump_max(kills.max);
  dump_prefix("k1",   "kills first");			dump_stat(kills.first);
  dump_prefix("k1p",  "kills first potential");		dump_stat(kills.first_potential);
  dump_prefix("k1c",  "kills first converted");		dump_stat(kills.first_converted);
  dump_prefix("k2",   "kills second");			dump_stat(kills.second);
  dump_prefix("k2p",  "kills second potential");	dump_stat(kills.second_potential);
  dump_prefix("k2c",  "kills second converted");	dump_stat(kills.second_converted);
  dump_prefix("kbp",  "kills by phaser");		dump_stat(kills.phasered);
  dump_prefix("kbt",  "kills by torp");			dump_stat(kills.torped);
  dump_prefix("kbs",  "kills by smack");		dump_stat(kills.plasmaed);
  dump_prefix("dt",   "deaths total");			dump_stat(deaths.total);
  dump_prefix("dpc",  "deaths as potential carrier");	dump_stat(deaths.potential);
  dump_prefix("dcc",  "deaths as converted carrier");	dump_stat(deaths.converted);
  dump_prefix("ddc",  "deaths as dooshed carrier");	dump_stat(deaths.dooshed);
  dump_prefix("dbp",  "deaths by phaser");		dump_stat(deaths.phasered);
  dump_prefix("dbt",  "deaths by torp");		dump_stat(deaths.torped);
  dump_prefix("dbs",  "deaths by smack");		dump_stat(deaths.plasmaed);
  dump_prefix("acc",  "actual carriers created");	dump_stat(deaths.acc);
  dump_prefix("ptt",  "planets taken total");		dump_stat(planets.taken);
  dump_prefix("pdt",  "planets destroyed total");	dump_stat(planets.destroyed);
  dump_prefix("bpt",  "bombed planets total");		dump_stat(bomb.planets);
  dump_prefix("bp8",  "bombed planets <=8");		dump_stat(bomb.planets_8);
  dump_prefix("bpc",  "bombed planets core");		dump_stat(bomb.planets_core);
  dump_prefix("bat",  "bombed armies total");		dump_stat(bomb.armies);
  dump_prefix("ba8",  "bombed_armies <= 8");		dump_stat(bomb.armies_8);
  dump_prefix("bac",  "bombed armies core");		dump_stat(bomb.armies_core);
  dump_prefix("oat",  "ogged armies total");		dump_stat(ogged.armies);
  dump_prefix("odc",  "ogged dooshed carrier");		dump_stat(ogged.dooshed);
  dump_prefix("occ",  "ogged converted carrier");	dump_stat(ogged.converted);
  dump_prefix("opc",  "ogged potential carrier");	dump_stat(ogged.potential);
  dump_prefix("o>c",  "ogged bigger carrier");		dump_stat(ogged.bigger_ship);
  dump_prefix("o=c",  "ogged same carrier");		dump_stat(ogged.same_ship);
  dump_prefix("o<c",  "ogger smaller carrier");		dump_stat(ogged.smaller_ship);
  dump_prefix("osba", "ogged sb armies");		dump_stat(ogged.sb_armies);
  dump_prefix("ofc",  "ogged friendly carrier");	dump_stat(ogged.friendly);
  dump_prefix("ofa",  "ogged friendly armies");		dump_stat(ogged.friendly_armies);
  dump_prefix("at",   "armies carried total");		dump_stat(armies.total);
  dump_prefix("aa",   "armies used to attack");		dump_stat(armies.attack);
  dump_prefix("ar",   "armies used to reinforce");	dump_stat(armies.reinforce);
  dump_prefix("af",   "armies ferried");		dump_stat(armies.ferries);
  dump_prefix("ak",   "armies killed");			dump_stat(armies.killed);
  dump_prefix("ct",   "carries total");			dump_stat(carries.total);
  dump_prefix("cp",   "carries partial");		dump_stat(carries.partial);
  dump_prefix("cc",   "carries completed");		dump_stat(carries.completed);
  dump_prefix("ca",   "carries to attack");		dump_stat(carries.attack);
  dump_prefix("cr",   "carries to reinforce");		dump_stat(carries.reinforce);
  dump_prefix("cf",   "carries to ferry");		dump_stat(carries.ferries);
  dump_prefix("tt",   "ticks total");			dump_stat(ticks.total);
  dump_prefix("tyel", "ticks in yellow");		dump_stat(ticks.yellow);
  dump_prefix("tred", "ticks in red");			dump_stat(ticks.red);
  dump_prefix("tz0",  "ticks in zone 0");		dump_stat(ticks.zone[0]);
  dump_prefix("tz1",  "ticks in zone 1");		dump_stat(ticks.zone[1]);
  dump_prefix("tz2",  "ticks in zone 2");		dump_stat(ticks.zone[2]);
  dump_prefix("tz3",  "ticks in zone 3");		dump_stat(ticks.zone[3]);
  dump_prefix("tz4",  "ticks in zone 4");		dump_stat(ticks.zone[4]);
  dump_prefix("tz5",  "ticks in zone 5");		dump_stat(ticks.zone[5]);
  dump_prefix("tz6",  "ticks in zone 6");		dump_stat(ticks.zone[6]);
  dump_prefix("tz7",  "ticks in zone 7");		dump_stat(ticks.zone[7]);
  dump_prefix("tpc",  "ticks as potential carrier");	dump_stat(ticks.potential);
  dump_prefix("tcc",  "ticks as carrier++");		dump_stat(ticks.carrier);
  dump_prefix("tr",   "ticks in repair");		dump_stat(ticks.repair);
  dump_prefix("dr",   "damage repaired");		dump_stat(damage_repaired);
  dump_prefix("wpf",  "weap phaser fired");		dump_stat(weapons.phaser.fired);
  dump_prefix("wph",  "weap phaser hit");		dump_stat(weapons.phaser.hit);
  dump_prefix("wpdi", "weap phaser damage inflicted");	dump_stat(weapons.phaser.damage.inflicted);
  dump_prefix("wpdt", "weap phaser damage taken");	dump_stat(weapons.phaser.damage.taken);
  dump_prefix("wtf",  "weap torp fired");		dump_stat(weapons.torps.fired);
  dump_prefix("wth",  "weap torp hit");			dump_stat(weapons.torps.hit);
  dump_prefix("wtd",  "weap torp detted");		dump_stat(weapons.torps.detted);
  dump_prefix("wts",  "weap torp self detted");		dump_stat(weapons.torps.selfdetted);
  dump_prefix("wtw",  "weap torp hit wall");		dump_stat(weapons.torps.wall);
  dump_prefix("wtdi", "weap torp damage inflicted");	dump_stat(weapons.torps.damage.inflicted);
  dump_prefix("wtdt", "weap torp damage taken");	dump_stat(weapons.torps.damage.taken);
  dump_prefix("wsf",  "weap smack fired");		dump_stat(weapons.plasma.fired);
  dump_prefix("wsh",  "weap smack hit");		dump_stat(weapons.plasma.hit);
  dump_prefix("wsp",  "weap smack phasered");		dump_stat(weapons.plasma.phasered);
  dump_prefix("wsw",  "weap smack hit wall");		dump_stat(weapons.plasma.wall);
  dump_prefix("wsdi", "weap smack damage inflicted");	dump_stat(weapons.plasma.damage.inflicted);
  dump_prefix("wsdt", "weap smack damage taken");	dump_stat(weapons.plasma.damage.taken);

  fprintf(fp, "\n\n");

}

static void dump_sb_header1(void) {

  fprintf(fp,
          "                  |         kills          |  armies   |(m)    time (%%)\n"
          "Name            |R| kt dt kbp kbt max  kph | at  ak pad| tt| tr tcc red yel\n"
          "----------------|-|--- -- --- --- --- -----|--- --- ---|---|--- --- --- ---\n"
          );

}

static void dump_sb_stats1(char *name, char race, struct ltd_stats *ltd) {

  ltd = &(ltd[LTD_SB]);

  if (ltd->ticks.total < 10) return;

  fprintf(fp,
          "%-16s %c %3d %2d %3d %3d %3d %5.1f %3d %3d %3d %3d %3d %3d %3d %3d\n",
          name,
          race,
          ltd->kills.total,
          ltd->deaths.total,
          ltd->kills.phasered,
          ltd->kills.torped,
          (int) ltd->kills.max,
          (float) ltd->kills.total * 36000.0f / (float) ltd->ticks.total,
          ltd->armies.total,
          ltd->armies.killed,
          (ltd->armies.total > 0)?
            (ltd->armies.total - ltd->armies.killed) * 100 / ltd->armies.total:
            0,
          ltd->ticks.total / 600,
          ltd->ticks.repair * 100 / ltd->ticks.total,
          ltd->ticks.carrier * 100 / ltd->ticks.total,
          ltd->ticks.red * 100 / ltd->ticks.total,
          ltd->ticks.yellow * 100 / ltd->ticks.total);

}

static void dump_sb_header2(void) {

  fprintf(fp,
          "                  |         oggs          |     (%%) weapons    (%%)      |repair\n"
          "Name            |R|oat odc occ opc ofc ofa| wpf wph wpdia  wtf wth wtdia| drpm\n"
          "----------------|-|--- --- --- --- --- ---|---- --- ----- ---- --- -----|-----\n"
          );

}

static void dump_sb_stats2(char *name, char race, struct ltd_stats *ltd) {

  ltd = &(ltd[LTD_SB]);

  if (ltd->ticks.total < 10) return;
  fprintf(fp,
          "%-16s %c %3d %3d %3d %3d %3d %3d %4d %3d %5.1f %4d %3d %5.1f %5.3f\n",
          name,
          race,
          ltd->ogged.armies,
          ltd->ogged.dooshed,
          ltd->ogged.converted,
          ltd->ogged.potential,
          ltd->ogged.friendly,
          ltd->ogged.friendly_armies,
          ltd->weapons.phaser.fired,
          (ltd->weapons.phaser.fired > 0)?
            ltd->weapons.phaser.hit * 100 / ltd->weapons.phaser.fired:
          0,
          (ltd->weapons.phaser.hit > 0)?
            (float) ltd->weapons.phaser.damage.inflicted / (float) ltd->weapons.phaser.hit:
          0.0f,
          ltd->weapons.torps.fired,
          (ltd->weapons.torps.fired > 0)? ltd->weapons.torps.hit * 100 / ltd->weapons.torps.fired:
          0,
          (ltd->weapons.torps.hit > 0)?
            (float) ltd->weapons.torps.damage.inflicted / (float) ltd->weapons.torps.hit:
          0.0f,
          (float) ltd->damage_repaired * 600 / ltd->ticks.total);

}

int main(const int argc, const char *argv[]) {

  struct statentry p;
  int plf;
  int race;
  char *who;

#ifdef LTD_PER_RACE

  const char *races[] = {
    "FED",
    "ROM",
    "KLI",
    "ORI",
    "IND"
  };

#else

  const char *races[] = {
    "ALL"
  };

#endif

  fp = stdout;

  getpath();

  if (argc == 2)
    plf = open(argv[1], O_RDONLY, 0744);
  else 
    plf = open(PlayerFile, O_RDONLY, 0744);

  if (argc == 3)
    who = (char *)argv[2];
  else
    who = NULL;

  if (plf <= -1) {

    fprintf(stderr, "ltd_dump: can't open playerfile\n");
    exit(-1);

  }

  fprintf(fp, "%s\n\n", rcsid);


  fprintf(fp, "+LTD Starbase Statistics\n\n");

  dump_sb_header1();

  while (read(plf, &p, sizeof(struct statentry)) > 1) {

    for (race=0; race<LTD_NUM_RACES; race++) {

      if ((who == NULL)||(strcmp(who, p.name) == 0)) {

	dump_sb_stats1(p.name, races[race][0], p.stats.ltd[race]);

      }

    }

  }

  if (lseek(plf, 0, SEEK_SET) == -1) {

    fprintf(stderr, "ltd_dump: lseek failed\n");
    exit(-1);

  }

  fprintf(fp, "\n");

  dump_sb_header2();

   while (read(plf, &p, sizeof(struct statentry)) > 1) {

    for (race=0; race<LTD_NUM_RACES; race++) {

      if ((who == NULL)||(strcmp(who, p.name) == 0)) {

	dump_sb_stats2(p.name, races[race][0], p.stats.ltd[race]);

      }

    }

  }

  fprintf(fp, "\n\n");
  fprintf(fp, "+LTD Extended Player Statistics Dump\n\n");

  if (lseek(plf, 0, SEEK_SET) == -1) {

    fprintf(stderr, "ltd_dump: lseek failed\n");
    exit(-1);

  }
 
  while (read(plf, &p, sizeof(struct statentry)) > 1) {


    for (race=0; race<LTD_NUM_RACES; race++) {

      if ((who == NULL)||(strcmp(who, p.name) == 0)) {

	dump_full(p.name, races[race], p.stats.ltd[race]);

      }

    }

  }


  close(plf);

  return 0;

}

#else /* LTD_STATS */

int main(void) {

  printf("ltd_dump: This program can only be used with LTD_STATS enabled.\n");
  return 1;
}

#endif /* LTD_STATS */
