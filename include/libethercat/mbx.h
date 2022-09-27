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

#include <libosal/types.h>
#include <libosal/osal.h>

#include "libethercat/common.h"
#include "libethercat/coe.h"
#include "libethercat/soe.h"
#include "libethercat/foe.h"
#include "libethercat/eoe.h"
#include "libethercat/pool.h"

#define MAILBOX_WRITE   (osal_uint16_t)(0u)
#define MAILBOX_READ    (osal_uint16_t)(1u)

// forward declarations
struct ec;
typedef struct ec ec_t;
    
#define MESSAGE_POOL_DEBUG(type) {}

/*
#define N_MESSAGE_POOL_DEBUG(type) \
{                                                                                       \
    int avail = 0;                                                                      \
    sem_getvalue(&slv->mbx.message_pool_ ## type->avail_cnt, &avail);                   \
    ec_log(10, __func__, "slave %d: " #type " pool avail %d\n", slave, avail);          \
}
*/
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
    osal_uint16_t  length;           //!< \brief mailbox length
    osal_uint16_t  address;          //!< \brief mailbox address
    osal_uint8_t   priority;         //!< \brief priority
    osal_uint8_t   mbxtype : 4;      //!< \brief mailbox type
    osal_uint8_t   counter : 4;      //!< \brief counter
} PACKED ec_mbx_header_t;

//! ethercat mailbox data
typedef struct PACKED ec_mbx_buffer {
    ec_mbx_header_t mbx_hdr;    //!< \brief mailbox header
    ec_data_t      mbx_data;    //!< \brief mailbox data
} PACKED ec_mbx_buffer_t;

typedef struct ec_mbx {
    osal_uint32_t handler_flags;     //!< \brief Flags signalling handler recv of send action.
    osal_mutex_t sync_mutex;    //!< \brief Sync mutex for handler flags.
    osal_binary_semaphore_t sync_sem;

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
    osal_task_t handler_tid;    //!< \brief Mailbox handler thread handle.
    
    osal_mutex_t lock;          //!< mailbox lock
                                /*!<
                                 * Only one simoultaneous access to the 
                                 * EtherCAT slave mailbox is possible at the 
                                 * moment.
                                 */

    pool_t message_pool_send_queued;//!< \brief Pool with mailbox buffers ready to be sent.

    ec_coe_t coe;               //!< \brief Structure for CANOpen over EtherCAT mailbox.
    ec_soe_t soe;               //!< \brief Structure for Servodrive over EtherCAT mailbox.
    ec_foe_t foe;               //!< \brief Structure for File over EtherCAT mailbox.
    ec_eoe_t eoe;               //!< \brief Strucutre for Ethernet over EtherCAT mailbox.
    
    osal_uint8_t *sm_state;          //!< Sync manager state of read mailbox.
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

//! \brief Initialize mailbox structure.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_mbx_init(ec_t *pec, osal_uint16_t slave);

//! \brief Deinit mailbox structure
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_mbx_deinit(ec_t *pec, osal_uint16_t slave);

//! \brief Enqueue mailbox message to send queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue_head(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry);

//! \brief Enqueue mailbox message to send queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue_tail(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry);

//! \brief Trigger read of mailbox.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
void ec_mbx_sched_read(ec_t *pec, osal_uint16_t slave);

//! \brief Checks if mailbox protocol is supported by slave
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] mbx_flag  Mailbox protocol flag to be checked
 *
 * \return 1 if supported, 0 otherwise
 */
int ec_mbx_check(ec_t *pec, int slave, osal_uint16_t mbx_flag);

#define ec_mbx_get_free_recv_buffer(pec, slave, entry, timeout, lock) \
    pool_get(&(pec)->mbx_message_pool_recv_free, &(entry), (timeout))

#define ec_mbx_get_free_send_buffer_old(pec, slave, entry, timeout, lock) \
    pool_get(&(pec)->mbx_message_pool_send_free, &(entry), (timeout))

//! \brief Get free mailbox send buffer from slaves send message pool.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[out] pp_entry Pointer to pool entry pointer where buffer 
 *                      is returned.
 * \param[in] timeout   Pointer to timeout or NULL.
 *
 * \return EC_OK on success, otherwise EC_ERROR_MAILBOX_* code.
 */
int ec_mbx_get_free_send_buffer(ec_t *pec, osal_uint16_t slave, pool_entry_t **pp_entry, osal_timer_t *timeout);

#define ec_mbx_return_free_send_buffer(pec, slave, entry) \
    pool_put(&(pec)->mbx_message_pool_send_free, (entry)) 

#define ec_mbx_return_free_recv_buffer(pec, slave, entry) \
    pool_put(&(pec)->mbx_message_pool_recv_free, (entry))


#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_MBX_H

