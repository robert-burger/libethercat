cmake_minimum_required(VERSION 3.3)
project(libethercat)

set(CMAKE_CXX_STANDARD 11)

include_directories(
        /volume/USERSTORE/crem_ja/PUBLIC/rtems/bsps/arm/arm-rtems4.12/xilinx_zynq_zedboard/lib/include
        src
)

set(SOURCE_FILES
        build/rtems/config.h
        src/ethercatdiag/ethercatdiag.c
        src/libethercat/coe.c
        src/libethercat/coe.h
        src/libethercat/common.c
        src/libethercat/common.h
        src/libethercat/datagram.c
        src/libethercat/datagram.h
        src/libethercat/datagram_pool.c
        src/libethercat/datagram_pool.h
        src/libethercat/dc.c
        src/libethercat/dc.h
        src/libethercat/ec.c
        src/libethercat/ec.h
        src/libethercat/eeprom.c
        src/libethercat/eeprom.h
        src/libethercat/error_codes.h
        src/libethercat/foe.c
        src/libethercat/foe.h
        src/libethercat/hw.c
        src/libethercat/hw.h
        src/libethercat/idx.c
        src/libethercat/idx.h
        src/libethercat/main.c
        src/libethercat/mbx.c
        src/libethercat/mbx.h
        src/libethercat/message_pool.c
        src/libethercat/message_pool.h
        src/libethercat/regs.h
        src/libethercat/slave.c
        src/libethercat/slave.h
        src/libethercat/soe.c
        src/libethercat/soe.h
        src/libethercat/timer.c
        src/libethercat/timer.h 
        src/ethercatdiag/rtems_config.h)

add_definitions(-DHAVE_NET_BPF_H)

add_executable(libethercat ${SOURCE_FILES})