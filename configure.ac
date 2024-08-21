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


AC_ARG_ENABLE([debug],
              AS_HELP_STRING([--enable-debug], [Enable debug messages.]), 
              AC_DEFINE_UNQUOTED([DEBUG], [1], [Enable debug meassage.]), [])

AC_ARG_WITH([max-slaves],
              AS_HELP_STRING([--with-max-slaves=MAX_SLAVES], [Set maximum number of slaves supported.]), 
              AC_DEFINE_UNQUOTED([MAX_SLAVES], [${withval}], [Maximum number of slaves supported.]), [])
AC_ARG_WITH([max-groups],
              AS_HELP_STRING([--with-max-groups=MAX_GROUPS], [Set maximum number of groups supported.]), 
              AC_DEFINE_UNQUOTED([MAX_GROUPS], [${withval}], [Maximum number of groups supported.]), [])
AC_ARG_WITH([max-pdlen],
              AS_HELP_STRING([--with-max-pdlen=MAX_PDLEN], [Set maximum number of pdlen supported.]), 
              AC_DEFINE_UNQUOTED([MAX_PDLEN], [${withval}], [Maximum number of pdlen supported.]), [])
AC_ARG_WITH([max-mbx-entries],
              AS_HELP_STRING([--with-max-mbx-entries=MAX_MBX_ENTRIES], [Set maximum number of mbx-entries supported.]), 
              AC_DEFINE_UNQUOTED([MAX_MBX_ENTRIES], [${withval}], [Maximum number of mbx-entries supported.]), [])
AC_ARG_WITH([max-init-cmd-data],
              AS_HELP_STRING([--with-max-init-cmd-data=MAX_INIT_CMD_DATA], [Set maximum number of init-cmd-data supported.]), 
              AC_DEFINE_UNQUOTED([MAX_INIT_CMD_DATA], [${withval}], [Maximum number of init-cmd-data supported.]), [])
AC_ARG_WITH([max-slave-fmmu],
              AS_HELP_STRING([--with-max-slave-fmmu=MAX_SLAVE_FMMU], [Set maximum number of slave-fmmu supported.]), 
              AC_DEFINE_UNQUOTED([MAX_SLAVE_FMMU], [${withval}], [Maximum number of slave-fmmu supported.]), [])
AC_ARG_WITH([max-slave-sm],
              AS_HELP_STRING([--with-max-slave-sm=MAX_SLAVE_SM], [Set maximum number of slave-sm supported.]), 
              AC_DEFINE_UNQUOTED([MAX_SLAVE_SM], [${withval}], [Maximum number of slave-sm supported.]), [])
AC_ARG_WITH([max-datagrams],
              AS_HELP_STRING([--with-max-datagrams=MAX_DATAGRAMS], [Set maximum number of datagrams supported.]), 
              AC_DEFINE_UNQUOTED([MAX_DATAGRAMS], [${withval}], [Maximum number of datagrams supported.]), [])
AC_ARG_WITH([max-eeprom-cat-sm],
              AS_HELP_STRING([--with-max-eeprom-cat-sm=MAX_EEPROM_CAT_SM], [Set maximum number of eeprom-cat-sm supported.]), 
              AC_DEFINE_UNQUOTED([MAX_EEPROM_CAT_SM], [${withval}], [Maximum number of eeprom-cat-sm supported.]), [])
AC_ARG_WITH([max-eeprom-cat-fmmu],
              AS_HELP_STRING([--with-max-eeprom-cat-fmmu=MAX_EEPROM_CAT_FMMU], [Set maximum number of eeprom-cat-fmmu supported.]), 
              AC_DEFINE_UNQUOTED([MAX_EEPROM_CAT_FMMU], [${withval}], [Maximum number of eeprom-cat-fmmu supported.]), [])
AC_ARG_WITH([max-eeprom-cat-pdo],
              AS_HELP_STRING([--with-max-eeprom-cat-pdo=MAX_EEPROM_CAT_PDO], [Set maximum number of eeprom-cat-pdo supported.]), 
              AC_DEFINE_UNQUOTED([MAX_EEPROM_CAT_PDO], [${withval}], [Maximum number of eeprom-cat-pdo supported.]), [])
AC_ARG_WITH([max-eeprom-cat-pdo-entries],
              AS_HELP_STRING([--with-max-eeprom-cat-pdo-entries=MAX_EEPROM_CAT_PDO_ENTRIES], [Set maximum number of eeprom-cat-pdo-entries supported.]), 
              AC_DEFINE_UNQUOTED([MAX_EEPROM_CAT_PDO_ENTRIES], [${withval}], [Maximum number of eeprom-cat-pdo-entries supported.]), [])
AC_ARG_WITH([max-eeprom-cat-strings],
              AS_HELP_STRING([--with-max-eeprom-cat-strings=MAX_EEPROM_CAT_STRINGS], [Set maximum number of eeprom-cat-strings supported.]), 
              AC_DEFINE_UNQUOTED([MAX_EEPROM_CAT_STRINGS], [${withval}], [Maximum number of eeprom-cat-strings supported.]), [])
AC_ARG_WITH([max-eeprom-cat-dc],
              AS_HELP_STRING([--with-max-eeprom-cat-dc=MAX_EEPROM_CAT_DC], [Set maximum number of eeprom-cat-dc supported.]), 
              AC_DEFINE_UNQUOTED([MAX_EEPROM_CAT_DC], [${withval}], [Maximum number of eeprom-cat-dc supported.]), [])
AC_ARG_WITH([max-string-len],
              AS_HELP_STRING([--with-max-string-len=MAX_STRING_LEN], [Set maximum number of string-len supported.]), 
              AC_DEFINE_UNQUOTED([MAX_STRING_LEN], [${withval}], [Maximum number of string-len supported.]), [])
AC_ARG_WITH([max-data],
              AS_HELP_STRING([--with-max-data=MAX_DATA], [Set maximum number of data supported.]), 
              AC_DEFINE_UNQUOTED([MAX_DATA], [${withval}], [Maximum number of data supported.]), [])
AC_ARG_WITH([max-ds402-subdevs],
              AS_HELP_STRING([--with-max-ds402-subdevs=MAX_DS402_SUBDEVS], [Set maximum number of ds402-subdevs supported.]), 
              AC_DEFINE_UNQUOTED([MAX_DS402_SUBDEVS], [${withval}], [Maximum number of ds402-subdevs supported.]), [])
AC_ARG_WITH([max-coe-emergencies],
              AS_HELP_STRING([--with-max-coe-emergencies=MAX_COE_EMERGENCIES], [Set maximum number of coe-emergencies supported.]), 
              AC_DEFINE_UNQUOTED([MAX_COE_EMERGENCIES], [${withval}], [Maximum number of coe-emergencies supported.]), [])

AC_ARG_ENABLE([device-file], AS_HELP_STRING([--enable-device-file], [Enable file hw device layer.]),
              [
               BUILD_DEVICE_FILE=true
               AC_DEFINE([BUILD_DEVICE_FILE], [1], [Build with file hw device layer.])
              ],
              AC_DEFINE([BUILD_DEVICE_FILE], [0], [Build with file hw device layer.]))
AC_ARG_ENABLE([device-sock-raw], AS_HELP_STRING([--enable-device-sock-raw], [Enable sock-raw hw device layer.]),
              [
               BUILD_DEVICE_SOCK_RAW_LEGACY=true
               AC_DEFINE([BUILD_DEVICE_SOCK_RAW_LEGACY], [1], [Build with sock-raw hw device layer.])
              ],
              AC_DEFINE([BUILD_DEVICE_SOCK_RAW_LEGACY], [0], [Build with sock-raw hw device layer.]))
AC_ARG_ENABLE([device-sock-raw-mmaped], AS_HELP_STRING([--enable-device-sock-raw-mmaped], [Enable sock-raw mmaped hw device layer.]),
              [
               BUILD_DEVICE_SOCK_RAW_MMAPED=true
               AC_DEFINE([BUILD_DEVICE_SOCK_RAW_MMAPED], [1], [Build with sock-raw mmaped hw device layer.])
              ],
              AC_DEFINE([BUILD_DEVICE_SOCK_RAW_MMAPED], [0], [Build with sock-raw mmaped hw device layer.]))
AC_ARG_ENABLE([device-bpf], AS_HELP_STRING([--enable-device-bpf], [Enable bpf hw device layer.]),
              [
               BUILD_DEVICE_BPF=true
               AC_DEFINE([BUILD_DEVICE_BPF], [1], [Build with bpf hw device layer.])
              ],
              AC_DEFINE([BUILD_DEVICE_BPF], [0], [Build with bpf hw device layer.]))
AC_ARG_ENABLE([device-pikeos], AS_HELP_STRING([--enable-device-pikeos], [Enable pikeos hw device layer.]),
              [
               BUILD_DEVICE_PIKEOS=true
               AC_DEFINE([BUILD_DEVICE_PIKEOS], [1], [Build with pikeos hw device layer.])
              ],
              AC_DEFINE([BUILD_DEVICE_PIKEOS], [0], [Build with pikeos hw device layer.]))

case $target_os in
    linux*)
        BUILD_POSIX=true
        AC_DEFINE([BUILD_POSIX], [1], [Use POSIX build on linux])
        AC_DEFINE([BUILD_VXWORKS], [0], [Use VxWorks build])
        AC_DEFINE([BUILD_PIKEOS], [0], [Use PikeOS build])
        ;;
    vxworks*)
        BUILD_VXWORKS=true
        AC_DEFINE([BUILD_POSIX], [0], [Use POSIX build on linux])
        AC_DEFINE([BUILD_VXWORKS], [1], [Use VxWorks build])
        AC_DEFINE([BUILD_PIKEOS], [0], [Use PikeOS build])
        ;;
    pikeos*)
        BUILD_PIKEOS=true
        AC_DEFINE([BUILD_POSIX], [0], [Use POSIX build on linux])
        AC_DEFINE([BUILD_VXWORKS], [0], [Use VxWorks build])
        AC_DEFINE([BUILD_PIKEOS], [1], [Use PikeOS build])
        ;;
    **)
        AC_MSG_NOTICE([unknown target os: $target_os, $target_cpu, $target_vendor! Got target $target])
        ;;
esac

AM_CONDITIONAL([BUILD_POSIX], [ test x$BUILD_POSIX = xtrue]) 
AM_CONDITIONAL([BUILD_VXWORKS], [ test x$BUILD_VXWORKS = xtrue]) 
AM_CONDITIONAL([BUILD_PIKEOS], [ test x$BUILD_PIKEOS = xtrue]) 

AM_CONDITIONAL([BUILD_DEVICE_SOCK_RAW_LEGACY], [ test x$BUILD_DEVICE_SOCK_RAW_LEGACY = xtrue]) 
AM_CONDITIONAL([BUILD_DEVICE_SOCK_RAW_MMAPED], [ test x$BUILD_DEVICE_SOCK_RAW_MMAPED = xtrue]) 
AM_CONDITIONAL([BUILD_DEVICE_BPF],             [ test x$BUILD_DEVICE_BPF = xtrue]) 
AM_CONDITIONAL([BUILD_DEVICE_FILE],            [ test x$BUILD_DEVICE_FILE = xtrue]) 
AM_CONDITIONAL([BUILD_DEVICE_PIKEOS],          [ test x$BUILD_DEVICE_PIKEOS = xtrue]) 

AC_ARG_ENABLE([mbx-support-eoe], AS_HELP_STRING([--disable-mbx-support-eoe], [Disable Mailbox EoE support.]),
[
    AC_DEFINE([MBX_SUPPORT_EOE], [0], [Disable Mailbox EoE support.])
],
[
    MBX_SUPPORT_EOE=true
    AC_DEFINE([MBX_SUPPORT_EOE], [1], [Enable Mailbox EoE support.])
])

AM_CONDITIONAL([MBX_SUPPORT_EOE], [ test x$MBX_SUPPORT_EOE == xtrue ])

AC_ARG_ENABLE([mbx-support-coe], AS_HELP_STRING([--disable-mbx-support-coe], [Disable Mailbox CoE support.]),
[
    AC_DEFINE([MBX_SUPPORT_COE], [0], [Disable Mailbox CoE support.])
],
[
    MBX_SUPPORT_COE=true
    AC_DEFINE([MBX_SUPPORT_COE], [1], [Enable Mailbox CoE support.])
])

AM_CONDITIONAL([MBX_SUPPORT_COE], [ test x$MBX_SUPPORT_COE == xtrue ])

AC_ARG_ENABLE([mbx-support-foe], AS_HELP_STRING([--disable-mbx-support-foe], [Disable Mailbox FoE support.]),
[
    AC_DEFINE([MBX_SUPPORT_FOE], [0], [Disable Mailbox FoE support.])
],
[
    MBX_SUPPORT_FOE=true
    AC_DEFINE([MBX_SUPPORT_FOE], [1], [Enable Mailbox FoE support.])
])

AM_CONDITIONAL([MBX_SUPPORT_FOE], [ test x$MBX_SUPPORT_FOE == xtrue ])

AC_ARG_ENABLE([mbx-support-soe], AS_HELP_STRING([--disable-mbx-support-soe], [Disable Mailbox SoE support.]),
[
    AC_DEFINE([MBX_SUPPORT_SOE], [0], [Enable Mailbox SoE support.])
],
[
    MBX_SUPPORT_SOE=true
    AC_DEFINE([MBX_SUPPORT_SOE], [1], [Enable Mailbox SoE support.])
])

AM_CONDITIONAL([MBX_SUPPORT_SOE], [ test x$MBX_SUPPORT_SOE == xtrue ])

# Checks for libraries.
PKG_CHECK_MODULES([LIBOSAL], [libosal], [], [AC_MSG_ERROR([libosal not found])])

# Checks for header files.
AC_HEADER_ASSERT
AC_CHECK_HEADERS([arpa/inet.h winsock.h netinet/in.h net/util/inet.h net/if.h net/bpf.h fcntl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

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

# Checks for system libs
PTHREAD_LIBS=""
RT_LIBS=""
AC_CHECK_LIB(pthread, pthread_create, PTHREAD_LIBS="-pthread")
AC_CHECK_LIB(rt, clock_gettime, RT_LIBS="-lrt")
AC_CHECK_LIB(m, sqrt, MATH_LIBS="-lm")
AC_SUBST(PTHREAD_LIBS)
AC_SUBST(RT_LIBS)
AC_SUBST(MATH_LIBS)

AC_CONFIG_FILES([Makefile src/Makefile tools/ethercatdiag/Makefile tools/eepromtool/Makefile tools/example_with_dc/Makefile tools/foe_tool/Makefile libethercat.pc])
AC_OUTPUT

