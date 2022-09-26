#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libethercat], 
	m4_esyscmd_s([cat project.properties | grep VERSION    | cut -d'=' -f2 | xargs printf "%s"]),
	m4_esyscmd_s([cat project.properties | grep MAINTAINER | cut -d'=' -f2 | xargs printf "%s"]))

AC_CANONICAL_TARGET

# Automake initialization
AM_INIT_AUTOMAKE([-Wall -Werror foreign silent-rules])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
DX_INIT_DOXYGEN($PACKAGE_NAME, doxygen.cfg)
AX_PREFIX_CONFIG_H(include/libethercat/config.h) # prep mylib/_config.h from it..

# Checks for programs.
AC_PROG_CC
m4_ifdef([AM_PROG_CC_C_O], [AM_PROG_CC_C_O])
AC_PROG_CC_STDC
AC_PROG_LIBTOOL
m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG])
m4_ifdef([PKG_INSTALLDIR], [PKG_INSTALLDIR], [pkgconfigdir='${libdir}/pkgconfig' AC_SUBST([pkgconfigdir])])
        
case $target_os in
    linux*)
        BUILD_POSIX=true
        AC_DEFINE([BUILD_POSIX], [true], [Use POSIX build on linux])
        ;;
    vxworks*)
        BUILD_VXWORKS=true
        AC_DEFINE([BUILD_VXWORKS], [true], [Use VxWorks build])
        ;;
    pikeos*)
        BUILD_PIKEOS=true
        AC_DEFINE([BUILD_PIKEOS], [true], [Use PikeOS build])
        ;;
esac

AM_CONDITIONAL([BUILD_POSIX], [ test x$BUILD_POSIX = xtrue]) 
AM_CONDITIONAL([BUILD_VXWORKS], [ test x$BUILD_VXWORKS = xtrue]) 
AM_CONDITIONAL([BUILD_PIKEOS], [ test x$BUILD_PIKEOS = xtrue]) 

# Checks for libraries.
PKG_CHECK_MODULES([LIBOSAL], [libosal], [], [AC_MSG_ERROR([libosal not found])])

# Checks for header files.
AC_HEADER_ASSERT
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
AC_CHECK_FUNCS([clock_gettime memset socket strdup strerror strndup]) #pthread_setaffinity_np])

AC_CONFIG_FILES([Makefile src/Makefile tools/ethercatdiag/Makefile tools/eepromtool/Makefile libethercat.pc])
AC_OUTPUT

