//! ethercat distributed clocks
/*!
 * author: Robert Burger
 *
 * $Id$
 */

/*
 * This file is part of libethercat.
 *
 * libethercat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libethercat is distributed in the hope that 
 * it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libethercat
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "libethercat/dc.h"
#include "libethercat/hw.h"
#include "libethercat/ec.h"

/** 1st sync pulse delay in ns here 100ms */
#define SYNC_DELAY       ((int64_t)10000000)
//#define SYNC_DELAY       ((int64_t)100000000)

//! configure slave for distributed clock sync 0 pulse
/*/
 * \param pec ethercat master pointer 
 * \oaran slave slave number
 * \param active dc active flag
 * \param cycle_time cycle time to program to fire sync 0 in [ns]
 * \param cycle_shift shift of first sync 0 start in [ns]
 */
void ec_dc_sync0(ec_t *pec, uint16_t slave, int active, uint32_t cycle_time, int32_t cycle_shift) {
    uint16_t wkc;
    ec_slave_t *slv = &pec->slaves[slave];
    if (!(slv->features & 0x04)) // dc not available
        return;

    // stop cyclic operation, ready for next trigger
    uint8_t dc_active = 0;
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYNCACT, &dc_active, sizeof(dc_active), &wkc);

    // set write access to ethercat
    uint8_t dc_cuc = 0;
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCCUC, &dc_cuc, sizeof(dc_cuc), &wkc);

    if (active) 
        while (pec->dc.act_diff == 0) {
            // wait until dc's are ready
            struct timespec ts = { 0, 1000000 };
            nanosleep(&ts, NULL);
        }

    /* Calculate first trigger time, always a whole multiple of CyclTime rounded up
       plus the shifttime (can be negative)
       This insures best sychronisation between slaves, slaves with the same CyclTime
       will sync at the same moment (you can use CyclShift to shift the sync) */
    uint64_t rel_rtc_time = (pec->dc.timer_prev - pec->dc.rtc_sto);
    if (pec->dc.mode == 0) 
        rel_rtc_time -= pec->dc.act_diff;
    int64_t dc_start = rel_rtc_time + SYNC_DELAY + cycle_shift;
   
    // program first trigger time and cycle time
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCSTART0, &dc_start, sizeof(dc_start), &wkc);
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCCYCLE0, &cycle_time, sizeof(cycle_time), &wkc);

    if (active) {
        // activate distributed clock on slave
        dc_active = 1 + 2;
        ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYNCACT, &dc_active, sizeof(dc_active), &wkc);
    }
    
    ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: dc_systime %lld, dc_start %lld, cycletime %d, dc_active %X\n", 
            slave, rel_rtc_time, dc_start, cycle_time, dc_active);
}

//! configure slave for distributed clock sync 0 and sync 1 pulse
/*/
 * \param pec ethercat master pointer 
 * \oaran slave slave number
 * \param active dc active flag
 * \param cycle_time_0 cycle time to program to fire sync 0 in [ns]
 * \param cycle_time_1 cycle time to program to fire sync 1 in [ns]
 * \param cycle_shift shift of first sync 0 start in [ns]
 */
void ec_dc_sync01(ec_t *pec, uint16_t slave, int active, 
        uint32_t cycle_time_0, uint32_t cycle_time_1, int32_t cycle_shift) {
    uint16_t wkc;
    ec_slave_t *slv = &pec->slaves[slave];
    if (!(slv->features & 0x04)) // dc not available
        return;

    // stop cyclic operation, ready for next trigger
    uint8_t dc_active = 0;
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYNCACT, &dc_active, sizeof(dc_active), &wkc);

    // set write access to ethercat
    uint8_t dc_cuc = 0;
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCCUC, &dc_cuc, sizeof(dc_cuc), &wkc);

    /* Calculate first trigger time, always a whole multiple of CyclTime rounded up
       plus the shifttime (can be negative)
       This insures best sychronisation between slaves, slaves with the same CyclTime
       will sync at the same moment (you can use CyclShift to shift the sync) */
    uint64_t rel_rtc_time = (pec->dc.timer_prev - pec->dc.rtc_sto);
    int64_t dc_start = rel_rtc_time + SYNC_DELAY + cycle_shift;
   
    // program first trigger time and cycle time
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCSTART0, &dc_start, sizeof(dc_start), &wkc);
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCCYCLE0, &cycle_time_0, sizeof(cycle_time_0), &wkc);    
    ec_fpwr(pec, slv->fixed_address, EC_REG_DCCYCLE1, &cycle_time_1, sizeof(cycle_time_1), &wkc);

    if (active) {
        // activate distributed clock on slave
        dc_active = 1 + 2 + 4;
        ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYNCACT, &dc_active, sizeof(dc_active), &wkc);
    }
    
    ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: dc_systime %lld, dc_start %lld, "
            "cycletime_0 %d, cycletime_1 %d, dc_active %X\n", 
            slave, rel_rtc_time, dc_start, cycle_time_0, cycle_time_1, dc_active);
}

/* latched port time of slave */
/*inline*/ int32_t ec_dc_porttime(ec_t *pec, uint16_t slave, uint8_t port) {
    if (port < 4)
        return pec->slaves[slave].dc.receive_times[port].time;

    return 0;
}

/* calculate previous active port of a slave */
/*inline*/ uint8_t ec_dc_prevport(ec_t *pec, uint16_t slave, uint8_t port) {
    switch(port) {
#define eval_port(...) { \
            int port_idx[] = { __VA_ARGS__ }; \
            for (int i = 0; i < 3; ++i) \
                if(pec->slaves[slave].active_ports & (1 << port_idx[i])) \
                    return port_idx[i]; }
        case 0: 
            eval_port(2, 1, 3);
            break;
        case 1:
            eval_port(3, 0, 2);
            break;
        case 2:
            eval_port(1, 3, 0);
            break;
        case 3:
            eval_port(0, 2, 1);
            break;
    }      

    return port;
}

/* search unconsumed ports in parent, consume and return first open port */
/*inline*/ uint8_t ec_dc_parentport(ec_t *pec, uint16_t parent) {
    /* search order is important, here 3 - 1 - 2 - 0 */
    int port_idx[] = { 3, 1, 2, 0 };
    uint8_t parentport = 0;

    ec_log(100, "DISTRIBUTED_CLOCK", "parent %d, available_ports 0x%X\n", parent, pec->slaves[parent].dc.available_ports);

    for (int i = 0; i < 4; ++i) {
        int port = port_idx[i];

        if (pec->slaves[parent].dc.available_ports & (1 << port)) {
            parentport = port;
            pec->slaves[parent].dc.available_ports &= ~(1 << port);
            break;
        }
    }

    return parentport;
}

/**
 * Locate DC slaves, measure propagation delays.
 *
 * @param [in] dev              ethercat device
 * return boolean if slaves are found with DC
 */
int ec_dc_config(ec_t *pec) {
    int i, parent, child;
    int parenthold = 0;
    int32_t delay_childs, delay_previous_slaves, delay_slave_with_childs;
    uint8_t entry_port = 0;
    uint16_t wkc;

    pec->dc.have_dc = 0;
    pec->dc.master_address = 0;

    // latch DC receive time of all slaves
    int32_t dc_time0 = 0;
    ec_bwr(pec, EC_REG_DCTIME0, &dc_time0, sizeof(dc_time0), &wkc);

    int prev = -1;

    for (int slave = 0; slave < pec->slave_cnt; slave++) {        
        ec_slave_t *slv = &pec->slaves[slave];
        slv->dc.available_ports = slv->active_ports;

        if (!(slv->dc.use_dc && (slv->features & 0x04))) { // dc available
            for (i = 0; i < 4; ++i)
                slv->dc.receive_times[i].time = 0;

            parent = slv->parent;
            /* if non DC slave found on first position on branch hold root parent */
            if ((parent > 0) && (pec->slaves[parent].link_cnt > 2))
                parenthold = parent;

            /* if branch has no DC slaves consume port on root parent */
            if (parenthold && (slv->link_cnt == 1)) {
                ec_dc_parentport(pec, parenthold);
                parenthold = 0;
            }

            continue; // check next slave
        }
        
        if (!pec->dc.have_dc) {                
            pec->dc.master_address = slv->fixed_address;
            pec->dc.have_dc = 1;
            pec->dc.offset_compensation = 250;
            pec->dc.offset_compensation_cnt = 0;
            pec->dc.offset_compensation_max = 1000000;

            pec->dc.timer_override = -1;
            pec->dc.timer_prev = 0;

            pec->dc.prev_rtc = 0;
            pec->dc.prev_dc = 0;

            pec->dc.next = slave;
            slv->dc.prev = -1;                
        } else {
            pec->slaves[prev].dc.next = slave;
            slv->dc.prev = prev;
        }

        // this branch has DC slave so remove parenthold 
        parenthold = 0;
        prev = slave;

        // read receive time of all ports and try to find entry port
        slv->entry_port = 0;
        for (i = 0; i < 4; ++i) {
            slv->dc.receive_times[i].time = 0;
            ec_fprd(pec, slv->fixed_address, EC_REG_DCTIME0 + (i * sizeof(int32_t)), 
                    &slv->dc.receive_times[i].time, sizeof(slv->dc.receive_times[i].time), &wkc);

            if (slv->active_ports & (1 << i)) {
                if (slv->dc.receive_times[i].time < slv->dc.receive_times[slv->entry_port].time)
                    slv->entry_port = i; // port with smallest value is entry port

                ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: receive time port %d - %lld\n", 
                        slave, i, slv->dc.receive_times[i].time);
            }
        }

        // read out distributed clock slave offset and use as offset to set local time to 0
        int64_t dcsof = 0;
        ec_fprd(pec, slv->fixed_address, EC_REG_DCSOF, &dcsof, sizeof(dcsof), &wkc);
        dcsof *= -1;
        ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYSOFFSET, &dcsof, sizeof(dcsof), &wkc);

        if (pec->dc.master_address == slv->fixed_address) {
            pec->dc.dc_sto = dcsof;
            ec_timer_t tmr;
            ec_timer_gettime(&tmr);
            pec->dc.rtc_sto = tmr.sec * 1E9 + tmr.nsec;
        }

        // remove entry_port from available ports
        ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: available_ports 0x%X, removing entry_port %d\n", slave, 
                slv->dc.available_ports, entry_port);
        slv->dc.available_ports &= (uint8_t)~(1 << entry_port);

        // find parent with active distributed clocks
        int parent = slave;
        do {
            child = parent;
            parent = pec->slaves[parent].parent;
            if (parent >= 0)
                ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: checking parent %d, dc 0x%X\n", 
                        slave, parent, pec->slaves[parent].features);
        } while (!((parent == -1) || (pec->slaves[parent].dc.use_dc && (pec->slaves[parent].features & 0x04))));

        ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: parent %d\n", slave, parent);

        // no need to calculate propagation delay for distributed clocks master slave
        if (parent >= 0) {
            // find port on parent this slave is connected to
            slv->parentport = ec_dc_parentport(pec, parent);
            if (pec->slaves[parent].link_cnt == 1)
                slv->parentport = pec->slaves[parent].entry_port;

            ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: port on parentport %d\n", slave, slv->parentport);
            delay_childs = 0;
            delay_previous_slaves = 0;

            int parentport_previous = ec_dc_prevport(pec, parent, slv->parentport);
            int64_t port_time_parent = ec_dc_porttime(pec, parent, slv->parentport);
            int64_t port_time_parent_previous = ec_dc_porttime(pec, parent, parentport_previous);
            ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: ports %d, %d, times %d, %d\n", 
                    slave, slv->parentport, parentport_previous, 
                    port_time_parent, port_time_parent_previous);

            // this describes the delay from the actual slave and all it's childrens
            delay_slave_with_childs = port_time_parent - port_time_parent_previous;

            // if we have childrens, get the delay of all childs
            if (slv->link_cnt > 1)
                delay_childs = ec_dc_porttime(pec, slave, ec_dc_prevport(pec, slave, slv->entry_port)) -
                    ec_dc_porttime(pec, slave, slv->entry_port);
            if (delay_childs > delay_slave_with_childs) delay_childs = -delay_childs;

            // get delay of previous slaves on parent, if any
            if ((child - parent) > 0) {
                delay_previous_slaves = port_time_parent_previous -
                    ec_dc_porttime(pec, parent, pec->slaves[parent].entry_port);

                if (delay_previous_slaves < 0) 
                    delay_previous_slaves = -delay_previous_slaves;
            }

            // calculate current slave delay from delta times
            // assumption : forward delay equals return delay
            slv->pdelay = ((delay_slave_with_childs - delay_childs) / 2) + 
                delay_previous_slaves + pec->slaves[parent].pdelay;

            ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: delay_childs %d, delay_previous_slaves %d, delay_slave_with_childs %d\n", 
                    slave, delay_childs, delay_previous_slaves, delay_slave_with_childs);

            // write propagation delay
            ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: sysdelay %d\n", slave, slv->pdelay);
            ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYSDELAY, &slv->pdelay, sizeof(slv->pdelay), &wkc);
        }
    }

    uint64_t temp_dc = 0;
    ec_frmw(pec, pec->dc.master_address, EC_REG_DCSYSTIME,
            &temp_dc, 8, &wkc);

    return 1;
}
