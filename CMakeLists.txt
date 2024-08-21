cmake_minimum_required(VERSION 3.5)
file(READ "project.properties" PROJECT_PROPERTIES)
string(REGEX MATCH "VERSION = ([0-9]*.[0-9]*.[0-9]*)" _ ${PROJECT_PROPERTIES})
set(PROJECT_VERSION ${CMAKE_MATCH_1})
project(libethercat VERSION ${PROJECT_VERSION})

set(ECAT_DEVICE "sock_raw" CACHE STRING "EtherCAT device layer as `+` separated list")
string(REPLACE "+" ";" ECAT_DEVICE ${ECAT_DEVICE})

include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckLibraryExists)
include(ExternalProject)
include(CMakePackageConfigHelpers)

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
check_include_files("netinet/in.h" LIBETHERCAT_HAVE_NETINET_IN_H)
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

list(FIND ECAT_DEVICE "sock_raw" HAS_SOCK_RAW)
list(FIND ECAT_DEVICE "sock_raw_mmaped" HAS_SOCK_RAW_MMAPED)
list(FIND ECAT_DEVICE "file" HAS_SOCK_FILE)
list(FIND ECAT_DEVICE "pikeos" HAS_SOCK_PIKEOS)
list(FIND ECAT_DEVICE "bpf" HAS_SOCK_BPF)

if (${HAS_SOCK_RAW} GREATER -1)
    message("Include device sock_raw")
    list(APPEND SRC_HW_LAYER src/hw_sock_raw.c)
    set(LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_LEGACY 1)
endif()
if (${HAS_SOCK_RAW_MMAPED} GREATER -1)
    message("Include device sock_raw_mmaped")
    list(APPEND SRC_HW_LAYER src/hw_sock_raw_mmaped.c)
    set(LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_MMAPED 1)
endif()
if (${HAS_SOCK_FILE} GREATER -1)
    message("Include device file")
    list(APPEND SRC_HW_LAYER src/hw_file.c)
    set(LIBETHERCAT_BUILD_DEVICE_FILE 1)
endif()
if (${HAS_SOCK_PIKEOS} GREATER -1)
    message("Include device pikeos")
    list(APPEND SRC_HW_LAYER src/hw_pikeos.c)
    set(LIBETHERCAT_BUILD_DEVICE_PIKEOS 1)
endif()
if (${HAS_SOCK_BPF} GREATER -1)
    message("Include device bpf")
    list(APPEND SRC_HW_LAYER src/hw_bpf.c)
    set(LIBETHERCAT_BUILD_DEVICE_BPF 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/libethercat/config.h)

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

# Install 
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libethercatConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

set(INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
set(ETHERCAT_LIB_NAME ethercat)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/libethercatConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libethercatConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/libethercat
    PATH_VARS ETHERCAT_LIB_NAME INSTALL_DIR
)

install(TARGETS ethercat EXPORT libethercatTargets)
install(
    EXPORT libethercatTargets 
    FILE libethercatTargets.cmake
    DESTINATION lib/cmake/libethercat
)
install(DIRECTORY include/libethercat/ DESTINATION include/libethercat 
	FILES_MATCHING PATTERN "*.h"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libethercatConfig.cmake" 
    "${CMAKE_CURRENT_BINARY_DIR}/libethercatConfigVersion.cmake"
    DESTINATION lib/cmake/libethercat
)