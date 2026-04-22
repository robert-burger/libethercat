/**
 * \file mbx_gateway.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Apr 2026
 *
 * \brief ethercat mailbox gateway functions
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

#ifndef LIBETHERCAT_MBX_GATEWAY_H
#define LIBETHERCAT_MBX_GATEWAY_H

#include "libosal/types.h"

/** \defgroup mailbox_group Mailbox Gateway
 *
 * This modules contains EtherCAT mailbox gateway functions.
 *
 * @{
 */

// forward declarations
struct ec;
struct pool_entry;

struct echdr {
    osal_uint16_t length : 11;
    osal_uint16_t reserved : 1;
    osal_uint16_t data_type : 4;
};

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Enqueue MBX Gateway message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_mbx_gateway_enqueue(struct ec *pec, struct pool_entry *p_entry);

//! \brief Handle a mailbox gateway request.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in,out] echdr Pointer to EC-header of mailbox gateway request.
 *                      Response is returned here.
 * \param[in] len       Lenght of echdr buffer.
 *
 * \return EC_OK on success.
 */
int ec_mbx_gateway_handle(struct ec *pec, struct echdr *echdr, size_t len);

#ifdef __cplusplus
};
#endif

/** @} */

#endif // LIBETHERCAT_MBX_GATEWAY_H

