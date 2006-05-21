#include "copyright.h"
#include <stdio.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <math.h>
#include <errno.h>

#undef MAX
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "robot.h"

struct r_shmemt {

   int	num_robots;			/* number connected */

   int	robot[MAXPLAYER];		/* which slots used */

   int	sync[MAXPLAYER];		/* their sync */
   int	target[MAXPLAYER];		/* their target */
   int	tx[MAXPLAYER];			/* their location */
   int	ty[MAXPLAYER];

   int	px[MAXPLAYER][MAXPLAYER];	/* their idea of p_px */
   int	py[MAXPLAYER][MAXPLAYER];	/* their idea of p_py */
   int	tv[MAXPLAYER][MAXPLAYER];	/* time they saw it */
};

static struct r_shmemt 	*r_shmem;
static int		shmid;

shmem_rloc(i, x, y)

   int	i;
   int	*x,*y;
{
   if(!shmem) return;

   if(!r_shmem->robot[i]) return;

   /*
   printf("shmem_rloc: given coord %d,%d, shmem: %d,%d\n",
      *x, *y, r_shmem->tx[i], r_shmem->ty[i]);
   */

   *x = r_shmem->tx[i];
   *y = r_shmem->ty[i];
}

shmem_updmyloc(x, y)

   int	x,y;
{
   if(!shmem) return;
   r_shmem->tx[me->p_no] = x;
   r_shmem->ty[me->p_no] = y;
}

shmem_rtarg(i)

   int	i;
{
   if(!shmem) return -1;

   if(!r_shmem->robot[i]) return -1;
   return r_shmem->target[i]-1;
}

shmem_updmytarg(t)

   int	t;
{
   if(!shmem) return;

   r_shmem->target[me->p_no] = t+1;
}

shmem_rsync(i)

   int	i;
{
   if(!shmem) return -1;

   if(!r_shmem->robot[i]) return -1;
   return r_shmem->sync[i] -1;
}

shmem_updmysync(t)

   int	t;
{
   if(!shmem) return;

   r_shmem->sync[me->p_no] = t+1;
}

/* on startup */

shmem_open()
{
   int	create = 0;

   shmid = shmget(PKEY, sizeof(struct r_shmemt), IPC_CREAT|IPC_EXCL|0700);
   if(shmid < 0){
      if(errno != EEXIST){
	 perror("shmget");
	 shmem = 0;
      }
      else {
#ifdef linux
	 shmid = shmget(PKEY, sizeof(struct r_shmemt), 0);
#else
	 shmid = shmget(PKEY, 0, 0);
#endif
	 if(shmid < 0){
	    perror("shmget");
	    shmem = 0;
	 }
      }
   }
   else{
      if(DEBUG & DEBUG_SHMEM)
	 printf("shared memory created\n");
      create = 1;
   }

   r_shmem = (struct r_shmemt *) shmat(shmid, 0, 0);
   if((int)r_shmem < 0){
      perror("shmat");
      shmem = 0;
      return;
   }
   if(DEBUG & DEBUG_SHMEM)
      printf("shared memory attached\n");

   if(create)
      bzero(r_shmem, sizeof(struct r_shmemt));

   r_shmem->robot[me->p_no] = PALIVE;
   r_shmem->num_robots ++;
   r_shmem->sync[me->p_no] = 0;
   r_shmem->target[me->p_no] = 0;
}

/* on exit */

shmem_check()
{
   r_shmem->robot[me->p_no] = 0;
   r_shmem->num_robots --;
   if(r_shmem->num_robots == 0){
      shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0);
      if(DEBUG & DEBUG_SHMEM)
	 printf("shared memory removed.\n");
   }
}

/* functions */
shmem_cloakd(j)

   struct player	*j;
{
   register		i, k=0;
   register		sumx=0, sumy=0;
   int			tv = mtime(0), htv;

   if(!(j->p_flags & PFCLOAK)){
      mfprintf(stderr, "shmem_cloakd: player not cloaked\n");
      return 0;
   }
   /* assign my idea of location */
   r_shmem->px[me->p_no][j->p_no] = j->p_x;
   r_shmem->py[me->p_no][j->p_no] = j->p_y;
   r_shmem->tv[me->p_no][j->p_no] = tv;

   if(r_shmem -> num_robots < 2)	/* XXX */
      return 0;
   
   for(i=0; i< MAXPLAYER; i++){
      if(!r_shmem->robot[i]) continue;
      htv = r_shmem->tv[i][j->p_no];
      if(ABS(htv - tv) > 200) continue;
      k++;
      sumx += r_shmem->px[i][j->p_no];
      sumy += r_shmem->py[i][j->p_no];
   }
   if(DEBUG & DEBUG_SHMEM){
      printf("found %d values\n", k);
   }
   if(k < 2)
      return 0;

   if(DEBUG & DEBUG_SHMEM)
      printf("old value (%d,%d)\n", j->p_x, j->p_y);
   j->p_x = sumx / k;
   j->p_y = sumy / k;
   if(DEBUG & DEBUG_SHMEM){
      printf("new value (%d,%d)\n", j->p_x, j->p_y);
   }

   if(k > 6)
      return 1;
   else
      return 0;
}
