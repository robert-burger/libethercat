# libethercat [![Build Status](https://rmc-jenkins.robotic.dlr.de/jenkins/job/common/job/libethercat/job/master/badge/icon)](https://rmc-jenkins.robotic.dlr.de/jenkins/job/common/job/libethercat/job/master/)

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

# Legal notices

Please note that the use of the EtherCAT technology, the EtherCAT 
brand name and the EtherCAT logo is only permitted if the property 
rights of Beckhoff Automation GmbH are observed. For further 
information please contact Beckhoff Automation GmbH & Co. KG, 
Hülshorstweg 20, D-33415 Verl, Germany (www.beckhoff.com) or the 
EtherCAT Technology Group, Ostendstraße 196, D-90482 Nuremberg, 
Germany (ETG, www.ethercat.org).

# Tools
Tools

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

### example_with_dc

This is a more complex example on how to use libethercat in realtime/control systems. 

It tries to do the following:

* Scan the EtherCAT network.
* Try to do a default configuration of the found slaves
* Configures distributed clocks of all slaves (if supported)
* Create a periodic realtime task which does cyclic data (process data) exchange.
* Do some jitter logging.
