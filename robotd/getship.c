/*
 * getship.c
 */
#include "copyright.h"

#include <stdio.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

/* fill in ship characteristics */

getship(shipp, s_type)
struct ship *shipp;
int s_type;
{
    register i=0;
    
    if(!_state.chaos){
	switch (s_type) {
	case SCOUT:
	    shipp->s_turns = 570000;
	    shipp->s_accint = 200; /* was 185 */
	    shipp->s_decint = 270; /* was 270 */
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 25; /* was 25 */
	    shipp->s_plasmadamage = -1;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 75; /* was 75 */
	    shipp->s_torpspeed = 16; /* was 16 */
	    shipp->s_torpfuse = 16 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 0;
	    shipp->s_plasmafuse = 0;
	    shipp->s_plasmaturns = 0;
	    shipp->s_maxspeed = 12;
	    shipp->s_repair = 80;
	    shipp->s_maxfuel = 5000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 7 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 20 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 7 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 2;
	    shipp->s_cloakcost = 17;
	    shipp->s_recharge = 8;
	    shipp->s_maxarmies = 2;
	    shipp->s_maxshield = 75; /* was 75 */
	    shipp->s_maxdamage = 75;
	    shipp->s_wpncoolrate = 3;
	    shipp->s_egncoolrate = 8;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_type = SCOUT;
	    shipp->s_mass = 1500;
	    shipp->s_tractstr = 2000;
	    shipp->s_tractrng = .7;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    break;
	case DESTROYER:
	    shipp->s_turns = 310000;
	    shipp->s_accint = 200;
	    shipp->s_decint = 300;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 30;
	    shipp->s_plasmadamage = 75;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 85;
	    shipp->s_torpspeed = 14;
	    shipp->s_torpfuse = 30 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 30;
	    shipp->s_plasmaturns = 1;
	    shipp->s_maxspeed = 10;
	    shipp->s_repair = 100;
	    shipp->s_maxfuel = 7000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 7 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 30 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 7 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 3;
	    shipp->s_cloakcost = 21;
	    shipp->s_recharge = 11;
	    shipp->s_maxarmies = 5;
	    shipp->s_maxshield = 85;
	    shipp->s_maxdamage = 85;
	    shipp->s_wpncoolrate = 2;
	    shipp->s_egncoolrate = 6;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 1800;
	    shipp->s_tractstr = 2500;
	    shipp->s_tractrng = .9;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = DESTROYER;
	    break;
	case BATTLESHIP: 
     	    shipp->s_turns = 75000; 
            shipp->s_accint = 80;
	    shipp->s_decint = 180;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 40;
	    shipp->s_plasmadamage = 130;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 105;
	    shipp->s_torpspeed = 12;
	    shipp->s_torpfuse = 40 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 35;
	    shipp->s_plasmaturns = 1;
	    shipp->s_maxspeed = 8;
	    shipp->s_repair = 125;
	    shipp->s_maxfuel = 14000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 9 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 30 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 10 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 6;
	    shipp->s_cloakcost = 30;
	    shipp->s_recharge = 14;
	    shipp->s_maxarmies = 6;
	    shipp->s_maxshield = 130;
	    shipp->s_maxdamage = 130;
	    shipp->s_wpncoolrate = 3;
	    shipp->s_egncoolrate = 6;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 2300;
	    shipp->s_tractstr = 3700;
	    shipp->s_tractrng = 1.2;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = BATTLESHIP;
	    break;
	case ASSAULT:
	    shipp->s_turns = 120000;
	    shipp->s_accint = 100;
	    shipp->s_decint = 200;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 30;
	    shipp->s_plasmadamage = -1;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 80;
	    shipp->s_torpspeed = 16;
	    shipp->s_torpfuse = 30 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 0;
	    shipp->s_plasmafuse = 0;
	    shipp->s_plasmaturns = 0;
	    shipp->s_maxspeed = 8;
	    shipp->s_repair = 120;
	    shipp->s_maxfuel = 6000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 9 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 20 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 7 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 3;
	    shipp->s_cloakcost = 17;
	    shipp->s_recharge = 10;
	    shipp->s_maxarmies = 20;
	    shipp->s_maxshield = 80;
	    shipp->s_maxdamage = 200;
	    shipp->s_wpncoolrate = 2;
	    shipp->s_egncoolrate = 6;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1200;
	    shipp->s_mass = 2300;
	    shipp->s_tractstr = 2500;
	    shipp->s_tractrng = .7;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = ASSAULT;
	    break;
	case STARBASE:
	    shipp->s_turns = 50000;
	    shipp->s_accint = 100;
	    shipp->s_decint = 200;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 30;
	    shipp->s_plasmadamage = 150;
	    shipp->s_phaserfuse = 4;
	    shipp->s_phaserdamage = 120;
	    shipp->s_torpspeed = 14;
	    shipp->s_torpfuse = 30 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 25;
	    shipp->s_plasmaturns = 1;
	    shipp->s_repair = 140;
	    shipp->s_maxfuel = 60000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 10 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 25 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 8 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 10;
	    shipp->s_cloakcost = 75;
	    shipp->s_recharge = 35;
	    shipp->s_maxarmies = 25;
	    shipp->s_maxshield = 500;
	    shipp->s_maxdamage = 600;
	    shipp->s_wpncoolrate = 4;
	    shipp->s_egncoolrate = 4;
	    shipp->s_maxspeed = 2;
	    shipp->s_maxwpntemp = 1300;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 5000;
	    shipp->s_tractstr = 8000;
	    shipp->s_tractrng = 1.5;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = STARBASE;
	    break;
	case GALAXY: 
     	    shipp->s_turns = 85000; 
            shipp->s_accint = 100;
	    shipp->s_decint = 150;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 50;
	    shipp->s_plasmadamage = 120;
	    shipp->s_phaserfuse = 8;
	    shipp->s_phaserdamage = 100;
	    shipp->s_torpspeed = 13;
	    shipp->s_torpfuse = 35 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 30;
	    shipp->s_plasmaturns = 1;
	    shipp->s_maxspeed = 9;
	    shipp->s_repair = 125;
	    shipp->s_maxfuel = 12000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 9 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 30 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 9 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 5;  /* was 6 */
	    shipp->s_cloakcost = 35;
	    shipp->s_recharge = 14;
	    shipp->s_maxarmies = 13;
	    shipp->s_maxshield = 140;
	    shipp->s_maxdamage = 120;
	    shipp->s_wpncoolrate = 4;
	    shipp->s_egncoolrate = 5;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 2450;
	    shipp->s_tractstr = 3500;
	    shipp->s_tractrng = 1.0;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = GALAXY;
	    break;
#ifdef nodef
	case ATT:
	    shipp->s_turns = 2140000000;
	    shipp->s_accint = 3000;
	    shipp->s_decint = 3000;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 1000;
	    shipp->s_plasmadamage = 1000;
	    shipp->s_phaserfuse = 2;
	    shipp->s_phaserdamage = 10000;
	    shipp->s_torpspeed = 30;
	    shipp->s_torpfuse = 200 + 20;
	    shipp->s_torpturns = 10;
	    shipp->s_plasmaspeed = 45;
	    shipp->s_plasmafuse = 10000;
	    shipp->s_plasmaturns = 14;
	    shipp->s_maxspeed = 60;
	    shipp->s_repair = 30000;
	    shipp->s_maxfuel = 60000;
	    shipp->s_detcost = 1;
	    shipp->s_torpcost = 1;
	    shipp->s_plasmacost = 1;
	    shipp->s_phasercost = 1;
	    shipp->s_warpcost = 1;
	    shipp->s_cloakcost = 1;
	    shipp->s_recharge = 1000;
	    shipp->s_maxarmies = 1000;
	    shipp->s_maxshield = 30000;
	    shipp->s_maxdamage = 30000;
	    shipp->s_wpncoolrate = 100;
	    shipp->s_egncoolrate = 100;
	    shipp->s_maxwpntemp = 10000;
	    shipp->s_maxegntemp = 10000;
	    shipp->s_mass = 32765;
	    shipp->s_tractstr = 32765;
	    shipp->s_tractrng = 25;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = ATT;
	    break;
#endif
	default:
	    shipp->s_turns = 170000;
	    shipp->s_accint = 150;
	    shipp->s_decint = 200;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 40;
	    shipp->s_plasmadamage = 100;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 100;
	    shipp->s_torpspeed = 12;
	    shipp->s_torpfuse = 40 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 35;
	    shipp->s_plasmaturns = 1;
	    shipp->s_maxspeed = 9;
	    shipp->s_repair = 110;
	    shipp->s_maxfuel = 10000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 7 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 30 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 7 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 4;
	    shipp->s_cloakcost = 26;
	    shipp->s_recharge = 12;
	    shipp->s_maxarmies = 10;
	    shipp->s_maxshield = 100;
	    shipp->s_maxdamage = 100;
	    shipp->s_wpncoolrate = 2;
	    shipp->s_egncoolrate = 6;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 2000;
	    shipp->s_tractstr = 3000;
	    shipp->s_tractrng = 1.0;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = CRUISER;
	    break;
	}
    }
    else
    {
	switch (s_type) {
	case SCOUT:
	    shipp->s_turns = 570000;
	    shipp->s_accint = 185; /* was 185 */
	    shipp->s_decint = 270; /* was 270 */
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 25; /* was 25 */
	    shipp->s_plasmadamage = -1;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 75; /* was 75 */
	    shipp->s_torpspeed = 16; /* was 16 */
	    shipp->s_torpfuse = 30 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 0;
	    shipp->s_plasmafuse = 0;
	    shipp->s_plasmaturns = 0;
	    shipp->s_maxspeed = 12;
	    shipp->s_repair = 80;
	    shipp->s_maxfuel = 5000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 7 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 20 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 7 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 2;
	    shipp->s_cloakcost = 18;
	    shipp->s_recharge = 16;
	    shipp->s_maxarmies = 2;
	    shipp->s_maxshield = 75; /* was 75 */
	    shipp->s_maxdamage = 75;
	    shipp->s_wpncoolrate = 3;
	    shipp->s_egncoolrate = 7;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1200;
	    shipp->s_type = SCOUT;
	    shipp->s_mass = 1500;
	    shipp->s_tractstr = 2000;
	    shipp->s_tractrng = .7;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    break;
	case DESTROYER:
	    shipp->s_turns = 310000;
	    shipp->s_accint = 200;
	    shipp->s_decint = 300;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 30;
	    shipp->s_plasmadamage = 75;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 85;
	    shipp->s_torpspeed = 14;
	    shipp->s_torpfuse = 60 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 60;
	    shipp->s_plasmaturns = 2;
	    shipp->s_maxspeed = 10;
	    shipp->s_repair = 100;
	    shipp->s_maxfuel = 7000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 7 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 30 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 7 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 3;
	    shipp->s_cloakcost = 20;
	    shipp->s_recharge = 22;
	    shipp->s_maxarmies = 5;
	    shipp->s_maxshield = 85;
	    shipp->s_maxdamage = 85;
	    shipp->s_wpncoolrate = 2;
	    shipp->s_egncoolrate = 7;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 1800;
	    shipp->s_tractstr = 2500;
	    shipp->s_tractrng = .9;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = DESTROYER;
	    break;
	case BATTLESHIP: 
     	    shipp->s_turns = 75000; 
            shipp->s_accint = 80;
	    shipp->s_decint = 180;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 45;
	    shipp->s_plasmadamage = 130;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 105;
	    shipp->s_torpspeed = 15;
	    shipp->s_torpfuse = 80 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 70;
	    shipp->s_plasmaturns = 2;
	    shipp->s_maxspeed = 8;
	    shipp->s_repair = 125;
	    shipp->s_maxfuel = 14000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 9 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 30 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 10 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 6;
	    shipp->s_cloakcost = 30;
	    shipp->s_recharge = 28;
	    shipp->s_maxarmies = 6;
	    shipp->s_maxshield = 130;
	    shipp->s_maxdamage = 130;
	    shipp->s_wpncoolrate = 3;
	    shipp->s_egncoolrate = 6;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 2300;
	    shipp->s_tractstr = 4000;
	    shipp->s_tractrng = 1.2;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = BATTLESHIP;
	    break;
	case ASSAULT:
	    shipp->s_turns = 120000;
	    shipp->s_accint = 100;
	    shipp->s_decint = 200;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 30;
	    shipp->s_plasmadamage = -1;
	    shipp->s_phaserfuse = 10;
	    shipp->s_phaserdamage = 80;
	    shipp->s_torpspeed = 16;
	    shipp->s_torpfuse = 60 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 0;
	    shipp->s_plasmafuse = 0;
	    shipp->s_plasmaturns = 0;
	    shipp->s_maxspeed = 8;
	    shipp->s_repair = 120;
	    shipp->s_maxfuel = 6000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 9 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 20 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 7 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 3;
	    shipp->s_cloakcost = 17;
	    shipp->s_recharge = 20;
	    shipp->s_maxarmies = 20;
	    shipp->s_maxshield = 80;
	    shipp->s_maxdamage = 200;
	    shipp->s_wpncoolrate = 2;
	    shipp->s_egncoolrate = 6;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1200;
	    shipp->s_mass = 2300;
	    shipp->s_tractstr = 2500;
	    shipp->s_tractrng = .7;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = ASSAULT;
	    break;
	case STARBASE:
	    shipp->s_turns = 50000;
	    shipp->s_accint = 100;
	    shipp->s_decint = 200;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 30;
	    shipp->s_plasmadamage = 200;
	    shipp->s_phaserfuse = 4;
	    shipp->s_phaserdamage = 120;
	    shipp->s_torpspeed = 15;
	    shipp->s_torpfuse = 50 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 50;
	    shipp->s_plasmaturns = 2;
	    shipp->s_maxspeed=3;
	    shipp->s_repair = 140;
	    shipp->s_maxfuel = 60000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 10 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 25 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 8 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 10;
	    shipp->s_cloakcost = 75;
	    shipp->s_recharge = 40;
	    shipp->s_maxarmies = 25;
	    shipp->s_maxshield = 500;
	    shipp->s_maxdamage = 600;
	    shipp->s_wpncoolrate = 4;
	    shipp->s_egncoolrate = 4;
	    shipp->s_maxwpntemp = 1300;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 5000;
	    shipp->s_tractstr = 8000;
	    shipp->s_tractrng = 1.5;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = STARBASE;
	    break;
	case GALAXY: 
     	    shipp->s_turns = 85000;
            shipp->s_accint = 100;
	    shipp->s_decint = 150;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 50;
	    shipp->s_plasmadamage = 120;
	    shipp->s_phaserfuse = 6;
	    shipp->s_phaserdamage = 100;
	    shipp->s_torpspeed = 16;/*14*/
	    shipp->s_torpfuse = 70 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 60;
	    shipp->s_plasmaturns = 2;
	    shipp->s_maxspeed = 9;
	    shipp->s_repair = 125;
	    shipp->s_maxfuel = 12000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 9 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 30 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 9 * shipp->s_phaserdamage;	/* was 11 */
	    shipp->s_warpcost = 5;	/* was 6 */
	    shipp->s_cloakcost = 35;
	    shipp->s_recharge = 28;
	    shipp->s_maxarmies = 13;
	    shipp->s_maxshield = 140;
	    shipp->s_maxdamage = 120;
	    shipp->s_wpncoolrate = 4;
	    shipp->s_egncoolrate = 5;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 2450;
	    shipp->s_tractstr = 3800;
	    shipp->s_tractrng = 1.0;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = GALAXY;
	    break;
#ifdef nodef
	case ATT:
	    shipp->s_turns = 2140000000;
	    shipp->s_accint = 3000;
	    shipp->s_decint = 3000;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 1000;
	    shipp->s_plasmadamage = 1000;
	    shipp->s_phaserfuse = 2;
	    shipp->s_phaserdamage = 10000;
	    shipp->s_torpspeed = 30;
	    shipp->s_torpfuse = 200 + 20;
	    shipp->s_torpturns = 10;
	    shipp->s_plasmaspeed = 45;
	    shipp->s_plasmafuse = 10000;
	    shipp->s_plasmaturns = 14;
	    shipp->s_maxspeed = 60;
	    shipp->s_repair = 30000;
	    shipp->s_maxfuel = 60000;
	    shipp->s_detcost = 1;
	    shipp->s_torpcost = 1;
	    shipp->s_plasmacost = 1;
	    shipp->s_phasercost = 1;
	    shipp->s_warpcost = 1;
	    shipp->s_cloakcost = 1;
	    shipp->s_recharge = 1000;
	    shipp->s_maxarmies = 1000;
	    shipp->s_maxshield = 30000;
	    shipp->s_maxdamage = 30000;
	    shipp->s_wpncoolrate = 100;
	    shipp->s_egncoolrate = 100;
	    shipp->s_maxwpntemp = 10000;
	    shipp->s_maxegntemp = 10000;
	    shipp->s_mass = 32765;
	    shipp->s_tractstr = 32765;
	    shipp->s_tractrng = 25;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = ATT;
	    break;
#endif
	default:
	    shipp->s_turns = 170000;
	    shipp->s_accint = 150;
	    shipp->s_decint = 200;
	    shipp->s_accs = 100;
	    shipp->s_torpdamage = 40;
	    shipp->s_plasmadamage = 100;
	    shipp->s_phaserfuse = 20;
	    shipp->s_phaserdamage = 100;
	    shipp->s_torpspeed = 12;
	    shipp->s_torpfuse = 40 + 20;
	    shipp->s_torpturns = 0;
	    shipp->s_plasmaspeed = 15;
	    shipp->s_plasmafuse = 70;
	    shipp->s_plasmaturns = 2;
	    shipp->s_maxspeed = 9;
	    shipp->s_repair = 110;
	    shipp->s_maxfuel = 10000;
	    shipp->s_detcost = 100;
	    shipp->s_torpcost = 7 * shipp->s_torpdamage;
	    shipp->s_plasmacost = 30 * shipp->s_plasmadamage;
	    shipp->s_phasercost = 7 * shipp->s_phaserdamage;
	    shipp->s_warpcost = 4;
	    shipp->s_cloakcost = 26;
	    shipp->s_recharge = 24;
	    shipp->s_maxarmies = 10;
	    shipp->s_maxshield = 100;
	    shipp->s_maxdamage = 100;
	    shipp->s_wpncoolrate = 2;
	    shipp->s_egncoolrate = 5;
	    shipp->s_maxwpntemp = 1000;
	    shipp->s_maxegntemp = 1000;
	    shipp->s_mass = 2000;
	    shipp->s_tractstr = 3000;
	    shipp->s_tractrng = 1.0;
	    shipp->s_width = 20;
	    shipp->s_height = 20;
	    shipp->s_type = CRUISER;
	    break;
	}
    }
}


