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

#ifndef LIBETHERCAT_MBX_H
#define LIBETHERCAT_MBX_H

#include "libethercat/common.h"
#include "libethercat/coe.h"
#include "libethercat/soe.h"
#include "libethercat/foe.h"
#include "libethercat/eoe.h"
#include "libethercat/pool.h"

#define MAILBOX_WRITE   (0)
#define MAILBOX_READ    (1)

// forward declarations
struct ec;
typedef struct ec ec_t;
    
#define MESSAGE_POOL_DEBUG(type) {}

#define _MESSAGE_POOL_DEBUG(type) \
{                                                                                       \
    int avail = 0;                                                                      \
    sem_getvalue(&slv->mbx.message_pool_ ## type->avail_cnt, &avail);                   \
    ec_log(10, __func__, "slave %d: " #type " pool avail %d\n", slave, avail);          \
}

//! mailbox types
enum {
    EC_MBX_ERR = 0x00,          //!< \brief error mailbox
    EC_MBX_AOE,                 //!< \brief ADS       over EtherCAT mailbox
    EC_MBX_EOE,                 //!< \brief Ethernet  over EtherCAT mailbox
    EC_MBX_COE,                 //!< \brief CANopen   over EtherCAT mailbox
    EC_MBX_FOE,                 //!< \brief File      over EtherCAT mailbox
    EC_MBX_SOE,                 //!< \brief Servo     over EtherCAT mailbox
    EC_MBX_VOE = 0x0f           //!< \brief Vendor    over EtherCAT mailbox
};

//! ethercat mailbox header
typedef struct PACKED ec_mbx_header {
    uint16_t  length;           //!< \brief mailbox length
    uint16_t  address;          //!< \brief mailbox address
    uint8_t   priority;         //!< \brief priority
    unsigned  mbxtype : 4;      //!< \brief mailbox type
    unsigned  counter : 4;      //!< \brief counter
} PACKED ec_mbx_header_t;

//! ethercat mailbox data
typedef struct PACKED ec_mbx_buffer {
    ec_mbx_header_t mbx_hdr;    //!< \brief mailbox header
    ec_data_t      mbx_data;    //!< \brief mailbox data
} PACKED ec_mbx_buffer_t;

typedef struct ec_mbx {
    uint32_t handler_flags;     //!< \brief Flags signalling handler recv of send action.
    pthread_cond_t recv_cond;   //!< \brief Sync condition for handler wait.
    pthread_mutex_t recv_mutex; //!< \brief Sync mutex for handler flags.

    int handler_running;        //!< \brief Mailbox handler thread running flag.
    ec_t *pec;                  //!< \brief Pointer to ethercat master structure.
                                /*!< 
                                 * Used by handler thread wrapper to call mailbox
                                 * handler function.
                                 */
    int slave;                  //!< \brief Number of EtherCAT slave.
                                /*!< 
                                 * Used by handler thread wrapper to call mailbox
                                 * handler function.
                                 */
    pthread_t handler_tid;      //!< \brief Mailbox handler thread handle.

    pool_t *message_pool_free;
    pool_t *message_pool_queued;

    ec_coe_t coe;
    ec_soe_t soe;
    ec_foe_t foe;
    ec_eoe_t eoe;
    
    uint8_t *sm_state;          //!< Sync manager state of read mailbox.
                                /*!<
                                 * The field is used to receive the mailbox 
                                 * sync manager state. This is useful to 
                                 * determine if the mailbox is full or empty
                                 * without the need to poll the state manually.
                                 */
} ec_mbx_t;

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

//! \brief Deinit mailbox structure
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_mbx_deinit(ec_t *pec, uint16_t slave);

//! \brief Enqueue mailbox message to send queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry);

//! \brief Trigger read of mailbox.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
void ec_mbx_sched_read(ec_t *pec, uint16_t slave);

#if 0 
{
#endif
#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_MBX_H

