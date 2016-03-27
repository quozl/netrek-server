/*
 * defs.h
 */
#include "copyright.h"

#define SHRINKFRAME 1
#define REALLYFAST 1
#define XSG 1

/* record defines */

/* translate index variables to actual updates per second */
#define UPS(x)			(1000000/uspeeds[x])

#define NICE			"nice"
#ifdef SYSV
#define COMPRESS		"/usr/bin/compress"
#define UNCOMPRESS		"/usr/bin/uncompress"
#else
#define COMPRESS		"/usr/ucb/compress"
#define UNCOMPRESS		"/usr/ucb/uncompress"
#endif

#define MAXFRAMES   200
#define FORWARD     0x1
#define BACKWARD    0x2
#define SINGLE_STEP 0x4
#define DO_STEP     0x8
#define T_SKIP      0x10
#define T_FF	    0x20	/* TSH 4/93 */

/* These are configuration definitions */

#undef torpColor
#define torpColor(t)		\
	(myTorp(t) ? myColor : shipCol[remap[players[(t)->t_owner].p_team]])
#undef plasmatorpColor
#define plasmatorpColor(t)	torpColor(t)
#undef phaserColor
#define phaserColor(p)		\
	(myPhaser(p) ? myColor : shipCol[remap[players[(p) - phasers].p_team]])
/* 
 * Cloaking phase (and not the cloaking flag) is the factor in determining 
 * the color of the ship.  Color 0 is white (same as 'myColor' used to be).
 */
#undef playerColor
#define playerColor(p)		\
	(myPlayer(p) ? myColor : shipCol[remap[(p)->p_team]])
#undef planetColor
#define planetColor(p)		\
	(shipCol[remap[(p)->pl_owner]])

#undef planetFont
#define planetFont(p)		\
	(myPlanet(p) ? W_BoldFont : friendlyPlanet(p) ? W_UnderlineFont \
	    : W_RegularFont)
#undef shipFont
#define shipFont(p)		\
	(myPlayer(p) ? W_BoldFont : friendlyPlayer(p) ? W_UnderlineFont \
	    : W_RegularFont)

char *getdefault();

#if defined (SYSV) && !defined(NBR)
#define bcopy(s,d,l) memmove((d), (s), (l))
#define bzero(b, l)  memset((b), 0, (l))
#define bcmp(s1, s2, n) (memcmp((s1), (s2), n)!=0)
#define index(s, c)  strchr((s), (c))
#define rindex(s, c) strrchr((s), (c))
#define random       rand
#endif
