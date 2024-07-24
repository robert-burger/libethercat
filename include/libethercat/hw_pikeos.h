/**
 * \file hw_pikeos.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief PIKEOS device hardware access functions
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

#ifndef LIBETHERCAT_HW_PIKEOS_H
#define LIBETHERCAT_HW_PIKEOS_H

#include <libethercat/hw.h>

typedef struct hw_pikeos {
    struct hw_common common;

    osal_bool_t use_sbuf;

    vm_file_desc_t fd;                      //!< \brief Driver file descriptor.
    drv_sbuf_desc_t sbuf;                   //!< \brief Driver SBUF descriptor.
    
    osal_uint8_t send_frame[ETH_FRAME_LEN]; //!< \brief Static send frame.
    osal_uint8_t recv_frame[ETH_FRAME_LEN]; //!< \brief Static receive frame.
} hw_pikeos_t;

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 *
 * \return 0 or negative error code
 */
int hw_device_pikeos_open(struct hw_pikeos *phw, const osal_char_t *devname);

#endif // LIBETHERCAT_HW_PIKEOS_H


