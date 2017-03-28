ACLOCAL_AMFLAGS = -I m4

libdir = $(prefix)/lib
includedir = $(prefix)/include/libethercat
lib_LTLIBRARIES = libethercat.la 
#libethercat_pic.la
include_HEADERS = \
				  $(srcdir)/include/libethercat/coe.h \
				  $(srcdir)/include/libethercat/common.h \
				  $(srcdir)/include/libethercat/datagram.h \
				  $(srcdir)/include/libethercat/datagram_pool.h \
				  $(srcdir)/include/libethercat/dc.h \
				  $(srcdir)/include/libethercat/ec.h \
				  $(srcdir)/include/libethercat/eeprom.h \
				  $(srcdir)/include/libethercat/error_codes.h \
				  $(srcdir)/include/libethercat/foe.h \
				  $(srcdir)/include/libethercat/hw.h \
				  $(srcdir)/include/libethercat/mbx.h \
				  $(srcdir)/include/libethercat/message_pool.h \
				  $(srcdir)/include/libethercat/regs.h \
				  $(srcdir)/include/libethercat/slave.h \
				  $(srcdir)/include/libethercat/soe.h \
				  $(srcdir)/include/libethercat/idx.h \
				  $(srcdir)/include/libethercat/timer.h


libethercat_la_SOURCES  = coe.c slave.c datagram.c datagram_pool.c message_pool.c ec.c \
						  hw.c soe.c foe.c mbx.c eeprom.c dc.c timer.c idx.c
libethercat_la_CFLAGS   = @PROGRAM_CFLAGS@ -std=gnu99 -Wall -Werror -I$(srcdir)/include
libethercat_la_LDFLAGS  = -Wall -Werror -release $(PACKAGE_VERSION) 

#libethercat_pic_la_SOURCES  = coe.c slave.c datagram.c datagram_pool.c message_pool.c ec.c \
#						  hw.c soe.c foe.c mbx.c eeprom.c dc.c timer.c idx.c
#libethercat_pic_la_CFLAGS   = @PROGRAM_CFLAGS@ -std=gnu99 -Wall -Werror -I$(srcdir)/include -fPIC -DPIC 
#libethercat_pic_la_LDFLAGS  = -Wall -Werror -release $(PACKAGE_VERSION) 

bin_PROGRAMS = ethercatdiag
ethercatdiag_SOURCES = ethercatdiag.c 
ethercatdiag_CFLAGS = @PROGRAM_CFLAGS@ -I$(srcdir)/include
ethercatdiag_LDFLAGS = 

#@PROGRAM_LDFLAGS@
ethercatdiag_LDADD = libethercat.la 

