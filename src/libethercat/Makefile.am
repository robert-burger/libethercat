ACLOCAL_AMFLAGS = -I m4

includedir = $(prefix)/include/libethercat

lib_LTLIBRARIES = libethercat.la 
include_HEADERS = coe.h common.h datagram.h datagram_pool.h dc.h ec.h eeprom.h \
				  error_codes.h foe.h hw.h mbx.h message_pool.h regs.h slave.h \
				  soe.h idx.h timer.h mii.h

libethercat_la_SOURCES  = coe.c common.c slave.c datagram.c datagram_pool.c message_pool.c ec.c \
						  hw.c soe.c foe.c mbx.c eeprom.c dc.c timer.c idx.c mii.c
libethercat_la_CFLAGS   = -std=gnu99 -Wall -Werror -I$(top_srcdir)/src
libethercat_la_LDFLAGS  = -Wall -Werror -release $(PACKAGE_VERSION) 

