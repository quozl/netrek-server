/*
 * $Id: mkkey.c,v 1.1 2005/03/21 05:23:44 jerub Exp $
 *
 * This program creates the public, private and modulo keys for Netrek
 * RSA verification, writes out a keycap file, and generates an RSA
 * "black box" that contains obfuscated form of the code to do RSA
 * encryption with the private key.
 *
 * mkkey is used for two things:
 *
 * 1. Generating new RSA keys, creating the two keycap files key_name
 * and key_name.secret. key_name.secret is the keycap with the
 * additional field sk, which holds the secret (private) key.
 *
 * 2. Reading in a secret keycap file (or old-style keys.h file) and
 * generating a rsa "black box".  The "black box" is a set of C source
 * files named basename.c, basename_0.c, ... basename_N.c.  Together
 * these define a function called that looks like this:
 *
 * void rsa_black_box(unsigned char* out, unsigned char* in,
 *                    unsigned char* public, unsigned char* global)
 *
 * Each of these parameters is expected to be an array KEY_SIZE of
 * elements, except public and global which may be NULL.  The function
 * will perform an RSA encryption on in, writing the result to out.
 * If public (or global) is non-NULL the public (or global) key will
 * be copied to public (or global).
 *
 * The obfuscation process used to generate the "black box" relies on
 * the fact that an encryption with a known private key can be
 * expressed as a sequence of operations (which I call X & Y, these
 * correspond to the two different steps of the standard expmod
 * algorithm) on two variables, the message being encrypted and the
 * encryption result.  So instead of storing the private key, we
 * represent it by the sequence of X & Y's.
 *
 * To make the sequence harder to follow we generate arrays of
 * messages and results and store the "real" message/result in one of
 * the slots.  X & Y's are semi-randomly performed on the elements of
 * the array, and the elements are shuffled every once in a while.  I
 * think of this as playing a shell game with the message/result.
 *
 * As a final defense, the X & Y's & shufflings (which are called
 * swaps in the code) are split into N separate files.
 *
 * This is a merger of genkey.c (author: Ray Jones) and mkrsaclient.c
 * plus too many features.
 *
 * NOTE: the generated rsa "black box" files do not need any of the
 * rsa_util{mp}.c, or rsa_clientutil.c files.  They *do* need to be
 * linked with GNU MP, and not say SunOS's MP.
 *
 * If you make any non-trivial enhancements or find any bugs I'd like
 * to hear about them.  In any case, you are free to do whatever you
 * want with this code...
 *
 * Sam Shen (sls@aero.org) */

/* mkkey generates client RSA code that requires GMP.  So, it makes
 * sense to enforce mkkey to require GMP.  GMP 1.3.2 and 2.0.2 are
 * supported.  This code is also ANSIfied.  -da */

/* if you hack mkkey and release it change the following line in
 * some reasonable way, perhaps like this:
 * static char version[] = "[atm: July 4, 1993] based on [sls: June 7, 1993]";
 */

static char version[] = "[RES-RSA 2.9.2: Mar. 13, 2000][GMP]";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>

/* mkkey now only supports GMP only, no Berkeley MP */
#ifndef WIN32

#include <gmp.h>

#else /* WIN32 */

#include <gmp\gmp.h>
#include <windows.h>
#define getpid() GetCurrentProcessId()
#define getuid() (int) GetModuleHandle(NULL)
#define getgid() GetCurrentProcessId()

#endif /* WIN32 */

#include "config.h"


#if __GNU_MP_VERSION < 2

typedef MP_INT mpz_t[1];

#define mpz_fdiv_qr	mpz_mdivmod
#define mpz_fdiv_qr_ui	mpz_mdivmod_ui

#endif

#define SIZE 32
#define HALF 16

#define DEFAULT_N_SHELLS         3
#define DEFAULT_SWAP_STEPS       2
#define DEFAULT_RSA_BOX_FILE     "rsa_box"
#define DEFAULT_N_FILES          5
#define DEFAULT_FILE_RATIO       0.8

/* fadden's World Famous random stuff */
int rand_state = 0;		/* which state we're using */
char rand_state1[256];		/* big honking state for random() */
char rand_state2[256];		/* even more big honking state for random() */

static mpz_t zero;

#define check_positive(n)	assert(mpz_cmp(n, zero) >= 0)

#define TEST(a, b)		assert(!mpz_cmp(a, b))

/* get random value between 0 and 255 */
static int random256(void) {

  unsigned long tmp;

  rand_state = 1 - rand_state;		/* alternate between generators */

  if (rand_state)			/* select the appropriate state */
    setstate(rand_state1);
  else
    setstate(rand_state2);

  tmp = random();

  return (int) ((tmp >> 24) ^ ((tmp >> 16)  & 0xff) ^
               ((tmp >> 8) & 0xff) ^ (tmp & 0xff));
}

static void rand_raw(unsigned char *str, const int num) {

  int i;

  for (i=0; i<(num - 1); i++)
    str[i] = random256();

  /* force it to be num digits long */
  str[i] = 0;

  while (!str[i])
    str[i] = random256();

  i++;

  for (; i<SIZE; i++)
    str[i] = 0;
}

static void raw_to_num(mpz_t out, unsigned char *in) {

  int i;

  mpz_t temp;
  mpz_t twofiftysix;
  mpz_t thisval;

  mpz_init(temp);
  mpz_init(twofiftysix);
  mpz_init(thisval);

  mpz_set_ui(temp, 0);
  mpz_set_ui(twofiftysix, 256);

  for (i=0; i<SIZE; i++) {

    mpz_mul(temp, temp, twofiftysix);
    mpz_set_ui(thisval, in[SIZE - i - 1]);
    mpz_add(temp, temp, thisval);

  }

  mpz_set(out, temp);

  mpz_clear(temp);
  mpz_clear(twofiftysix);
  mpz_clear(thisval);

}
 
static void num_to_raw(unsigned char *out, mpz_t in) {

  /* note: mpz_t in overwritten */

  int i;
  unsigned long temp;

  for (i=0; i<SIZE; i++) {

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
}

static void mmod(mpz_t a, mpz_t b, mpz_t c) {

  mpz_t quotient;

  mpz_init(quotient);
  mpz_set_ui(quotient, 0);

  mpz_fdiv_qr(quotient, c, a, b);

  mpz_clear(quotient);

}

/*
 * GNU MP (and possibly other implementations of mp) does not have
 * invert so this was transliterated from the equivalent routine in
 * PGP 2.2
 *
 * rjones had an my_invert here but it didn't seem to work
 */

#define iplus1  ((i == 2)? 0: (i + 1))	/* used by Euclid algorithms */
#define iminus1 ((i == 0)? 2: (i - 1))	/* used by Euclid algorithms */

static void invert(mpz_t a, mpz_t n, mpz_t x) {

  mpz_t y;
  mpz_t temp;
  mpz_t temp2;
  mpz_t g[3];
  mpz_t v[3];

  int i;

  /* initialize mpz variables */
  mpz_init(y);
  mpz_init(temp);
  mpz_init(temp2);

  for (i=0; i<3; i++) {
    mpz_init(g[i]);
    mpz_init(v[i]);
  }

  /* set the start values */
  mpz_set_ui(y, 0);
  mpz_set_ui(temp, 0);
  mpz_set_ui(temp2, 0);

  mpz_set(g[0], n);
  mpz_set(g[1], a);

  mpz_set_ui(g[2], 0);

  mpz_set_ui(v[0], 0);
  mpz_set_ui(v[1], 1);
  mpz_set_ui(v[2], 0);

  i = 1;

  while (mpz_cmp(g[i], zero)) {

    check_positive(g[iminus1]);
    check_positive(g[i]);

    mpz_fdiv_qr(y, g[iplus1], g[iminus1], g[i]);
    mpz_mul(temp, y, v[i]);

    mpz_set(v[iplus1], v[iminus1]);
    mpz_sub(temp2, v[iplus1], temp);
    mpz_set(v[iplus1], temp2);

    i = iplus1;

  }

  mpz_set(x, v[iminus1]);

  while (mpz_cmp(x, zero) < 0) {

    mpz_add(temp2, x, n);
    mpz_set(x, temp2);

  }

  /* clear out mpz_t variables */

  mpz_clear(y);
  mpz_clear(temp);
  mpz_clear(temp2);

  for (i=0; i<3; i++) {
    mpz_clear(g[i]);
    mpz_clear(v[i]);
  }

}

/* verify_key checks to make sure a RSA key actually works
 */

static void verify_key(unsigned char *raw_public, unsigned char *raw_private,
                       unsigned char *raw_global) {

  mpz_t orig_message;
  mpz_t message;
  mpz_t crypt_text;
  mpz_t public;
  mpz_t private;
  mpz_t global;

  int failures = 0;
  int i;
  unsigned char temp[SIZE];

  mpz_init(orig_message);
  mpz_init(message);
  mpz_init(crypt_text);
  mpz_init(public);
  mpz_init(private);
  mpz_init(global);

  mpz_set_ui(orig_message, 0);
  mpz_set_ui(message, 0);
  mpz_set_ui(crypt_text, 0);
  mpz_set_ui(public, 0);
  mpz_set_ui(private, 0);
  mpz_set_ui(global, 0);

  printf("Testing key ");
  fflush(stdout);

  raw_to_num(public, raw_public);
  raw_to_num(private, raw_private);
  raw_to_num(global, raw_global);

  for (i=0; i<50; i++) {

    rand_raw(temp, SIZE - 1);
    raw_to_num(orig_message, temp);

    mpz_powm(crypt_text, orig_message, private, global);
    mpz_powm(message, crypt_text, public, global);

    if (mpz_cmp(message, orig_message) != 0) {

      putchar('!');
      failures++;

    }
    else {

      putchar('.');

    }

    fflush(stdout);

  }

  if (failures == 0) {
    printf(" key seems OK\n");
  }
  else {
    printf("\nSorry, this key is bogus.\n");
    exit(1);
  }

  mpz_clear(orig_message);
  mpz_clear(message);
  mpz_clear(crypt_text);
  mpz_clear(public);
  mpz_clear(private);
  mpz_clear(global);

}

/* kgetkeyname, kgetstr and kgetkey are stolen from Ted Hadley's
 * rsa_keycomp.c
 */

#define SEP                     ':'
#define GLOBAL_KEY_FIELD        "gk="
#define PUBLIC_KEY_FIELD        "pk="
#define SECRET_KEY_FIELD        "sk="
#define CLIENT_TYPE_FIELD       "ct="
#define CREATOR_FIELD           "cr="
#define CREATED_FIELD           "cd="
#define ARCH_TYPE_FIELD         "ar="
#define COMMENTS_FIELD          "cm="
#define KEY_SIZE                SIZE

/*
 * Extract the key name descriptor from an entry buffer
 * "Key Name:"
 */

int kgetkeyname(char *src, char *dest) {

  int l;

  l = strchr(src, SEP) - src;
  strncpy(dest, src, l);
  dest[l] = '\0';

  return 1;

}

/*
 * Place contents of specified field entry into destination string.
 * "pk=Text Text Text:"
 */

int kgetstr(char *src, char *field, char *dest) {

  char *s;
  int l;

  s = strstr(src, field);

  if (s == NULL)
    return 0;

  s += strlen(field);
  l = strchr(s, SEP) - s;
  strncpy(dest, s, l);
  dest[l] = 0;

  return 1;

}

/*
 * Place contents of specified binary field entry into destination string.
 * "pk=Text Text Text:"
 */

int kgetkey(char *src, char *field, unsigned char *dest) {

  char uuencoded_dest[KEY_SIZE * 2 + 1];
  char uc[3];

  char *s;
  unsigned int c;


  if(!kgetstr(src, field, uuencoded_dest))
    return 0;

  uc[2] = '\0';

  /* convert encoded binary to binary */
  s = uuencoded_dest;

  while(*s){
    uc[0] = *s++;
    uc[1] = *s++;
    sscanf(uc, "%x", &c);
    *dest++ = (unsigned char) c;
  }

  return 1;

}

int scan_array_element(char *buffer, int *xp, int *np) {

  char *bp = buffer;

  while (*bp && isspace(*bp))
    bp++;

  if (*bp == '0' && *(bp+1) == 'x') {
    if (sscanf(bp+2, "%x", xp) != 1)
      return 0;

    bp += 2;
  }
  else if (sscanf(bp, "%d", xp) != 1)
    return 0;

  while (*bp && isxdigit(*bp))
    bp++;

  while (*bp && *bp != ',')
    bp++;

  bp++;
  *np = bp - buffer;

  return 1;

}

/* look through the text in buffer for name, and read in len comma
 * separated ints into data
 */
void get_array(char *buffer, char *name, unsigned char *data, int len) {


  int i, n, x;
  char* start;

  start = strstr(buffer, name);

  if (start == NULL)
    goto err;

  while (*start && *start != '{')
    start++;

  if (!*start)
    goto err;

  start++;

  for (i=0; i<len; i++) {

    if (scan_array_element(start, &x, &n) != 1)
      goto err;

    data[i] = x;
    start += n;

  }

  return;

 err:
  fprintf(stderr, "Ooops! Couldn't get array %s in keys.h file.\n", name);
  exit(1);

}

static const char *header =
"\n"
"/*\n"
" * DO NOT EDIT THIS FILE!!! GENERATED AUTOMATICALLY!!!\n"
" *\n"
" * mkkey version %s\n"
" */\n"
"\n"
"#include <stdio.h>\n"
"#include <gmp.h>\n"
"\n"
"/* rsa_partial_box prototypes */\n"
"void rsa_partial_box_0(MP_INT *, MP_INT *, MP_INT *);\n"
"void rsa_partial_box_1(MP_INT *, MP_INT *, MP_INT *);\n"
"void rsa_partial_box_2(MP_INT *, MP_INT *, MP_INT *);\n"
"void rsa_partial_box_3(MP_INT *, MP_INT *, MP_INT *);\n"
"void rsa_partial_box_4(MP_INT *, MP_INT *, MP_INT *);\n"
"\n"
"\n"
"void rsa_black_box(unsigned char *out, unsigned char *in,\n"
"                   unsigned char *public, unsigned char *global) {\n"
"\n"
"  MP_INT m_msg;\n"
"  MP_INT m_result;\n"
"  MP_INT m_public;\n"
"  MP_INT m_global;\n"
"  MP_INT m_tmp;\n"
"  int i;\n"
"  mpz_init(&m_msg);\n"
"  mpz_init(&m_result);\n"
"  mpz_init(&m_public);\n"
"  mpz_init(&m_global);\n"
"  mpz_init(&m_tmp);\n"
"  for (i = 0; i < %d; i++) {\n"
"    mpz_mul_2exp(&m_msg, &m_msg, 8);\n"
"    mpz_add_ui(&m_msg, &m_msg, in[%d-1-i]);\n"
"  }\n";

static const char *trailer =
"  for (i = 0; i < %d; i++) {\n"
"    mpz_divmod_ui(&m_result, &m_tmp, &m_result, 256);\n"
"    *out++ = (unsigned char) mpz_get_ui(&m_tmp);\n"
"  }\n"
"  mpz_clear(&m_msg);\n"
"  mpz_clear(&m_result);\n"
"  mpz_clear(&m_public);\n"
"  mpz_clear(&m_global);\n"
"  mpz_clear(&m_tmp);\n"
"}\n";

static void gen_key(FILE *fp, char *name, unsigned char *key, int len) {

  int i;
  unsigned long limb;

  for (i=(len - 4); i >= 0; i-=4) {

    limb = (key[i+3] << 24) + (key[i+2] << 16) + (key[i+1] << 8) + key[i];
    fprintf(fp, "  mpz_mul_2exp(&m_%s, &m_%s, 32);\n", name, name);
    fprintf(fp, "  mpz_add_ui(&m_%s, &m_%s, (unsigned long) 0x%x);\n",
            name, name, limb);

  }

  fprintf(fp, "  if (%s != NULL) {\n", name);

  for (i=0; i<len; i++)
    fprintf(fp, "    %s[%d] = %d;\n", name, i, key[i]);

  fprintf(fp, "  }\n");
}

static const char *rsa_box_defs =
"#define X(m, r, g) \\\n"
"    mpz_mul(r, m, r);\\\n"
"    mpz_mod(r, r, g)\n"
"\n"
"#define Y(m, r, g) \\\n"
"    mpz_mul(m, m, m);\\\n"
"    mpz_mod(m, m, g)\n"
"\n"
"#define SWAP(i, j) \\\n"
"    { \\\n"
"      MP_INT tmp; \\\n"
"      tmp = m[i]; m[i] = m[j]; m[j] = tmp; \\\n"
"      tmp = r[i]; r[i] = r[j]; r[j] = tmp; \\\n"
"    }\n"
"\n";

static const char *sequence_header =
"  {\n"
"    MP_INT r[%d], m[%d];\n"
"    for (i = 0; i < %d; i++) {\n"
"      mpz_init_set_ui(&r[i], 1); mpz_init(&m[i]);\n"
"    }\n"
"    mpz_set(&m[%d], &m_msg);\n"
"#define g &m_global\n";

static const char *sequence_trailer = ""
"\n"
"    mpz_set(&m_result, &r[%d]);\n"
"    for (i = 0; i < %d; i++) {\n"
"      mpz_clear(&r[i]); mpz_clear(&m[i]);\n"
"    }\n"
"  }\n";

static const char *per_box_header =
"#include <gmp.h>\n"
"\n"
"void rsa_partial_box_%d(MP_INT *m, MP_INT *r, MP_INT *g) {\n"
"\n";

static const char *per_box_trailer =
"\n}\n";

/*
 * Write out an obfuscated rsa computation.  This code is a little
 * ugly, forgive me...
 */

static void gen_rsa_sequence(FILE *clientsfp, char *clients_basename,
                             int n_files, unsigned char *raw_private,
                             int n_shells, int swap_steps,
                             double file_ratio) {

  FILE *fp;
  int i, j, real_j, tmp, done, bit, n_bits, file_no, in_fp;
  int n_bits_in_section, bit_in_section;
  char buf[200];
  char* op;
  double ratio;

  /* first, get a rough idea how many bits there are in raw_private */
  for (i=(SIZE - 1); i>=0; --i) {
    if (raw_private[i] != 0)
      break;
  }

  n_bits = i * 8;

  real_j = random256() % n_shells;

  fprintf(clientsfp, sequence_header, n_shells, n_shells, n_shells, real_j);
  fprintf(clientsfp, rsa_box_defs);

  bit = 0;
  file_no = 0;
  fp = NULL;
  bit_in_section = 0;
  n_bits_in_section = -1;
  in_fp = 0;

  for (;;) {

    /* decide if we want to continue writing to wherever or to make a
     * change */

    done = 0;

  finish:
    if (done || (bit_in_section > n_bits_in_section)) {

      if (in_fp) {

        fprintf(fp, per_box_trailer);
        fclose(fp);

        fprintf(clientsfp, "    rsa_partial_box_%d(m, r, &m_global);\n",
                file_no);
        file_no++;
        fp = clientsfp;
        in_fp = 0;

      }
      else if (file_no < n_files) {

        sprintf(buf, "%s_%d.c", clients_basename, file_no);

        fp = fopen(buf, "w");

        if (fp == NULL) {
          fprintf(stderr, "Couldn't open rsa box file \"%s\"!\n", buf);
          exit(1);
        }

        fprintf(fp, per_box_header, file_no);
        fprintf(fp, rsa_box_defs);
        in_fp = 1;

      }

      if (file_no == n_files)
        n_bits_in_section = n_bits - bit;

      else {

        int bits_remaining;
        if (in_fp)
          ratio = file_ratio;

        else
          ratio = 1.0 - file_ratio;

        bits_remaining = n_bits - bit;
        n_bits_in_section = (int) (ratio *
                                   (bits_remaining / (n_files - file_no)));

        /* have n_bits_in_section vary by 50% */
        n_bits_in_section = (5 * n_bits_in_section / 4 -
                             ((random256() % n_bits_in_section) / 2));

        printf("%d bits left, %d files left, %d bits in %s\n",
               bits_remaining, n_files - file_no, n_bits_in_section,
               in_fp ? buf : "main file");

      }

      bit_in_section = 0;

    }

    if (done)
      break;

    /* check if we're done */
    done = 1;

    for (i=0; i<SIZE; i++)
      if (raw_private[i] != 0) {
        done = 0;
        break;
      }

    if (done)
      goto finish;

    fprintf(fp, "    /* real_j is %d, bit %d is %d */\n", real_j, bit,
            raw_private[0] & 0x1);

    for (j=0; j<n_shells; j++) {

      if (j == real_j) {

        if (raw_private[0] & 0x1)
          op = "X";

        else
          op = 0;

      }
      else if ((random256() % 2) == 0) {
        op = "X";
      }
      else {
        op = "Y";
      }

      if (op != 0)
        fprintf(fp, "    %s(&m[%d], &r[%d], g);\n", op, j, j);

    }

    for (j=0; j<n_shells; j++) {

      if (j == real_j || ((random256() % 2) == 0)) {
        op = "Y";
      }
      else {
        op = "X";
      }

      fprintf(fp, "    %s(&m[%d], &r[%d], g);\n", op, j, j);

    }

    if ((random256() % (swap_steps + 1)) == 0) {

      for (j=0; j<n_shells; j++) {

        tmp = random256() % (j + 1);

        if (real_j == tmp) {
          real_j = j;
        }
        else if (real_j == j) {
          real_j = tmp;
        }

        fprintf(fp, "    SWAP(%d, %d);\n", j, tmp);

      }
    }

    for (i=0; i<(SIZE - 1); i++) {

      raw_private[i] >>= 1;

      if (raw_private[i+1] & 0x1)
        raw_private[i] |= 0x80;

    }

    raw_private[SIZE-1] >>= 1;
    bit++;
    bit_in_section++;

  }

  fprintf(fp, sequence_trailer, real_j, n_shells);

}

static char *allocbuf(void) {

  char *buf = (char *) malloc(BUFSIZ);
  assert(buf != NULL);
  return buf;

}

static const char* usage_lines[] = {
    "",
    "-v",
    "\tVerify the key only.",
    "",
    "-h keys.h-file [ key_name client_type architecture/OS creator comments ]",
    "\tRead key from old-style keys.h file using key_name ... to complete",
    "\tthe key (keys.h files don't have this information.)  If -v was used",
    "\tthen key_name etc are optional.",
    "",
    "-k keycap.secret-file",
    "\tRead key from keycap file with sk (secret key) field.",
    "",
    "-n n-shells",
    "\tSet the number of shells in the shell-game obfuscation.",
    "",
    "-s n-steps-to-swap",
    "\tSet the average number of steps between swaps.",
    "",
    "-nt",
    "\tSkip testing the key.",
    "",
    "-f n-files",
    "\tWrite out rsa computation into n-files separate files.",
    "",
    "-b client-sourcefile-basename",
    "\tUse client-sourcefile-basename as base name for rsa computation files.",
    "",
    "key_name client_type architecture/OS creator comments",
    "\tCreate a new key.  You may have to quote some of the args to make",
    "\tyour shell do the right thing.",
    "",
    NULL
};

static void usage(void) {

  int i;

  for (i=0; usage_lines[i]; i++) {
    printf("%s\n", usage_lines[i]);
  }

  exit(1);

}

int main(int argc, char *argv[]) {

  mpz_t x, y, global, xyminus1, private, public, one, two;

  unsigned char temp[SIZE];
  unsigned char raw_global[SIZE], raw_private[SIZE], raw_public[SIZE];

  FILE *clientsfp;		/* clientsfp is the client source file */
  FILE *keycapfp;		/* keycapfp is the keycap file */
  FILE *skeycapfp;		/* skeycapfp is the keycap file with
				 * the secret key included
				 */

  int i, n, verify_only, n_shells, swap_steps, test_key, n_files;
  double file_ratio;
  char buf[200];
  time_t today;
  char *key_name, *client_type, *architecture, *creator, *comments;
  char *created;
  char *keydoth, *keycap;
  char *clients_basename;

  /*
   * Initialize the random number generators.  Both get 256-byte pieces
   * of state (default is 128).  The second is partially seeded with
   * output from first.
   *
   * Total interesting bits in the seeds:
   *   time: 24 bits (assuming you can guess when it was generated)
   *   pid: 15 bits (most systems use a signed short)
   *   uid: 10 bits (most users will fall in this range)
   *   gid: ~3 bits (these are easy to guess)
   *
   * That's about 52 bits, perturbed in strange and unusual ways.  If this
   * ain't random, I'm lost.
   */

  /*srandom(time(0) ^ (getpid()<<16) ^ getuid());*/

  initstate(time(0) ^ (getpid()<<16) ^ getgid(), rand_state1, 256);
  initstate((getuid() ^ random()) | (time(0) % 65239) << 16, rand_state2, 256);

  rand_state = 0;

  printf("mkkey version %s\n", version);

  created = NULL;
  keydoth = NULL;
  keycap = NULL;
  verify_only = 0;
  n_shells = DEFAULT_N_SHELLS;
  swap_steps = DEFAULT_SWAP_STEPS;
  n_files = DEFAULT_N_FILES;
  clients_basename = DEFAULT_RSA_BOX_FILE;
  file_ratio = DEFAULT_FILE_RATIO;

  mpz_init(zero);
  mpz_set_ui(zero, 0);

  test_key = 1;
  n = 1;

  while (argc > n) {

    if (!strcmp(argv[n], "-h")) {
      keydoth = argv[n+1];
      n += 2;
    }
    else if (!strcmp(argv[n], "-k")) {
      keycap = argv[n+1];
      n += 2;
    }
    else if (!strcmp(argv[n], "-v")) {
      verify_only = 1;
      n++;
    }
    else if (!strcmp(argv[n], "-n")) {
      n_shells = atoi(argv[n+1]);
      n += 2;
    }
    else if (!strcmp(argv[n], "-s")) {
      swap_steps = atoi(argv[n+1]);
      n += 2;
    }
    else if (!strcmp(argv[n], "-nt")) {
      test_key = 0;
      n++;
    }
    else if (!strcmp(argv[n], "-b")) {
      clients_basename = argv[n+1];
      n += 2;
    }
    else if (!strcmp(argv[n], "-f")) {
      n_files = atoi(argv[n+1]);
      n += 2;
    }
    else if (!strcmp(argv[n], "-c")) {
      n_files = random() % 10+1;
      n++;
    }
    else if (!strcmp(argv[n], "-r")) {
      sscanf(argv[n+1], "%lf", &file_ratio);
      n += 2;
    }
    else
      break;

  }

  if (keycap != NULL && keydoth != NULL) {
    fprintf(stderr, "%s: can't use both a keys.h file and keycap file\n",
            argv[0]);
    usage();
  }

  if (keycap == NULL && verify_only == 0 && (argc - n) != 5) {
    usage();
  }

  if (n_shells < 1) {
    fprintf(stderr, "%s: number of shells must >= 1\n", argv[0]);
    usage();
  }

  if (swap_steps < 0) {
    fprintf(stderr, "%s: average number of steps between stops must be >= 0\n",
            argv[0]);
    usage();
  }

  if (n_files < 1) {
    fprintf(stderr, "%s: number of files must be >= 1\n", argv[0]);
    usage();
  }

  if (keycap == NULL) {
    key_name = argv[n];
    client_type = argv[n+1];
    architecture = argv[n+2];
    creator = argv[n+3];
    comments = argv[n+4];
  }

  printf("Source basename: \"%s\"\n", clients_basename);
  printf("Number of shells: %d\n", n_shells);
  printf("Number of steps between swaps: %d\n", swap_steps);
  printf("Number of files: %d\n", n_files);
  printf("Ratio of computation in files to main file: %lg\n", file_ratio);

  if (keydoth == NULL && keycap == NULL) {
    printf("Making new key, hold on....\n");

#   define is_prime(n) mpz_probab_prime_p(n, 50)

    mpz_init(one);
    mpz_init(two);
    mpz_init(x);
    mpz_init(y);
    mpz_init(global);
    mpz_init(private);
    mpz_init(public);
    mpz_init(xyminus1);

    mpz_set_ui(one, 1);
    mpz_set_ui(two, 2);
    mpz_set_ui(x, 0);
    mpz_set_ui(y, 0);
    mpz_set_ui(global, 0);
    mpz_set_ui(private, 0);
    mpz_set_ui(public, 0);
    mpz_set_ui(xyminus1, 0);

    /* here we find x and y, two large primes */

    rand_raw(temp, HALF);
    temp[0] |= 1; /* force odd */
    raw_to_num(x, temp);

    while (!is_prime(x))
      mpz_add(x, x, two);

    check_positive(x);
    assert(is_prime(x));

    rand_raw(temp, HALF);
    temp[0] |= 1; /* force odd */
    raw_to_num(y, temp);

    while (!is_prime(y))
      mpz_add(y, y, two);

    check_positive(y);
    assert(is_prime(y));
	
    /* the private key is a large prime (it should be the larger than
     * x & y) */

    rand_raw(temp, HALF + 1);
    temp[0] |= 1; /* force odd */
    raw_to_num(private, temp);

    while (!is_prime(private))
      mpz_add(private, private, two);

    check_positive(private);
    assert(is_prime(private));

    if (mpz_cmp(x, private) > 0) {

      mpz_t tmp;

      mpz_init(tmp);

      mpz_set(tmp, x);
      mpz_set(x, private);
      mpz_set(private, tmp);

      mpz_clear(tmp);

    }

    if (mpz_cmp(y, private) > 0) {

      mpz_t tmp;

      mpz_init(tmp);

      mpz_set(tmp, y);
      mpz_set(y, private);
      mpz_set(private, tmp);

      mpz_clear(tmp);

    }

    assert(mpz_cmp(private, x) > 0);
    assert(mpz_cmp(private, y) > 0);

    /* the modulus global is x * y */

    mpz_mul(global, x, y);
    check_positive(global);

    /* the public key is such that (public * private) mod ((x - 1) *
     * (y - 1)) == 1 */


    mpz_sub(x, x, one);
    mpz_sub(y, y, one);

    mpz_mul(xyminus1, x, y);
    assert(!is_prime(xyminus1));
    invert(private, xyminus1, public);
    check_positive(public);

    /* check to make sure the invert worked */

    {

      mpz_t ps, m, one;

      mpz_init(ps);
      mpz_init(m);
      mpz_init(one);

      mpz_set_ui(ps, 0);
      mpz_set_ui(m, 0);
      mpz_set_ui(one, 1);

      mpz_mul(ps, private, public);

      mmod(ps, xyminus1, m);

      if (mpz_cmp(m, one) != 0) {
        printf("Ooops! invert failed!\n");
        exit(1);
      }

      mpz_clear(ps);
      mpz_clear(m);
      mpz_clear(one);

    }

    /* convert to raw format */


    num_to_raw(raw_global, global);
    num_to_raw(raw_public, public);
    num_to_raw(raw_private, private);

    /* cleanup */
    mpz_clear(one);
    mpz_clear(two);
    mpz_clear(x);
    mpz_clear(y);
    mpz_clear(global);
    mpz_clear(private);
    mpz_clear(public);
    mpz_clear(xyminus1);

  }
  else if (keydoth != NULL) {
    FILE* fp = fopen(keydoth, "r");
    char* buffer;
    struct stat statbuf;

    printf("Reading old key from keys.h file \"%s\"...\n", keydoth);

    if (fp == NULL) {
      printf("%s: can't open keys.h file \"%s\"\n", argv[0], keydoth);
      perror("");
      exit(1);
    }

    if (fstat(fileno(fp), &statbuf) < 0) {
      perror("fstat");
      exit(1);
    }

    buffer = (char*) malloc(statbuf.st_size);
    assert(buffer != NULL);
    fread(buffer, 1, statbuf.st_size, fp);
    fclose(fp);
    get_array(buffer, "key_global", raw_global, SIZE);
    get_array(buffer, "key_public", raw_public, SIZE);
    get_array(buffer, "key_private", raw_private, SIZE);
    free(buffer);

  }
  else {
    char* buffer;
    struct stat statbuf;
    FILE* fp = fopen(keycap, "r");

    printf("Reading key from keycap file \"%s\"...\n", keycap);

    if (fp == NULL) {
      printf("%s: can't open keycap file \"%s\"\n", argv[0], keycap);
      perror("");
      exit(1);
    }

    if (fstat(fileno(fp), &statbuf) < 0) {
      perror("fstat");
      exit(1);
    }

    buffer = (char*) malloc(statbuf.st_size);
    assert(buffer != NULL);
    fread(buffer, 1, statbuf.st_size, fp);
    fclose(fp);
    key_name = allocbuf();
    client_type = allocbuf();
    architecture = allocbuf();
    creator = allocbuf();
    created = allocbuf();
    comments = allocbuf();
    if (!kgetkeyname(buffer, key_name) ||
        !kgetstr(buffer, CLIENT_TYPE_FIELD, client_type) ||
        !kgetstr(buffer, CREATOR_FIELD, creator) ||
        !kgetstr(buffer, CREATED_FIELD, created) ||
        !kgetstr(buffer, ARCH_TYPE_FIELD, architecture) ||
        !kgetstr(buffer, COMMENTS_FIELD, comments) ||
        !kgetkey(buffer, GLOBAL_KEY_FIELD, raw_global) ||
        !kgetkey(buffer, PUBLIC_KEY_FIELD, raw_public) ||
        !kgetkey(buffer, SECRET_KEY_FIELD, raw_private)) {

      fprintf(stderr, "%s: can't parse keycap file \"%s\"\n", argv[0], keycap);
      exit(1);
    }

    free(buffer);

  }

  if (test_key)
    verify_key(raw_public, raw_private, raw_global);

  if (verify_only)
    exit(0);

  if (keycap == NULL) {

    keycapfp = fopen(key_name, "w");
    assert(keycapfp != NULL);
    strcpy(buf, key_name);
    strcat(buf, ".secret");
    skeycapfp = fopen(buf, "w");
    assert(skeycapfp != NULL);
  }
  else {
    keycapfp = NULL;
  }

  sprintf(buf, "%s.c", clients_basename);
  printf("Writing...\n");
  clientsfp = fopen(buf, "w");
  assert(clientsfp != NULL);

  /* first write out some generic info */


  fprintf(clientsfp, "char key_name[] = \"%s\";\n", key_name);
  fprintf(clientsfp, "char client_type[] = \"%s\";\n", client_type);
  fprintf(clientsfp, "char client_arch[] = \"%s\";\n", architecture);
  fprintf(clientsfp, "char client_creator[] = \"%s\";\n", creator);
  fprintf(clientsfp, "char client_comments[] = \"%s\";\n", comments);

  if (created == NULL) {

    time(&today);
    strftime(buf, sizeof(buf), "%B %Y", gmtime(&today));
    created = buf;

  }

  fprintf(clientsfp, "char client_key_date[] = \"%s\";\n", created);

  if (keycapfp != NULL) {

    fprintf(keycapfp, "%s:ct=%s:cr=%s:\\\n   :cd=%s:ar=%s:\\\n   :cm=%s:\\\n",
            key_name, client_type, creator, buf, architecture, comments);

    fprintf(skeycapfp, "%s:ct=%s:cr=%s:\\\n   :cd=%s:ar=%s:\\\n   :cm=%s:\\\n",
            key_name, client_type, creator, buf, architecture, comments);

  }

  /* write the client source header */


  fprintf(clientsfp, header, version, KEY_SIZE, KEY_SIZE);

  /* write the global modulos */


  if (keycapfp != NULL) {

    fprintf(keycapfp, "   :gk=");
    fprintf(skeycapfp, "   :gk=");

    for (i=0; i<SIZE; i++) {
      fprintf(keycapfp, "%02x", (int) raw_global[i]);
      fprintf(skeycapfp, "%02x", (int) raw_global[i]);
    }

    fprintf(keycapfp, ":\\\n");
    fprintf(skeycapfp, ":\\\n");

  }

  gen_key(clientsfp, "global", raw_global, SIZE);

  /* write the public key */

  if (keycapfp != NULL) {

    fprintf(keycapfp, "   :pk=");
    fprintf(skeycapfp, "   :pk=");

    for (i=0; i<SIZE; i++) {
      fprintf(keycapfp, "%02x", (int) raw_public[i]);
      fprintf(skeycapfp, "%02x", (int) raw_public[i]);
    }

    fprintf(keycapfp, ":\n");
    fprintf(skeycapfp, ":\\\n");

  }

  gen_key(clientsfp, "public", raw_public, SIZE);

  /* write the private key */

  if (keycapfp != NULL) {

    fprintf(skeycapfp, "   :sk=");
    for (i=0; i<SIZE; i++)
      fprintf(skeycapfp, "%02x", (int) raw_private[i]);

    fprintf(skeycapfp, ":\n");

  } 

  /* write the sequence of computations that compute (out ** private)
   * mod global */

  gen_rsa_sequence(clientsfp, clients_basename, n_files,
                   raw_private, n_shells, swap_steps, file_ratio);

  /* write the source trailer */

  fprintf(clientsfp, trailer, KEY_SIZE);

  putc('\n', stdout);

  /* cleanup */
  fclose(clientsfp);

  if (keycapfp != NULL) {
    fclose(keycapfp);
    fclose(skeycapfp);
  }

  return 0;

}
