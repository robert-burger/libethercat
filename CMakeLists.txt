cmake_minimum_required(VERSION 3.5)
project(libethercat)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

include_directories(${PROJECT_BINARY_DIR} include src)

configure_file(config.h.in config.h)

INCLUDE(CheckIncludeFiles)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

CHECK_INCLUDE_FILES("netinet/in.h" HAVE_NETINET_IN_H)
CHECK_INCLUDE_FILES("arpa/inet.h" HAVE_ARPA_INET_H)
CHECK_INCLUDE_FILES("net/if.h" HAVE_NET_IF_H)
CHECK_INCLUDE_FILES("net/bpf.h" HAVE_NET_BPF_H)
CHECK_INCLUDE_FILES("fcntl.h" HAVE_FCNTL_H)
CHECK_INCLUDE_FILES("limits.h" HAVE_LIMITS_H)
CHECK_INCLUDE_FILES("stdint.h" HAVE_STDINT_H)
CHECK_INCLUDE_FILES("stdlib.h" HAVE_STDLIB_H)
CHECK_INCLUDE_FILES("string.h" HAVE_STRING_H)
CHECK_INCLUDE_FILES("sys/ioctl.h" HAVE_SYS_IOCTL_H)
CHECK_INCLUDE_FILES("sys/socket.h" HAVE_SYS_SOCKET_H)
CHECK_INCLUDE_FILES("sys/time.h" HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILES("unistd.h" HAVE_UNISTD_H)

include_directories(${PROJECT_BINARY_DIR} include src)

configure_file(cmake_config.h.in config.h)

find_package (Threads)

set(DEFAULT_DEFS HAVE_STDLIB_H HAVE_NET_IF_H HAVE_ARPA_INET_H)

set(SRC_ETHERCAT 
        src/coe.c
        src/common.c
        src/datagram.c
        src/datagram_pool.c
        src/dc.c
        src/ec.c
        src/eeprom.c
        src/foe.c
        src/hw.c
        src/idx.c
        src/mbx.c
        src/message_pool.c
        src/mii.c
        src/slave.c
        src/soe.c
        src/timer.c
        include/libethercat/coe.h
        include/libethercat/common.h
        include/libethercat/datagram.h
        include/libethercat/datagram_pool.h
        include/libethercat/dc.h
        include/libethercat/ec.h
        include/libethercat/eeprom.h
        include/libethercat/error_codes.h
        include/libethercat/foe.h
        include/libethercat/hw.h
        include/libethercat/idx.h
        include/libethercat/mbx.h
        include/libethercat/message_pool.h
        include/libethercat/mii.h
        include/libethercat/regs.h
        include/libethercat/slave.h
        include/libethercat/soe.h
        include/libethercat/timer.h
        include/libethercat/rtlog.h
        include/libethercat/rtlog_static.h)

# LIBS
add_library(ethercat ${SRC_ETHERCAT})
target_compile_definitions(ethercat PRIVATE $DEFAULT_DEFS)
target_link_libraries (ethercat ${CMAKE_THREAD_LIBS_INIT})

# TOOLS
add_executable(eepromtool tools/eepromtool/eepromtool.c)
target_compile_definitions(eepromtool PRIVATE $DEFAULT_DEFS)
target_link_libraries (eepromtool ${CMAKE_THREAD_LIBS_INIT})

add_executable(ethercatdiag tools/ethercatdiag/ethercatdiag.c)
target_compile_definitions(ethercatdiag PRIVATE $DEFAULT_DEFS)
target_link_libraries (ethercatdiag ${CMAKE_THREAD_LIBS_INIT})

# TESTS
add_executable(test_rtlog_level test/rtlog/test_rtlog_level.c test/rtlog/test.h src/rtlog.c include/libethercat/rtlog.h)
target_compile_definitions(test_rtlog_level PRIVATE $DEFAULT_DEFS RTLOG_MODE_DEBUG=1)
target_link_libraries (test_rtlog_level ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_rtlog_overflow test/rtlog/test_rtlog_overflow.c test/rtlog/test.h src/rtlog.c include/libethercat/rtlog.h)
target_compile_definitions(test_rtlog_overflow PRIVATE $DEFAULT_DEFS RTLOG_MODE_DEBUG=1)
target_link_libraries (test_rtlog_overflow ${CMAKE_THREAD_LIBS_INIT})

add_executable(smoketest_rtlog test/rtlog/smoketest_rtlog.c test/rtlog/test.h src/rtlog.c include/libethercat/rtlog.h)
target_compile_definitions(smoketest_rtlog PRIVATE $DEFAULT_DEFS RTLOG_MODE_DEBUG=1)
target_link_libraries (smoketest_rtlog ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_rtlog_bugs test/rtlog/test_rtlog_bugs.c test/rtlog/test.h src/rtlog.c include/libethercat/rtlog.h)
target_compile_definitions(test_rtlog_bugs PRIVATE $DEFAULT_DEFS RTLOG_MODE_DEBUG=1)
target_link_libraries (test_rtlog_bugs ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_rtlog_static test/rtlog/test_rtlog_static.c test/rtlog/test.h src/rtlog.c src/rtlog_static.c include/libethercat/rtlog.h)
target_compile_definitions(test_rtlog_static PRIVATE $DEFAULT_DEFS RTLOG_MODE_DEBUG=1)
target_link_libraries (test_rtlog_static ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_rtlog_flush test/rtlog/test_rtlog_flush.c test/rtlog/test.h src/rtlog.c include/libethercat/rtlog.h)
target_compile_definitions(test_rtlog_flush PRIVATE $DEFAULT_DEFS RTLOG_MODE_DEBUG=1)
target_link_libraries (test_rtlog_flush ${CMAKE_THREAD_LIBS_INIT})

