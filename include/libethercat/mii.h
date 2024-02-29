/**
 * \file mii.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief EtherCAT MII access fuctions
 *
 * These functions are used to ensure access to the EtherCAT
 * slaves MII.
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

#ifndef LIBETHERCAT_MII_H
#define LIBETHERCAT_MII_H

#include <libosal/types.h>

#include "libethercat/common.h"
#include "libethercat/ec.h"

//! Read 16-bit word via MII.
/*!
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 * \param[in] phy_adr           Address of PHY attached via MII.
 * \param[in] phy_reg           Register of PHY selected by \p phy_adr.
 * \param[out] data             Returns read 16-bit data value.
 *
 * \retval EC_O    On success
 */
int ec_miiread(struct ec *pec, osal_uint16_t slave, 
        osal_uint8_t phy_adr, osal_uint16_t phy_reg, osal_uint16_t *data);

// Write 16-bit word via MII.
/*
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 * \param[in] phy_adr           Address of PHY attached via MII.
 * \param[in] phy_reg           Register of PHY selected by \p phy_adr.
 * \param[in] data              Data contains 16-bit data value to write.
 *
 * \retval EC_OK    On success
 */
int ec_miiwrite(struct ec *pec, osal_uint16_t slave, 
        osal_uint8_t phy_adr, osal_uint16_t phy_reg, osal_uint16_t *data);

#endif // LIBETHERCAT_MII_H

