#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libethercat], [0.1.beta3], [Robert Burger robert.burger@dlr.de])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
DX_INIT_DOXYGEN($PACKAGE_NAME, doxygen.cfg)

# Checks for programs.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_CC
AC_PROG_CC_STDC
m4_ifdef([AM_PROG_CC_C_O], [AM_PROG_CC_C_O])
AC_PROG_LIBTOOL
m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG])
m4_ifdef([PKG_INSTALLDIR], [PKG_INSTALLDIR], [pkgconfigdir='${libdir}/pkgconfig' AC_SUBST([pkgconfigdir])])

# Checks for libraries.
AX_CHECK_COMPILE_FLAG([-pthread], [AX_APPEND_FLAG([-pthread])] )
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([rt], [sem_init])

AX_GCC_ARCHFLAG([PORTABLE?], [], [])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h net/if.h net/bpf.h fcntl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])
AC_CHECK_HEADER([net/bpf.h], [AX_APPEND_FLAG([-DUSE_BPF])])

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

# Platform specific setup
# Check for which host we are on and setup a few things
# specifically based on the host
case $target in
    *vxworks* )
        PROGRAM_LDFLAGS+="$LDFLAGS -non-static"
        ;;
    *qnx* )
        PROGRAM_CFLAGS+="-DUSE_BPF"
        ;;
    *rtems* )
        PROGRAM_CFLAGS+="-DUSE_BPF -I/volume/USERSTORE/crem_ja/PUBLIC/rtems-scosa/4.12/arm/arm-rtems4.12/xilinx_zynq_zedboard/lib/include --pipe -B/volume/USERSTORE/crem_ja/PUBLIC/rtems-scosa/4.12/arm/arm-rtems4.12/xilinx_zynq_zedboard/lib/ -specs bsp_specs -qrtems -ffunction-sections -fdata-sections -march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard -mtune=cortex-a9"
        PROGRAM_LDFLAGS+="--pipe -B/volume/USERSTORE/crem_ja/PUBLIC/rtems-scosa/4.12/arm/arm-rtems4.12/xilinx_zynq_zedboard/lib/ -specs bsp_specs -qrtems -ffunction-sections -fdata-sections -march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard -mtune=cortex-a9 -Wl,--gc-sections -march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard -mtune=cortex-a9"
        ;;
    *)
        PROGRAM_LDFLAGS+="$LDFLAGS "
        ;;
esac

AC_SUBST([PROGRAM_CFLAGS])
AC_SUBST([PROGRAM_LDFLAGS])

AC_CONFIG_FILES([Makefile src/libethercat/Makefile src/ethercatdiag/Makefile libethercat.pc libethercat.pt])
AC_OUTPUT

