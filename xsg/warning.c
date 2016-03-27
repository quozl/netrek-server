/*
 * warning.c
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

/*
** The warning in text will be printed in the warning window.
** The message will last WARNTIME/10 seconds unless another message
** comes through and overwrites it.
*/
warning(text)
char *text;
{
    warntimer = udcounter + WARNTIME;  /* set the line to be cleared */

    if (warncount > 0) {
	W_ClearArea(warnw, 5,5, W_Textwidth*warncount, W_Textheight,backColor);
    }
    warncount = strlen(text);
    W_WriteText(warnw, 5, 5, textColor, text, warncount, W_RegularFont);
}
