cmake_minimum_required(VERSION 3.5)
project(libethercat)

set(ECAT_DEVICE "sock_raw" CACHE STRING "EtherCAT device layer")

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

find_package (libosal)

include_directories(${libosal_INCLUDE_DIRS} include)

configure_file(cmake_config.h.in config.h)

set(DEFAULT_DEFS HAVE_STDLIB_H HAVE_NET_IF_H HAVE_ARPA_INET_H)

set(SRC_ETHERCAT 
    src/async_loop.c
    src/coe.c
    src/coe_master.c
    src/datagram.c
    src/dc.c
    src/ec.c
    src/eeprom.c
    src/eoe.c
    src/foe.c
    src/hw.c
    src/idx.c
    src/mbx.c
    src/mii.c
    src/pool.c
    src/slave.c
    src/soe.c)

if (${ECAT_DEVICE} STREQUAL "sock_raw")
    set(SRC_HW_LAYER src/hw_sock_raw.c)
elseif (${ECAT_DEVICE} STREQUAL "file")
    set(SRC_HW_LAYER src/hw_file.c)
elseif (${ECAT_DEVICE} STREQUAL "pikeos")
    set(SRC_HW_LAYER src/hw_pikeos.c)
elseif (${ECAT_DEVICE} STREQUAL "bpf")
    set(SRC_HW_LAYER src/hw_bpf.c)
endif()

# LIBS
add_library(ethercat ${SRC_ETHERCAT} ${SRC_HW_LAYER})
target_compile_definitions(ethercat PRIVATE $DEFAULT_DEFS)
target_link_libraries (ethercat ${CMAKE_THREAD_LIBS_INIT} ${libosal_LIBS})

# TOOLS
add_executable(eepromtool tools/eepromtool/eepromtool.c)
target_compile_definitions(eepromtool PRIVATE $DEFAULT_DEFS)
target_link_libraries (eepromtool ethercat)

add_executable(ethercatdiag tools/ethercatdiag/ethercatdiag.c)
target_compile_definitions(ethercatdiag PRIVATE $DEFAULT_DEFS)
target_link_libraries (ethercatdiag ethercat)

add_executable(example_with_dc tools/example_with_dc/example_with_dc.c)
target_compile_definitions(example_with_dc PRIVATE $DEFAULT_DEFS)
target_link_libraries (example_with_dc ethercat m)

