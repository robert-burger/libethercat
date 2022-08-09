/**
 * \file dc.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Nov 2016
 *
 * \brief ethercat distributed clocks support.
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
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBETHERCAT_DC_H
#define LIBETHERCAT_DC_H

#include <libosal/types.h>

#include "libethercat/common.h"
#include "libethercat/idx.h"
#include "libethercat/pool.h"

typedef struct ec_dc_info_slave {
    int use_dc;                 //!< flag, whether to use dc
    int next;                   //!< marker for next dc slave
    int prev;                   //!< marker for previous dc slave

    osal_uint8_t available_ports;    //!< available ports for dc config
    osal_int32_t receive_times[4];   //!< latched port receive times
            
    int type;                   //!< dc type, 0 = sync0, 1 = sync01
    osal_uint32_t cycle_time_0;      //!< cycle time of sync 0 [ns]
    osal_uint32_t cycle_time_1;      //!< cycle time of sync 1 [ns]
    osal_uint32_t cycle_shift;       //!< cycle shift time [ns]
} ec_dc_info_slave_t;

typedef struct ec_dc_info {
    osal_uint16_t master_address;
    int have_dc;
    int next;
    int prev;

    osal_uint64_t dc_time;
    osal_int64_t dc_sto;
    osal_uint64_t rtc_time;
    osal_int64_t rtc_sto;
    osal_int64_t act_diff;
    osal_int64_t timer_override;

    enum {
        dc_mode_master_clock = 0,
        dc_mode_ref_clock,
        dc_mode_master_as_ref_clock
    } mode;

    pool_entry_t *p_de_dc;
    idx_entry_t *p_idx_dc;
} ec_dc_info_t;

struct ec;

#ifdef __cplusplus
extern "C" {
#endif

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
 * on the slaves. this has to be done with \link ec_dc_sync0 \endlink
 * or \link ec_dc_sync01 \endlink.
 *
 *
 * \param pec ethercat master pointer
 * \return supported dc
 */
int ec_dc_config(struct ec *pec);

//! Configure EtherCAT slave for distributed clock sync0 and sync1 pulse
/*!
 * This function writes the cycle time, calculates the DC first start time 
 * wrt the cycle shift 
 * and enables sync0 and sync1 pulse generation on the corresponding device. 
 * It can also be use to disable DC's on the EtherCAT slave.
 * 
 * \param pec ethercat master pointer 
 * \param slave slave number
 * \param dc_active dc active flag
 * \param cycle_time_0 cycle time to program to fire sync0 in [ns]
 * \param cycle_time_1 cycle time to program to fire sync1 in [ns]
 * \param cycle_shift shift of first sync0 start in [ns]
 */
void ec_dc_sync(struct ec *pec, osal_uint16_t slave, osal_uint8_t dc_active, 
        osal_uint32_t cycle_time_0, osal_uint32_t cycle_time_1, osal_int32_t cycle_shift);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_DC_H

