/*
 * $Id: rsa_math.h,v 1.1 2005/03/21 05:23:44 jerub Exp $
 *
 * This is the server-side custom math code that is used by
 * rsa_util.c.  No crypto code is contained in this file.
 *
 * Originally written by Ray Jones.  Updated by Dave Ahn.
 * */

#ifndef __INCLUDED_rsa_math_h__
#define __INCLUDED_rsa_math_h__

/****************************************************************************
 * to work correctly, SIZE must be 2 * the largest number you plan to deal 
 * with, and also a power of 2.  Unfortunate, but true.  For the netrek-RSA 
 * scheme, it has to be at least twice 80. 
 */

#define SIZE 64

/* function prototypes */

void raw_to_num(int *out, const unsigned char *in, const int digits);
void num_to_raw(unsigned char *out, const int *in, const int digits);

void setup_modulus(const int *modulus);
void expmod(int *out, const int *a, const int *b);
void cleanup_modulus(void);

#endif /* __INCLUDED_rsa_math_h__ */
