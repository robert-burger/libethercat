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

#ifndef LIBETHERCAT_COE_H
#define LIBETHERCAT_COE_H

#include <libosal/types.h>
#include <libosal/mutex.h>

#include "libethercat/common.h"
#include "libethercat/idx.h"
#include "libethercat/pool.h"

//! Message queue qentry
typedef struct ec_coe_emergency_message {
    osal_timer_t timestamp;     //!< \brief timestamp, when emergency was received
    osal_size_t msg_len;        //!< \brief length
    osal_uint8_t msg[LEC_MAX_COE_EMERGENCY_MSG_LEN];        //!< \brief message itself
} ec_coe_emergency_message_t;

typedef struct ec_coe {
    pool_t recv_pool;
    
    osal_mutex_t lock;          //!< \brief CoE mailbox lock.
                                /*!<
                                 * Only one simoultaneous access to the 
                                 * EtherCAT slave CoE mailbox is possible 
                                 */

    uint32_t emergency_next_read;
    uint32_t emergency_next_write;
    ec_coe_emergency_message_t emergencies[LEC_MAX_COE_EMERGENCIES];    //!< message pool queue
} ec_coe_t;

//! CoE mailbox types
enum {
    EC_COE_EMERGENCY  = 0x01,               //!< \brief emergency message
    EC_COE_SDOREQ,                          //!< \brief service data object request
    EC_COE_SDORES,                          //!< \brief service data object response
    EC_COE_TXPDO,                           //!< \brief transmit PDO
    EC_COE_RXPDO,                           //!< \brief receive PDO
    EC_COE_TXPDO_RR,                        //!< \brief transmit PDO RR
    EC_COE_RXPDO_RR,                        //!< \brief receive PDO RR
    EC_COE_SDOINFO                          //!< \brief service data object information
};

//! service data object command
enum {
    EC_COE_SDO_DOWNLOAD_SEQ_REQ = 0x00,     //!< \brief sdo download seq request
    EC_COE_SDO_DOWNLOAD_REQ     = 0x01,     //!< \brief sdo download request
    EC_COE_SDO_UPLOAD_REQ       = 0x02,     //!< \brief sdo upload request
    EC_COE_SDO_ABORT_REQ        = 0x04      //!< \brief sdo abort request
};

//! service data object information type
enum {
    EC_COE_SDO_INFO_ODLIST_REQ = 0x01,      //!< \brief object dict list request
    EC_COE_SDO_INFO_ODLIST_RESP,            //!< \brief object dict list response
    EC_COE_SDO_INFO_GET_OBJECT_DESC_REQ,    //!< \brief object description request
    EC_COE_SDO_INFO_GET_OBJECT_DESC_RESP,   //!< \brief object description response
    EC_COE_SDO_INFO_GET_ENTRY_DESC_REQ,     //!< \brief entry description request
    EC_COE_SDO_INFO_GET_ENTRY_DESC_RESP,    //!< \brief entry description response
    EC_COE_SDO_INFO_ERROR_REQUEST,          //!< \brief error request
};

//! datatypes
enum {
    DEFTYPE_NULL                = 0x0000, 
    DEFTYPE_BOOLEAN             = 0x0001, 
    DEFTYPE_INTEGER8            = 0x0002, 
    DEFTYPE_INTEGER16           = 0x0003, 
    DEFTYPE_INTEGER32           = 0x0004, 
    DEFTYPE_UNSIGNED8           = 0x0005, 
    DEFTYPE_UNSIGNED16          = 0x0006, 
    DEFTYPE_UNSIGNED32          = 0x0007, 
    DEFTYPE_REAL32              = 0x0008, 
    DEFTYPE_VISIBLESTRING       = 0x0009, 
    DEFTYPE_OCTETSTRING         = 0x000A, 
    DEFTYPE_UNICODE_STRING      = 0x000B, 
    DEFTYPE_TIME_OF_DAY         = 0x000C, 
    DEFTYPE_TIME_DIFFERENCE     = 0x000D, 
    DEFTYPE_INTEGER24           = 0x0010, 
    DEFTYPE_REAL64              = 0x0011, 
    DEFTYPE_INTEGER40           = 0x0012, 
    DEFTYPE_INTEGER48           = 0x0013, 
    DEFTYPE_INTEGER56           = 0x0014, 
    DEFTYPE_INTEGER64           = 0x0015, 
    DEFTYPE_UNSIGNED24          = 0x0016, 
    DEFTYPE_UNSIGNED40          = 0x0018, 
    DEFTYPE_UNSIGNED48          = 0x0019, 
    DEFTYPE_UNSIGNED56          = 0x001A, 
    DEFTYPE_UNSIGNED64          = 0x001B, 
    DEFTYPE_GUID                = 0x001D, 
    DEFTYPE_BYTE                = 0x001E, 
    DEFTYPE_WORD                = 0x001F, 
    DEFTYPE_DWORD               = 0x0020, 
    DEFTYPE_PDOMAPPING          = 0x0021, 
    DEFTYPE_IDENTITY            = 0x0023, 
    DEFTYPE_COMMAND             = 0x0025, 
    DEFTYPE_PDOCOMPAR           = 0x0027, 
    DEFTYPE_ENUM                = 0x0028, 
    DEFTYPE_SMPAR               = 0x0029, 
    DEFTYPE_RECORD              = 0x002A, 
    DEFTYPE_BACKUP              = 0x002B, 
    DEFTYPE_MDP                 = 0x002C, 
    DEFTYPE_BITARR8             = 0x002D, 
    DEFTYPE_BITARR16            = 0x002E, 
    DEFTYPE_BITARR32            = 0x002F, 
    DEFTYPE_BIT1                = 0x0030, 
    DEFTYPE_BIT2                = 0x0031, 
    DEFTYPE_BIT3                = 0x0032, 
    DEFTYPE_BIT4                = 0x0033, 
    DEFTYPE_BIT5                = 0x0034, 
    DEFTYPE_BIT6                = 0x0035, 
    DEFTYPE_BIT7                = 0x0036, 
    DEFTYPE_BIT8                = 0x0037, 
    DEFTYPE_ARRAY_OF_INT        = 0x0260, 
    DEFTYPE_ARRAY_OF_SINT       = 0x0261, 
    DEFTYPE_ARRAY_OF_DINT       = 0x0262, 
    DEFTYPE_ARRAY_OF_UDINT      = 0x0263, 
    DEFTYPE_ERRORHANDLING       = 0x0281, 
    DEFTYPE_DIAGHISTORY         = 0x0282, 
    DEFTYPE_SYNCSTATUS          = 0x0283, 
    DEFTYPE_SYNCSETTINGS        = 0x0284, 
    DEFTYPE_FSOEFRAME           = 0x0285, 
    DEFTYPE_FSOECOMMPAR         = 0x0286,
};

enum {
    OBJCODE_VAR                 = 0x07,    //!< Object code VARIABLE
    OBJCODE_ARR                 = 0x08,    //!< Object code ARRAY
    OBJCODE_REC                 = 0x09,    //!< Object code RECORD
};

enum {
    ACCESS_READWRITE            = 0x003F,  //!< Read/write in all states
    ACCESS_READ                 = 0x0007,  //!< Read only in all states
    ACCESS_READ_PREOP           = 0x0001,  //!< Read only in PreOP
    ACCESS_READ_SAFEOP          = 0x0002,  //!< Read only in SafeOP
    ACCESS_READ_OP              = 0x0004,  //!< Read only in OP
    ACCESS_WRITE                = 0x0038,  //!< Write only in all states
    ACCESS_WRITE_PREOP          = 0x0008,  //!< Write only in PreOP
    ACCESS_WRITE_SAFEOP         = 0x0010,  //!< Write only in SafeOP
    ACCESS_WRITE_OP             = 0x0020,  //!< Write only in OP
};

#define CANOPEN_MAXNAME             40u
#define CANOPEN_MAXDATA             128u
    
//! CanOpen over EtherCAT sdo descriptor
typedef struct PACKED ec_coe_sdo_desc {
    osal_uint16_t data_type;                //!< \brief element data type
    osal_uint8_t  obj_code;                 //!< \brief object type
    osal_uint8_t  max_subindices;           //!< \brief maximum number of subindices
    osal_char_t   name[CANOPEN_MAXNAME];    //!< \brief element name
    osal_size_t   name_len;                 //!< \brief element name len
} PACKED ec_coe_sdo_desc_t;

typedef struct PACKED ec_coe_sdo_entry_desc {
    osal_uint8_t  value_info;               //!< \brief valueinfo, how to interpret data
    osal_uint16_t data_type;                //!< \brief entry data type
    osal_uint16_t bit_length;               //!< \brief entry bit length
    osal_uint16_t obj_access;               //!< \brief object access
    osal_uint8_t  data[CANOPEN_MAXDATA];    //!< \brief entry name
    osal_size_t   data_len;                 //!< \brief length of name
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

//! Read CoE service data object (SDO) of master
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
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
int ec_coe_master_sdo_read(ec_t *pec, osal_uint16_t index, 
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

//! Write CoE service data object (SDO) of master
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] index         CoE SDO index number.
 * \param[in] sub_index     CoE SDO sub index number.
 * \param[in] complete      SDO Complete access (only if \p sub_index == 0)
 * \param[in] buf           Buffer with data which will be written.
 * \param[in] len           Length of buffer.
 * \param[out] abort_code   Returns the abort code if we got abort request
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_master_sdo_write(ec_t *pec, osal_uint16_t index, 
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

//! Read CoE SDO description of master
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] index         CoE SDO index number.
 * \param[out] desc         Returns CoE SDO description.
 * \param[out] error_code   Returns the error code if we got one.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_master_sdo_desc_read(const ec_t *pec, osal_uint16_t index, 
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

//! Read CoE SDO entry description of master
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] index         CoE SDO index number.
 * \param[in] sub_index     CoE SDO sub index number.
 * \param[in] value_info    Bitset which description values you want to get
 * \param[in] desc          Return CoE entry description.
 * \param[out] error_code   Returns the error code if we got one.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_master_sdo_entry_desc_read(const ec_t *pec, osal_uint16_t index,
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

//! Read CoE object dictionary list of master
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[out] buf          Pointer to a preallocated buffer.
 * \param[in,out] len       Length of buffer, outputs read length.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_master_odlist_read(ec_t *pec, osal_uint8_t *buf, osal_size_t *len);

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

//! Get next CoE emergency message.
/*!
 * \param[in]   pec     Pointer to EtherCAT mater struct.
 * \param[in]   slave   Number of EtherCAT slave connected to bus.
 * \param[out]  msg     Pointer to return emergency message.
 *
 * \return EC_OK on success, errorcode otherwise
 */
int ec_coe_emergency_get_next(ec_t *pec, osal_uint16_t slave, ec_coe_emergency_message_t *msg);

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

