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

