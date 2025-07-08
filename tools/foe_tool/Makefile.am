ACLOCAL_AMFLAGS = -I m4

LDADD = $(top_builddir)/src/.libs/libethercat.la
LIBS  = @LIBOSAL_LIBS@ @RT_LIBS@ @PTHREAD_LIBS@

bin_PROGRAMS = foe_tool
foe_tool_SOURCES = foe_tool.c 
foe_tool_CFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include @LIBOSAL_CFLAGS@

