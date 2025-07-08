ACLOCAL_AMFLAGS = -I m4

LDADD = $(top_builddir)/src/.libs/libethercat.la
LIBS  = @LIBOSAL_LIBS@ @RT_LIBS@ @PTHREAD_LIBS@

bin_PROGRAMS = ethercatdiag
ethercatdiag_SOURCES = ethercatdiag.c 
ethercatdiag_CFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include @LIBOSAL_CFLAGS@

