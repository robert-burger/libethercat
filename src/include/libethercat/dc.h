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

#ifndef __LIBETHERCAT_DC_H__
#define __LIBETHERCAT_DC_H__

#include "libethercat/common.h"

typedef struct PACKED ec_dc_info_slave {
    int use_dc;
    int next;
    int prev;

    int available_ports;
    int32_t receive_times[4];
            
    int64_t system_time_offset;
            
    int type;              //! dc type, 0 = sync0, 1 = sync01
    uint32_t cycle_time_0; //! cycle time of sync 0 [ns]
    uint32_t cycle_time_1; //! cycle time of sync 1 [ns]
    uint32_t cycle_shift;  //! cycle shift time [ns]
} ec_dc_info_slave_t;

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

//! Configure EtherCAT slave for distributed clock sync0 pulse
/*!
 * This function writes the cycle time, calculates the DC first start time 
 * wrt the cycle shift 
 * and enables sync0 pulse generation on the corresponding device. It can also
 * be use to disable DC's on the EtherCAT slave.
 *
 * \param pec ethercat master pointer 
 * \param slave slave number
 * \param active dc active flag
 * \param cycle_time cycle time to program to fire sync0 in [ns]
 * \param cycle_shift shift of first sync0 start in [ns]
 */
void ec_dc_sync0(struct ec *pec, uint16_t slave, int active, 
        uint32_t cycle_time, int32_t cycle_shift);

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
void ec_dc_sync01(struct ec *pec, uint16_t slave, int active, 
        uint32_t cycle_time_0, uint32_t cycle_time_1, int32_t cycle_shift);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_DC_H__

