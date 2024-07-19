ACLOCAL_AMFLAGS = -I m4

LDADD = $(top_builddir)/src/.libs/libethercat.la
LIBS  = @LIBOSAL_LIBS@ @RT_LIBS@ @PTHREAD_LIBS@

bin_PROGRAMS = eepromtool
eepromtool_SOURCES = eepromtool.c 
eepromtool_CFLAGS = -I$(top_srcdir)/include @LIBOSAL_CFLAGS@
