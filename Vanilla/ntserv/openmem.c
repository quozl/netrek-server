/*
 * $Id: openmem.c,v 1.4 2006/05/06 14:02:37 quozl Exp $
 */
#include "copyright.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include <time.h>		/* 7/16/91 TC */
#include "defs.h"
#include "struct.h"
#include "data.h"

/*
 *  This file contains the routines necessary to start the daemon
 *  and connect to the shared memory.  startdaemon() should never
 *  be called except through openmem.  
 *
 *  openmem(int trystart) is used to connect to the memory and
 *  perhaps create it.  It is called an integer argument:
 *
 *  openmem(1) tries to start the daemon if memory does not exist.
 *    It returns 1 if successful and exits if not.
 *  openmem(0) tries to connect to the memory, but will NOT start
 *    the daemon.  It returns 1 if successful, and exits if not.
 *    This is useful for tools, such as xtkill.
 *  openmem(-1) tries to connect to the memory, but will NOT start
 *    the daemon.  It returns 1 if successful, and 0 if not.  
 *    This is useful for tools such as players, which need to 
 *    return data if no one is playing.
 *
 *  setupmem() is called only by the daemon and sets up the shared
 *    memory.  This is not part of openmem, because:
 *    a)  It checks for other existing segments
 *    b)  Sets the memory access requirements
 *    c)  Bzeros the entire memory structure.
 *    It return 1 if successful and 0 on failure
 *  removemem() is called only by the daemon and removes the shared
 *    memory segment.  Returns 1 on success, o on failure
 *
 *  setup_memory() is the local routine that assigns all the global variables.
 *
 *  These routines require data.[ch], to predefine the shared
 *  memory variables as global.
 *
 *  Michael Kantner, March 18, 1994
 *
 */

static void startdaemon(void)
{
    pid_t i;

    i = fork();
    if (i == (pid_t)0) {
	execl(Daemon, "daemon", (char *) NULL);
	perror(Daemon);
	ERROR(1,("Couldn't start daemon!!!\n"));
	_exit(1);
    }
}

static void setup_memory(struct memory *sharedMemory)
{
    players = sharedMemory->players;
    torps = sharedMemory->torps;
    context = sharedMemory->context;
    status = sharedMemory->status;
    planets = sharedMemory->planets;
    phasers = sharedMemory->phasers;
    mctl = sharedMemory->mctl;
    messages = sharedMemory->messages;
    teams = sharedMemory->teams;
    shipvals = sharedMemory->shipvals;
    queues = sharedMemory->queues;
    waiting = sharedMemory->waiting;
    bans = sharedMemory->bans;
}

/*ARGSUSED*/
int openmem(int trystart)
{
    extern int errno;
    int	shmemKey = PKEY;
    struct memory	*sharedMemory;
    int shmid;

    errno = 0;
   
    shmid = shmget(shmemKey, SHMFLAG, 0);
    if (shmid < 0) {            /* Could not find the shared memory */
	if (errno != ENOENT) {  /* Error other not created yet */
	    perror("shmget");
	    exit(1);
	}
	if (trystart==1){          /* Create the memory */
	  startdaemon();
	  sleep(2);
	}
	else if (trystart < 0){    /* Just checking if it exists */
	  return 0;
	}
	else {                     /* Wanted to use it, but...   */
	  fprintf(stderr,"Warning: Daemon not running!\n");
	  exit(1);
	}
	shmid = shmget(shmemKey, SHMFLAG, 0);
	if (shmid < 0) {  /* This is a bummer of an error */
	    ERROR(1, ("Daemon not running (err:%d)\n",errno));
	    exit(1);
	}
    }
    sharedMemory = (struct memory *) shmat(shmid, 0, 0);
    if (sharedMemory == (struct memory *) -1) {
	printf("Error number: %d\n",errno);
	perror("shared memory");
	exit(1);
    }
    setup_memory(sharedMemory);

    return 1;
}


/* Static variable only used by these routines in the daemon*/
static int shmid;  /* The id of the shared memory segment */

int setupmem(void) {
    extern int errno;
    int       shmemKey = PKEY;
    struct memory     *sharedMemory;
    struct shmid_ds smbuf;
  
    errno = 0;

    /* Kill any existing segments */
    if ((shmid = shmget(shmemKey, SHMFLAG , 0)) >= 0) {
        ERROR(2,("setupmem: Killing existing segment\n"));
      shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0);
    }

    /* Get them memory id */
    shmid = shmget(shmemKey, sizeof(struct memory), IPC_CREAT | 0777);
    if (shmid < 0) {
      ERROR(1,("setupmem: Can't open shared memory, error %i\n",errno));
      return 0;
    }

    /* Set memory access restrictions */
    shmctl(shmid, IPC_STAT, &smbuf);
    smbuf.shm_perm.uid = geteuid();
    smbuf.shm_perm.mode = 0700;
    shmctl(shmid, IPC_SET, &smbuf);

    /* Attach to the memory and bzero it */
    sharedMemory = (struct memory *) shmat(shmid, 0, 0);
    if (sharedMemory == (struct memory *) -1) {
      ERROR(1,("setupmem: Can't attach to memory, error %i\n",errno));
      return 0;
    }

    /* Zero the memory vector */
    MZERO((void *) sharedMemory, sizeof (struct memory));

    setup_memory(sharedMemory);

    return 1;
}

int removemem(void) {
  if (shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0) < 0){
    return 0;
  }
  return 1;
}

#ifdef nodef
static int detachmem(void) {
  return shmdt((void*)players); /* cheat! what is a better way?? jc */
}
#endif
