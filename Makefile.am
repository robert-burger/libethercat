ACLOCAL_AMFLAGS = -I m4

include $(top_srcdir)/aminclude.am

SUBDIRS=src 

if BUILD_POSIX
SUBDIRS+=tools/ethercatdiag 
SUBDIRS+=tools/eepromtool
SUBDIRS+=tools/example_with_dc
if MBX_SUPPORT_FOE
SUBDIRS+=tools/foe_tool
endif
endif

if BUILD_PIKEOS
#SUBDIRS+=tools/example_with_dc
endif

pkgconfig_DATA = libethercat.pc

