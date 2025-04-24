/**
 * \file hw_stm32.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 * \author Marcel Beausencourt <marcel.beausencourt@dlr.de>
 *
 * \date 26 Nov 2024
 *
 * \brief file/char device hardware access functions
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

#ifndef LIBETHERCAT_HW_STM32_H
#define LIBETHERCAT_HW_STM32_H

#include "eth.h"
#include <libethercat/hw.h>
//#include <stm32_hal_legacy.h>
//#include <stm32h7xx_hal_def.h>

typedef struct hw_stm32 {
    struct hw_common common;

    int frames_sent;
    ETH_TxPacketConfig TxConfig;

    osal_uint8_t send_frame[ETH_FRAME_LEN]; //!< \brief Static send frame.
    osal_uint8_t recv_frame[ETH_FRAME_LEN]; //!< \brief Static receive frame.
} hw_stm32_t;

// HTONS MACRO -> configured in config.h (libethcat)
#if 0
#define HTONS(x) (((x) << 8) | ((x) >> 8))
#define htons(x) (((x) << 8) | ((x) >> 8))
#endif

#ifndef htons
#define htons(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))
#endif


//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   pec     	Pointer to EtherCAT master structure.
 *
 * \return 0 or negative error code
 */
int hw_device_stm32_open(struct hw_stm32 *phw, struct ec *pec);
int hw_device_stm32_send(struct hw_common *phw, ec_frame_t *pframe, pooltype_t pool_type);
int hw_device_stm32_recv(struct hw_common *phw);

#endif // LIBETHERCAT_HW_STM32_H

