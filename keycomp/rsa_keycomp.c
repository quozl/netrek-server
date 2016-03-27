/*
 * keycomp.c
 * Compile a "termcap"-like description of RSA keys into the .reserved 
 * RSA binary file.
 */

#include "config.h"
#include <stdio.h>

#ifdef RSA
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h> /* ultrix insists on types.h before stst.h */
#include <time.h>
#include <sys/file.h>
#include <string.h>
#include <fcntl.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

/* ONLY IF YOU HAVE  char key_name[2*KEYSIZE] in struct rsa_key. */
/* #define RSA_KEYNAME */
/* comment out as needed */

#define RSA_KEY_EXCLUDE_FILE	".exclude_rsa_keys"
#define RSA_MOTDLIST		"rsa_motdlist"
#define RSA_CLASS_FILE		".rsa_class"
#define RSA_EXCLUDE_CLASS	".rsa_class_exclude"

#define SEP			':'

#define CLIENT_TYPE_FIELD	"ct="
#define CREATOR_FIELD		"cr="
#define CREATED_FIELD           "cd="
#define ARCH_TYPE_FIELD		"ar="
#define CLASS_FIELD		"cl="
#define COMMENTS_FIEILD		"cm="
#define GLOBAL_KEY_FIELD	"gk="
#define PUBLIC_KEY_FIELD	"pk="

struct  motd_keylist { 
   char *client;
   int	num_arch;
   char **arch;
};

static struct rsa_key key_list[1000];
int key_count=0;

#if defined(__STDC__) || defined(__cplusplus)
#define P_(s) s
#else
#define P_(s) ()
#endif

/* rsa_keycomp.c */
int main P_((int argc, char **argv));
void backup P_((char *f));
struct rsa_key *comp_key P_((FILE *fi, int *err));
int read_key P_((FILE *fi, char *bp));
int kgetkeyname P_((char *src, char *dest));
int kgetstr P_((char *src, char *field, char *dest));
int kgetkey P_((char *src, char *field, unsigned char *dest));
char **read_exclude_file P_((char *n));
char *read_class_file P_((char *n));
int excluded P_((char *keyname));
void sort_motdlist P_((int nk));
int class_ok P_((char *cl1, char *cl2));
void store_keydesc P_((struct motd_keylist *keys, char *client, char *arch));
void usage P_((char *p));
void loadkeys P_((char *reservedf));
int exist P_((struct rsa_key *k));

#ifdef _NEED_SYS_PROTOS
extern int                    fprintf P_((FILE *, const char *, ...));
extern void                   perror P_((const char *));
extern size_t                 fwrite P_((const void *, size_t, size_t, FILE *));
extern size_t                 fread P_((void *, size_t, size_t, FILE *));
extern int                    fclose P_((FILE *));
extern void *                 malloc P_((size_t));
extern int                    link P_((const char *, const char *));
extern int                    unlink P_((const char *));
extern void                   free P_((void *));
extern char *                 strstr P_((const char *, const char *));
extern char *                 strchr P_((const char *, int));
extern int                    sscanf P_((const char *, const char *, ...));
extern int                    printf P_((const char *, ...));
extern int                    _filbuf P_((FILE *));
extern int                    isspace P_((int));
extern void                   memset P_((void *, size_t));

#endif	/* _NEED_SYS_PROTOS */

#undef P_

int	verbose=0;
int	check=0;
int 	line=1;
char	*keyf = NULL;
char	*reservedf = RSA_Key_File;
char	*excludef = RSA_KEY_EXCLUDE_FILE;
char	*motdlist = RSA_MOTDLIST;
char	**exclude_list = NULL;
char	*classf = RSA_CLASS_FILE;
char	*classlist = NULL;
char	*class_exclude_list = NULL;
char	*classexclude = RSA_EXCLUDE_CLASS;

int
main(argc, argv)
   
   int	argc;
   char	**argv;
{
   FILE				*fi, *fo, *motd_fo;
   register struct rsa_key	*k;
   register int			i, c;
   int				err;

   i=1;
   getpath();
   while(i < argc){
      if(strcmp(argv[i], "-e")==0){
	 if(++i < argc)
	    excludef = argv[i];
	 else
	    usage(argv[0]);
      }
      else if(strcmp(argv[i], "-o")==0){
	 if(++i < argc)
	    reservedf = argv[i];
	 else
	    usage(argv[0]);
      }
      else if(strcmp(argv[i], "-m")==0){
	 if(++i < argc)
	    motdlist = argv[i];
	 else
	    usage(argv[0]);
      }
      else if(strcmp(argv[i], "-c")==0) {
	 check=1;
      }
      else if(strcmp(argv[i], "-v")==0) {
	 verbose=1;
      }
      else if(strcmp(argv[i], "-t")==0){
	 if(++i < argc)
	    classlist = argv[i];
	 else
	    usage(argv[0]);
      }
      else if(strcmp(argv[i], "-x")==0){
         if(++i < argc)
            class_exclude_list = argv[i];
         else
            usage(argv[0]);
      }
      else if(strcmp(argv[i], "-l")==0){
	 if(++i < argc)
	    classf = argv[i];
	 else
	    usage(argv[0]);
      }
      else
	 keyf = argv[i];
      i++;
   }
   if(!keyf) usage(argv[0]);

   exclude_list = read_exclude_file(excludef);

   if(!classlist)
      classlist = read_class_file(classf);

   if (!class_exclude_list)
      class_exclude_list = read_class_file(classexclude);

   if(verbose){
      if(!classlist)
	 fprintf(stderr, 
	    "No client classes given.  All clients will be accepted.\n");
      else
	 fprintf(stderr,
	    "Only clients in class(es) %s will be accepted.\n", classlist);
      if (class_exclude_list)
	 fprintf(stderr,
	    "Clients with class(es) %s will be excluded.\n",class_exclude_list);
   }
   
   if(!(fi=fopen(keyf, "r"))){
      perror(keyf);
      exit(1);
   }

   if (check)
	loadkeys(reservedf);

   backup(reservedf);

   if(!(fo=fopen(reservedf, "w"))){
      perror(reservedf);
      exit(1);
   }
   if(!(motd_fo=fopen(motdlist, "w"))){
      perror(motdlist);
      exit(1);
   }

   err = 0;
   c=0;

   while(!err){
      k=comp_key(fi, &err);
      if(k){
	 if ((check) && (!exist(k))) 
	    fprintf(stderr,"New key: %s  %s\n",k->client_type,k->architecture);
	 c++;
	 if(fwrite((char *)k, sizeof(struct rsa_key), 1, fo) != 1){
	    perror("fwrite");
	    exit(1);
	 }
	 fprintf(motd_fo, "\"%s\" \"%s\"\n", k->client_type,
					     k->architecture);
	 free((char *)k);
      }
   }
   
   fclose(fi);
   fclose(fo);
   fclose(motd_fo);

   sort_motdlist(c);

   fprintf(stderr,"%s: compiled %d keys to \"%s\".\n", argv[0], c, reservedf);
   return 1;		/* satisfy lint */
}

void
backup(f)

   char	*f;
{
   char		*backup_f;
   struct stat	sbuf;
   
   if(stat(f, &sbuf) < 0)
      return;
   backup_f = (char *)malloc(strlen(f)+2);
   if(!backup_f) { perror("malloc"); exit(1); }
   sprintf(backup_f, "%s~", f);
   (void)link(f, backup_f);
   (void)unlink(f);
   free((char *)backup_f);
}

/*
 * Read in old .reserved so we can compare the new key_file to see which
 * keys are new 
 */

void
loadkeys(file)
char *file;
{
    int fd;
    int done;

    fd = open(file, O_RDONLY ,0600);
    if (fd < 0)
    {
        fprintf(stderr, "Can't open the RSA key file %s\n", file);
        exit(1);
    }

    done = 0;
    do
    {
        if (read(fd, key_list[key_count].client_type, KEY_SIZE) != KEY_SIZE)
                done = 1;
        if (read(fd, key_list[key_count].architecture, KEY_SIZE) != KEY_SIZE)
                done = 1;
        if (read(fd, key_list[key_count].global, KEY_SIZE) != KEY_SIZE) 
                done = 1;
        if (read(fd, key_list[key_count].public, KEY_SIZE) != KEY_SIZE)
                done = 1;
        key_count++;
    } while (! done);

    key_count--;

    close(fd);
}

int exist(k)
    struct rsa_key      *k;	
{
    int count;

    for (count =0; count<key_count; count++) {
          if ((!memcmp(k->global,key_list[count].global,KEY_SIZE)) &&
             (!memcmp(k->public,key_list[count].public,KEY_SIZE))) {
		return 1;
          }
    }
    return 0;
}


/*
 * Read an ascii key representation from the current position in the
 * input file and create and return a struct rsa_key.
 */

struct rsa_key *
comp_key(fi, err)

   FILE	*fi;
   int	*err;
{
   char			ibuf[BUFSIZ], obuf[BUFSIZ];
   struct rsa_key	*key;
   char			keyname[2*KEY_SIZE];

   *err = 0;

   if(read_key(fi, ibuf) <= 0){
      *err = 1;	/* not necessarily an error but we want the program to quit */
      return NULL;
   }

   key = (struct rsa_key *)malloc(sizeof(struct rsa_key));
   if(!key) { perror("malloc"); exit(1); }

   if(!kgetkeyname(ibuf, keyname)){
      fprintf(stderr, "%s: No key name for entry around line %d\n",
	 keyf, line);
      *err = 1;
      return NULL;
   }
   if(excluded(keyname)){
      printf("key \"%s\" excluded (found in \"%s\").\n", keyname, excludef);
      return NULL;
   }
   /*
   printf("read key \"%s\"\n", keyname);
   */
#ifdef RSA_KEYNAME
   strncpy(key->key_name, keyname, 2*KEY_SIZE-1);
   key->key_name[2*KEY_SIZE-1] = 0;
#endif

   /* could use tgetstr here but then we'd have to link with libtermlib */

   if(!kgetstr(ibuf, CLIENT_TYPE_FIELD, obuf)){
      fprintf(stderr, "%s: No client type given for key %s around line %d\n",
	 keyf, keyname, line);
      *err = 1;
      return NULL;
   }
   strncpy((char *)key->client_type, obuf, 31);
   key->client_type[31] = '\0';

   if(!kgetstr(ibuf, ARCH_TYPE_FIELD, obuf)){
      fprintf(stderr, "%s: No architecture given for key %s around line %d\n",
	 keyf, keyname, line);
      *err = 1;
      return NULL;
   }
   strncpy((char *)key->architecture, obuf, 31);
   key->architecture[31] = '\0';

   if(!kgetstr(ibuf, CLASS_FIELD, obuf)){
      if(verbose)
	 fprintf(stderr, 
	    "%s: Warning: no class field given for key %s around line %d\n", 
	       keyf, keyname, line);
      /* not an error, but if we specified a class list, this client
	 will be excluded */
      if(classlist || class_exclude_list){
	 if(verbose){
	    fprintf(stderr, "%s (no class) excluded.\n", 
	       key->client_type);
	 }
	 return NULL;
      }
   }
      /* correct for user-requested class? */
   else {
      if (class_exclude_list) {
        if (class_ok(class_exclude_list, obuf)) {
           if (verbose) 
               fprintf(stderr, "%s (class \"%s\") excluded.\n",
                   key->client_type, obuf);
           return NULL;
	}
      } 
      else if(!class_ok(classlist, obuf)){
        if (verbose)
	   fprintf(stderr, "%s (class \"%s\") excluded.\n", 
	       key->client_type, obuf);
        return NULL;
      }
   }

   if(!kgetkey(ibuf, GLOBAL_KEY_FIELD, key->global)){
      fprintf(stderr, "%s: No global key given for key %s around line %d\n",
	 keyf, keyname, line);
      *err = 1;
      return NULL;
   }

   if(!kgetkey(ibuf, PUBLIC_KEY_FIELD, key->public)){
      fprintf(stderr, "%s: No public key given for key %s around line %d\n",
	 keyf, keyname, line);
      *err = 1;
      return NULL;
   }

   return key;
}

int
read_key(fi, bp)

   FILE	*fi;
   char	*bp;
{
   register char	*cp;
   register char	c;

   do {
      cp = bp;
      while((c=getc(fi))!=EOF){
	 if(c == '\n'){
	    line ++;
	    while(cp > bp && isspace(cp[-1])) cp--;
	    if(cp > bp && cp[-1] == '\\'){
	       cp--;
	       continue;
	    }
	    break;
	 }
	 if(cp >= bp+BUFSIZ){
	    fprintf(stderr, "%s: entry exceeded %d chars around line %d\n",
	       keyf, BUFSIZ, line);
	    return -1;
	 }
	 else
	    *cp++ = c;
      }
   } while(bp[0] == '#' && c != EOF);
   *cp = 0;

   return c!= EOF;
}

/*
 * Extract the key name descriptor from an entry buffer
 * "Key Name:"
 */

int
kgetkeyname(src, dest)

   char	*src;
   char	*dest;
{
   char	*r = strchr(src,SEP);
   int	l;

   if(!r) return 0;
   l = (int) (strchr(src,SEP)-src);
   strncpy(dest, src, l);
   dest[l] = 0;
   return 1;
}

/*
 * Place contents of specified field entry into destination string.
 * "pk=Text Text Text:"
 */

int
kgetstr(src, field, dest)
   
   char	*src, *field, *dest;
{
   char	*s = strstr(src, field), *r;
   int	l;
   if(!s)
      return 0;
   
   s += strlen(field);
   r = strchr(s, SEP);
   if(!r) return 0;
   l = r - s;
   strncpy(dest, s, l);
   dest[l] = 0;
   return 1;
}

/*
 * Place contents of specified binary field entry into destination string.
 * "pk=Text Text Text:"
 */

int
kgetkey(src, field, dest)
   
   char			*src, 
			*field;
   unsigned char 	*dest;
{
   char			unencoded_dest[KEY_SIZE*2+1],
			uc[3];
   register char	*s;
   unsigned int		c;

   if(!kgetstr(src, field, unencoded_dest))
      return 0;
   
   /* convert encoded binary to binary */
   s = unencoded_dest;
   while(*s){
      uc[0] = *s++;
      uc[1] = *s++;
      uc[2] = 0;
      sscanf(uc, "%x", &c);
      *dest++ = (unsigned char)c;
   }
   return 1;
}

char **
read_exclude_file(n)

   char	*n;
{
   char		buf[80];
   char		**s, *nl;
   register int	l;
   FILE		*fi = fopen(n, "r");
   if(!fi){
      /*
      perror(n);
      */
      return NULL;
   }

   l=0;
   while(fgets(buf, 80, fi))
      l++;
   fclose(fi);
   s = (char **) malloc(sizeof(char *) * (l+1));
   if(!s) { perror("malloc"); exit(1); }
   fi = fopen(n, "r");
   l = 0;
   while(fgets(buf, 80, fi)){
      if((nl = strchr(buf, '\n')))
	 *nl = 0;
      s[l++] = strdup(buf);
   }
   s[l] = NULL;
   fclose(fi);
   return s;
}

char *
read_class_file(n)

   char	*n;
{
   char		buf[BUFSIZ], *s=buf;
   FILE		*fi = fopen(n, "r");
   register char c;
   if(!fi)
      return NULL;

   while((c=getc(fi)) != EOF){
      if(isspace(c) || c == '\n')
	 continue;
      *s++ = c;
   }
   *s = 0;
   fclose(fi);
   if(*buf)
      return strdup(buf);
   else
      return NULL;
}

char *
read_class_exclude(n)

   char *n;
{
   char         buf[BUFSIZ], *s=buf;
   FILE         *fi = fopen(n, "r");
   register char c;
   if(!fi)
      return NULL;

   while((c=getc(fi)) != EOF){
      if(isspace(c) || c == '\n')
         continue;
      *s++ = c;
   }
   *s = 0;
   fclose(fi);
   if(*buf)
      return strdup(buf);
   else
      return NULL;
}


int
excluded(keyname)

   char	*keyname;
{
   register char	**s;

   if(!exclude_list) return 0;

   for(s=exclude_list; *s; s++){
      if(strcmp(*s, keyname)==0)
	 return 1;
   }
   return 0;
}

   /* inefficient but what the heck.. */

void
sort_motdlist(nk)
   
   int	nk;
{
   char			buf[80];
   struct  motd_keylist *keys, *k;
   char			client[80], arch[80];
   register int	i, f;
   FILE			*fi = fopen(motdlist, "r"), *fo;

   if(!fi) { perror(motdlist); return; }

   keys = (struct motd_keylist *) malloc(sizeof(struct motd_keylist)*nk);
   memset(keys, 0, sizeof(struct motd_keylist) * nk);
   for(i=0; i< nk; i++){
      keys[i].arch = (char **) malloc(sizeof(char *) * nk);
      memset(keys[i].arch, 0, sizeof(char *) * nk);
   }

   while(fgets(buf, 80, fi)){
      if(sscanf(buf, "\"%[^\"]\" \"%[^\"]", client, arch) != 2)
	 abort();	/* DEBUG */
      store_keydesc(keys, client, arch);
   }
   fclose(fi);

   fo = fopen(motdlist, "w");
   if(!fo) { 
	perror(motdlist); 
	return; 
   }
   for(k=keys; k->client; k++){
      f = (70 - (strlen(k->client)+4))/2;
      for(i=0; i< f; i++)
         putc('-', fo);
      fprintf(fo, "[ %s ]", k->client);
      for(i=0; i< f; i++)
         putc('-', fo);
      putc('\n', fo);

      for(i=0; i< k->num_arch; i ++){
	 fprintf(fo, "%-36s", k->arch[i]);
	 i++;
	 if(i < k->num_arch){
	    fprintf(fo, "%s\n", k->arch[i]);
	 }
	 else
	    fprintf(fo, "\n");
      }
   }
   fclose(fo);
}

void
store_keydesc(keys, client, arch)
   
   struct motd_keylist	*keys;
   char			*client;
   char			*arch;
{
   register struct motd_keylist	*k;

   for(k=keys; k->client; k++){
      if(strcmp(k->client, client)==0){
	 k->arch[k->num_arch] = strdup(arch);
	 k->num_arch++;
	 return;
      }
   }
   k->client = strdup(client);
   k->arch[0] = strdup(arch);
   k->num_arch++;
}

/*
 * Given two strings of the form "class,class,..." determine if they have
 * any fields in common
 */

int
class_ok(cl1, cl2)

   char	*cl1, *cl2;
{
   register char        *s1, *r1, *s2, *r2;
   register int         l1, l2;

   /* if either field is null, we assume that means any class is ok */
   if(!cl1 || !cl2) return 1;

   s1 = cl1;
   while(1){
      r1 = strchr(s1, ',');
      if(!r1)
         l1 = strlen(s1);
      else
         l1 = r1 - s1;
      /* class is s1, length l1 */
      s2 = cl2;
      while(1){
         r2 = strchr(s2, ',');
         if(!r2)
            l2 = strlen(s2);
         else
            l2 = r2 - s2;
         /* class is s2, length l2 */
/*
         printf("comparing %s to %s length %d\n", s1,s2,l2);
*/
         if(l1 == l2 && strncmp(s1, s2, l2)==0)
            return 1;
         if(!s2[l2]) break;
         s2 += l2+1;
      }
      if(!s1[l1]) break;
      s1 += l1+1;
   }
   return 0;
}

void
usage(p)
   
   char	*p;
{
   char	*mess = "usage:\n\
%s [-options] metaserver-key-file\n\n\
where options include:\n\
   -e excludefile	a file containing a list of excluded keys by keyname.\n\
   -m motdlist		a file to be created that will contain RSA key \n\
			descriptions suitable for inclusion in a server motd.\n\
   -o reservedfile	the compiled key descriptions used by the server.\n\
   -t class,class,...	select only clients in the given class where class is:\n\
			   inl - INLC-approved clients for use in INL games\n\
			   standard - other clients\n\
			   borg - borg clients\n\
			   etc...\n\
			multiple classes are seperated by `,' with no spaces.\n\
   -f classfile		a file containing a list of classes in the above format.\n\
   -c			show new keys during compilation.\n\
   -v			verbosity.\n\
   -x			select client classes to be excluded where class is:\n\
                           inl - INLC-approved clients for use in INL games\n\
                           standard - other clients\n\
                           borg - borg clients\n\
			   etc...\n\
			multiple classes are seperated by `,' with no spaces.\n\n\
Files:\n\
   %-32s default exclude file (input).\n\
   %-32s default motd list file (output).\n\
   %-32s default class list file (input).\n\
   %-32s default reserved file (output).\n\n\
%s takes an ascii RSA key-descriptions file downloaded from the \n\
metaserver (currently from telnet clientkeys.netrek.org 3530) and compiles it into\n\
a binary form usable by the RSA authentication mechanism of the netrek server.\n";

   fprintf(stderr, mess, p, RSA_KEY_EXCLUDE_FILE, RSA_MOTDLIST, RSA_CLASS_FILE,
	   RSA_Key_File,
	   p);
			    
   exit(0);
}

#else

int 
main()
{
	printf("You don't have RSA option on.\n");
	return 1;		/* satisfy lint */
}
#endif
