ACLOCAL_AMFLAGS = -I m4

includedir = $(prefix)/include/libethercat

lib_LTLIBRARIES = libethercat.la

include_HEADERS = $(top_srcdir)/include/libethercat/coe.h \
                  $(top_srcdir)/include/libethercat/common.h \
                  $(top_srcdir)/include/libethercat/datagram.h \
                  $(top_srcdir)/include/libethercat/datagram_pool.h \
                  $(top_srcdir)/include/libethercat/dc.h \
                  $(top_srcdir)/include/libethercat/ec.h \
                  $(top_srcdir)/include/libethercat/eeprom.h \
				  $(top_srcdir)/include/libethercat/error_codes.h \
				  $(top_srcdir)/include/libethercat/foe.h \
				  $(top_srcdir)/include/libethercat/hw.h \
				  $(top_srcdir)/include/libethercat/mbx.h \
				  $(top_srcdir)/include/libethercat/message_pool.h \
				  $(top_srcdir)/include/libethercat/regs.h \
				  $(top_srcdir)/include/libethercat/slave.h \
				  $(top_srcdir)/include/libethercat/soe.h \
				  $(top_srcdir)/include/libethercat/idx.h \
				  $(top_srcdir)/include/libethercat/timer.h \
				  $(top_srcdir)/include/libethercat/mii.h

libethercat_la_SOURCES  = coe.c common.c slave.c datagram.c datagram_pool.c message_pool.c ec.c \
						  hw.c soe.c foe.c mbx.c eeprom.c dc.c timer.c idx.c mii.c eoe.c
libethercat_la_CFLAGS   = -std=gnu99 -Wall -Werror -I$(top_srcdir)/include
libethercat_la_LDFLAGS  = -Wall -Werror -release $(PACKAGE_VERSION) 

