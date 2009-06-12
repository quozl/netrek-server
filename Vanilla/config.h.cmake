#ifndef NETREK_SERVER_CONFIG_H
#define NETREK_SERVER_CONFIG_H

#cmakedefine VERSION "@VERSION@"
#cmakedefine BINDIR "@BIN_INSTALL_DIR"
#cmakedefine SBINDIR "@SBIN_INSTALL_DIR"
#cmakedefine SYSCONFDIR "@SYSCONF_INSTALL_DIR"
#cmakedefine LOCALSTATEDIR "@LOCALSTATE_INSTALL_DIR"
#cmakedefine MANDIR "@MAN_INSTALL_DIR"
#cmakedefine DATADIR "@DATA_INSTALL_DIR"
#cmakedefine LIBDIR "@LIB_INSTALL_DIR"

#cmakedefine HAVE_FCNTL_H
#cmakedefine HAVE_SYS_TIME_H
#cmakedefine HAVE_SYS_SELECT_H
#cmakedefine HAVE_UNISTD_H
#cmakedefine HAVE_SYS_UTSNAME_H
#cmakedefine HAVE_SYS_TIMEB_H
#cmakedefine HAVE_SYS_SOCKET_H
#cmakedefine HAVE_SYS_PARAM_H
#cmakedefine HAVE_NETINET_IN_H
#cmakedefine HAVE_NETINET_IP_H
#cmakedefine HAVE_ARPA_INET_H
#cmakedefine HAVE_NETDB_H
#cmakedefine HAVE_TERMIOS_H
#cmakedefine HAVE_SYS_TYPES_H
#cmakedefine HAVE_SYS_WAIT_H
#cmakedefine HAVE_SYS_IOCTL_H
#cmakedefine HAVE_STDINT_H
#cmakedefine HAVE_SYS_FILE_H
#cmakedefine HAVE_POLL_H
#cmakedefine HAVE_SYS_POLL_H
#cmakedefine HAVE_SYS_STROPTS_H
#cmakedefine HAVE_SYS_STAT_H
#cmakedefine HAVE_PWD_H
#cmakedefine HAVE_GRP_H
#cmakedefine HAVE_DIR_H
#cmakedefine HAVE_DIRENT_H
#cmakedefine HAVE_NDIR_H
#cmakedefine HAVE_SYS_DIR_H
#cmakedefine HAVE_SYS_NDIR_H
#cmakedefine HAVE_DIRECT_H
#cmakedefine HAVE_SYS_MMAN_H
#cmakedefine HAVE_SYS_EVENT_H
#cmakedefine HAVE_SYS_EPOLL_H
#cmakedefine HAVE_SYS_RESOURCE_H
#cmakedefine HAVE_SQLITE3_H
#cmakedefine HAVE_PCAP_H
#cmakedefine HAVE_WINDOWS_H
#cmakedefine HAVE_WINSOCK2_H
#cmakedefine HAVE_PROCESS_H
#cmakedefine HAVE_STDLIB_H
#cmakedefine HAVE_STRING_H
#cmakedefine HAVE_SYS_FCNTL_H
#cmakedefine HAVE_SYS_SELECT_H
#cmakedefine HAVE_SYS_WAIT_H
#cmakedefine HAVE_SYS_FILIO_H
#cmakedefine HAVE_ERRNO_H
#cmakedefine HAVE_CRYPT_H

#cmakedefine SIZEOF_UNSIGNED_CHAR "SIZEOF_UNSIGNED_CHAR}
#cmakedefine SIZEOF_UNSIGNED_SHORT ${SIZEOF_UNSIGNED_SHORT}
#cmakedefine SIZEOF_UNSIGNED_INT ${SIZEOF_UNSIGNED_INT}
#cmakedefine SIZEOF_UNSIGNED_LONG ${SIZEOF_UNSIGNED_LONG}
#cmakedefine SIZEOF_UNSIGNED_LONG_LONG ${SIZEOF_UNSIGNED_LONG_LONG}
#cmakedefine SIZEOF_SIGNED_CHAR ${SIZEOF_SIGNED_CHAR}
#cmakedefine SIZEOF_SIGNED_SHORT ${SIZEOF_SIGNED_SHORT}
#cmakedefine SIZEOF_SIGNED_INT ${SIZEOF_SIGNED_INT}
#cmakedefine SIZEOF_SIGNED_LONG ${SIZEOF_SIGNED_LONG}
#cmakedefine SIZEOF_SIGNED_LONG_LONG ${SIZEOF_SIGNED_LONG_LONG}

#cmakedefine HAVE_MMAP
#cmakedefine HAVE_GETHOSTNAME
#cmakedefine HAVE_GETTIMEOFDAY
#cmakedefine HAVE_SELECT
#cmakedefine HAVE_SOCKET
#cmakedefine HAVE_STRDUP
#cmakedefine HAVE_STRTOUL
#cmakedefine HAVE_INET_ATON
#cmakedefine HAVE_INET_NTOA
#cmakedefine HAVE_UNAME
#cmakedefine HAVE_RECV
#cmakedefine HAVE_SEND
#cmakedefine HAVE_RECVFROM
#cmakedefine HAVE_SENDTO
#cmakedefine HAVE_UNAME
#cmakedefine HAVE_FORK
#cmakedefine HAVE_GETPID
#cmakedefine HAVE_SIGACTION
#cmakedefine HAVE_SIGPROCMASK
#cmakedefine HAVE_SIGADDSET
#cmakedefine HAVE_SETPGID
#cmakedefine HAVE_FTIME
#cmakedefine HAVE_STRCASECMP
#cmakedefine HAVE_STRNCASECMP
#cmakedefine HAVE_STRICMP
#cmakedefine HAVE_STRNICMP
#cmakedefine HAVE_STRNLEN
#cmakedefine HAVE_CHDIR
#cmakedefine HAVE_DIFFTIME
#cmakedefine HAVE_STRCHR
#cmakedefine HAVE_STRRCHR
#cmakedefine HAVE_INDEX
#cmakedefine HAVE_RINDEX
#cmakedefine HAVE_WAIT
#cmakedefine HAVE_WAITPID
#cmakedefine HAVE_PIPE
#cmakedefine HAVE_GETENV
#cmakedefine HAVE_IOCTL
#cmakedefine HAVE_SETSID
#cmakedefine HAVE_POLL
#cmakedefine HAVE_GETHOSTBYNAME
#cmakedefine HAVE_GETSERVBYNAME
#cmakedefine HAVE_GETLOGIN
#cmakedefine HAVE_GETPWNAME
#cmakedefine HAVE_GETGRNAM
#cmakedefine HAVE_GETUID
#cmakedefine HAVE_GETGID
#cmakedefine HAVE_SETUID
#cmakedefine HAVE_MKDIR
#cmakedefine HAVE__MKDIR
#cmakedefine MKDIR_TAKES_ONE_ARG 
#cmakedefine HAVE_STRSEP
#cmakedefine HAVE_GETOPT
#cmakedefine HAVE_KQUEUE
#cmakedefine HAVE_SETITIMER
#cmakedefine HAVE_EPOLL_CREATE
#cmakedefine HAVE_GETRLIMIT
#cmakedefine HAVE_VSNPRINTF
#cmakedefine HAVE__VSNPRINTF
#cmakedefine HAVE_SNPRINTF
#cmakedefine HAVE__SNPRINTF
#cmakedefine HAVE_SETPGRP
#cmakedefine HAVE_USLEEP
#cmakedefine HAVE_WAIT3
#cmakedefine HAVE_STRNCPY
#cmakedefine HAVE_SIGNAL
#cmakedefine HAVE_PAUSE
#cmakedefine HAVE_SIGSETMASK
#cmakedefine HAVE_MEMCMP
#cmakedefine HAVE_MEMCPY
#cmakedefine HAVE_MEMSET
#cmakedefine HAVE_STRCHR
#cmakedefine HAVE_RANDOM
#cmakedefine HAVE_SRANDOM
#cmakedefine HAVE_CRYPT


// Platform dependent programs
#cmakedefine UPTIME_EXECUTABLE "${uptime_EXECUTABLE}"
#cmakedefine XBIFF_EXECUTABLE "${xbiff_EXECUTABLE}"

// Just plain nasty, isn't there a cleaner way to detect 32bit vs 64bit (taken from the old config.h.in)
#if (SIZEOF_LONG == 8)
#define _64BIT
#define LONG int
#define U_LONG u_int
#else
#define LONG long
#define U_LONG u_long
#endif

// Do we really care about sequent machines?
#if defined (__sequent__)
#define WAIT_TYPE    union wait
#define WSTOPSIG(foo)   0
#else
#define WAIT_TYPE int
#endif

// Crufty? shmget(2) int shmget(key_t key, size_t size, int shmflg); Still need sizeof?
#ifdef linux
#define SHMFLAG         sizeof(struct memory)
#else
#define SHMFLAG         0
#endif

// Eeeeewwww! Seriously? Taken from config.h.in
// We don't care about SEQUENT anymore right? Removed PERMS macro
// osx and linux both don't need the ioctl that's in config.h.in, removed it
#define DETACH {\
    int i;                                                      \
    if ((i = open("/dev/tty", O_RDWR, 0)) >=0) {                 \
        (void) close (i);                                       \
    }                                                           \
    setpgrp();                                                  \
    }

#endif
