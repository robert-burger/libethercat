ACLOCAL_AMFLAGS = -I m4

includedir = $(prefix)/include/libethercat

lib_LTLIBRARIES = libethercat.la

include_HEADERS = $(top_srcdir)/include/libethercat/coe.h \
				  $(top_srcdir)/include/libethercat/common.h \
				  $(top_srcdir)/include/libethercat/config.h \
				  $(top_srcdir)/include/libethercat/datagram.h \
				  $(top_srcdir)/include/libethercat/pool.h \
				  $(top_srcdir)/include/libethercat/dc.h \
				  $(top_srcdir)/include/libethercat/ec.h \
				  $(top_srcdir)/include/libethercat/eeprom.h \
				  $(top_srcdir)/include/libethercat/error_codes.h \
				  $(top_srcdir)/include/libethercat/foe.h \
				  $(top_srcdir)/include/libethercat/eoe.h \
				  $(top_srcdir)/include/libethercat/hw.h \
				  $(top_srcdir)/include/libethercat/mbx.h \
				  $(top_srcdir)/include/libethercat/async_loop.h \
				  $(top_srcdir)/include/libethercat/regs.h \
				  $(top_srcdir)/include/libethercat/slave.h \
				  $(top_srcdir)/include/libethercat/soe.h \
				  $(top_srcdir)/include/libethercat/idx.h \
				  $(top_srcdir)/include/libethercat/mii.h

libethercat_la_SOURCES	= coe.c coe_master.c slave.c datagram.c pool.c async_loop.c ec.c \
						  hw.c soe.c foe.c mbx.c eeprom.c dc.c idx.c mii.c eoe.c

if BUILD_DEVICE_SOCK_RAW_LEGACY
libethercat_la_SOURCES += hw_sock_raw.c
endif

if BUILD_DEVICE_SOCK_RAW_MMAPED
libethercat_la_SOURCES += hw_sock_raw.c
endif

if BUILD_DEVICE_FILE
libethercat_la_SOURCES += hw_file.c
endif

if BUILD_DEVICE_BPF
libethercat_la_SOURCES += hw_bpf.c
endif

if BUILD_PIKEOS
libethercat_la_SOURCES += hw_pikeos.c
endif

libethercat_la_CFLAGS	= -std=gnu99 -Wall -Werror -Wno-unused-const-variable -I$(top_srcdir)/include @LIBOSAL_CFLAGS@
libethercat_la_LDFLAGS	= -Wall -Werror -no-undefined @LIBOSAL_LIBS@
libethercat_la_LIBS     = @LIBOSAL_LIBS@

install-exec-hook: 
	find $(DESTDIR)$(libdir) -type f -name \*.la -delete


