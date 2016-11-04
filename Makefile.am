ACLOCAL_AMFLAGS = -I m4

libdir = $(prefix)/lib
lib_LTLIBRARIES = libethercat.la
BUILT_SOURCES = ln_messages.h
CLEANFILES = ln_messages.h

libethercat_la_SOURCES  = coe.c slave.c datagram.c datagram_pool.c message_pool.c ec.c \
						  hw.c soe.c foe.c mbx.c eeprom.c dc.c timer.c

libethercat_la_CFLAGS   = -std=gnu99 -Wall -I$(srcdir) 

libethercat_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

