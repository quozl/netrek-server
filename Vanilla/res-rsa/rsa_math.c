/*
 * $Id: rsa_math.c,v 1.1 2005/03/21 05:23:44 jerub Exp $
 *
 * This is the server-side custom math code that is used by
 * rsa_util.c.  No crypto code is contained in this file.
 *
 * Originally written by Ray Jones.  Updated by Dave Ahn.
 * */

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "rsa_math.h"

#undef  MATH_DEBUG	/* enable debugging */
#undef  MATH_FAST	/* use macros instead of compiler inlining */


/****************************************************************************
 * to work correctly, SIZE must be 2 * the largest number you plan to deal 
 * with, and also a power of 2.  Unfortunate, but true.  For the netrek-RSA 
 * scheme, it has to be at least twice 80. 
 */

/* this should already be defined by rsa_math.h */
#ifndef SIZE
#define SIZE 64
#endif

inline
static void math_exit(const int i) {

  printf("math_exit: value = %d\n", i);
  exit(1);

}

/* debugging */

#ifdef MATH_DEBUG

inline
static void print_num(const int *n, const int digits) {

  int i;

  for (i=0; i<digits; i++) {

    if (i && !(i % 40)) printf("\\\n");
    printf("%0.2X", n[digits - i - 1]);

  }

  printf("\n");
}

#endif /* MATH_DEBUG */


/****************************************************************************
 * the algorithms here are not picky about representations.  Strictly
 * speaking, it's a polynomial with SIZE coefficients, evaluated at x = 256.
 * Sometimes, a coefficient will move above 255.  renormalize() takes care of
 * this to prevent overflow and other problems.  
 */

inline
static void renormalize(int *out) {

  int i, temp;

  temp = 0;
    
  for (i=0; i<SIZE; i++) {

    temp += *out;
    *(out++) = temp & 0xFF;
    temp >>= 8;

  }

  if (temp) math_exit(temp);
}

/****************************************************************************
 * Add and Subtract.  Simple implementations, obviously, given that we don't
 * need to keep the number normalized.
 */

#ifdef RSA_FAST

/* these macros really aren't necessary with modern day compilers that
   are smart about inlining */

#define add(out, a, b, digits) { \
  int tempi, *tempout, *tempa, *tempb;\
  for(tempi=0,tempa=(a),tempb=(b),tempout=(out);tempi<(digits);tempi++)\
    *(tempout++)= *(tempa++)+ *(tempb++);\
}

#define subtract(out, a, b, digits) {\
  int tempi, *tempout, *tempa, *tempb;\
  for(tempi=0,tempa=(a),tempb=(b),tempout=(out);tempi<(digits);tempi++)\
    *(tempout++)= *(tempa++)- *(tempb++);\
}

#else /* !RSA_FAST */

inline
static void add(int *out, const int *a, const int *b, const int digits) {

  int i;

  for (i=0; i<digits; i++) {
    *(out++) = *(a++) + *(b++);
  }

}

inline
static void subtract(int *out, const int *a, const int *b, const int digits) {

  int i;

  for (i=0; i<digits; i++) {
    *(out++) = *(a++) - *(b++);
  }

}

#endif /* RSA_FAST */


/****************************************************************************
 * The multiply and square functions are forms of an algorithm used to
 * multiply polynomials.  it works based on the fact that
 * (ax + b) * (cx + d) = (a * c)x^2 + ((a + b) * (c + d) - (a * c) - (b * d)
 *    (a * c)x^2 + ((a + b) * (c + d) - (a * c) - (b * d))x + (b * d)
 * This allows us to use only 3 multiplies rather than the brute force 4.
 * This algorithm is based on the equivalent given in "Algorithms in C" by
 * Robert Sedgewick.
 */

/* a and b are digits long, out is 2 * digits */
inline
static void multiply(int *out, int *a, int *b, const int digits) {

  int temp[SIZE];
  int mid1[SIZE];
  int mid2[SIZE];
  int i, new_digits;

  if (new_digits = digits >> 1) {

    multiply(out, a, b, new_digits);
    multiply(&(out[digits]), &(a[new_digits]), &(b[new_digits]), new_digits);

    add(mid1, a, &(a[new_digits]), new_digits);
    add(mid2, b, &(b[new_digits]), new_digits);
    multiply(temp, mid1, mid2, new_digits);
    subtract(temp, temp, out, digits);
    subtract(temp, temp, &(out[digits]), digits);
	
    add(&(out[new_digits]), &(out[new_digits]), temp, digits);

    return;

  }

  i = (*a) * (*b);
  *out = i & 0xFF;
  *(out + 1) = i >> 8;

}

/* a is digits long, out is 2 * digits */
inline
static void square(int *out, int *a, const int digits) {

  int temp[SIZE];
  int i, new_digits;

  if (new_digits = digits >> 1) {

    square(out, a, new_digits);
    square(&(out[digits]), &(a[new_digits]), new_digits);
	
    multiply(temp, a, &(a[new_digits]), new_digits);

    /* multiply by 2 */
    for (i=0; i<digits; i++) {
      temp[i] <<= 1;
    }

    add(&(out[new_digits]), &(out[new_digits]), temp, digits);
    return;

  }

  i = (*a) * (*a);
  *out = i & 0xFF;
  *(out + 1) = i >> 8;

}

/****************************************************************************
 * bitwise shifts, copy, and compare.  compare() only accepts normalized
 * numbers.   
 */

inline
static void shift_left1(int *n) {

  int i;

  for (i=0; i<SIZE; i++) {
    n[i] <<= 1;
  }

}

inline
static void shift_right1(int *n) {

  int i;

  for (i=0; i<(SIZE - 1); i++) {

    n[i] >>= 1;

    if (n[i + 1] & 0x1) {
      n[i] += 0x80;
    }

  }

  n[i] >>= 1;

}

inline
static void copy(int *out, const int *in) {

  int i;

  for (i=0; i<SIZE; i++) {
    out[i] = in[i];
  }

}

/* returns 1 if a > b, -1 if b > a, or 0 if a == b */
inline
static int compare(const int *a, const int *b)  {

  int i;

  for (i=0; i<SIZE; i++) {

    if (a[SIZE - i - 1] > b[SIZE - i - 1]) return 1;
    if (a[SIZE - i - 1] < b[SIZE - i - 1]) return -1;
  }

  return 0;

}

/****************************************************************************
 * This method for doing mods is from the same book as the multiply function.
 * It's based on the fact that 
 * (ax + b) % m = (a(x % m) + b) % m
 * Before calling modulus or expmod, you have to call setup_modulus.  You don't
 * have to pass the modulus into modulus or expmod, since it's in the table.
 * cleanup_modulus will clean up the tables afterwards.
 */

static int modulus_size;
static int modulus_in_table[SIZE];
static int modulus_table[SIZE][SIZE];

void setup_modulus(const int *modulus) {

  int i;
  int temp[SIZE];

  copy(modulus_in_table, modulus);

  for (i=0; i<SIZE; i++) {
    temp[i] = 0;
  }

  temp[0] = 1;

  for (i=0; i<SIZE; i++) {
    if (modulus[i]) {
      modulus_size = i + 1;
    }
  }

  i = 0;

  /* while ((i >> 3) < SIZE) */
  while (i<(SIZE << 3)) {

    if (!(i & 0x7)) {
      copy(modulus_table[i >> 3], temp);
    }


    shift_left1(temp);
    renormalize(temp);

    if (compare(temp, modulus) > 0) {

      subtract(temp, temp, modulus, SIZE);
      renormalize(temp);
    }

    i++;

  }

  for (i=0; i<SIZE; i++) {
    temp[i] = 0;
  }

}

inline
static void modulus(int *out, int *a) {

  int temp[SIZE], from_table[SIZE];
  int i, j, c;

  renormalize(a);
  copy (temp, a);

  while ((c = compare(temp, modulus_in_table)) > 0) {

    for (i=0; i<(modulus_size - 1); i++) {
      from_table[i] = temp[i];
    }

    for (; i<SIZE; i++) {
      from_table[i] = 0;
    }

    for (i=(modulus_size - 1); i<SIZE; i++) {

      if (temp[i]) {

        for (j=0; j<modulus_size; j++) {
          from_table[j] += modulus_table[i][j] * temp[i];
        }

      }

    }

    renormalize(from_table);

    if (compare(from_table, modulus_in_table) != -1) {

      subtract(from_table, from_table, modulus_in_table, SIZE);
      renormalize(from_table);

    }

    copy(temp, from_table);

  }

  if (!c) { /* temp == mod_in_table */

    for (i=0; i<SIZE; i++) {
      out[i] = 0;
    }
  
  }
  else {
    copy(out, temp);
  }

  /* clean up */
  for (i=0; i<SIZE; i++) {
    temp[i] = from_table[i] = 0;
  }

}


/* (a ^ b) % modulus_in_table */
void expmod(int *out, const int *a, const int *b) {

  int temp[SIZE], temp_a[SIZE], temp_b[SIZE];
  int temp_2[SIZE << 1];
  int i, done;

  for (i=0; i<SIZE; i++) {
    temp[i] = temp_a[i] = 0;
  }

  temp[0] = 1;
  copy(temp_a, a);
  copy(temp_b, b);

  for (;;) {

    done = 1;

    for (i=0; (i<SIZE) && done; i++) {

      if (temp_b[i]) {
        done = 0;
      }

    }

    if (done) {

      copy(out, temp);

      /* clean up */
      for (i=0; i<SIZE; i++) {
        temp[i] = temp_a[i] = temp_2[i] = temp_2[i + SIZE] = 0;
      }

      return;

    }

    if (temp_b[0] & 0x1) { /* b is odd */


      multiply(temp_2, temp, temp_a, SIZE);
      renormalize(temp_2);
      modulus(temp, temp_2);

    }

    shift_right1(temp_b);
    square(temp_2, temp_a, SIZE);
    renormalize(temp_2);
    modulus(temp_a, temp_2);

  }

}

void cleanup_modulus(void) {

  int i, j;

  for (i=0; i<SIZE; i++) {

    modulus_in_table[i] = 0;

    for (j=0; j<SIZE; j++) {
      modulus_table[i][j] = 0;
    }
  }

  modulus_size = 0;

}

/****************************************************************************
 * these function convert between the unsigned char form (as used by the
 * netrek RSA scheme) and the internal form.  Pretty basic, since they're the
 * same thing, but the internal form has more room to make computation
 * easier.  num_to_raw will fail unless in is normalized.
 */

void raw_to_num(int *out, const unsigned char *in, const int digits) {

  int i;

  for (i=0; i<digits; i++) {
    out[i] = ((unsigned int) in[i]);
  }

  for (; i<SIZE; i++) {
    out[i] = 0;
  }

}

void num_to_raw(unsigned char *out, const int *in, const int digits) {

  int i;

  for (i=0; i<digits; i++) {
    out[i] = ((unsigned char) in[i]);
  }

}

