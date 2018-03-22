cmake_minimum_required(VERSION 3.5)
project(libethercat)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

include_directories(${PROJECT_BINARY_DIR} include src)

configure_file(config.h.in config.h)

add_library(ethercat
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
        include/libethercat/timer.h)

add_executable(eepromtool tools/eepromtool/eepromtool.c)
add_executable(ethercatdiag tools/ethercatdiag/ethercatdiag.c)
