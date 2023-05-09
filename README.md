[![Build Status](https://rmc-jenkins.robotic.dlr.de/jenkins/job/common/job/libethercat/job/master/badge/icon)](https://rmc-jenkins.robotic.dlr.de/jenkins/job/common/job/libethercat/job/master/)

# libethercat
EtherCAT master library  

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


# Tools

libethercat also provides some small helper programs for the EtherCAT bus.

## eepromtool

With eepromtool you can read and write EtherCAT slave's eeprom. 

### read eeprom

To do a read operation simply run:

    eepromtool -i eth1 -s 0 -r -f eeprom.bin

If no filename if specified, eepromtool will print the contents to stdout, which you can pipe to hexdump for example.

    eepromtool -i eth1 -s 0 -r | hexdump -v -C | less -S

### write

To do a write operation simply run:

    eepromtool -i eth1 -s 0 -w -f eeprom.bin

If no filename if specified, eepromtool will read from stdin.

    cat eeprom.bin | eepromtool -i eth1 -s 0 -w


