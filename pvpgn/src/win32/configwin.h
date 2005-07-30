/* ../include/config.h.win32  Generated by hand from ../include/config.h.in.  */
/* ../include/config.h.in.  Generated automatically from configure.in by autoheader.  */

#ifdef __BORLANDC__
# define __FUNCTION__ __FUNC__
#endif

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if the closedir function returns void instead of int.  */
/* #undef CLOSEDIR_VOID */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the _vsnprintf function.  */
#define HAVE__VSNPRINTF 1

/* Define if you have the _snprintf function.  */
#define HAVE__SNPRINTF 1

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if the `setpgrp' function takes no argument.  */
/* #undef SETPGRP_VOID */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
/* #undef TIME_WITH_SYS_TIME */

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* The number of bytes in a unsigned char.  */
#define SIZEOF_UNSIGNED_CHAR 1

/* The number of bytes in a signed char.  */
#define SIZEOF_SIGNED_CHAR 1

/* The number of bytes in a unsigned int.  */
#define SIZEOF_UNSIGNED_INT 4

/* The number of bytes in a signed int.  */
#define SIZEOF_SIGNED_INT 4

/* The number of bytes in a unsigned long.  */
#define SIZEOF_UNSIGNED_LONG 4

/* The number of bytes in a signed long.  */
#define SIZEOF_SIGNED_LONG 4

/* The number of bytes in a unsigned long long.  */
/* Borland doesn't do ull, but VC++ does */
#ifdef __BORLANDC__
# define SIZEOF_UNSIGNED_LONG_LONG 0
#else
# define SIZEOF_UNSIGNED_LONG_LONG 8
#endif

/* The number of bytes in a signed long long.  */
/* Borland doesn't do ll, but VC++ does */
#ifdef __BORLANDC__
# define SIZEOF_SIGNED_LONG_LONG 0
#else
# define SIZEOF_SIGNED_LONG_LONG 8
#endif

/* The number of bytes in a unsigned short.  */
#define SIZEOF_UNSIGNED_SHORT 2

/* The number of bytes in a signed short.  */
#define SIZEOF_SIGNED_SHORT 2

/* Define if you have the _mkdir function.  */
#define HAVE__MKDIR 1

/* Define if you have the bcopy function.  */
/* #undef HAVE_BCOPY */

/* Define if you have the chdir function.  */
/* #undef HAVE_CHDIR */

/* Define if you have the difftime function.  */
#define HAVE_DIFFTIME 1

/* Define if you have the fork function.  */
/* #undef HAVE_FORK */

/* Define if you have the ftime function.  */
#define HAVE_FTIME 1

/* Define if you have the getenv function.  */
#define HAVE_GETENV 1

/* Define if you have the getgid function.  */
/* #undef HAVE_GETGID */

/* Define if you have the getgrnam function.  */
/* #undef HAVE_GETGRNAM */

/* Define if you have the gethostbyname function.  */
#define HAVE_GETHOSTBYNAME 1

/* Define if you have the gethostname function.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have the getlogin function.  */
/* #undef HAVE_GETLOGIN */

/* Define if you have the getpid function.  */
/* #undef HAVE_GETPID */

/* Define if you have the getpwnam function.  */
/* #undef HAVE_GETPWNAM */

/* Define if you have the getservbyname function.  */
#define HAVE_GETSERVBYNAME 1

/* Define if you have the gettimeofday function.  */
/* #undef HAVE_GETTIMEOFDAY */

/* Define if you have the getuid function.  */
/* #undef HAVE_GETUID */

/* Define if you have the index function.  */
/* #undef HAVE_INDEX */

/* Define if you have the inet_aton function.  */
/* #undef HAVE_INET_ATON */

/* Define if you have the inet_ntoa function.  */
#define HAVE_INET_NTOA 1

/* Define if you have the ioctl function.  */
/* #undef HAVE_IOCTL */

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the memset function.  */
#define HAVE_MEMSET 1

/* Define if you have the mkdir function.  */
#define HAVE_MKDIR

/* Define if you have the mktime function.  */
#define HAVE_MKTIME 1

/* Define if you have the pipe function.  */
/* #undef HAVE_PIPE */

/* Define if you have the poll function.  */
/* #undef HAVE_POLL */

/* Define if you have the pow function.  */
#define HAVE_POW

/* Define if you have the recv function.  */
#define HAVE_RECV 1

/* Define if you have the recvfrom function.  */
#define HAVE_RECVFROM 1

/* Define if you have the rindex function.  */
/* #undef HAVE_RINDEX */

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the send function.  */
#define HAVE_SEND 1

/* Define if you have the sendto function.  */
#define HAVE_SENDTO 1

/* Define if you have the setgid function.  */
/* #undef HAVE_SETGID */

/* Define if you have the setpgid function.  */
/* #undef HAVE_SETPGID */

/* Define if you have the setpgrp function.  */
/* #undef HAVE_SETPGRP */

/* Define if you have the setsid function.  */
/* #undef HAVE_SETSID */

/* Define if you have the setuid function.  */
/* #undef HAVE_SETUID */

/* Define if you have the sigaction function.  */
/* #undef HAVE_SIGACTION */

/* Define if you have the sigaddset function.  */
/* #undef HAVE_SIGADDSET */

/* Define if you have the sigprocmask function.  */
/* #undef HAVE_SIGPROCMASK */

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the strcasecmp function.  */
/* #undef HAVE_STRCASECMP */

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strsep function.  */
/* #undef HAVE_STRSEP */

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the stricmp function.  */
#define HAVE_STRICMP 1

/* Define if you have the strncasecmp function.  */
/* #undef HAVE_STRNCASECMP */

/* Define if you have the strnicmp function.  */
#define HAVE_STRNICMP 1

/* Define if you have the strrchr function.  */
#define HAVE_STRRCHR 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the uname function.  */
/* #undef HAVE_UNAME */

/* Define if you have the wait function.  */
/* #undef HAVE_WAIT */

/* Define if you have the waitpid function.  */
/* #undef HAVE_WAITPID */

/* Define if you have the <arpa/inet.h> header file.  */
/* #undef HAVE_ARPA_INET_H */

#ifdef __BORLANDC__
/* Define if you have the <dir.h> header file.  */
# define HAVE_DIR_H
#else
/* Define if you have the <direct.h> header file.  */
#define HAVE_DIRECT_H 1
#endif

/* Define if you have the <dirent.h> header file.  */
/* #undef HAVE_DIRENT_H */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <grp.h> header file.  */
/* #undef HAVE_GRP_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <malloc.h> header file.  */
#define HAVE_MALLOC_H 1

/* Define if you have the <memory.h> header file.  */
/* #undef HAVE_MEMORY_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <netdb.h> header file.  */
/* #undef HAVE_NETDB_H */

/* Define if you have the <netinet/in.h> header file.  */
/* #undef HAVE_NETINET_IN_H */

/* Define if you have the <poll.h> header file.  */
/* #undef HAVE_POLL_H */

/* Define if you have the <pwd.h> header file.  */
/* #undef HAVE_PWD_H */

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

/* Define if you have the <stddef.h> header file.  */
#define HAVE_STDDEF_H 1

/* Define if you have the <stdint.h> header file.  */
/* #undef HAVE_STDINT_H */

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
/* #undef HAVE_STRINGS_H */

/* Define if you have the <stropts.h> header file.  */
/* #undef HAVE_STROPTS_H */

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/file.h> header file.  */
/* #undef HAVE_SYS_FILE_H */

/* Define if you have the <sys/ioctl.h> header file.  */
/* #undef HAVE_SYS_IOCTL_H */

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/param.h> header file.  */
/* #undef HAVE_SYS_PARAM_H */

/* Define if you have the <sys/poll.h> header file.  */
/* #undef HAVE_SYS_POLL_H */

/* Define if you have the <sys/select.h> header file.  */
/* #undef HAVE_SYS_SELECT_H */

/* Define if you have the <sys/socket.h> header file.  */
/* #undef HAVE_SYS_SOCKET_H */

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/stropts.h> header file.  */
/* #undef HAVE_SYS_STROPTS_H */

/* Define if you have the <sys/time.h> header file.  */
/* #undef HAVE_SYS_TIME_H */

/* Define if you have the <sys/timeb.h> header file.  */
#define HAVE_SYS_TIMEB_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/utsname.h> header file.  */
/* #undef HAVE_SYS_UTSNAME_H */

/* Define if you have the <sys/wait.h> header file.  */
/* #undef HAVE_SYS_WAIT_H */

/* Define if you have the <termios.h> header file.  */
/* #undef HAVE_TERMIOS_H */

/* Define if mkdir takes only one argument. */
#define MKDIR_TAKES_ONE_ARG 1

/* Define if you have the <varargs.h> header file.  */
#define HAVE_VARARGS_H 1

/* Define if you have the <unistd.h> header file.  */
/* #undef HAVE_UNISTD_H */

/* Define if you support inline */
#define inline __inline

/* Define if you support assertions */
#define HAVE_ASSERT_H 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the <time.h> header file.  */
#define HAVE_TIME_H 1

