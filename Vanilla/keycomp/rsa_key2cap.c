#include "config.h"
#include <stdio.h>

#ifdef RSA
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <string.h>
#include "defs.h"
#include "struct.h"

#define SEP                     ':'

#define TEXT_FIELD_LENGTH	80

#define CLIENT_TYPE_FIELD       "ct="
#define CREATOR_FIELD           "cr="
#define CREATED_FIELD		"cd="
#define ARCH_TYPE_FIELD         "ar="
#define COMMENTS_FIELD		"cm="
#define GLOBAL_KEY_FIELD        "gk="
#define PUBLIC_KEY_FIELD        "pk="

#if defined(__STDC__) || defined(__cplusplus)
#define P_(s) s
#else
#define P_(s) ()
#endif

void 			      encode P_((unsigned char *bin, char *buf));
void 			      getin P_((int l, char *prompt, char *str));

#ifdef _NEED_SYS_PROTOS
extern size_t                 fread P_((void *, size_t, size_t, FILE *));
extern int                    fprintf P_((FILE *, const char *, ...));
extern void                   perror P_((const char *));
extern int                    fclose P_((FILE *));
extern int                    printf P_((const char *, ...));
extern int                    fflush P_((FILE *));
#endif /* _NEED_SYS_PROTOS */

#undef P_

int
main(argc, argv)
   
   int	argc;
   char	**argv;
{
   char			*keyf;
   FILE			*fi;
   unsigned char	global[KEY_SIZE], 
			public[KEY_SIZE];

   char			gk[KEY_SIZE*2+1],
   			pk[KEY_SIZE*2+1],
   			client_key_name[TEXT_FIELD_LENGTH],
   			client_type[TEXT_FIELD_LENGTH],
			arch_type[TEXT_FIELD_LENGTH],
			creator[TEXT_FIELD_LENGTH],
			created[TEXT_FIELD_LENGTH],
			comments[TEXT_FIELD_LENGTH];

   if(argc < 2){
      fprintf(stderr, "usage: %s <binary key file>\n",
	      argv[0]);
      exit(0);
   }
   keyf = argv[1];

   if(!(fi=fopen(keyf, "r"))){
      perror(keyf);
      exit(1);
   }

   if(fread((char *)global, KEY_SIZE, 1, fi) != 1){
      fprintf(stderr, "%s: can't read global key.\n", keyf);
      fclose(fi);
      exit(1);
   }
   if(fread((char *)public, KEY_SIZE, 1, fi) != 1){
      fprintf(stderr, "%s: can't read public key.\n", keyf);
      fclose(fi);
      exit(1);
   }
   fclose(fi);

   encode(global, gk);
   encode(public, pk);

   getin(32, "Key name: ", client_key_name);
   getin(32, "Client: ", client_type);
   getin(TEXT_FIELD_LENGTH, "Creator: ", creator);
   getin(TEXT_FIELD_LENGTH, "Created: ", created);
   getin(32, "Arch/OS: ", arch_type);
   getin(TEXT_FIELD_LENGTH, "Comments: ", comments);

   printf("#\n");
   printf("%s%c", client_key_name, SEP);
   printf("   :%s%s%c", CLIENT_TYPE_FIELD, client_type, SEP);
   printf("   :%s%s%c\\\n", CREATOR_FIELD, creator, SEP);
   printf("   :%s%s%c", CREATED_FIELD, created, SEP);
   printf("   :%s%s%c\\\n", ARCH_TYPE_FIELD, arch_type, SEP);
   printf("   :%s%s%c\\\n", COMMENTS_FIELD, comments, SEP);
   printf("   :%s%s%c\\\n", GLOBAL_KEY_FIELD, gk, SEP);
   printf("   :%s%s%c\n", PUBLIC_KEY_FIELD, pk, SEP);
   return 1;		/* satisfy lint */
}

void
encode(bin, buf)

   unsigned char	*bin;
   char			*buf;
{
   register int i;
   for(i=0; i< KEY_SIZE; i++){
      sprintf(buf, "%02x", bin[i]);
      buf += 2;
   }
   *buf = '\0';
}

void
getin(l, prompt, str)

   int	l;
   char	*prompt, *str;
{
   char			buf[BUFSIZ];

   fprintf(stderr, "%s", prompt);
   fflush(stderr);
   fgets(buf, BUFSIZ, stdin);
   strncpy(str, buf, l-1);
   str[l-1] = '\0';
}
#else
int
main()
{
	printf("You don't have RSA option on\n");
	return 1;	/* satisfy lint */
}
#endif

