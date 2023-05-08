ACLOCAL_AMFLAGS = -I m4

include $(top_srcdir)/aminclude.am

SUBDIRS=src 

if BUILD_POSIX
SUBDIRS+=tools/ethercatdiag 
SUBDIRS+=tools/eepromtool
SUBDIRS+=tools/example_with_dc
SUBDIRS+=tools/foe_download
endif

pkgconfig_DATA = libethercat.pc

