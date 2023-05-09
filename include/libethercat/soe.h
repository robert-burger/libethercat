/**
 * \file soe.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 25 Nov 2016
 *
 * \brief EtherCAT SoE functions.
 *
 * Implementaion of the Servodrive over EtherCAT mailbox protocol
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

#ifndef LIBETHERCAT_SOE_H
#define LIBETHERCAT_SOE_H

#include <libosal/types.h>
#include <libosal/mutex.h>

#include "libethercat/common.h"

typedef struct ec_soe {
    pool_t recv_pool;
    osal_mutex_t lock;
} ec_soe_t;

//! ServoDrive attributes of an IDN
typedef struct PACKED ec_soe_idn_attribute {
    osal_uint32_t evafactor   :16;          //!< \brief Evalution factor .
    osal_uint32_t length      :2;           //!< \brief IDN length.
    osal_uint32_t list        :1;           //!< \brief IDN is list.
    osal_uint32_t command     :1;           //!< \brief IDN is command.
    osal_uint32_t datatype    :3;           //!< \brief Datatype according to ServoDrive Specification.
    osal_uint32_t reserved1   :1;
    osal_uint32_t decimals    :4;           //!< \brief If float, number of decimals.
    osal_uint32_t wp_preop    :1;           //!< \brief Write protect in PREOP.
    osal_uint32_t wp_safeop   :1;           //!< \brief Write protect in SAFEOP.
    osal_uint32_t wp_op       :1;           //!< \brief Write protect in OP.
    osal_uint32_t reserved2   :1;
} PACKED ec_soe_idn_attribute_t;

//! ServoDrive elements of an IDN
enum ec_soe_element {
    EC_SOE_DATASTATE   = 0x01,
    EC_SOE_NAME        = 0x02,              //!< \brief idn name
    EC_SOE_ATTRIBUTE   = 0x04,              //!< \brief idn attributes
    EC_SOE_UNIT        = 0x08,              //!< \brief idn unit
    EC_SOE_MIN         = 0x10,              //!< \brief idn minimum value
    EC_SOE_MAX         = 0x20,              //!< \brief idn maximum value
    EC_SOE_VALUE       = 0x40,              //!< \brief idn value
    EC_SOE_DEFAULT     = 0x80       
};

#ifdef __cplusplus
extern "C" {
#endif

//! initialize SoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_soe_init(ec_t *pec, osal_uint16_t slave);

//! deinitialize SoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_soe_deinit(ec_t *pec, osal_uint16_t slave);

//! \brief Enqueue SoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_soe_enqueue(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry);

//! Read elements of soe ID number
/*!
 * This reads all ID number elements from given EtherCAT slave's drive number. This function
 * enables read access to the ServoDrive dictionary on SoE enabled devices. The call to 
 * \link ec_soe_read \endlink is a synchronous call and will block until it's either finished or aborted.
 *
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] atn       AT number according to ServoDrive Bus Specification. 
 *                      In most cases this will be 0, if you are using a 
 *                      multi-drive device this may be 0,1,... .
 * \param[in] idn       ID number according to ServoDrive Bus Specification.
 *                      IDN's in range 1...32767 are described in the Specification,
 *                      IDN's greater 32768 are manufacturer specific.
 * \param[in,out] elements  ServoDrive elements according to ServoDrive Bus Specification.
 * \param[in,out] buf   Buffer for where to store the answer. 
 * \param[in,out] len   Length of \p buf, see 'buf' descriptions. Returns length of answer.
 * \return 0 on successs
 */
int ec_soe_read(ec_t *pec, osal_uint16_t slave, osal_uint8_t atn, osal_uint16_t idn, 
        osal_uint8_t *elements, osal_uint8_t *buf, osal_size_t *len);

//! Write elements of soe ID number
/*!
 * This writes all ID number elements from given EtherCAT slave's drive number. This function
 * enables write access to the ServoDrive dictionary on SoE enabled devices. The call to 
 * \link ec_soe_read \endlink is a synchronous call and will block until it's either finished or aborted.
 *
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] atn       AT number according to ServoDrive Bus Specification. 
 *                      In most cases this will be 0, if you are using a 
 *                      multi-drive device this may be 0,1,... .
 * \param[in] idn       ID number according to ServoDrive Bus Specification.
 *                      IDN's in range 1...32767 are described in the Specification,
 *                      IDN's greater 32768 are manufacturer specific.
 * \param[in] elements  ServoDrive elements according to ServoDrive Bus Specification.
 * \param[in] buf       Buffer with values to write to the given \p idn.
 * \param[in] len       Length of \p buf.
 * \return 0 on successs
 */
int ec_soe_write(ec_t *pec, osal_uint16_t slave, osal_uint8_t atn, osal_uint16_t idn, 
        osal_uint8_t elements, osal_uint8_t *buf, osal_size_t len);

//! Generate sync manager process data mapping via soe
/*!
 * This calculates the sync managers size according to the slave's ServoDrive configuration. 
 * Therefore it reads IDN 16 and IDN 24 to calculate the sync manager 2/3 sizes.
 *
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \return 0 on success
 */
int ec_soe_generate_mapping(ec_t *pec, osal_uint16_t slave);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_SOE_H

