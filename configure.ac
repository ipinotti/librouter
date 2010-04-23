#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([libconfig], [0.1], [tgrande@pd3.com.br])
AC_CONFIG_SRCDIR([src/typedefs.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Check endianess
AC_C_BIGENDIAN

# Checks for libraries.

# Checks for header files.
AC_HEADER_RESOLV
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/mount.h sys/param.h sys/socket.h sys/time.h syslog.h termios.h unistd.h utmp.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_CHECK_FUNCS([bzero ftruncate getpagesize inet_ntoa localtime_r memmove memset munmap socket stime strcasecmp strchr strdup strerror strncasecmp strrchr strstr strtol tzset])

AM_PROG_LIBTOOL

SHAVE_INIT([m4], [enable]) # dnl Make the output pretty

AC_CONFIG_FILES([m4/shave
		 m4/shave-libtool
		 Makefile
		 src/Makefile])
AC_OUTPUT
