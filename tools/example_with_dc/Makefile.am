ACLOCAL_AMFLAGS = -I m4

LDADD = $(top_builddir)/src/.libs/libethercat.la
LIBS  = @LIBOSAL_LIBS@ @MATH_LIBS@ @RT_LIBS@ @PTHREAD_LIBS@

bin_PROGRAMS = example_with_dc
example_with_dc_SOURCES = example_with_dc.c 
example_with_dc_CFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include @LIBOSAL_CFLAGS@
