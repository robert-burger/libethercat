/**
 * \file dc.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief ethercat distributed clocks
 *
 * These functions are used to enable distributed clocks support
 * on the EtherCAT master and to configure one ore more EtherCAT
 * slaves to enable the sync0 and/or sync1 pulse generation.
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
 * If not, see <www.gnu.org/licenses/>.
 */

/* autoconf header goes first */
#include <libethercat/config.h>

/* system includes */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

/* libethercat header includes */
#include "libethercat/dc.h"
#include "libethercat/hw.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"

//! Configure EtherCAT slave for distributed clock sync0 and sync1 pulse
/*!
 * This function writes the cycle time, calculates the DC first start time 
 * wrt the cycle shift 
 * and enables sync0 and sync1 pulse generation on the corresponding device. 
 * It can also be use to disable DC's on the EtherCAT slave.
 * 
 * \param pec ethercat master pointer 
 * \param slave slave number
 * \param active dc active flag
 * \param cycle_time_0 cycle time to program to fire sync0 in [ns]
 * \param cycle_time_1 cycle time to program to fire sync1 in [ns]
 * \param cycle_shift shift of first sync0 start in [ns]
 */
void ec_dc_sync(ec_t *pec, osal_uint16_t slave, osal_uint8_t active, 
        osal_uint32_t cycle_time_0, osal_uint32_t cycle_time_1, osal_int32_t cycle_shift) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    if ((slv->features & 0x04u) != 0u) { // dc available
        osal_uint8_t dc_cuc = 0u;
        osal_uint8_t dc_active = 0u;
        osal_uint16_t wkc = 0u;
        osal_int64_t dc_start = 0;
        osal_int64_t dc_time = 0;
        osal_int64_t tmp_time = 0;

        // deactivate DC's to stop cyclic operation, enable write access of dc's
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYNCACT, &dc_active, sizeof(dc_active), &wkc);
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCCUC, &dc_cuc, sizeof(dc_cuc), &wkc);

        dc_active = active;

        // program first trigger time and cycle time
        check_ec_fprd(pec, slv->fixed_address, EC_REG_DCSYSTIME, &dc_time, sizeof(dc_time), &wkc);

        // Calculate DC start time as a sum of the actual EtherCAT master time,
        // the generic first sync delay and the cycle shift. the first sync delay 
        // has to be a multiple of cycle time.  
        tmp_time = ((dc_time - (osal_int64_t)pec->dc.rtc_time) / (osal_int64_t)pec->dc.timer_override) + 100;
        dc_start = (osal_int64_t)pec->dc.rtc_time + (tmp_time * (osal_int64_t)pec->dc.timer_override) + cycle_shift;
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSTART0, &dc_start, sizeof(dc_start), &wkc);
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCCYCLE0, &cycle_time_0, sizeof(cycle_time_0), &wkc);    
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCCYCLE1, &cycle_time_1, sizeof(cycle_time_1), &wkc);
        
        // activate distributed clock on slave
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYNCACT, &dc_active, sizeof(dc_active), &wkc);

        if (dc_active != 0u) {
            ec_log(10, "DC_SYNC", "slave %2d: dc_systime %" PRIu64 ", dc_start "
                    "%" PRId64 ", slv dc_time %" PRId64 "\n", slave, pec->dc.rtc_time, dc_start, dc_time);
            ec_log(10, "DC_SYNC", "slave %2d: cycletime_0 %d, cycletime_1 %d, "
                    "dc_active %d\n", slave, cycle_time_0, cycle_time_1, dc_active);
        } else {
            // if not active, the DC's stay inactive
            ec_log(100, "DC_SYNC", "slave %2hu: disabled distributed clocks\n", slave);
        }
    }
}

static osal_uint8_t eval_port(ec_t *pec, osal_uint16_t slave, osal_uint8_t a, osal_uint8_t b, osal_uint8_t c, osal_uint8_t def) {
    int ret_port = def;

    const osal_uint8_t port_idx[] = { a, b, c }; 
    for (osal_uint8_t i = 0u; i < 3u; ++i) { 
        if ((pec->slaves[slave].active_ports & (1u << port_idx[i])) != 0u) { 
            ret_port = port_idx[i]; 
            break;
        } 
    }

    return ret_port;
}

//! determine previous active port, starting at port
static inline osal_uint8_t ec_dc_previous_port(ec_t *pec, osal_uint16_t slave, osal_uint8_t port) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    osal_uint8_t ret_port = port;

    switch(port) {
        case 0: {
            ret_port = eval_port(pec, slave, 2, 1, 3, port);
            break;
        }
        case 1: {
            ret_port = eval_port(pec, slave, 3, 0, 2, port);
            break;
        }
        case 2: {
            ret_port = eval_port(pec, slave, 1, 3, 0, port);
            break;
        }
        case 3: {
            ret_port = eval_port(pec, slave, 0, 2, 1, port);
            break;
        }      
        default: {
            break;
        }
    }

    return ret_port;
}

//! search for available port and return
static inline osal_uint8_t ec_dc_port_on_parent(ec_t *pec, osal_uint16_t parent) {
    // read et1100 docs about port order
    const osal_uint8_t port_idx[] = { 3, 1, 2, 0 };
    osal_uint8_t port_on_parent = 0;
    
    assert(pec != NULL);
    assert(parent < pec->slave_cnt);

    ec_log(100, "DC_CONFIG", "parent %d, available_ports mask 0x%X\n", 
            parent, pec->slaves[parent].dc.available_ports);

    for (int i = 0; i < 4; ++i) {
        osal_uint8_t port = port_idx[i];

        if ((pec->slaves[parent].dc.available_ports & (1u << port)) != 0u) {
            // mask it out for next port search
            port_on_parent = port;
            pec->slaves[parent].dc.available_ports &= ~(1u << port);
            break;
        }
    }
    
    ec_log(100, "DC_CONFIG", "parent %d, port_on_parent %d\n", 
            parent, port_on_parent);

    return port_on_parent;
}

//! Prepare EtherCAT master and slaves for distributed clocks
/*!
 * Check all slaves if they support distributed clocks and measure delays.
 *
 * DC support can be determined from EtherCAT slave's feature register (0x08). 
 * which is automatically read on EtherCAT master's INIT phase. On all 
 * slaves supporting DC's the system time is read and written to
 * the system time offset to set slave time to 0. afterwards the port times
 * are taken and the propagation delays are calculated and written. 
 *
 * This function does not enable distributed clock sync0/1 pulse generation
 * on the slaves. this has to be done with \link ec_dc_sync \endlink
 *
 * \param pec ethercat master pointer
 * \return supported dc
 */
int ec_dc_config(struct ec *pec) {
    assert(pec != NULL);

    osal_uint32_t i;
    int parent;
    int child;
    int parenthold = 0;
    osal_int64_t delay_childs;
    osal_int64_t delay_previous_slaves;
    osal_int64_t delay_slave_with_childs;
    osal_uint16_t wkc;

    pec->dc.have_dc = 0;
    pec->dc.master_address = 0;

    // latch DC receive time on slaves
    osal_int32_t dc_time0 = 0;
    check_ec_bwr(pec, EC_REG_DCTIME0, &dc_time0, sizeof(dc_time0), &wkc);
    int prev = -1;

    for (osal_uint16_t slave = 0; slave < pec->slave_cnt; slave++) {        
        ec_slave_ptr(slv, pec, slave);
        slv->dc.available_ports = slv->active_ports;

        if ((slv->dc.use_dc == 0) || ((slv->features & 0x04u) == 0u)) {
            ec_log(100, "DC_CONFIG", "slave %2d: not using DC (use %d, features 0x%X)\n", 
                    slave, slv->dc.use_dc, slv->features);

            slv->dc.use_dc = 0;

            // dc not available or not activated
            for (i = 0u; i < 4u; ++i) {
                slv->dc.receive_times[i] = 0;
            }

            parent = slv->parent;
            
            // if non DC slave found on first position on branch hold root 
            // parent
            if ((parent > 0) && (pec->slaves[parent].link_cnt > 2u)) {
                parenthold = parent;
            }

            // if branch has no DC slaves consume port on root parent
            if (parenthold && (slv->link_cnt == 1u)) {
                (void)ec_dc_port_on_parent(pec, parenthold);
                parenthold = 0;
            }

            continue; // check next slave
        }
        
        if (!pec->dc.have_dc) {
            // first slave with enabled dc's
            pec->dc.master_address = slv->fixed_address;
            pec->dc.have_dc = 1;
            pec->dc.rtc_time = 0;
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
        check_ec_fprd(pec, slv->fixed_address, EC_REG_DCTIME0, &slv->dc.receive_times[0], 4 * sizeof(slv->dc.receive_times[0]), &wkc);
        slv->entry_port = 0;
        for (i = 0u; i < 4u; ++i) {
            if ((slv->active_ports & (1u << i)) != 0u) {
                if (slv->dc.receive_times[i] < slv->dc.receive_times[slv->entry_port]) {
                    // port with smallest value is entry port
                    slv->entry_port = i; 
                }

                ec_log(100, "DC_CONFIG", "slave %2d: receive time port %d - %d\n", 
                        slave, i, slv->dc.receive_times[i]);
            }
        }
            
        // remove entry_port from available ports
        ec_log(100, "DC_CONFIG", "slave %2d: available_ports 0x%X, "
                "entry_port %d\n", slave, slv->dc.available_ports, slv->entry_port);
        slv->dc.available_ports &= (osal_uint8_t)~(1u << (osal_uint32_t)slv->entry_port);

        // read out distributed clock slave offset and use as offset 
        // to set local time to 0
        osal_int64_t dcsof = 0;
        check_ec_fprd(pec, slv->fixed_address, EC_REG_DCSOF, &dcsof, sizeof(dcsof), &wkc);
        dcsof *= -1;
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYSOFFSET, &dcsof, sizeof(dcsof), &wkc);

        // store our system time offsets if we got the dc master clock
        if (pec->dc.master_address == slv->fixed_address) {
            pec->dc.dc_sto = dcsof;
            pec->dc.rtc_sto = osal_timer_gettime_nsec();
            ec_log(100, "DC_CONFIG", "initial dc_sto %" PRId64 ", rtc_sto %" PRId64 "\n", pec->dc.dc_sto, pec->dc.rtc_sto);
        }

        // find parent with active distributed clocks
        parent = slave;
        do {
            child = parent;
            parent = pec->slaves[parent].parent;
            if (parent >= 0) {
                ec_log(100, "DC_CONFIG", "slave %2d: checking parent "
                        "%d, dc 0x%X\n", slave, parent, pec->slaves[parent].features);
            }
        } while (!((parent == -1) || 
                    ((pec->slaves[parent].dc.use_dc) && 
                     (pec->slaves[parent].features & 0x04u))));

        ec_log(100, "DC_CONFIG", "slave %2d: parent %d\n", slave, parent);

        // no need to calculate propagation delay for distributed clocks master slave
        slv->pdelay = 0;
        if (parent >= 0) {
            // find port on parent this slave is connected to
            slv->port_on_parent = ec_dc_port_on_parent(pec, parent);
            if (pec->slaves[parent].link_cnt == 1u) { 
                slv->port_on_parent = pec->slaves[parent].entry_port;
            }

            ec_log(100, "DC_CONFIG", "slave %2d: port on port_on_parent "
                    "%d\n", slave, slv->port_on_parent);
            delay_childs = 0;
            delay_previous_slaves = 0;

            int port_on_parent_previous = ec_dc_previous_port(pec, parent, slv->port_on_parent);
            osal_int64_t port_time_parent = pec->slaves[parent].dc.receive_times[slv->port_on_parent];
            osal_int64_t port_time_parent_previous = pec->slaves[parent].dc.receive_times[port_on_parent_previous];

            ec_log(100, "DC_CONFIG", "slave %2d: ports %d, %d, times "
                    "%" PRId64 ", %" PRId64 "\n", slave, slv->port_on_parent, port_on_parent_previous, 
                    port_time_parent, port_time_parent_previous);

            // this describes the delay from the actual slave and all it's 
            // childrens
            delay_slave_with_childs = port_time_parent - port_time_parent_previous;

            // if we have childrens, get the delay of all childs
            if (slv->link_cnt > 1u) {
                int prev_port = ec_dc_previous_port(pec, slave, slv->entry_port);
                delay_childs = pec->slaves[slave].dc.receive_times[prev_port] - pec->slaves[slave].dc.receive_times[slv->entry_port];
            }

            if (delay_childs > delay_slave_with_childs) {
                delay_childs = -delay_childs;
            }

            // get delay of previous slaves on parent, if any
            if ((child - parent) > 0) {
                delay_previous_slaves = port_time_parent_previous - pec->slaves[parent].dc.receive_times[pec->slaves[parent].entry_port];

                if (delay_previous_slaves < 0) {
                    delay_previous_slaves = -delay_previous_slaves;
                }
            }

            // calculate current slave delay from delta times
            // assumption : forward delay equals return delay
            slv->pdelay = (((delay_slave_with_childs - delay_childs) / 2) + 
                delay_previous_slaves + pec->slaves[parent].pdelay);

            ec_log(100, "DC_CONFIG", "slave %2d: delay_childs %" PRId64 ", "
                    "delay_previous_slaves %" PRId64 ", delay_slave_with_childs %" PRId64 "\n", 
                    slave, delay_childs, delay_previous_slaves, 
                    delay_slave_with_childs);

        }

        // write propagation delay
        ec_log(100, "DC_CONFIG", "slave %2d: sysdelay %d\n", slave, slv->pdelay);
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYSDELAY, &slv->pdelay, sizeof(slv->pdelay), &wkc);
    }

    osal_uint64_t temp_dc = 0;
    check_ec_frmw(pec, pec->dc.master_address, EC_REG_DCSYSTIME, &temp_dc, 8, &wkc);

    return EC_OK;
}

