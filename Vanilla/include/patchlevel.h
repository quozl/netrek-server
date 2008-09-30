/* patchlevel.h -- the current patch level of the source
 *
 * Whenever you make a patch that you want to send in, name it in
 *   the patchname array below, make patch, and submit it.
 * 
 * Generally, the number of patchnames should not be very high,
 *   probably no more than one.  However, if you patch over someone
 *   else's patch before they get into a patch release, this will
 *   help keep the revisions straight.
 *
 * The code maintainer is to
 *  (a) reset this to zero before each major release, and;
 *  (b) increment this number before each patch release.
 */
#define PATCHLEVEL 0
#if !defined(NULL)
#define NULL 0
#endif
