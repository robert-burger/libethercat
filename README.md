# libethercat 

[![Build and Publish Debian Package](https://github.com/robert-burger/libethercat/actions/workflows/build-deb.yaml/badge.svg)](https://github.com/robert-burger/libethercat/actions/workflows/build-deb.yaml)
[![License: LGPL-V3](https://img.shields.io/badge/license-LGPL--V3-green.svg)](LICENSE)
[![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)](#)
[![Debian](https://img.shields.io/badge/Debian-A81D33?logo=debian&logoColor=fff)](#)
[![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?logo=ubuntu&logoColor=white)](#)
[![PikeOS](https://img.shields.io/badge/PikeOS-purple.svg)](LICENSE)

[![Packaging status](https://repology.org/badge/vertical-allrepos/libethercat.svg)](https://repology.org/project/libethercat/versions)

EtherCAT-master library  

This library provides all functionality to communicate with EtherCAT slaves attached to a Network interface. 

* support for a queue with mailbox init command for every slave
* distributed clock support
* mailbox support CoE, SoE, FoE, 

* scan EtherCAT bus in INIT-to-INIT state transition, thus switching to INIT in every case will do a bus re-scan.
* switching to PREOP state will enable full mailbox support if it is supported by the corresponding EtherCAT slave.
* a PREOP-to-SAFEOP prepares all slaves configured in a process data group
    * sending init command
    * calculate cyclic process data sizes
    * create sync manager configuration
    * create fmmu configuration
    * configure slave's for distributed clocks
    * cyclically provide meassurement of process data
* a SAFEOP-to-OP transition additionally sends command to the attached slaves in every group cycle.
* efficient frame scheduling: EtherCAT datagrams are only queued in state SAFEOP and OP. They will be put in one ore many Ethernet frames and sent all cyclically with one call to hw_tx()

# Network device access

- **raw socket** » The most common way sending ethernet frames in Linux is opening a raw network socket (`SOCK_RAW`). Therefor the program must either be run as root or with the capability flag `CAP_NET_RAW`. Either do sth like: `sudo setcap cap_net_raw=ep .libs/example_with_dc` or checkout grant_cap_net_raw kernel module from Flo Schmidt (https://gitlab.com/fastflo/open_ethercat).
- **raw_socket_mmaped** » Like above but don't use read/write to provide frame buffers to kernel and use mmaped buffers directly from kernel.
- **file** » Most performant/determinstic interface to send/receive frames with network hardware. Requires hacked linux network driver. Can also be used without interrupts to avoid context switches. For how to compile and use such a driver head over to [drivers readme](linux/README.md).
- **pikeos** » Special pikeos hardware access.

# Legal notices

This software was developed and made available to the public by the [Institute of Robotics and Mechatronics of the German Aerospace Center (DLR)](https://www.dlr.de/rm).

Please note that the use of the EtherCAT technology, the EtherCAT 
brand name and the EtherCAT logo is only permitted if the property 
rights of Beckhoff Automation GmbH are observed. For further 
information please contact Beckhoff Automation GmbH & Co. KG, 
Hülshorstweg 20, D-33415 Verl, Germany (www.beckhoff.com) or the 
EtherCAT Technology Group, Ostendstraße 196, D-90482 Nuremberg, 
Germany (ETG, www.ethercat.org).

## Usage 

See INTRODUCTION.md or [gh-pages](https://robert-burger.github.io/libethercat/) for reference.

## Build from source

`libethercat` uses autotools as build system. 

### Prerequisites

Ensure that `libosal` is installed in your system. You can get `libosal` from here https://github.com/robert-burger/libosal.git . To build `libethercat` from source execute something like:

```
git clone https://github.com/robert-burger/libethercat.git
cd libethercat
./bootstrap.sh
autoreconf -is
./configure
make
sudo make install
```

This will build and install a static as well as a dynamic library. For use in other project you can use the generated pkg-config file to retreave cflags and linker flags.

### CMake

Steps to build:
```bash
mkdir build
cd build
# Please change the path to the install dir. If you chose a global install you can omit the CMAKE_PREFIX_PATH option
# You can specify which EtherCAT devices should be included into the build with -DECAT_DEVICE="sock_raw+sock_raw_mmaped+..."
cmake -DCMAKE_PREFIX_PATH=<installdir of libosal> -DECAT_DEVICE="sock_raw+sock_raw_mmaped" ..
cmake --build . 
```

#### Configuration parameters

| Parameter         | Default  | Description                                                                                               |
|-------------------|----------|-----------------------------------------------------------------------------------------------------------|
| CMAKE_PREFIX_PATH |          | Install directory of the libosal                                                                          |
| ECAT_DEVICE       | sock_raw | List of EtherCAT devices as `+` separated list. Possible values: sock_raw+sock_raw_mmaped+file+pikeos+bpf |
| BUILD_SHARED_LIBS | OFF      | Flag to build shared libraries instead of static ones.                                                    |
| MBX_SUPPORT_COE   | ON       | Flag to enable or disable Mailbox CoE support
| MBX_SUPPORT_FOE   | ON       | Flag to enable or disable Mailbox FoE support
| MBX_SUPPORT_SOE   | ON       | Flag to enable or disable Mailbox SoE support
| MBX_SUPPORT_EOE   | ON       | Flag to enable or disable Mailbox EoE support

## Tools

libethercat also provides some small helper programs for the EtherCAT bus.

### eepromtool

With eepromtool you can read and write EtherCAT slave's eeprom. 

#### read eeprom

To do a read operation simply run:

    eepromtool -i eth1 -s 0 -r -f eeprom.bin

If no filename if specified, eepromtool will print the contents to stdout, which you can pipe to hexdump for example.

    eepromtool -i eth1 -s 0 -r | hexdump -v -C | less -S

#### write

To do a write operation simply run:

    eepromtool -i eth1 -s 0 -w -f eeprom.bin

If no filename if specified, eepromtool will read from stdin.

    cat eeprom.bin | eepromtool -i eth1 -s 0 -w

#### example_with_dc

This is a more complex example on how to use libethercat in realtime/control systems. 

It tries to do the following:

* Scan the EtherCAT network.
* Try to do a default configuration of the found slaves
* Configures distributed clocks of all slaves (if supported)
* Create a periodic realtime task which does cyclic data (process data) exchange.
* Do some jitter logging.
