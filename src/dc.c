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
 * libethercat is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * libethercat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public 
 * License along with libethercat (LICENSE.LGPL-V3); if not, write 
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth 
 * Floor, Boston, MA  02110-1301, USA.
 * 
 * Please note that the use of the EtherCAT technology, the EtherCAT 
 * brand name and the EtherCAT logo is only permitted if the property 
 * rights of Beckhoff Automation GmbH are observed. For further 
 * information please contact Beckhoff Automation GmbH & Co. KG, 
 * Hülshorstweg 20, D-33415 Verl, Germany (www.beckhoff.com) or the 
 * EtherCAT Technology Group, Ostendstraße 196, D-90482 Nuremberg, 
 * Germany (ETG, www.ethercat.org).
 *
 */

/* autoconf header goes first */
#include <libethercat/config.h>

/* system includes */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libethercat header includes */
#include "libethercat/dc.h"
#include "libethercat/hw.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"

// mesaure packet duration (important in case of master_as_ref_clock !)
static inline osal_uint64_t get_packet_duration(ec_t *pec) {
    assert(pec != NULL);

    pool_entry_t *p_entry;
    ec_datagram_t *p_dg;
    idx_entry_t *p_idx;

    osal_uint64_t duration = 0u;

    if (ec_index_get(&pec->idx_q, &p_idx) != EC_OK) {
        ec_log(1, "MASTER_TRANSCEIVE", "error getting ethercat index\n");
        //ret = EC_ERROR_OUT_OF_INDICES;
    } else if (pool_get(&pec->pool, &p_entry, NULL) != EC_OK) {
        ec_index_put(&pec->idx_q, p_idx);
        ec_log(1, "MASTER_TRANSCEIVE", "error getting datagram from pool\n");
        //ret = EC_ERROR_OUT_OF_DATAGRAMS;
    } else {
        p_dg = ec_datagram_cast(p_entry->data);

        (void)memset(p_dg, 0, sizeof(ec_datagram_t) + 2u + 2u);
        p_dg->cmd = EC_CMD_BRD;
        p_dg->idx = 0;
        p_dg->adr = 0;
        p_dg->len = 2;
        p_dg->irq = 0;

        p_entry->p_idx = p_idx;
        p_entry->user_cb = ({
                void anon_cb(struct ec *cb_pec, pool_entry_t *cb_p_entry, ec_datagram_t *cb_p_dg) {      
                    osal_binary_semaphore_post(&cb_p_entry->p_idx->waiter);
                } &anon_cb; });

        // queue frame and trigger tx
        pool_put(&pec->hw.tx_high, p_entry);

        osal_uint64_t start = osal_timer_gettime_nsec();
        hw_tx(&pec->hw);
        
        // wait for completion
        osal_timer_t to;
        osal_timer_init(&to, 100000000);
        int local_ret = osal_binary_semaphore_timedwait(&p_idx->waiter, &to);
        if (local_ret == OSAL_OK) {
            duration = osal_timer_gettime_nsec() - start;
        } else {
            duration = 0;
        }
        
        pool_put(&pec->pool, p_entry);
        ec_index_put(&pec->idx_q, p_idx);
    }

    return duration;
}

// port order (ET1100, Section 1, 3.2)
const osal_uint8_t forward_port_order[4][4] = { { 3, 1, 2, 0 }, { 2, 0, 3, 1 }, { 0, 3, 1, 2 }, { 1, 2, 0, 3 } };
const osal_uint8_t reverse_port_order[4][4] = { { 2, 1, 3, 0 }, { 3, 0, 2, 1 }, { 1, 3, 0, 2 }, { 0, 2, 1, 3 } };

//! determine previous active port, starting at port
static inline osal_uint8_t get_previous_active_port(ec_slave_t *slv, osal_uint8_t port) {
    assert(slv != NULL);
    assert((port >= 0) && (port < 4));

    const osal_uint8_t *port_order = &reverse_port_order[port][0];
    osal_uint8_t ret_port = port;

    for (osal_uint8_t i = 0u; i < 3u; ++i) { 
        if ((slv->active_ports & (1u << port_order[i])) != 0u) { 
            ret_port = port_order[i]; 
            break;
        } 
    }

    return ret_port;
}

//! search for available next outgoing port and return
static inline osal_uint8_t get_and_consume_next_available_port(ec_slave_t *slv, osal_uint8_t port) {
    assert(slv != NULL);
    assert((port >= 0) && (port < 4));

    const osal_uint8_t *port_order = &forward_port_order[port][0];
    osal_uint8_t ret_port = port;

    for (int i = 0; i < 4; ++i) {
        if ((slv->dc.available_ports & (1u << port_order[i])) != 0u) {
            // mask it out for next port search
            ret_port = port_order[i];
            slv->dc.available_ports &= ~(1u << ret_port);
            break;
        }
    }

    return ret_port;
}

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

    if ((slv->features & EC_REG_ESCSUP__DC_SUPP) != 0u) { // dc available
        osal_uint8_t dc_cuc = 0u;
        osal_uint8_t dc_active = 0u;
        osal_uint16_t wkc = 0u;
        osal_int64_t dc_start = 0;
        osal_int64_t dc_time = 0;
        osal_int64_t tmp_time = 0;
        osal_uint16_t speed_counter_start = 0u;

        // deactivate DC generation, enable write access of dc's, read local time
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYNCACT, &dc_active, sizeof(dc_active), &wkc);
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCCUC, &dc_cuc, sizeof(dc_cuc), &wkc);
        check_ec_fprd(pec, slv->fixed_address, EC_REG_DCSYSTIME, &dc_time, sizeof(dc_time), &wkc);

        // Calculate DC start time as a sum of the actual EtherCAT master time,
        // the generic first sync delay and the cycle shift. the first sync delay 
        // has to be a multiple of cycle time.  
        tmp_time = ((dc_time - (osal_int64_t)pec->dc.rtc_time) / (osal_int64_t)pec->main_cycle_interval) + 100;
        if (tmp_time < 0) tmp_time = 100;
        dc_start = (osal_int64_t)pec->dc.rtc_time + (tmp_time * (osal_int64_t)pec->main_cycle_interval) + cycle_shift;

        // program first trigger time and cycle time
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSTART0, &dc_start, sizeof(dc_start), &wkc);
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCCYCLE0, &cycle_time_0, sizeof(cycle_time_0), &wkc);    
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCCYCLE1, &cycle_time_1, sizeof(cycle_time_1), &wkc);

        // resetting the time control loop (ET1100, Section 1, 9.1.4)
        check_ec_fprd(pec, slv->fixed_address, EC_REG_DCSPEEDCNT, &speed_counter_start, sizeof(speed_counter_start), &wkc);
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSPEEDCNT, &speed_counter_start, sizeof(speed_counter_start), &wkc);
        
        // activate distributed clock on slave
        dc_active = active;
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
    osal_uint16_t wkc;

    pec->dc.have_dc = 0;
    pec->dc.master_address = 0;

    // latch DC receive time on slaves (ET1100, Section 1, 9.1.8)
    osal_int32_t dc_time0 = 0;
    check_ec_bwr(pec, EC_REG_DCTIME0, &dc_time0, sizeof(dc_time0), &wkc);
    int prev = -1;

    osal_uint64_t packet_duration = get_packet_duration(pec);
    ec_log(100, "DC_CONFIG", "master packet duration %" PRIu64 "\n", packet_duration);

    for (osal_uint16_t slave = 0; slave < pec->slave_cnt; slave++) {        
        ec_slave_ptr(slv, pec, slave);
        slv->dc.available_ports = slv->active_ports;

        if ((slv->dc.use_dc == 0) || ((slv->features & EC_REG_ESCSUP__DC_SUPP) == 0u)) {
            ec_log(100, "DC_CONFIG", "slave %2d: not using DC (use %d, features 0x%X)\n", 
                    slave, slv->dc.use_dc, slv->features);

            // dc not available or not activated
            slv->dc.use_dc = 0;
            for (i = 0u; i < 4u; ++i) { slv->dc.receive_times[i] = 0; }

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

        prev = slave;

        // read receive time of all ports and try to find entry port
        check_ec_fprd(pec, slv->fixed_address, EC_REG_DCTIME0, &slv->dc.receive_times[0], 4 * sizeof(slv->dc.receive_times[0]), &wkc);
        slv->entry_port = -1;
        for (i = 0u; i < 4u; ++i) {
            if ((slv->active_ports & (1u << i)) != 0u) {
                if ((slv->entry_port == -1) || (slv->dc.receive_times[i] < slv->dc.receive_times[slv->entry_port])) {
                    // port with smallest value is our entry port (which should be port 0 in most cases, see ET1100, Section 1, 3.7)
                    slv->entry_port = i; 
                }

                ec_log(100, "DC_CONFIG", "slave %2d: receive time port %d is %u\n", slave, i, slv->dc.receive_times[i]);
            }
        }

        if (slv->entry_port != 0) {
            // we have weird order, processing unit sits behind port 0, so processing is done in wrong order
            ec_log(5, "DC_CONFIG", "slave %2d: entry port is not the first port, check wiring order!\n", slave);

            // this is only needed on ET1200
            if (slv->type == 0x1200) {
                osal_uint8_t reverse = 1u;
                ec_fpwr(pec, slv->fixed_address, 0x936, &reverse, 1, &wkc);

                // latch DC receive time on slaves
                dc_time0 = 0;
                check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCTIME0, &dc_time0, sizeof(dc_time0), &wkc);
                check_ec_fprd(pec, slv->fixed_address, EC_REG_DCTIME0, &slv->dc.receive_times[0], 4 * sizeof(slv->dc.receive_times[0]), &wkc);
                for (i = 0u; i < 4u; ++i) {
                    ec_log(100, "DC_CONFIG", "slave %2d: receive time port %d is %u\n", 
                            slave, i, slv->dc.receive_times[i]);
                }
            }
        }
            
        // remove entry_port from available ports
        ec_log(100, "DC_CONFIG", "slave %2d: available_ports 0x%X, entry_port %d\n", slave, slv->dc.available_ports, slv->entry_port);
        slv->dc.available_ports &= (osal_uint8_t)~(1u << (osal_uint32_t)slv->entry_port);

        // read out distributed clock receive time ECAT Processing Unit and use 
        // it as negative offset to set slave's local time to 0. (Our reference clock
        // is also set to 0). (See ET1100, Section 1, 9.1.8)
        osal_int64_t dcsof = 0;
        check_ec_fprd(pec, slv->fixed_address, EC_REG_DCSOF, &dcsof, sizeof(dcsof), &wkc);
        dcsof *= -1;
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYSOFFSET, &dcsof, sizeof(dcsof), &wkc);

        // store our system time offsets if we got the dc master clock
        if (pec->dc.master_address == slv->fixed_address) {
            pec->dc.dc_sto = 0; // we did a reset 5 lines above
            pec->dc.rtc_sto = osal_timer_gettime_nsec();
            ec_log(100, "DC_CONFIG", "initial dc_sto %" PRId64 ", rtc_sto %" PRId64 "\n", pec->dc.dc_sto, pec->dc.rtc_sto);
        }

        // find parent with active distributed clocks
        for (parent = slv->parent; parent > 0; parent = pec->slaves[parent].parent) {
            ec_log(100, "DC_CONFIG", "slave %2d: checking parent %d, dc 0x%X\n", slave, parent, pec->slaves[parent].features);
            if ((pec->slaves[parent].dc.use_dc) && (pec->slaves[parent].features & EC_REG_ESCSUP__DC_SUPP)) {
                break;
            }
        }

        ec_log(100, "DC_CONFIG", "slave %2d: parent %d\n", slave, parent);
            
        const int t_diff = 20; // ET1100, Section 3, Table 56
        osal_uint32_t *times_slave  = &slv->dc.receive_times[0];
        int last_connected_port  = get_previous_active_port(slv, slv->entry_port);
            
        slv->dc.t_delay_childs = (osal_int32_t)times_slave[last_connected_port] - times_slave[slv->entry_port];

        if (parent >= 0) {
            int parent_previous_port;

            ec_slave_ptr(slv_parent, pec, parent);
            osal_uint32_t *times_parent = &slv_parent->dc.receive_times[0];

            // find port on parent this slave is connected to
            slv->port_on_parent = get_and_consume_next_available_port(slv_parent, slv_parent->entry_port);
            parent_previous_port = get_previous_active_port(slv_parent, slv->port_on_parent); 

            slv->dc.t_delay_with_childs = abs((osal_int32_t)times_parent[slv->port_on_parent] - times_parent[parent_previous_port]);
            slv->dc.t_delay_slave = abs((slv->dc.t_delay_with_childs - slv->dc.t_delay_childs + t_diff) / 2);
            slv->dc.t_delay_parent_previous = times_parent[parent_previous_port] - times_parent[slv_parent->entry_port];
            if (slv->entry_port == 0) {
                slv->pdelay = slv_parent->pdelay + slv->dc.t_delay_parent_previous + slv->dc.t_delay_slave;
            } else {
                slv->pdelay = slv_parent->pdelay + (abs(times_slave[slv->entry_port] - times_slave[0] + t_diff) / 2);
            }
        } else if (pec->dc.mode == dc_mode_master_as_ref_clock) {
            slv->dc.t_delay_with_childs = packet_duration;
            slv->dc.t_delay_slave = abs(slv->dc.t_delay_with_childs - slv->dc.t_delay_childs + t_diff) / 2;
            slv->dc.t_delay_parent_previous = 0;
            slv->pdelay = slv->dc.t_delay_slave;
        }
        
        ec_log(100, "DISTRIBUTED_CLOCK", "slave %2d: delay_childs %d, delay_slave %d, delay_parent_previous_slaves %d, delay_with_childs %d\n", 
                slave, slv->dc.t_delay_childs, slv->dc.t_delay_slave, slv->dc.t_delay_parent_previous, slv->dc.t_delay_with_childs);

        // write propagation delay
        ec_log(100, "DC_CONFIG", "slave %2d: sysdelay %d\n", slave, slv->pdelay);
        check_ec_fpwr(pec, slv->fixed_address, EC_REG_DCSYSDELAY, &slv->pdelay, sizeof(slv->pdelay), &wkc);
    }

    osal_uint64_t temp_dc = 0;
    check_ec_frmw(pec, pec->dc.master_address, EC_REG_DCSYSTIME, &temp_dc, 8, &wkc);

    return EC_OK;
}

