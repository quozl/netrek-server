#include <sys/time.h>

/* bind to any IP address available on the local host */
#define SOLICIT_BIND_ANY 0

/* do not report players who are local */
#define SOLICIT_NOLOCAL 1

/* bytes to reserve for outgoing packet to metaserver */
#define MAXMETABYTES 2048

/* maximum number of metaservers lines supported */
#define MAXMETASERVERS 8
/* Note: for an INL server we need two entries in metaservers file for
   each real metaserver on the internet */

/* minimum allowable delay between updates to metaserver */
#define META_MINIMUM_DELAY 60
/* Note: changing this may cause the metaserver to delist your server */

/* maximum delay between updates to metaserver */
#define META_MAXIMUM_DELAY 900

/* ship classes (wish these were in data.c/data.h) */
/* static char *ships[] = {"SC", "DD", "CA", "BB", "AS", "SB", "GA"}; */

/* structure of information about a single metaserver */
struct metaserver
{
  /* data items derived from metaservers file */
  char host[32];		/* address of metaserver (DNS)		*/
  int port;			/* port of metaserver			*/
  int minimum;			/* minimum update time			*/
  int maximum;			/* maximum update time			*/
  char ours[32];		/* DNS address of server 	 	*/
  char type[2];			/* server type code (B/P/C/H/?)		*/
  int pport;			/* server main player port (e.g. 2592)	*/
  int oport;			/* server observer player port		*/
  char comment[32];		/* comment string			*/

  /* our own data about the communication with the metaserver */
  int sock;			/* our socket number                    */
  struct sockaddr_in address;	/* address of metaserver		*/
  time_t sent;			/* date time metaserver last updated	*/
  char prior[MAXMETABYTES];	/* prior packet sent			*/
};

void solicit(int force);
void solicit_delist(char *meta, char *host);
