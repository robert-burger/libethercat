/**
 * \file coe.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 22 Nov 2016
 *
 * \brief EtherCAT coe functions.
 *
 * Implementaion of the CanOpen over EtherCAT mailbox protocol
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

#ifndef LIBETHERCAT_COE_H
#define LIBETHERCAT_COE_H

#include <libosal/types.h>
#include <libosal/mutex.h>

#include "libethercat/common.h"
#include "libethercat/idx.h"
#include "libethercat/pool.h"

//! Message queue qentry
typedef struct ec_coe_emergency_message_entry {
    TAILQ_ENTRY(ec_coe_emergency_message_entry) qh;
                                //!< handle to message entry queue
    osal_timer_t timestamp;     //!< timestamp, when emergency was received
    osal_size_t msg_len;        //!< length
    osal_uint8_t msg[1];        //!< message itself
} ec_coe_emergency_message_entry_t;

TAILQ_HEAD(ec_coe_emergency_message_queue, ec_coe_emergency_message_entry);
typedef struct ec_coe_emergency_message_queue ec_coe_emergency_message_queue_t;

typedef struct ec_coe {
    pool_t recv_pool;
    
    osal_mutex_t lock;          //!< \brief CoE mailbox lock.
                                /*!<
                                 * Only one simoultaneous access to the 
                                 * EtherCAT slave CoE mailbox is possible 
                                 */

    ec_coe_emergency_message_queue_t emergencies;    //!< message pool queue
} ec_coe_t;

//! CoE mailbox types
enum {
    EC_COE_EMERGENCY  = 0x01,   //!< emergency message
    EC_COE_SDOREQ,              //!< service data object request
    EC_COE_SDORES,              //!< service data object response
    EC_COE_TXPDO,               //!< transmit PDO
    EC_COE_RXPDO,               //!< receive PDO
    EC_COE_TXPDO_RR,            //!< transmit PDO RR
    EC_COE_RXPDO_RR,            //!< receive PDO RR
    EC_COE_SDOINFO              //!< service data object information
};

//! service data object command
enum {
    EC_COE_SDO_DOWNLOAD_SEQ_REQ = 0x00,     //!< sdo download seq request
    EC_COE_SDO_DOWNLOAD_REQ     = 0x01,     //!< sdo download request
    EC_COE_SDO_UPLOAD_REQ       = 0x02,     //!< sdo upload request
    EC_COE_SDO_ABORT_REQ        = 0x04      //!< sdo abort request
};

//! service data object information type
enum {
    EC_COE_SDO_INFO_ODLIST_REQ = 0x01,      //!< object dict list request
    EC_COE_SDO_INFO_ODLIST_RESP,            //!< object dict list response
    EC_COE_SDO_INFO_GET_OBJECT_DESC_REQ,    //!< object description request
    EC_COE_SDO_INFO_GET_OBJECT_DESC_RESP,   //!< object description response
    EC_COE_SDO_INFO_GET_ENTRY_DESC_REQ,     //!< entry description request
    EC_COE_SDO_INFO_GET_ENTRY_DESC_RESP,    //!< entry description response
    EC_COE_SDO_INFO_ERROR_REQUEST,          //!< error request
};

#define DEFTYPE_PDOMAPPING          0x0021

#define CANOPEN_MAXNAME             40u
#define CANOPEN_MAXDATA             128u
    
//! CanOpen over EtherCAT sdo descriptor
typedef struct PACKED ec_coe_sdo_desc {
    osal_uint16_t data_type;             //!< \brief element data type
    osal_uint8_t  obj_code;              //!< \brief object type
    osal_uint8_t  max_subindices;        //!< \brief maximum number of subindices
    osal_char_t   name[CANOPEN_MAXNAME]; //!< \brief element name
    osal_size_t   name_len;              //!< \brief element name len
} PACKED ec_coe_sdo_desc_t;

typedef struct PACKED ec_coe_sdo_entry_desc {
    osal_uint8_t  value_info;            //!< \brief valueinfo, how to interpret data
    osal_uint16_t data_type;             //!< \brief entry data type
    osal_uint16_t bit_length;            //!< \brief entry bit length
    osal_uint16_t obj_access;            //!< \brief object access
    osal_uint8_t  data[CANOPEN_MAXDATA]; //!< \brief entry name
    osal_size_t   data_len;              //!< \brief length of name
} PACKED ec_coe_sdo_entry_desc_t;

#define EC_COE_SDO_VALUE_INFO_ACCESS_RIGHTS      0x01
#define EC_COE_SDO_VALUE_INFO_OBJECT_CATEGORY    0x02
#define EC_COE_SDO_VALUE_INFO_MAPPABLE           0x04
#define EC_COE_SDO_VALUE_INFO_UNIT               0x08
#define EC_COE_SDO_VALUE_INFO_DEFAULT_VALUE      0x10
#define EC_COE_SDO_VALUE_INFO_MIN_VALUE          0x20
#define EC_COE_SDO_VALUE_INFO_MAX_VALUE          0x40

// forward declarations
struct ec;
typedef struct ec ec_t;

#ifdef __cplusplus
extern "C" {
#endif

//! initialize CoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_coe_init(ec_t *pec, osal_uint16_t slave);

//! deinitialize CoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_coe_deinit(ec_t *pec, osal_uint16_t slave);

//! Read CoE service data object (SDO) 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] index         CoE SDO index number.
 * \param[in] sub_index     CoE SDO sub index number.
 * \param[in] complete      SDO Complete access (only if \p sub_index == 0)
 * \param[out] buf          Buffer where to store the answer. User supplied
 *                          pointer to a buffer. If buffer is too small an
 *                          error code is returned and the needed length
 *                          is stored in 'len'.
 * \param[in,out] len       Length of buffer, outputs read length.
 * \param[out] abort_code   Returns the abort code if we got abort request
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_sdo_read(ec_t *pec, osal_uint16_t slave, osal_uint16_t index, 
        osal_uint8_t sub_index, int complete, osal_uint8_t *buf, osal_size_t *len, 
        osal_uint32_t *abort_code);

//! Write CoE service data object (SDO)
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] index         CoE SDO index number.
 * \param[in] sub_index     CoE SDO sub index number.
 * \param[in] complete      SDO Complete access (only if \p sub_index == 0)
 * \param[in] buf           Buffer with data which will be written.
 * \param[in] len           Length of buffer.
 * \param[out] abort_code   Returns the abort code if we got abort request
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_sdo_write(ec_t *pec, osal_uint16_t slave, osal_uint16_t index, 
        osal_uint8_t sub_index, int complete, osal_uint8_t *buf, osal_size_t len,
        osal_uint32_t *abort_code);

//! Read CoE SDO description
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] index         CoE SDO index number.
 * \param[out] desc         Returns CoE SDO description.
 * \param[out] error_code   Returns the error code if we got one.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_sdo_desc_read(ec_t *pec, osal_uint16_t slave, osal_uint16_t index, 
        ec_coe_sdo_desc_t *desc, osal_uint32_t *error_code);

//! Read CoE SDO entry description
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] index         CoE SDO index number.
 * \param[in] sub_index     CoE SDO sub index number.
 * \param[in] value_info    Bitset which description values you want to get
 * \param[in] desc          Return CoE entry description.
 * \param[out] error_code   Returns the error code if we got one.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_sdo_entry_desc_read(ec_t *pec, osal_uint16_t slave, osal_uint16_t index,
        osal_uint8_t sub_index, osal_uint8_t value_info, ec_coe_sdo_entry_desc_t *desc, 
        osal_uint32_t *error_code);

//! Read CoE object dictionary list
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[out] buf          Pointer to a preallocated buffer.
 * \param[in,out] len       Length of buffer, outputs read length.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_odlist_read(ec_t *pec, osal_uint16_t slave, osal_uint8_t *buf, osal_size_t *len);

//! generate sync manager process data mapping via coe
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 *
 * \retval 0 on success
 */
int ec_coe_generate_mapping(ec_t *pec, osal_uint16_t slave);

//! queue read mailbox content
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 */
void ec_coe_emergency_enqueue(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry);

//! \brief Enqueue CoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_coe_enqueue(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry);

//! \brief Get SDO INFO error string.
/*!
 * \param[in] error_code    Error code number.
 * \return string with decoded error.
 */
const osal_char_t *get_sdo_info_error_string(osal_uint32_t errorcode);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_COE_H

