/**
 * \file hw.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief hardware access functions
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

#ifndef __LIBETHERCAT_HW_H__
#define __LIBETHERCAT_HW_H__

#include <pthread.h>

#include "libethercat/datagram_pool.h"
#include "libethercat/datagram.h"

//! hardware structure
typedef struct hw {
    int sockfd;                     //!< raw socket file descriptor
    int mtu_size;                   //!< mtu size

    // receiver thread settings
    pthread_t rxthread;             //!< receiver thread handle
    int rxthreadrunning;            //!< receiver thread running flag
    int rxthreadprio;               //!< receiver thread priority
    int rxthreadcpumask;            //!< recevied thread cpu mask

    datagram_pool_t *tx_high;       //!< high priority datagrams
    datagram_pool_t *tx_low;        //!< low priority datagrams

    datagram_entry_t *tx_send[256]; //!< sent datagrams
} hw_t;   

#ifdef __cplusplus
extern "C" {
#endif

//! open a new hw
/*!
 * \param pphw return hw 
 * \param devname ethernet device name
 * \param prio receive thread prio
 * \param cpumask receive thread cpumask
 * \return 0 or negative error code
 */
int hw_open(hw_t **pphw, const char *devname, int prio, int cpumask);

//! destroys a hw
/*!
 * \param phw hw handle
 * \return 0 or negative error code
 */
int hw_close(hw_t *phw);

//! start sending queued ethercat datagrams
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx(hw_t *phw);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_HW_H__

