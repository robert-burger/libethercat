cmake_minimum_required(VERSION 3.5)
project(libethercat VERSION 0.4.0)

set(ECAT_DEVICE "sock_raw" CACHE STRING "EtherCAT device layer")

include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckLibraryExists)
include(ExternalProject)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

set(LIBETHERCAT_STDC_HEADERS 1)
set(LIBETHERCAT_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(LIBETHERCAT_VERSION "${CMAKE_PROJECT_VERSION}")
set(LIBETHERCAT_PACKAGE_URL "${CMAKE_PROJECT_HOMEPAGE_URL}")
set(LIBETHERCAT_PACKAGE "${CMAKE_PROJECT_NAME}")
set(LIBETHERCAT_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(LIBETHERCAT_PACKAGE_TARNAME "${CMAKE_PROJECT_NAME}")
set(LIBETHERCAT_PACKAGE_STRING "${CMAKE_PROJECT_NAME} ${CMAKE_PROJECT_VERSION}")

check_include_files("arpa/inet.h" LIBETHERCAT_HAVE_ARPA_INET_H)
check_symbol_exists("clock_gettime" "time.h" LIBETHERCAT_HAVE_CLOCK_GETTIME)
check_include_files("dlfcn.h" LIBETHERCAT_HAVE_DLFCN_H)
check_include_files("fcntl.h" LIBETHERCAT_HAVE_FCNTL_H)
check_include_files("inttypes.h" LIBETHERCAT_HAVE_INTTYPES_H)
find_library(HAS_RT_RAW rt)
if(HAS_RT_RAW)
    SET(LIBETHERCAT_HAVE_LIBRT 1)
endif()
check_include_files("limits.h" LIBETHERCAT_HAVE_LIMITS_H)
check_symbol_exists("malloc" "stdlib.h" LIBETHERCAT_HAVE_MALLOC)
check_include_files("memory.h" LIBETHERCAT_HAVE_MEMORY_H)
check_symbol_exists("memset" "string.h" LIBETHERCAT_HAVE_MALLOC)
check_include_files("net/bpf.h" LIBETHERCAT_HAVE_NET_BPF_H)
check_include_files("net/if.h" LIBETHERCAT_HAVE_NET_IF_H)
check_include_files("pthread.h" LIBETHERCAT_HAVE_PTHREAD)
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists("pthread_setaffinity_np" "pthread.h" LIBETHERCAT_HAVE_PTHREAD_SETAFFINITY_NP)
check_symbol_exists("realloc" "stdlib.h" LIBETHERCAT_HAVE_REALLOC)
check_symbol_exists("socket" "sys/socket.h" LIBETHERCAT_HAVE_SOCKET)
check_include_files("stdint.h" LIBETHERCAT_HAVE_STDINT_H)
check_include_files("stdlib.h" LIBETHERCAT_HAVE_STDLIB_H)
check_symbol_exists("strdup" "string.h" LIBETHERCAT_HAVE_STRDUP)
check_symbol_exists("strerror" "string.h" LIBETHERCAT_HAVE_STRERROR)
check_include_files("strings.h" LIBETHERCAT_HAVE_STRINGS_H)
check_include_files("string.h" LIBETHERCAT_HAVE_STRING_H)
check_symbol_exists("strndup" "string.h" LIBETHERCAT_HAVE_STRNDUP)
check_include_files("sys/ioctl.h" LIBETHERCAT_HAVE_SYS_IOCTL_H)
check_include_files("sys/socket.h" LIBETHERCAT_HAVE_SYS_SOCKET_H)
check_include_files("sys/stat.h" LIBETHERCAT_HAVE_SYS_STAT_H)
check_include_files("sys/time.h" LIBETHERCAT_HAVE_SYS_TIME_H)
check_include_files("sys/types.h" LIBETHERCAT_HAVE_SYS_TYPES_H)
check_include_files("unistd.h" LIBETHERCAT_HAVE_UNISTD_H)

find_package(libosal REQUIRED)
find_package (Threads REQUIRED)

include_directories(${libosal_INCLUDE_DIRS} include)
link_directories(${libosal_LIB_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/libethercat/config.h)


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
    add_compile_definitions(LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_LEGACY)
elseif (${ECAT_DEVICE} STREQUAL "sock_raw_mmaped")
    set(SRC_HW_LAYER src/hw_file.c)
    add_compile_definitions(LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_MMAPED)
elseif (${ECAT_DEVICE} STREQUAL "file")
    set(SRC_HW_LAYER src/hw_file.c)
    add_compile_definitions(LIBETHERCAT_BUILD_DEVICE_FILE)
elseif (${ECAT_DEVICE} STREQUAL "pikeos")
    set(SRC_HW_LAYER src/hw_pikeos.c)
    add_compile_definitions(LIBETHERCAT_BUILD_DEVICE_PIKEOS)
elseif (${ECAT_DEVICE} STREQUAL "bpf")
    set(SRC_HW_LAYER src/hw_bpf.c)
    add_compile_definitions(LIBETHERCAT_BUILD_DEVICE_BPD)
endif()

# LIBS
add_library(ethercat ${SRC_ETHERCAT} ${SRC_HW_LAYER})
target_link_libraries (ethercat ${CMAKE_THREAD_LIBS_INIT} ${libosal_LIBS})

# TOOLS
add_executable(eepromtool tools/eepromtool/eepromtool.c)
target_link_libraries (eepromtool ethercat ${libosal_LIBS})

add_executable(ethercatdiag tools/ethercatdiag/ethercatdiag.c)
target_link_libraries (ethercatdiag ethercat ${libosal_LIBS})

add_executable(example_with_dc tools/example_with_dc/example_with_dc.c)
target_link_libraries (example_with_dc ethercat ${libosal_LIBS} m)

