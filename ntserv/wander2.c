#include <stdlib.h>
#include <math.h>
#include "defs.h"
#include "struct.h"
#include "data.h"  /* Includes global variable extern defs */

#define COS(x) ((x) >= 0.0 ? Cosine[(int)(x)] : Cosine[(int)(-(x))])
#define SIN(x) ((x) >= 0.0 ? Sine[(int)(x)] : -Sine[(int)(-(x))])

static int pl_home[4];
static int pl_core[4][10];
static int pl_dist[4][10];
static const float increment = 0.016f;
static const float incrementrecip = 62.5f;
static float *Cosine, *Sine;

static void pfree(void)
{
  free(Cosine);
  free(Sine);
}

/* call only once */
void pinit(void)
{
    int i, j;
    int pre;

    void pmove();

    pre = 3.5 / increment;

    Cosine = (float *) malloc(pre * sizeof(float));
    if (Cosine == NULL) abort();
    Sine = (float *) malloc(pre * sizeof(float));
    if (Sine == NULL) abort();
    atexit(pfree);

    for (i = 0; i < pre; i++) {
	Cosine[i] = cosf(i*increment);
	Sine[i] = sinf(i*increment);
    }

    pl_home[0] = 0;
    pl_core[0][0] = 5;
    pl_core[0][1] = 7;
    pl_core[0][2] = 8;
    pl_core[0][3] = 9;
    pl_home[1] = 10;
    pl_core[1][0] = 12;
    pl_core[1][1] = 15;
    pl_core[1][2] = 16;
    pl_core[1][3] = 19;
    pl_home[2] = 20;
    pl_core[2][0] = 24;
    pl_core[2][1] = 26;
    pl_core[2][2] = 29;
    pl_core[2][3] = 25;
    pl_home[3] = 30;
    pl_core[3][0] = 34;
    pl_core[3][1] = 37;
    pl_core[3][2] = 38;
    pl_core[3][3] = 39;
    
    for (i = 0; i < 4; i++) {
	for (j = 0; j < 4; j++) {
	    pl_dist[i][j] = hypot(
		planets[pl_core[i][j]].pl_x - planets[pl_home[i]].pl_x,
		planets[pl_core[i][j]].pl_y - planets[pl_home[i]].pl_y);
	}
    }
}

void pmove(void)
{
    int i, j;
    float dir;
    static int planeti=0, planetj=0;

    /* todo: fps support */
    for (i = 0; i < 4; i++) {
	for (j = 0; j < 4; j++) { 
    i = planeti;
    j = planetj;
    planetj = (planetj + 1) % 4;
    if (planetj == 0)
	planeti = (planeti + 1) % 4;

	    dir = atan2f(planets[pl_core[i][j]].pl_y - planets[pl_home[i]].pl_y,
	                 planets[pl_core[i][j]].pl_x - planets[pl_home[i]].pl_x);
	    if (dir >= 0.f)
		dir = dir * incrementrecip + 1.5;
	    else
		dir = dir * incrementrecip + 0.5;
	    
	    planets[pl_core[i][j]].pl_x =
		planets[pl_home[i]].pl_x +
		    (int) (pl_dist[i][j] * COS(dir));
	    planets[pl_core[i][j]].pl_y =
		planets[pl_home[i]].pl_y +
		    (int) (pl_dist[i][j] * SIN(dir));

	    planets[pl_core[i][j]].pl_flags |= PLREDRAW;
	}

    }
}
