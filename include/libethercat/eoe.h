/**
 * \file eoe.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 22 Nov 2016
 *
 * \brief EtherCAT eoe functions.
 *
 * Implementaion of the Ethernet over EtherCAT mailbox protocol
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

#ifndef LIBETHERCAT_EOE_H
#define LIBETHERCAT_EOE_H

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
};

#define DEFTYPE_PDOMAPPING          0x0021

#define CANOPEN_MAXNAME 40
    
//! CanOpen over EtherCAT sdo descriptor
typedef struct PACKED ec_coe_sdo_desc {
    uint16_t data_type;             //!< element data type
    uint8_t  obj_code;              //!< object type
    uint8_t  max_subindices;        //!< maximum number of subindices
    char    *name;                  //!< element name (allocated by callee, 
                                    // freed by caller)
    size_t   name_len;              //!< element name len
} PACKED ec_coe_sdo_desc_t;

typedef struct PACKED ec_coe_sdo_entry_desc {
    uint8_t  value_info;            //!< valueinfo, how to interpret data
    uint16_t data_type;             //!< entry data type
    uint16_t bit_length;            //!< entry bit length
    uint16_t obj_access;            //!< object access
    uint8_t *data;                  //!< data pointer
    size_t   data_len;              //!< length of data
} PACKED ec_coe_sdo_entry_desc_t;

#define EC_COE_SDO_VALUE_INFO_ACCESS_RIGHTS      0x01
#define EC_COE_SDO_VALUE_INFO_OBJECT_CATEGORY    0x02
#define EC_COE_SDO_VALUE_INFO_MAPPABLE           0x04
#define EC_COE_SDO_VALUE_INFO_UNIT               0x08
#define EC_COE_SDO_VALUE_INFO_DEFAULT_VALUE      0x10
#define EC_COE_SDO_VALUE_INFO_MIN_VALUE          0x20
#define EC_COE_SDO_VALUE_INFO_MAX_VALUE          0x40

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

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
 * \param[out] buf          Buffer where to store the answer. Either a user supplied
 *                          pointer to a buffer or the call to \link ec_coe_sdo_read 
 *                          \endlink will allocate a big enough buffer. In last case
 *                          the buffer has to be freed by the user.
 * \param[in,out] len       Length of buffer, outputs read length.
 * \param[out] abort_code   Returns the abort code if we got abort request
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_sdo_read(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t **buf, size_t *len, 
        uint32_t *abort_code);

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
int ec_coe_sdo_write(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t len,
        uint32_t *abort_code);

//! Read CoE SDO description
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] index         CoE SDO index number.
 * \param[out] desc         Returns CoE SDO description.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_sdo_desc_read(ec_t *pec, uint16_t slave, uint16_t index, 
        ec_coe_sdo_desc_t *desc);

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
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_sdo_entry_desc_read(ec_t *pec, uint16_t slave, uint16_t index,
        uint8_t sub_index, uint8_t value_info, ec_coe_sdo_entry_desc_t *desc);

//! Read CoE object dictionary list
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[out] buf          Pointer to either a preallocated buffer or the buffer 
 *                          will be allocated by the \link ec_coe_odlist_read \endlink
 *                          call. In second case it has to be freed by caller.
 * \param[in,out] len       Length of buffer, outputs read length.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_coe_odlist_read(ec_t *pec, uint16_t slave, uint8_t **buf, size_t *len);

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
int ec_coe_generate_mapping(ec_t *pec, uint16_t slave);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_EOE_H

