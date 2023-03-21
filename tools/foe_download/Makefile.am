ACLOCAL_AMFLAGS = -I m4

LDADD = $(top_builddir)/src/.libs/libethercat.la
LIBS  = @LIBOSAL_LIBS@

bin_PROGRAMS = foe_download
foe_download_SOURCES = foe_download.c 
foe_download_CFLAGS = -I$(top_srcdir)/include @LIBOSAL_CFLAGS@

