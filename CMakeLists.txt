cmake_minimum_required(VERSION 3.5)
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VER
    RESULT_VARIABLE GIT_RESULT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(GIT_RESULT EQUAL 0)
    string(REGEX MATCH "v?([0-9]+\\.[0-9]+\\.[0-9]+)" _ ${GIT_VER})
    set(PROJECT_VERSION ${CMAKE_MATCH_1})
endif()

if(NOT PROJECT_VERSION)
    set(PROJECT_VERSION 0.0.0)
endif()

project(libethercat VERSION ${PROJECT_VERSION})

option(MBX_SUPPORT_COE "Flag to enable or disable Mailbox CoE support" ON)       
option(MBX_SUPPORT_FOE "Flag to enable or disable Mailbox FoE support" ON)
option(MBX_SUPPORT_SOE "Flag to enable or disable Mailbox SoE support" ON)
option(MBX_SUPPORT_EOE "Flag to enable or disable Mailbox EoE support" ON)
set(ECAT_DEVICE "sock_raw" CACHE STRING "EtherCAT device layer as `+` separated list")
string(REPLACE "+" ";" ECAT_DEVICE ${ECAT_DEVICE})

include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckLibraryExists)
include(ExternalProject)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)
set(PACKAGE_LIB_DIR ${CMAKE_INSTALL_LIBDIR})

set(LIBETHERCAT_STDC_HEADERS 1)
set(LIBETHERCAT_PACKAGE_VERSION "${PROJECT_VERSION}")
set(LIBETHERCAT_VERSION "${PROJECT_VERSION}")
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

message(INFO "libosal ${libosal_LIB_DIR}")
include_directories(${libosal_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/include)
link_directories(${libosal_LIB_DIR})
check_symbol_exists("LIBOSAL_BUILD_POSIX" "libosal/osal.h" LIBETHERCAT_BUILD_POSIX)
set(LIBETHERCAT_BUILD_POSIX 1)

set(SRC_ETHERCAT 
    src/async_loop.c
    src/datagram.c
    src/dc.c
    src/ec.c
    src/eeprom.c
    src/hw.c
    src/idx.c
    src/mbx.c
    src/mii.c
    src/pool.c
    src/slave.c
    )

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

if(${MBX_SUPPORT_COE})
    set(LIBETHERCAT_MBX_SUPPORT_COE 1)
    list(APPEND SRC_ETHERCAT src/coe.c src/coe_master.c)
endif()

if(${MBX_SUPPORT_FOE})
    set(LIBETHERCAT_MBX_SUPPORT_FOE 1)
    list(APPEND SRC_ETHERCAT src/foe.c)
endif()

if(${MBX_SUPPORT_SOE})
    set(LIBETHERCAT_MBX_SUPPORT_SOE 1)
    list(APPEND SRC_ETHERCAT src/soe.c)
endif()

if(${MBX_SUPPORT_EOE})
    set(LIBETHERCAT_MBX_SUPPORT_EOE 1)
    list(APPEND SRC_ETHERCAT src/eoe.c)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/libethercat/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/settings.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/libethercat/settings.h)

# LIBS
add_library(ethercat ${SRC_ETHERCAT} ${SRC_HW_LAYER})
target_link_libraries (ethercat ${CMAKE_THREAD_LIBS_INIT} ${libosal_LIBS})
target_compile_definitions(ethercat PUBLIC HAVE_CONFIG_H=1)

# TOOLS
add_executable(eepromtool tools/eepromtool/eepromtool.c)
target_link_libraries (eepromtool ethercat ${libosal_LIBS})

add_executable(ethercatdiag tools/ethercatdiag/ethercatdiag.c)
target_link_libraries (ethercatdiag ethercat ${libosal_LIBS})

add_executable(example_with_dc tools/example_with_dc/example_with_dc.c)
target_link_libraries (example_with_dc ethercat ${libosal_LIBS} m)

if (${MBX_SUPPORT_FOE})
    add_executable(foe_tool tools/foe_tool/foe_tool.c)
    target_link_libraries (foe_tool ethercat ${libosal_LIBS})
endif()

# Install 
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libethercatConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Define your version, prefix, and dirs (adjust as needed)
set(VERSION "${PROJECT_VERSION}")
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "${prefix}")
set(libdir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/libethercat.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/libethercat.pc
  @ONLY
)

set(INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
set(ETHERCAT_LIB_NAME ethercat)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/libethercatConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libethercatConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libethercat
    PATH_VARS ETHERCAT_LIB_NAME INSTALL_DIR
)

install(TARGETS ethercat EXPORT libethercatTargets)
install(
    EXPORT libethercatTargets 
    FILE libethercatTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libethercat
)
install(DIRECTORY include/libethercat/ DESTINATION include/libethercat 
	FILES_MATCHING PATTERN "*.h"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libethercatConfig.cmake" 
    "${CMAKE_CURRENT_BINARY_DIR}/libethercatConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libethercat
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libethercat.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
