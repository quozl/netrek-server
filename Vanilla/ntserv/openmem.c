#include "copyright.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <signal.h>
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

/* identifier of the shared memory segment */
static int shmid;

/* shared memory key */
static int pkey = PKEY;

/* shared memory pointer */
static struct memory *sharedMemory = NULL;

/* daemon initialisation is pending */
static int daemon_initialisation_pending;

/* set a different shared memory key for this process and children */
void setpkey(int nkey)
{
  char ev[32];
  pkey = nkey;
  sprintf(ev, "NETREK_PKEY=%d", nkey);
  if (putenv(ev) != 0) {
    perror("setpkey: putenv");
  }
}

/* get the current shared memory key for this process */
int getpkey()
{
  return pkey;
}

/* initialise shared memory key from environment variables */
static void initpkey()
{
  char *ev = getenv("NETREK_PKEY");
  if (ev == NULL) return;
  pkey = atoi(ev);
  if (pkey < 1) pkey = PKEY;
}

/* called when daemon initialisation is complete */
static void daemon_ready(int signum)
{
    daemon_initialisation_pending = 0;
}

/* start the daemon, wait for it to finish initialising */
static void startdaemon(void)
{
    pid_t i;

    /* ask to be told when daemon initialisation completes */
    daemon_initialisation_pending = 1;
    signal(SIGUSR1, daemon_ready);

    i = fork();
    if (i == (pid_t)0) {
	execl(Daemon, "daemon", "netrek daemon", "--tell", (char *) NULL);
	perror(Daemon);
	ERROR(1,("Couldn't start daemon!!!\n"));
	_exit(1);
    }

    /* wait until daemon has initialised */
    while (daemon_initialisation_pending) pause();
    signal(SIGUSR1, NULL);
}

/* connect all the pointers to the various areas of the segment */
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
    initpkey();
    shmid = shmget(pkey, SHMFLAG, 0);
    if (shmid < 0) {            /* Could not find the shared memory */
	if (errno != ENOENT) {  /* Error other not created yet */
	    perror("shmget");
	    exit(1);
	}
	if (trystart==1){          /* Create the memory */
	  startdaemon();
	}
	else if (trystart < 0){    /* Just checking if it exists */
	  return 0;
	}
	else {                     /* Wanted to use it, but...   */
	  fprintf(stderr,"Warning: Daemon not running!\n");
	  exit(1);
	}
	shmid = shmget(pkey, SHMFLAG, 0);
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


int setupmem(void) {
    struct shmid_ds smbuf;
  
    initpkey();
    /* Kill any existing segments */
    if ((shmid = shmget(pkey, SHMFLAG , 0)) >= 0) {
        ERROR(2,("setupmem: Killing existing segment\n"));
      shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0);
    }

    /* Get them memory id */
    shmid = shmget(pkey, sizeof(struct memory), IPC_CREAT | 0777);
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
  if (shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    perror("removemem");
    return 0;
  }
  return 1;
}

int detachmem(void) {
  if (shmdt(sharedMemory) < 0) {
    perror("detachmem");
    return 0;
  }
  return 1;
}
