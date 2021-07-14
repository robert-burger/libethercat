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

#ifndef __LIBETHERCAT_SOE_H__
#define __LIBETHERCAT_SOE_H__

#include "libethercat/common.h"

typedef struct ec_soe {
    pool_t *recv_pool;
} ec_soe_t;

//! ServoDrive attributes of an IDN
typedef struct PACKED ec_soe_idn_attribute {
    uint32_t evafactor   :16;       //!< Evalution factor .
    uint32_t length      :2;        //!< IDN length.
    uint32_t list        :1;        //!< IDN is list.
    uint32_t command     :1;        //!< IDN is command.
    uint32_t datatype    :3;        //!< Datatype according to ServoDrive Specification.
    uint32_t reserved1   :1;
    uint32_t decimals    :4;        //!< If float, number of decimals.
    uint32_t wp_preop    :1;        //!< Write protect in PREOP.
    uint32_t wp_safeop   :1;        //!< Write protect in SAFEOP.
    uint32_t wp_op       :1;        //!< Write protect in OP.
    uint32_t reserved2   :1;
} PACKED ec_soe_idn_attribute_t;

//! ServoDrive elements of an IDN
enum ec_soe_element {
    EC_SOE_DATASTATE   = 0x01,
    EC_SOE_NAME        = 0x02,      //!< idn name
    EC_SOE_ATTRIBUTE   = 0x04,      //!< idn attributes
    EC_SOE_UNIT        = 0x08,      //!< idn unit
    EC_SOE_MIN         = 0x10,      //!< idn minimum value
    EC_SOE_MAX         = 0x20,      //!< idn maximum value
    EC_SOE_VALUE       = 0x40,      //!< idn value
    EC_SOE_DEFAULT     = 0x80       
};

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! initialize SoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_soe_init(ec_t *pec, uint16_t slave);

//! \brief Wait for SoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] pp_entry  Returns pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_soe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry);

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
void ec_soe_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry);

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
 * \param[in] elements  ServoDrive elements according to ServoDrive Bus Specification.
 * \param[in,out] buf   Buffer for where to store the answer. This can either be
 *                      NULL with 'len' also set to zero, or a pointer to a 
 *                      preallocated buffer with set \p len field. In case \p buf is NULL
 *                      it will be allocated by \link ec_soe_read \endlink call. Then 
 *                      you have to make sure, that the buffer is freed by your application.
 * \param[in,out] len   Length of \p buf, see 'buf' descriptions. Returns length of answer.
 * \return 0 on successs
 */
int ec_soe_read(ec_t *pec, uint16_t slave, uint8_t atn, uint16_t idn, 
        uint8_t elements, uint8_t *buf, size_t *len);

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
int ec_soe_write(ec_t *pec, uint16_t slave, uint8_t atn, uint16_t idn, 
        uint8_t elements, uint8_t *buf, size_t len);

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
int ec_soe_generate_mapping(ec_t *pec, uint16_t slave);

#if 0 
{
#endif
#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_SOE_H__

