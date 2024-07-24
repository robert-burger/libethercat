/**
 * \file hw_sock_raw.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief SOCK_RAW hardware access functions
 *
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

#ifndef LIBETHERCAT_HW_SOCK_RAW_H
#define LIBETHERCAT_HW_SOCK_RAW_H

#include <libethercat/hw.h>
#include <libethercat/ec.h>
#include <libosal/task.h>

typedef struct hw_sock_raw {
    struct hw_common common;

    int sockfd;                     //!< raw socket file descriptor
    
    osal_uint8_t send_frame[ETH_FRAME_LEN]; //!< \brief Static send frame.
    osal_uint8_t recv_frame[ETH_FRAME_LEN]; //!< \brief Static receive frame.

    // receiver thread settings
    osal_task_t rxthread;           //!< receiver thread handle
    int rxthreadrunning;            //!< receiver thread running flag
} hw_sock_raw_t;

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   pec                 Pointer to master structure.
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 * \param[in]   prio        Priority for receiver thread.
 * \param[in]   cpu_mask    CPU mask for receiver thread.
 *
 * \return 0 or negative error code
 */
int hw_device_sock_raw_open(struct hw_sock_raw *phw, struct ec *pec, const osal_char_t *devname, int prio, int cpu_mask);

#endif // LIBETHERCAT_HW_SOCK_RAW_H

