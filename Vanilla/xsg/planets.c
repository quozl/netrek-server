/* 
 * planets.c
 *
 * Kevin P. Smith  2/21/89
 *
 * This file contains the galaxy definition as well as some support for 
 *  determining when parts of the galactic map need to be redrawn.
 */
#include "copyright2.h"

#include <stdio.h>
#include <math.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

/* Note:  DETAIL * MUST * be a factor of GWIDTH. */
#define DETAIL 40		/* Size of redraw array */
#define DIST 4500		/* Distance to turn on redraw flag */
#define SIZE (GWIDTH/DETAIL)

int redraws[DETAIL][DETAIL];
static int initialized=0;

initPlanets()
{
    int i,j,k;
    int endi, endj;
    struct planet *pl;
    int offset=DIST/SIZE+1;
    float dist;

    for (i=0; i<DETAIL; i++) {
	for (j=0; j<DETAIL; j++) {
	    redraws[i][j]= -1;
	}
    }

    for (k=0, pl=planets; k<MAXPLANETS; k++, pl++) {
	endi=pl->pl_x / SIZE+offset+1;
	for (i=(pl->pl_x / SIZE)-offset; i<endi && i<DETAIL; i++) {
	    if (i<0) i=0;
	    endj=pl->pl_y / SIZE+offset+1;
	    for (j=(pl->pl_y / SIZE) - offset; j<endj && j<DETAIL; j++) {
		dist=hypot((float) (pl->pl_x - (i*SIZE + SIZE/2)),
			   (float) (pl->pl_y - (j*SIZE + SIZE/2)));
		if (dist<=DIST) {
		    redraws[i][j]=k;
		}
	    }
	}
	updatePlanet[k] = 1;
    }
    initialized=1;
}

checkRedraw(x,y)
int x,y;
{
    int i;
    extern int plredraw[];

    if (!initialized || x<0 || y<0 || x>=GWIDTH || y>=GWIDTH) return;
    i=redraws[x/SIZE][y/SIZE];
    if (i==-1) return;
    /*planets[i].pl_flags |= PLREDRAW;*/
    plredraw[i] = 1;
}
