/*
 * shmem.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "xsg_defs.h"
#include "Wlib.h"
#include "struct.h"
#include "localdata.h"

openmem()
{
    extern int errno;
    int	shmemKey = PKEY;
    int	shmid;

    if(playback){
       sharedMemory = (struct memory *)malloc(allocFrames * 
	 sizeof(struct memory));
       if(sharedMemory)
	  memset(sharedMemory, 0, sizeof(struct memory) * allocFrames);
       while (sharedMemory == NULL && allocFrames > 10)
       {
	   allocFrames -= 10;
	   sharedMemory = (struct memory *)malloc(allocFrames * 
	    sizeof(struct memory));
	   if(sharedMemory)
	      memset(sharedMemory, 0, sizeof(struct memory) * allocFrames);
       }

       if (allocFrames <= 0)
       {
	   fprintf(stderr, "Not enough memory to allocate any frames.\n");
	   exit(0);
       }
/*
       else
	  printf("Allocated %d frames for rewind.\n", allocFrames);
*/
    }
    else {

       /* otherwise look for shared memory segment */
       errno = 0;
       shmid = shmget(shmemKey, 0, 0);
       if (shmid < 0) {
	   if (errno != ENOENT) {
	       perror("shmget");
	       exit(1);
	   }
	   shmid = shmget(shmemKey, 0, 0);
	   if (shmid < 0) {
	       fprintf(stderr, "Daemon not running\n");
	       exit (1);
	   }
       }
       sharedMemory = (struct memory *) shmat(shmid, 0, 0);
       if (sharedMemory == (struct memory *) -1) {
	   perror("shared memory");
	   exit (1);
       }
    }

    /* technically this isn't needed for playback but it's here to save some
       trouble elsewhere */

    players = sharedMemory->players;
    torps = sharedMemory->torps;
    status = sharedMemory->status;
    planets = sharedMemory->planets;
    phasers = sharedMemory->phasers;
    mctl = sharedMemory->mctl;
    messages = sharedMemory->messages;
    shipvals = sharedMemory->shipvals;
    teams = sharedMemory->teams;
}

