#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.57)

AC_INIT([librouter],[0.1],[tgrande@pd3.com.br])
AC_CONFIG_SRCDIR([src/typedefs.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.8 foreign dist-bzip2 no-dist-gzip])
AM_MAINTAINER_MODE

dnl for cross compiling
AC_CANONICAL_HOST

AC_ARG_ENABLE(gcc-warnings,
[  --enable-gcc-warnings   turn on lots of GCC warnings (not recommended)],
[case "${enableval}" in
   yes|no) ;;
   *)      AC_MSG_ERROR([bad value ${enableval} for gcc-warnings option]) ;;
 esac],
	      [enableval=no])
if test "${enableval}" = yes; then

MAYBE_WARN="-Wall -Wextra -pedantic \
-Wsign-compare -Werror-implicit-function-declaration \
-Wpointer-arith -Wwrite-strings -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations -Wnested-externs \
-Wpacked -Wswitch-enum -Wmissing-format-attribute \
-Wstrict-aliasing=2 -Winit-self  \
-Wdeclaration-after-statement -Wold-style-definition \
-Wno-missing-field-initializers -Wno-unused-parameter \
 -Wno-long-long -Winline"

#MAYBE_WARN="$MAYBE_WARN -Wunsafe-loop-optimizations -Wno-attributes"
MAYBE_WARN="$MAYBE_WARN -fno-strict-aliasing -fno-common"

MAYBE_WARN="$MAYBE_WARN -Wp,-D_FORTIFY_SOURCE=2"

CFLAGS="$CFLAGS $MAYBE_WARN"
fi

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB(pam, pam_get_data,, exit)
AC_CHECK_LIB(pam_misc, pam_misc_setenv,, exit)
AC_CHECK_LIB(crypto, RSA_public_encrypt,, exit)
AC_CHECK_LIB(netsnmp, snmpv3_store,, exit)
AC_CHECK_LIB(iptc, iptc_init,, exit)

# Checks for header files.
AC_HEADER_RESOLV
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/mount.h sys/param.h sys/socket.h sys/time.h syslog.h termios.h unistd.h utmp.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_BIGENDIAN
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

SHAVE_INIT([m4], [enable]) # dnl Make the output pretty

AC_CONFIG_FILES([m4/shave
		 m4/shave-libtool
		 Makefile
		 src/Makefile
		 src/libnetlink/Makefile])
AC_OUTPUT
