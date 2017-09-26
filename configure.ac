#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libethercat], 
	m4_esyscmd_s([cat project.properties | grep VERSION    | cut -d'=' -f2 | xargs printf "%s"]),
	m4_esyscmd_s([cat project.properties | grep MAINTAINER | cut -d'=' -f2 | xargs printf "%s"]))

# Automake initialization
AM_INIT_AUTOMAKE([-Wall -Werror foreign silent-rules])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
DX_INIT_DOXYGEN($PACKAGE_NAME, doxygen.cfg)

# Checks for programs.
AC_PROG_CC
m4_ifdef([AM_PROG_CC_C_O], [AM_PROG_CC_C_O])
AC_PROG_CC_STDC
AC_PROG_LIBTOOL
m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG])
m4_ifdef([PKG_INSTALLDIR], [PKG_INSTALLDIR], [pkgconfigdir='${libdir}/pkgconfig' AC_SUBST([pkgconfigdir])])

# Checks for pthread support
ACX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

# Checks for libraries.
AC_CHECK_LIB([rt], [sem_init])

# indead a good option, checks for processor features like avx, but then we get sigill on older machines
# AX_GCC_ARCHFLAG([PORTABLE?], [], [])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h net/if.h net/bpf.h fcntl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime memset socket strdup strerror strndup pthread_setaffinity_np])

AC_CONFIG_FILES([Makefile src/Makefile tools/ethercatdiag/Makefile tools/eepromtool/Makefile libethercat.pc])
AC_OUTPUT

