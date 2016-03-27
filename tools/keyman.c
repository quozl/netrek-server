/*

        Nick Trown      10-19-92
        trown@cscihp.ecst.csuchico.edu
        RSA key manipulation
*/


#include "config.h"
#include <stdio.h>
#ifdef RSA
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/file.h>
#include "defs.h"
#include <string.h>
#include <fcntl.h>
#include "struct.h"
#include "data.h"
#include "proto.h"

#define CATALOG        "catalog"

void readkeys(int convert);
void do_usage(void);
void listkeys(void);
void motdlist(void);
void check_exist(char *name);
void catalog(int num, char *id);
void convert(void);
void deletekey(char *number);
void modifykey(char *number);
void extractkey(char *number, char *filename);
void savekeys(void);
void exist(int num);
void addkey(char *name);
void reorder(int argc, char *argv[]);


struct rsa_key key_list[1000] = { {"\0", "\0", "\0"} };
int key_count=0;

void readkeys(int convert)
{
    int fd;
    int done;

    fd = open(RSA_Key_File, O_RDONLY ,0600);
    if (fd < 0)
    {
        fprintf(stderr, "Can't open the RSA key file %s\n", RSA_Key_File);
        exit(1);
    }

    done = 0;
    do
    {
        if (!convert) {
                if (read(fd, key_list[key_count].client_type, KEY_SIZE) != KEY_SIZE)
                    done = 1;
                if (read(fd, key_list[key_count].architecture, KEY_SIZE) != KEY_SIZE)
                    done = 1;
        }
        if (read(fd, key_list[key_count].global, KEY_SIZE) != KEY_SIZE) {
                done = 1;
        }
        if (read(fd, key_list[key_count].public, KEY_SIZE) != KEY_SIZE) {
                done = 1;
        }
        key_count++;
    } while (! done);

    key_count--;

    close(fd);
}

int main(int argc, char *argv[])
{
    char mode;

    mode = 'l';

    if (argc>1)
        mode = *argv[1];

    getpath();
    switch (mode) {

        case 'l' : listkeys();
            break;
	case 'L' : motdlist();
	    break;
        case 'c' :
                  convert();
            break;
        case 'm' :
            if (argc>2) {
                modifykey(argv[2]);
            }
            else do_usage();
            break;
	case 'x':
	    if (argc>3) {
		extractkey(argv[2], argv[3]);
	    }
	    else do_usage();
	    break;
        case 'd' : {
            if (argc>2) {
                deletekey(argv[2]);
            }
            else do_usage();
            }
            break;
        case 'a' : {
            if (argc>2) {
                addkey(argv[2]);
            }
            else do_usage();
            }
            break;
        case 'v' :  {
            if (argc>2) {
                 check_exist(argv[2]);
            }
            else do_usage();
            }
            break;
	case 'o' :
	    reorder(argc,argv);
	    break;	

        default :
                do_usage();
    }
    return 1;	/* satisfy lint */
}


void do_usage(void)
{
    printf("usage: keys mode <key #> <key file>\n");
    printf("Modes:\n");
    printf("l - list keys (default)\n");
    printf("c - convert. Add type names to RSA file\n");
    printf("d - delete key <key #>\n");
    printf("a - add key <key file>\n");
    printf("v - verify existence of <key file> in RSA file\n");
    printf("m - modify existing key <key #>\n");
    printf("x - extract a key <key #> to a file <res file>\n");
    printf("o - reorder the keys <key number list>\n");
    printf("L - produce a motd listing\n");
    printf("    (note <res file> will have id strings... can be appended to .reserved)\n");
    exit(0);
}

void listkeys(void)
{
    int count;

    readkeys(0);

    printf("There are %d keys in the RSA file\n",key_count);


    for (count = 0; count<key_count; count++) {
        printf("%3d: (%17s | %s)\n",count,key_list[count].client_type,
                                     key_list[count].architecture);
    }

    printf("\n");
    exit(0);
}

void motdlist(void)
{
    int count;
    int c;
    int groups[40][20];
    int groupnum[40];
    char line[40];
    char title[MSG_LEN];
    int groupcount=0;
    int scan;
    int found=0;
	
    readkeys(0);


    for (count=0; count<key_count; count++) {
      for (scan=0; scan<groupcount; scan++) {
	if (!strcmp((char *)key_list[count].client_type,
		    (char *)key_list[groups[scan][0]].client_type)) {
		groups[scan][groupnum[scan]++]= count;
		found=1;
		break;
	}
      }
      if (!found) {
	groups[groupcount][0]=count;
	groupnum[groupcount++]++;
      }
      found=0;
    }

    for (count=0; count<groupcount; count++) {
	for (c=0; c<30; c++)
        	line[c]='-';
	line[30]='\0';
    	strcat(line,"[ ");

	sprintf(title,"%s %-15s ]",line,key_list[groups[count][0]].client_type);
	for (c=0;strlen(title)<MSG_LEN;c++)
	  strcat(title,"-");	
/*
	printf("\n%40ss\n",key_list[groups[count][0]].client_type);
*/
	printf("%s\n",title);
	for (scan=0; scan<groupnum[count];) {
		printf("%-35s",key_list[groups[count][scan++]].architecture);
		if (scan<groupnum[count])
		  printf("%-35s\n",key_list[groups[count][scan++]].architecture);
		else printf("\n");
	}
    }
    printf("\n");
    exit(0);
}
		
	
void check_exist(char *name)
{
    int fd;
    void exist();

    readkeys(0);

    fd = open(name, O_RDONLY,0600);
    if (fd < 0)
    {
        fprintf(stderr, "Can't open key file %s\n", name);
        exit(1);
    }


    if (read(fd,key_list[key_count].global,KEY_SIZE) != KEY_SIZE) {
                printf("global key wrong size\n");
                exit(0);
    }
    if (read(fd,key_list[key_count].public,KEY_SIZE) != KEY_SIZE) {
                printf("public key wrong size\n");
                exit(0);
    }

    exist(key_count);
    printf("Key %s not in RSA file\n",name);
}


void catalog(int num, char *id)
{
    FILE *fp;
    time_t curtime;

    if ((fp = fopen(CATALOG,"a+"))==NULL) {
        printf("Error opening %s\n",CATALOG);
        exit(0);
    }
    curtime = time((time_t *)0);

    fprintf(fp,"Client: %s\n", key_list[num].client_type);
    fprintf(fp,"\tCreator ID: %s\n", id);
    fprintf(fp,"\tAdded     : %s", ctime(&curtime));
    fprintf(fp,"\tArch/OS   : %s\n", key_list[num].architecture);
    fprintf(fp,"\tComments  : \n");

    fclose(fp);

}

void convert(void)
{
    char buffer[MSG_LEN];
    int count;
    void savekeys();

    printf ("WARNING: This should only be used if your .reserved is a file of raw keys!\n");
    printf ("WARNING: This will prepend client type and architecture/OS fields to keys in .reserved.\n");
    printf ("WARNING: This will appent entries to your .catalog file.\n");
    printf ("WARNING: Be certain you want to do this!!!!!!!\n");
    printf ("WARNING: This procedure will alter your RSA_Key_File. Continue?(y/n)");
    fgets(buffer, MSG_LEN, stdin);
    if (!strcmp(buffer, "n")) {
      printf ("Ok, exiting...\n");
      exit (0);
    }
    if (strcmp(buffer, "y")) {
      printf ("unrecognized response... must enter 'y' or 'n'... exiting.\n");
      exit(1);
    }

    readkeys(1);

    printf("There are %d unconverted keys\n",key_count);

    for (count = 0; count< key_count; count++) {

      printf ("Key %d\n", count);

      do {
          printf("Enter type of client: ");
          fgets(buffer, MSG_LEN, stdin);
          if (strlen(buffer) > KEY_SIZE)
                  printf("Client type must be less than %d characters.\n",KEY_SIZE);
      } while (strlen(buffer) > KEY_SIZE);
      strcpy((char *)key_list[count].client_type,buffer);

      do {
          printf("Enter architecture/OS: ");
          fgets(buffer, MSG_LEN, stdin);
          if (strlen(buffer) > KEY_SIZE)
                  printf("Architecture field must be less than %d characters.\n",KEY_SIZE);
      } while (strlen(buffer) > KEY_SIZE);
      strcpy((char *)key_list[count].architecture,buffer);


      do {
        printf("Enter the Creator's ID: ");
        fgets(buffer, MSG_LEN, stdin);
        if (strlen(buffer) > KEY_SIZE)
              printf("ID size too long. Use the creator's login\n");
      } while (strlen(buffer)>KEY_SIZE);

      catalog (count, buffer);
    }

    savekeys();
}

void deletekey(char *number)
{
    int num = atoi(number);
    void savekeys();

    readkeys(0);

    if (num <0) {
        printf("Must be positive\n");
        exit(0);
    }

    if (num >= key_count) {
        printf("Number to large\n");
        exit(0);
    }

    printf("Warning: The entry (%s | %s) in the catalog needs to be deleted.\n",
               key_list[num].client_type, key_list[num].architecture);

    key_list[num] = key_list[--key_count];
    savekeys();
}

void modifykey(char *number)
{
    int num = atoi(number);
    char type[80];
    void savekeys();

    if (num <0) {
        printf("Must be positive\n");
        exit(0);
    }

    readkeys(0);

    if (num >= key_count) {
        printf("Number to large\n");
        exit(0);
    }


    do {
	printf ("Client: %s\n", key_list[num].client_type);
        printf("Enter new type of client (<enter> for no change): ");
        fgets(type, 80, stdin);
        if (strlen(type) > KEY_SIZE)
                printf("Client type must be less than %d characters.\n",KEY_SIZE);
    } while (strlen(type) > KEY_SIZE);
    if (strlen(type)==0)
	printf ("Client type not modified.\n");
    else
        strcpy((char *)key_list[num].client_type,type);

    do {
	printf("Arch/OS: %s\n", key_list[num].architecture);
        printf("Enter new architecture/OS (<enter> for no change): ");
        fgets(type, 80, stdin);
        if (strlen(type) > KEY_SIZE)
                printf("Architecture field must be less than %d characters.\n",KEY_SIZE);
    } while (strlen(type) > KEY_SIZE);
    if (strlen(type)==0)
	printf ("Arch/OS not modified.\n");
    else
    	strcpy((char *)key_list[num].architecture,type);

    savekeys();

}

void extractkey(char *number, char *filename)
{
    int num = atoi(number);
    int fd;
    void savekeys();

    if (num <0) {
        printf("Must be positive\n");
        exit(0);
    }

    readkeys(0);

    if (num >= key_count) {
        printf("Number to large\n");
        exit(0);
    }

    fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0600);
    if (fd < 0)
    {
        fprintf(stderr, "Can't open the new key file %s\n", filename);
        exit(1);
    }

    printf("saving key %d (%s | %s) in file %s\n", num, key_list[num].client_type,
                                                   key_list[num].architecture, filename);
    if (write(fd, &key_list[num], sizeof(struct rsa_key)) != sizeof(struct rsa_key))
        printf("Error during file write!\n");

    close(fd);
    exit(0);
}

void savekeys(void)
{
    int fd;
    int count;

    fd = open(RSA_Key_File, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0)
    {
        fprintf(stderr, "Can't open the RSA key file %s\n", RSA_Key_File);
        exit(1);
    }
    for (count=0; count<key_count; count++) {
        printf("saving key %d (%s | %s)\n", count, key_list[count].client_type,
                                                   key_list[count].architecture);
        if (write(fd, &key_list[count], sizeof(struct rsa_key)) != sizeof(struct rsa_key))
                printf("Error during file write!\n");
    }
    close(fd);
    exit(0);
}

void exist(int num)
{
    int count;

    for (count =0; count<key_count; count++) {
          if ((!memcmp(key_list[num].global,key_list[count].global,KEY_SIZE)) &&
             (!memcmp(key_list[num].public,key_list[count].public,KEY_SIZE))) {
                printf("That key is already in the database with type (%s | %s)\n"
                        ,key_list[count].client_type, key_list[count].architecture);
                exit(0);
          }
    }
}


void addkey(char *name)
{
    int fd;
    char type[80],
         id[KEY_SIZE]; /* creator id */

    readkeys(0);

    fd = open(name, O_RDONLY, 0600);
    if (fd < 0)
    {
        fprintf(stderr, "Can't open key file %s\n", name);
        exit(1);
    }


    if (read(fd,key_list[key_count].global,KEY_SIZE) != KEY_SIZE) {
                printf("global key wrong size\n");
                exit(0);
    }
    if (read(fd,key_list[key_count].public,KEY_SIZE) != KEY_SIZE) {
                printf("public key wrong size\n");
                exit(0);
    }
    exist(key_count);

    do {
      printf("Enter the Creator's ID: ");
      fgets(type, 80, stdin);
      if (strlen(type) > KEY_SIZE)
            printf("ID size too long. Use the creator's login\n");
    } while (strlen(type)>KEY_SIZE);
    strcpy (id, type);

    do {
        printf("Enter type of client:");
        fgets(type, 80, stdin);
        if (strlen(type) > KEY_SIZE)
                printf("Client type must be less than %d characters.\n",KEY_SIZE);
    } while (strlen(type) > KEY_SIZE);
    strcpy((char *)key_list[key_count].client_type,type);

    do {
        printf("Enter architecture/OS:");
        fgets(type, 80, stdin);
        if (strlen(type) > KEY_SIZE)
                printf("Architecture field must be less than %d characters.\n",KEY_SIZE);
    } while (strlen(type) > KEY_SIZE);
    strcpy((char *)key_list[key_count].architecture,type);


    close(fd);
    key_count++;
/*
    fd = open(RSA_Key_File, O_CREAT | O_WRONLY | O_APPEND, 0600);
    if (fd < 0)
    {
        fprintf(stderr, "Can't open the RSA key file %s\n",name);
        exit(1);
    }


    printf("saving key %d (%s | %s)\n", key_count, key_list[key_count].client_type,
                                                   key_list[key_count].architecture);
    if (write(fd, &key_list[key_count], sizeof(struct rsa_key)) != sizeof(struct rsa_key))
            printf("Error during file write!\n");

    close(fd);
*/

    savekeys();
    printf("added key to RSA file\n");

    catalog(key_count, id);

    exit(0);
}


void reorder(int argc, char *argv[])
{
    int count;
    struct rsa_key temp_list[1000];

    readkeys(0);
    if (argc-2<key_count) {
	printf("you must list all the keys\n");
	return;
    }


    memcpy(key_list,
           temp_list,
           key_count*(sizeof(struct rsa_key)));

    for (count=2; count<=argc; count++) {
        memcpy(&temp_list[atoi(argv[count])],
               &key_list[count-2],
               (size_t)sizeof(struct rsa_key));
    }

    savekeys();
}
#else
int main(void)
{
	printf("You don't have RSA verification on.\n");
	return 1;
}
#endif
