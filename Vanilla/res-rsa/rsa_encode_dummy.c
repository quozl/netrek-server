/*
 * $Id: rsa_encode_dummy.c,v 1.1 2005/03/21 05:23:44 jerub Exp $
 *
 * This is a dummy server-side RSA support code that does nothing.
 *
 * Originally written by Bob Tanner.  Updated by Dave Ahn.
 *
 */

#include <gmp.h>
#include "rsa_gmp.h"

static void raw_to_num(mpz_t out, const unsigned char *in, const int digits) {
}
 
static void num_to_raw(unsigned char *out, mpz_t in, const int digits) {
}

void rsa_encode(unsigned char *out, unsigned char *message,
                unsigned char *key, unsigned char *global, const int digits) {
}

