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
#include "libethercat/coe.h"
#include "libethercat/soe.h"
#include "libethercat/foe.h"
#include "libethercat/eoe.h"
#include "libethercat/pool.h"
    
#define MESSAGE_POOL_DEBUG(type) {}

#define _MESSAGE_POOL_DEBUG(type) \
{                                                                                       \
    int avail = 0;                                                                      \
    sem_getvalue(&slv->mbx.message_pool_ ## type->avail_cnt, &avail);                   \
    ec_log(10, __func__, "slave %d: " #type " pool avail %d\n", slave, avail);          \
}

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
typedef struct PACKED ec_mbx_buffer {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_data_t      mbx_data;    //!< mailbox data
} PACKED ec_mbx_buffer_t;

#define MBX_HANDLER_FLAGS_SEND  ((uint32_t)0x00000001u)
#define MBX_HANDLER_FLAGS_RECV  ((uint32_t)0x00000002u)

typedef struct ec_mbx {
    uint32_t handler_flags;
    pthread_cond_t recv_cond;
    pthread_mutex_t recv_mutex;

    pthread_t recv_tid;
    
    ec_t *pec;
    int slave;

    idx_queue_t idx_q;
    pool_t *message_pool_free;
    pool_t *message_pool_queued;

    pool_entry_t *sent[8];

    ec_coe_t coe;
    ec_soe_t soe;
    ec_foe_t foe;
    ec_eoe_t eoe;
} ec_mbx_t;

// forward declarations
struct ec;
typedef struct ec ec_t;

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! \brief Initialize mailbox structure.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_mbx_init(ec_t *pec, uint16_t slave);

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

//! \brief Push current received mailbox to received queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry);

void ec_mbx_sched_read(ec_t *pec, uint16_t slave);

#if 0 
{
#endif
#ifdef __cplusplus
}
#endif

#endif // __MBX_H__

