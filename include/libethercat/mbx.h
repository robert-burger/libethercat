/**
 * \file mbx.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Nov 2016
 *
 * \brief ethercat mailbox common access functions
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

#ifndef __LIBETHERCAT_MBX_H__
#define __LIBETHERCAT_MBX_H__

#include "libethercat/common.h"
#include "libethercat/ec.h"

//! mailbox types
enum {
    EC_MBX_ERR = 0x00,   //!< error mailbox
    EC_MBX_AOE,          //!< ADS       over EtherCAT mailbox
    EC_MBX_EOE,          //!< Ethernet  over EtherCAT mailbox
    EC_MBX_COE,          //!< CANopen   over EtherCAT mailbox
    EC_MBX_FOE,          //!< File      over EtherCAT mailbox
    EC_MBX_SOE,          //!< Servo     over EtherCAT mailbox
    EC_MBX_VOE = 0x0f    //!< Vendor    over EtherCAT mailbox
};

//! ethercat mailbox header
typedef struct PACKED ec_mbx_header {
    uint16_t  length;       //!< mailbox length
    uint16_t  address;      //!< mailbox address
    uint8_t   priority;     //!< priority
    unsigned  mbxtype : 4;  //!< mailbox type
    unsigned  counter : 4;  //!< counter
} PACKED ec_mbx_header_t;

//! ethercat mailbox data
typedef struct PACKED ec_mbx {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_data_t      mbx_data;    //!< mailbox data
} PACKED ec_mbx_t;

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! check if mailbox is empty
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param mbx_nr number of mailbox
 * \param nsec timeout in nanoseconds
 * \return full (0) or empty (1)
 */
int ec_mbx_is_empty(ec_t *pec, uint16_t slave, uint8_t mbx_nr, uint32_t nsec);

//! check if mailbox is full
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param mbx_nr number of mailbox
 * \param nsec timeout in nanoseconds
 * \return full (1) or empty (0)
 */
int ec_mbx_is_full(ec_t *pec, uint16_t slave, uint8_t mbx_nr, uint32_t nsec);

//! clears mailbox buffers 
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param read read mailbox (1) or write mailbox (0)
 */
void ec_mbx_clear(ec_t *pec, uint16_t slave, int read);

//! write mailbox to slave
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param nsec timeout in nanoseconds
 * \return working counter
 */
int ec_mbx_send(ec_t *pec, uint16_t slave, uint32_t nsec);

//! read mailbox from slave
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param nsec timeout in nanoseconds
 * \return working counter
 */
int ec_mbx_receive(ec_t *pec, uint16_t slave, uint32_t nsec);

//! push current received mailbox to received queue
/*!
 * \param[in] pec pointer to ethercat master
 * \param[in] slave slave number
 */
void ec_mbx_push(ec_t *pec, uint16_t slave);

#if 0 
{
#endif
#ifdef __cplusplus
}
#endif

#endif // __MBX_H__

