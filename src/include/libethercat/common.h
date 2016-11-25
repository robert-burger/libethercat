/**
 * \file common.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 23 Nov 2016
 *
 * \brief ethercat master common stuff
 *
 * 
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

#ifndef __LIBETHERCAT_COMMON_H__
#define __LIBETHERCAT_COMMON_H__

#include <stdint.h>
#include <pthread.h>

#define PACKED __attribute__((__packed__))

#ifdef min
#undef min
#endif

#define min(a, b)  ((a) < (b) ? (a) : (b))

#define free_resource(a) {  \
    if ((a)) {              \
        free((a));          \
        (a) = NULL;         \
    } }

#define alloc_resource(a, type, len) {      \
    if (len) {                              \
        (a) = (type *)malloc((len));        \
        memset((a), 0, (len)); } }

#define EC_MAX_DATA 4096

typedef union ec_data {
    uint8_t bdata[EC_MAX_DATA]; /* variants for easy data access */
    uint16_t wdata[EC_MAX_DATA];
    uint32_t ldata[EC_MAX_DATA];
} ec_data_t;

//! process data structure
typedef struct ec_pd {
    uint8_t *pd;        //!< pointer to process data
    size_t len;         //!< process data length
} ec_pd_t;

typedef uint16_t ec_state_t;
#define EC_STATE_INIT        0x01       //!< EtherCAT INIT state
#define EC_STATE_PREOP       0x02       //!< EtherCAT PREOP state
#define EC_STATE_BOOT        0x03       //!< EtherCAT BOOT state
#define EC_STATE_SAFEOP      0x04       //!< EtherCAT SAFEOP state
#define EC_STATE_OP          0x08       //!< EtherCAT OP state
#define EC_STATE_MASK        0x0F       //!< EtherCAT state mask
#define EC_STATE_ERROR       0x10       //!< EtherCAT ERROR
#define EC_STATE_RESET       0x10       //!< EtherCAT ERROR reset


#endif // __LIBETHERCAT_COMMON_H__

