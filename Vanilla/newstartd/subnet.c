/* 	$Id: subnet.c,v 1.1 2005/03/21 05:23:43 jerub Exp $	 */

#ifdef lint
static char vcid[] = "$Id: subnet.c,v 1.1 2005/03/21 05:23:43 jerub Exp $";
#endif /* lint */

/*#define SUBNET*/


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>

#ifdef SUBNET

/*
 * The following routines provide a general interface for
 * subnet support.  Like the library function "inet_netof",
 * which returns the standard (i.e., non-subnet) network
 * portion of an internet address, "inet_snetof" returns
 * the subnetwork portion -- if there is one.  If there
 * isn't, it returns 0.
 *
 * Subnets, under 4.3, are specific to a given set of
 * machines -- right down to the network interfaces.
 * Because of this, the function "getifconf" must be
 * called first.  This routine builds a table listing
 * all the (internet) interfaces present on a machine,
 * along with their subnet masks.  Then when inet_snetof
 * is called, it can quickly scan this table.
 *
 * Unfortunately, there "ain't no graceful way" to handle
 * certain situations.  For example, the kernel permits
 * arbitrary subnet bits -- that is, you could have a
 * 22 bit network field and a 10 bit subnet field.
 * However, due to braindamage at the user level, in
 * such sterling routines as getnetbyaddr, you need to
 * have a subnet mask which is an even multiple of 8.
 * Unless you are running with class C subnets, in which
 * case it should be a multiple of 4.  Because of this rot,
 * if you have non-multiples of 4 bits of subnet, you should
 * define DAMAGED_NETMASK when you compile.  This will round
 * things off to a multiple of 8 bits.
 *
 * And even that may not work.
 */

/*
 * One structure for each interface, containing
 * the network number and subnet mask, stored in HBO.
 */
struct in_if {
	U_LONG	i_net;		/* Network number, shifted right */
	U_LONG	i_subnetmask;	/* Subnet mask for this if */
	int	i_bitshift;	/* How many bits right for outside */
};

/*
 * Table (eventually, once we malloc) of
 * internet interface subnet informaiton.
 */
static	struct in_if	*in_ifsni;

static	int		if_count;

/*
 * Get the network interface configuration,
 * and squirrel away the network numbers and
 * subnet masks of each interface.  Return
 * number of interfaces found, or -1 on error.
 * N.B.: don't call this more than once...
 */

getifconf()
{
	register int	i, j;
	int		s;
	struct ifconf	ifc;
	char		buf[1024];
	register struct ifreq	*ifr;
	U_LONG		inet_netof();

	/*
	 * Find out how many interfaces we have, and malloc
	 * room for information about each one.
	 */

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return (-1);

	ifc.ifc_buf = buf;
	ifc.ifc_len = sizeof (buf);

	if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
		(void) close(s);
		return (-1);
	}

	/*
	 * if_count here is the count of possible
	 * interfaces we may be interested in... actual
	 * interfaces may be less (some may not be internet,
	 * not all are necessarily up, etc.)
	 */

	if_count = ifc.ifc_len / sizeof (struct ifreq);

	in_ifsni = (struct in_if *) malloc(if_count * sizeof (struct in_if));
	if (in_ifsni == 0) {
		(void) close(s);
		return (-1);
	}

	for (i = j = 0; i < if_count; ++i) {
		ifr = &ifc.ifc_req[i];
		if (ioctl(s, SIOCGIFFLAGS, ifr) < 0)
			continue;
		if ((ifr->ifr_flags & IFF_UP) == 0)
			continue;
		if (ioctl(s, SIOCGIFADDR, ifr) < 0)
			continue;
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		in_ifsni[j].i_net = inet_netof((*(struct sockaddr_in *)&ifr->ifr_addr).sin_addr);
		if (ioctl(s, SIOCGIFNETMASK, ifr) < 0)
			continue;
		in_ifsni[j].i_subnetmask = ntohl((*(struct sockaddr_in *)&ifr->ifr_addr).sin_addr.s_addr);
		in_ifsni[j].i_bitshift = bsr(in_ifsni[j].i_subnetmask);
		j++;
	}

	if_count = j;

	(void) close(s);

	return (if_count);
}


/*
 * Return the (sub)network number from an internet address.
 * "in" is in NBO, return value in host byte order.
 * If "in" is not a subnet, return 0.
 */

U_LONG
inet_snetof(in)
	U_LONG	in;
{
	register int	j;
	register U_LONG	i = ntohl(in);
	register U_LONG	net;
	U_LONG		inet_netof(), inet_lnaof();

	net = inet_netof(in);

	/*
	 * Check whether network is a subnet;
	 * if so, return subnet number.
	 */
	for (j = 0; j < if_count; ++j)
		if (net == in_ifsni[j].i_net) {
			net = i & in_ifsni[j].i_subnetmask;
			if (inet_lnaof(htonl(net)) == 0)
				return (0);
			else
				return (net >> in_ifsni[j].i_bitshift);
		}

	return (0);
}


/*
 * Return the number of bits required to
 * shift right a mask into a getnetent-able entitity.
 */

bsr(mask)
	register int	mask;
{
	register int	count = 0;

	while ((mask & 1) == 0) {
		++count;
		mask >>= 1;
	}
#ifdef DAMAGED_NETMASK
	count /= 8;			/* XXX gag retch puke barf */
	count *= 8;
#endif
	return (count);
}

#endif
