/*
 * $Id: rsa_gmp.h,v 1.1 2005/03/21 05:23:44 jerub Exp $
 *
 * This is the server-side RSA support code that uses the GNU MP
 * library.  Berkeley MP is no longer supported.  No crypto code is
 * contained in this file.
 *
 * Originally written by Ray Jones.  Updated by Dave Ahn.
 * */

#ifndef __INCLUDED_rsa_gmp_h__
#define __INCLUDED_rsa_gmp_h__

#include <gmp.h>

/* GMP 2.x has functions that GMP 1.x doesn't */

#if __GNU_MP_VERSION < 2

typedef MP_INT mpz_t[1];

#define mpz_fdiv_qr     mpz_mdivmod
#define mpz_fdiv_qr_ui  mpz_mdivmod_ui

#endif /* __GNU_MP_VERSION < 2 */

/* function prototypes */

void raw_to_num(mpz_t out, const unsigned char *in, const int digits);
void num_to_raw(unsigned char *out, mpz_t in, const int digits);


#endif /* __INCLUDED_rsa_gmp_h__ */
