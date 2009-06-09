/* colors.c
 * 
 * Kevin P. Smith  6/11/89
 */
#include "copyright2.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"		/* TSH 3/10/93 */
#include "localdata.h"

getColorDefs()
{
    borderColor=W_Grey;
    backColor=W_Black;
    foreColor=W_White;
    textColor=W_White;
    shipCol[0]=W_Grey;
    shipCol[1]=W_Yellow;
    shipCol[2]=W_Red;
    shipCol[3]=W_Green;
    shipCol[4]=W_Cyan;
    warningColor=W_Red;
    unColor=W_Grey;
    rColor=W_Red;
    yColor=W_Yellow;
    gColor=W_Green;
    myColor=W_White;
}

