/*
 * $Id: rsa_encode_gmp.c,v 1.1 2005/03/21 05:23:44 jerub Exp $
 *
 * This is the server-side RSA support code that uses the GNU MP
 * library.  Berkeley MP is no longer supported.
 *
 * Originally written by Ray Jones.  Updated by Dave Ahn.
 * */

#include <gmp.h>
#include "rsa.h"
#include "config.h"
#include "rsa_gmp.h"

/* RSA calculation using the message, key, global N modulus */

void rsa_encode(unsigned char *out, unsigned char *message,
                unsigned char *key, unsigned char *global, const int digits) {

  mpz_t result;
  mpz_t n_message;
  mpz_t n_key;
  mpz_t n_global;

  mpz_init(result);
  mpz_init(n_message);
  mpz_init(n_key);
  mpz_init(n_global);

  mpz_set_ui(result, 0);
  mpz_set_ui(n_message, 0);
  mpz_set_ui(n_key, 0);
  mpz_set_ui(n_global, 0);

  raw_to_num(n_global, global, digits);
  raw_to_num(n_message, message, digits);
  raw_to_num(n_key, key, digits);

  mpz_powm(result, n_message, n_key, n_global);

  num_to_raw(out, result, digits);

  mpz_clear(result);
  mpz_clear(n_message);
  mpz_clear(n_key);
  mpz_clear(n_global);
  
}
