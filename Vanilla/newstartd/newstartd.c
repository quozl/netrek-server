/*
 *
 *   Derived from distribution startd.c and xtrekII.sock.c (as of ~1/14/91).
 *
 *   Installation:  update the Prog and LogFile variables below, and
 *     set the default port appropriately (presently 2592).  Set debug=1
 *     to run this under dbx.  Link this with access.o and subnet.o.
 *
 *   Summary of changes:
 *   - subsumes xtrekII.sock into startd (this calls ntserv directly to
 *       reduce OS overhead)
 *   - some debugger support code added
 *   - uses reaper() to keep process table clean (removes <exiting> procs)
 *
 *   8/16/91 Terence Chang
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "ip.h"
#define MVERS
#include "version.h"
#include "patchlevel.h"

int restart;		/* global flag, set by SIGHUP, cleared by read	*/
int debug = 0;		/* programmers' debugging flag			*/

static int get_connection();
static int read_portfile(char *);
static void deny(char *ip);
static void statistics(int, char *ip);
static void process(int, char *ip);
static void reaper_block();
static void reaper_unblock();
static void reaper(int);
static void hangup(int);
static void putpid(void);

#define SP_BADVERSION 21
struct badversion_spacket {
  char type;
  char why;
  char pad2;
  char pad3;
};

#define MAXPROG 16			/* maximum ports to listen to	*/
#define MAXPROCESSES 8+MAXPLAYER+TESTERS+MAXWAITING
					/* maximum processes to create	*/

static int active;			/* number of processes running	*/

static
struct progrecord {			/* array of data read from file	*/
  int type;				/* SOCK_STREAM, or SOCK_DGRAM	*/
  unsigned short port;			/* port number to listen on	*/
  int sock;				/* socket created on port or -1	*/
  pid_t child;				/* for UDP, active child	*/
  int nargs;				/* number of arguments to prog	*/
  char prog[512];			/* program to start on connect	*/
  char progname[512];			/* more stuff for exec()	*/
  char arg[4][64];			/* arguments for program	*/
  int internal;				/* ports handled without fork()	*/
  int accepts;				/* count of accept() calls	*/
  int denials;				/* count of deny() calls	*/
  int forks;				/* count of fork() calls	*/
  unsigned long addr;			/* address to bind() to		*/
} prog[MAXPROG];

static int num_progs = 0;

int fd;					/* log file file descriptor	*/


/* sigaction() is a POSIX signal function.  On some systems, it is
   incompatible with SysV/BSD signals.  Some SysV signals provide
   sigaction(), others don't.  If you get odd signal behavior, undef
   this at the expense of zombie processes.  -da */

#undef REAPER_HANDLER
/* prevent child zombie processes -da */


#ifdef REAPER_HANDLER

void handle_reaper(void) {
  struct sigaction action;

  action.sa_flags = SA_NOCLDWAIT;
  action.sa_handler = reaper;
  sigemptyset(&(action.sa_mask));
  sigaction(SIGCHLD, &action, NULL);
}

#endif

int main (int argc, char *argv[])
{
  char *portfile = N_PORTS;
  int port_idx = -1;
  int i;
  pid_t pid;
  FILE *file;

  active = 0;
  getpath();
  setpath();

  /* if someone tries to ask for help, give 'em it */
  if (argc == 2 && argv[1][0] == '-') {
    fprintf(stderr, "Usage: %s [start|stop|reload]\n", argv[0]);
    fprintf(stderr, "       start, begins listening for client connections\n");
    fprintf(stderr, "       stop, stop listening for client connections\n");
    fprintf(stderr, "       reload, read the port file after it is changed\n");
    exit (1);
  }

  /* fetch our previous pid if available */
  file = fopen (N_NETREKDPID, "r");
  if (file != NULL) {
    fscanf (file, "%d", &pid);
    fclose (file);
  } else {
    /* only a total lack of the file is acceptable */
    if (errno != ENOENT) {
      perror (N_NETREKDPID);
      exit(1);
    }
  }

  /* check for a request to stop the listener */
  if (argc == 2) {
    if (!strcmp (argv[1], "stop")) {
      if (file != NULL) {
	if (kill (pid, SIGINT) == 0) {
	  remove (N_NETREKDPID);
	  fprintf (stderr, "netrekd: stopped pid %d\n", pid);
	  exit (0);
	}
	fprintf (stderr, "netrekd: cannot stop, pid %d\n, may be already stopped\n", pid);
	perror ("kill");
	exit (1);
      }
      fprintf (stderr, "netrekd: cannot stop, no %s file\n", N_NETREKDPID);
      exit (1);
    }
    if (!strcmp (argv[1], "reload")) {
      if (file != NULL) {
	if (kill (pid, SIGHUP) == 0) {
	  fprintf (stderr, "netrekd: sent SIGHUP to pid %d\n", pid);
	  exit (0);
	}
	fprintf (stderr, "netrekd: cannot reload, pid %d not present\n", pid);
	perror ("kill");
	exit (1);
      }
      fprintf (stderr, "netrekd: cannot reload, no %s file\n", N_NETREKDPID);
      exit (1);
    }
  }

  /* check for duplicate start */
  if (file != NULL) {
    /* there is a pid file, test the pid is valid */
    if (kill (pid, 0) == 0) {
      fprintf (stderr, "netrekd: cannot start, already running as pid %d\n", pid);
      exit (1);
    } else {
      if (errno != ESRCH) {
	fprintf (stderr, "netrekd: failed on test of pid %d existence\n", pid);
	perror ("kill");
	exit (1);
      }
    }

    /* pid was not valid but file was present, so remove it */
    remove (N_NETREKDPID);
  }

  /* allow user to specify a port file to use */
  if (argc == 2) {
    if (strcmp (argv[1], "start")) {
      portfile = argv[1];
    }
  }

  /* allow developer to ask for verbose output */
  if (argc == 3 && !strcmp(argv[2], "debug")) debug++;

  /* wander off to the place where we live */
  if (chdir (LIBDIR) != 0) {
    perror (LIBDIR);
    exit(1);
  }

  /* check file access before forking */
  if (access (portfile, R_OK) != 0) {
    perror (portfile);
    exit(1);
  }

  /* set up handlers for signals */
#ifdef REAPER_HANDLER
  handle_reaper();
#else
  signal(SIGCHLD, reaper);
#endif
  signal(SIGHUP, hangup);
  signal(SIGUSR1, SIG_IGN);

  /* open the connection log file */
  if ((fd = open (LogFile, O_CREAT | O_WRONLY | O_APPEND, 0600)) < 0) {
    perror (LogFile);
  }

  /* fork this as a daemon */
  if (!debug) {
    pid = fork();
    if (pid != 0) {
      if (pid < 0) {
	perror("fork");
	exit(1);
      }
      fprintf (stderr, "netrekd: %s %s.%d started, pid %d,\n"
               "netrekd: logging to %s\n",
               SERVER_NAME, SERVER_VERSION, PATCHLEVEL, pid, LogFile );
      exit(0);
    }

    /* detach from terminal */
    DETACH;
  }

  /* do not propogate the log to forked processes */
  fcntl(fd, F_SETFD, FD_CLOEXEC);

  /* close our standard file descriptors and attach them to the log */
  if (!debug) {
    (void) dup2 (fd, 0);
    (void) dup2 (fd, 1);
    (void) dup2 (fd, 2);
  }

  /* set the standard streams to be line buffered (ANSI C3.159-1989) */
  setvbuf (stderr, NULL, _IOLBF, 0);
  setvbuf (stdout, NULL, _IOLBF, 0);

  /* record our process id */
  putpid ();

  /* run forever */
  for (;;) {

    /* [re-]read the port file */
    num_progs = read_portfile (portfile);
    restart = 0;

    /* run until restart requested */
    while (!restart) {
      pid_t pid;
      struct sockaddr_in addr;

      /* wait for one connection */
      port_idx = get_connection (&addr);
      if (port_idx < 0) continue;
      prog[port_idx].accepts++;

      /* check this client against denied parties */
      if (ip_deny(inet_ntoa(addr.sin_addr))) {
	prog[port_idx].denials++;
	deny (inet_ntoa(addr.sin_addr));
	close (0);
	continue;
      }

      /* check for limit */
      if (active > MAXPROCESSES) {
	fprintf(stderr,
		"netrekd: hit maximum processes, connection closed\n");
	close (0);
	continue;
      }

      /* check for internals */
      if (prog[port_idx].internal) {
	statistics (port_idx, inet_ntoa(addr.sin_addr));
	continue;
      }

      reaper_block();
      /* fork to execute the program specified by .ports file */
      pid = fork ();
      if (pid == -1) {
	perror("fork");
	close (0);
	reaper_unblock();
	continue;
      }

      if (pid == 0) {
	process (port_idx, inet_ntoa(addr.sin_addr));
	/* process() returns only on error */
	exit (0);
      }

      /* record the pid of any UDP clients created */
      if (prog[port_idx].type == SOCK_DGRAM) {
	prog[port_idx].child = pid;
      }

      prog[port_idx].forks++;
      active++;

      if (debug) fprintf (stderr, "active++: %d: pid %d port %d\n", active,
			  pid, prog[port_idx].port);
      reaper_unblock();

      /* main program continues after fork, close socket to client */
      close (0);

    } /* while (!restart) */

    /* SIGHUP restart requested, close all listening sockets */
    fprintf (stderr, "netrekd: restarting on SIGHUP\n");

    for (i = 0; i < num_progs; i++) {
      if (prog[i].sock >= 0) close(prog[i].sock);
    }

    /* between closing the listening sockets and commencing the new ones
       there is a time during which incoming client connections are refused */
  }
}

static int get_connection(struct sockaddr_in *peer)
{
  struct sockaddr_in addr;
  socklen_t addrlen;
  fd_set accept_fds;
  int i, st, newsock;

  int foo = 1;

  /* stage one; for each port not yet open, set up listening socket */

  /* check all ports */
  for (i = 0; i < num_progs; i++) {
    sock = prog[i].sock;
    if (sock < 0) {

      fprintf(stderr, "netrekd: port %d, %s, ", prog[i].port, prog[i].prog);
      if ((sock = socket(AF_INET, prog[i].type, 0)) < 0) {
	perror("socket");
	sleep(1);
	return -1;
      }

      /* we can't cope with having socket zero for a listening socket */
      if (sock == 0) {
	if (debug) fprintf(stderr, "gack: don't want socket zero\n");
	if ((sock = socket(AF_INET, prog[i].type, 0)) < 0) {
	  perror("socket");
	  sleep(1);
	  close(0);
	  return -1;
	}
	close(0);
      }

      /* set the listening socket to close on exec so that children
	 don't hold open the file descriptor, thus preventing us from
	 restarting netrekd. */

      if (fcntl(sock, F_SETFD, FD_CLOEXEC) < 0) {
	perror("fcntl F_SETFD FD_CLOEXEC");
	close(sock);
	sleep(1);
	return -1;
      }

      /* set the listening socket to allow re-use of the address (port)
	 so that we don't have to pay the CLOSE_WAIT penalty if we need
	 to close and re-open the socket on restart. */

      if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		     (char *) &foo, sizeof(int)) < 0) {
	perror("setsockopt SOL_SOCKET SO_REUSEADDR");
	close(sock);
	sleep(1);
	return -1;
      }

      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = prog[i].addr;
      addr.sin_port = htons(prog[i].port);

      if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
	perror("bind");
	close(sock);
	sleep(1);
	continue;
      }

      if (prog[i].type == SOCK_STREAM) {
	if (listen(sock, 1) < 0) {
	  perror("listen");
	  close(sock);
	  sleep(1);
	  continue;
	}
      }

      prog[i].sock = sock;
      fprintf(stderr, "listening fd %d, to do \"%s\" %s %s %s %s\n",
	      prog[i].sock, prog[i].progname, prog[i].arg[0],
	      prog[i].arg[1], prog[i].arg[2], prog[i].arg[3]);
      fflush(stderr);
    }
  }

  /* stage two; wait for a connection on the listening sockets */

  for (;;) {				/* wait for a connection */
    st = 0;

    FD_ZERO(&accept_fds);		/* clear the file descriptor mask */
    for (i = 0; i < num_progs; i++) {	/* set bits in mask for each sock */
      sock = prog[i].sock;
      if (sock < 0) continue;
      if (prog[i].type == SOCK_STREAM)
	FD_SET(sock, &accept_fds);
      if (prog[i].type == SOCK_DGRAM)
	if (prog[i].child == 0)
	  FD_SET(sock, &accept_fds);
      st++;
    }

    /* paranoia, it could be possible that we have no sockets ready */
    if (st == 0) {
      fprintf(stderr, "netrekd: paranoia, no sockets to listen for\n");
      sleep(1);
      return -1;
    }

    /* block until select() indicates accept() will complete */
    st = select(32, &accept_fds, 0, 0, 0);
    if (st > 0) break;
    if (restart) return -1;
    if (errno == EINTR) continue;
    perror("select");
    sleep(1);
  }

  /* find one socket in mask that shows activity */
  for (i = 0; i < num_progs; i++) {
    sock = prog[i].sock;
    if (sock < 0) continue;
    if (FD_ISSET(sock, &accept_fds)) break;
  }

  /* none of the sockets showed activity? erk */
  if (i >= num_progs) return -1;

  if (prog[i].type == SOCK_STREAM) {
    /* accept stream connections as new socket */
    for (;;) {
      addrlen = sizeof(addr);
      newsock = accept(sock, (struct sockaddr *) peer, &addrlen);
      if (newsock >= 0) break;
      if (errno == EINTR) continue;
      fprintf(stderr, "netrekd: port %d, %s, ", prog[i].port, prog[i].prog);
      perror("accept");
      sleep(1);
      return -1;
    }
    if (newsock != 0) {
      if (dup2(newsock, 0) == -1) {
	perror("dup2");
      }
      close(newsock);
    }
  } else {
    /* datagrams will be read by child on duplicated socket */
    dup2(sock, 0);
  }
  return i;
}

static void deny(char *ip)
{
  struct badversion_spacket packet;

  /* if log is open, write an entry to it */
  if (fd != -1) {
    time_t curtime;
    char logname[BUFSIZ];

    time (&curtime);
    sprintf (logname, "Denied %-32.32s %s",
	     ip,
	     ctime(&curtime));
    write (fd, logname, strlen(logname));
  }

  /* issue a bad version packet to the client */
  memset(&packet, 0, sizeof(struct badversion_spacket));
  packet.type = SP_BADVERSION;
#define BADVERSION_DENIED 1 /* access denied by netrekd */
  packet.why = BADVERSION_DENIED;
  write (0, (char *) &packet, sizeof(packet));

  sleep (2);
}

static void statistics (int port_idx, char *ip)
{
  FILE *client;
  int i;

  if (fork() == 0) {
    static struct linger linger = {1, 500};
    setsockopt (0, SOL_SOCKET, SO_LINGER, (char *) &linger, sizeof (linger));
    client = fdopen (0, "w");
    if (client != NULL) {
      if (!strcmp (ip, "127.0.0.1")) {
	fprintf (client, "port\taccepts\tdenials\tforks\n" );
	for (i=0; i<num_progs; i++) {
	  fprintf (client,
		   "%d\t%d\t%d\t%d\n",
		   prog[i].port,
		   prog[i].accepts,
		   prog[i].denials,
		   prog[i].forks );
	}
      }
      fprintf(client, "%s\n%s.%d\n", SERVER_NAME, SERVER_VERSION, PATCHLEVEL);
      fflush (client);
      if (fclose (client) < 0) perror("fclose");
    } else {
      close (0);
    }
    exit (1);
  } else {
    close (0);
  }

  /* get rid of compiler warning */
  if (port_idx) return;
}

static void process (int port_idx, char *ip)
{
  struct progrecord *pr;

  /* note: we are a clone */

  /* if log is open, write an entry to it, and close it */
  if (fd != -1) {
    time_t curtime;
    char logname[BUFSIZ];

    time (&curtime);
    sprintf (logname, "       %-32.32s %s",
	     ip,
	     ctime(&curtime));
    write (fd, logname, strlen(logname));
    close (fd);
  }

  /* execute the program required */
  pr = &(prog[port_idx]);
  switch (pr->nargs) {
  case 0:
    execl (pr->prog, pr->progname, ip, (char *) NULL);
    break;
  case 1:
    execl (pr->prog, pr->progname, pr->arg[0], ip, (char *) NULL);
    break;
  case 2:
    execl (pr->prog, pr->progname, pr->arg[0], pr->arg[1],
	   ip, (char *) NULL);
    break;
  case 3:
    execl (pr->prog, pr->progname, pr->arg[0], pr->arg[1],
	   pr->arg[2], ip, (char *) NULL);
    break;
  case 4:
    execl (pr->prog, pr->progname, pr->arg[0], pr->arg[1],
	   pr->arg[2], pr->arg[3], ip, (char *) NULL);
    break;
  default: ;
  }
  fprintf (stderr, "Error in execl! -- %s\n", pr->prog);
  perror ("execl");
  close (0);
  exit (1);
}

/* set flag requesting restart, select() will get EINTR */
static void hangup (int sig)
{
  restart++;
  signal(SIGHUP, hangup);

  /* get rid of compiler warning */
  if (sig) return;
}

static void reaper_block()
{
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
    perror("sigprocmask: SIG_BLOCK SIGCHLD");
  }
}

static void reaper_unblock()
{
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
    perror("sigprocmask: SIG_UNBLOCK SIGCHLD");
  }
}

/* accept termination of any child processes */
static void reaper (int sig)
{
  int stat;
  pid_t pid;

  for (;;) {
    pid = wait3(&stat, WNOHANG, 0);
    if (pid > 0) {
      active--;
      if (debug) fprintf (stderr, "active--: %d: pid %d terminated\n",
			  active, pid);
      int i;
      for (i=0; i<num_progs; i++) {
	if (prog[i].child == pid) {
	  if (debug) fprintf(stderr, "reaper: prog[%d].child was %d, clearing\n", i, prog[i].child);
	  prog[i].child = 0;
	}
      }
    } else {
      break;
    }
  }

#ifdef REAPER_HANDLER
  handle_reaper();
#else
  signal(SIGCHLD, reaper);
#endif

  if (sig) return;
}

/* write process id to file to assist with automatic signalling */
static void putpid(void)
{
  FILE *file;

  file = fopen(N_NETREKDPID, "w");
  if (file == NULL) return;
  fprintf (file, "%d\n", (int) getpid());
  fclose (file);
}

static int read_portfile (char *portfile)
{
  FILE *fi;
  char buf[BUFSIZ], addrbuf[BUFSIZ], *port;
  int i = 0, n;
  struct stat sbuf;

  fi = fopen (portfile, "r");
  if (fi) {
    while (fgets (buf, BUFSIZ, fi)) {
      if (buf[0] == '#')
	continue;
      if ((n = sscanf (buf, "%s %s \"%[^\"]\" %s %s %s %s",
		       addrbuf,
		       prog[i].prog,
		       prog[i].progname,
		       prog[i].arg[0],
		       prog[i].arg[1],
		       prog[i].arg[2],
		       prog[i].arg[3])) >= 3) {
	/* port, host:port, portu, host:portu */
	if (!(port = strchr(addrbuf, ':'))) {
	  prog[i].addr = INADDR_ANY;
	  port = addrbuf;
	} else {
	  *port++ = '\0';
	  prog[i].addr = inet_addr(addrbuf);
	}
	prog[i].port = atoi(port);
	prog[i].type = strchr(port, 'u') == NULL ? SOCK_STREAM : SOCK_DGRAM;
	prog[i].nargs = n-3;
	prog[i].sock = -1;
	prog[i].internal = (!strcmp (prog[i].prog, "special"));
	prog[i].accepts = 0;
	prog[i].denials = 0;
	prog[i].forks = 0;

	/* check normal entries to make sure program mode is ok */
	if ((!prog[i].internal) &&
	    (stat (prog[i].prog, &sbuf) < 0 || !(sbuf.st_mode & S_IRWXU))) {
	  fprintf (stderr, "\"%s\" not accessible or not executable.\n",
		   prog[i].prog);
	  fflush (stderr);
	} else {
	  i++;
	}
      }
    }
    fclose (fi);
  } else {
    perror (portfile);
  }

  /* if the .ports file is empty, at least open the standard port */
  if (!i) {
    strcpy (prog[0].prog, Prog);
    strcpy (prog[0].progname, Prog);
    prog[0].port = PORT;
    prog[0].nargs = 0;
    prog[0].sock = -1;
    prog[0].accepts = 0;
    prog[0].denials = 0;
    prog[0].forks = 0;
    i = 1;
  }

  return i;
}
