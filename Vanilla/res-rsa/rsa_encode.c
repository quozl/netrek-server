/*
 * $Id: rsa_encode.c,v 1.1 2005/03/21 05:23:44 jerub Exp $
 *
 * This is the server-side RSA support code that has no dependencies
 * on GMP or Berkeley MP libraries.
 *
 * Originally written by Ray Jones.  Updated by Dave Ahn.
 * */

#include "config.h"
#include "rsa.h"
#include "rsa_math.h"

/****************************************************************************
 * The final product.  Take in a message, a key, and the global N to mod by,
 * and do the actual RSA calculation.  Return the new stuff.
 */

void rsa_encode(unsigned char *out, unsigned char *message,
                unsigned char *key, unsigned char *global, const int digits) {

  int i;
  int result[SIZE], n_message[SIZE], n_key[SIZE], n_global[SIZE];

  raw_to_num(n_global, global, digits);
  setup_modulus(n_global);

  raw_to_num(n_message, message, digits);
  raw_to_num(n_key, key, digits);
  expmod(result, n_message, n_key);

  /* clean up */
  for (i = 0; i < SIZE; i++) {
    n_key[i] = n_message[i] = 0;
  }

  cleanup_modulus();

  num_to_raw(out, result, digits);

}
