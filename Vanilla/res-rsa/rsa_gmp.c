/*
 * $Id: rsa_gmp.c,v 1.1 2005/03/21 05:23:44 jerub Exp $
 *
 * This is the server-side RSA math support code that uses the GNU MP
 * library.  Berkeley MP is no longer supported.  No crypto code is
 * contained in this file.
 *
 * Originally written by Ray Jones.  Updated by Dave Ahn.
 * */

#include <gmp.h>
#include "config.h"
#include "rsa_gmp.h"

/* convert from unsigned char array to GMP form */

void raw_to_num(mpz_t out, const unsigned char *in, const int digits) {

  int i;

  mpz_t temp;
  mpz_t twofiftysix;
  mpz_t thisval;

  mpz_init(temp);
  mpz_init(twofiftysix);
  mpz_init(thisval);

  mpz_set_ui(temp, 0);
  mpz_set_ui(twofiftysix, 256);

  for (i=0; i<digits; i++) {

    mpz_mul(temp, temp, twofiftysix);
    mpz_set_ui(thisval, in[digits - i - 1]);
    mpz_add(temp, temp, thisval);

  }

  mpz_set(out, temp);

  mpz_clear(temp);
  mpz_clear(twofiftysix);
  mpz_clear(thisval);

}

/* convert from GMP form to unsigned char array */
void num_to_raw(unsigned char *out, mpz_t in, const int digits) {

  /* note: mpz_t in overwritten */

  int i;
  unsigned long temp;
  mpz_t zero;

  mpz_init(zero);
  mpz_set_ui(zero, 0);

  for (i=0; i<digits; i++) {

    if (!mpz_cmp(in, zero))
      temp = 0;

    else {

#if __GNU_MP_VERSION < 2

      /* gmp 1 doesn't seem to have mpz_fdiv_q_ui that returns the
         remainder in integer form.  use fdiv_qr_ui and discard the
         remainder */

      mpz_t foo;
      mpz_init(foo);

      temp = mpz_fdiv_qr_ui(in, foo, in, 256);

      mpz_clear(foo);

#else

      /* gmp 2.x */
      temp = mpz_fdiv_q_ui(in, in, 256);

#endif

    }

    out[i] = (unsigned char) temp & 0xFF;

  }

  mpz_clear(zero);

}

