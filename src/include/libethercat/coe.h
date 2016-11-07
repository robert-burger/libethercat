//! ethercat master
/*!
 * author: Robert Burger
 *
 * $Id$
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

#ifndef __LIBETHERCAT_COE_H__
#define __LIBETHERCAT_COE_H__

/** CoE mailbox types */
enum {
    EC_COE_EMERGENCY  = 0x01,
    EC_COE_SDOREQ,
    EC_COE_SDORES,
    EC_COE_TXPDO,
    EC_COE_RXPDO,
    EC_COE_TXPDO_RR,
    EC_COE_RXPDO_RR,
    EC_COE_SDOINFO
};

enum {
    EC_COE_SDO_DOWNLOAD_SEQ_REQ = 0x00,
    EC_COE_SDO_DOWNLOAD_REQ     = 0x01,
    EC_COE_SDO_UPLOAD_REQ       = 0x02,
    EC_COE_SDO_ABORT_REQ        = 0x04
};

enum {
    EC_COE_SDO_INFO_ODLIST_REQ = 0x01,
    EC_COE_SDO_INFO_ODLIST_RESP,
    EC_COE_SDO_INFO_GET_OBJECT_DESC_REQ,
    EC_COE_SDO_INFO_GET_OBJECT_DESC_RESP,
    EC_COE_SDO_INFO_GET_ENTRY_DESC_REQ,
    EC_COE_SDO_INFO_GET_ENTRY_DESC_RESP,
};

#define DEFTYPE_PDOMAPPING          0x0021

#define CANOPEN_MAXNAME 40
    
//! CanOpen over EtherCAT sdo descriptor
typedef struct PACKED ec_coe_sdo_desc {
    uint16_t data_type;             //!< element data type
    uint8_t  obj_code;              //!< object type
    uint8_t  max_subindices;        //!< maximum number of subindices
    char    *name;                  //!< element name (allocated by callee, freed by caller)
    size_t   name_len;              //!< element name len
} PACKED ec_coe_sdo_desc_t;

typedef struct PACKED ec_coe_sdo_entry_desc {
    uint16_t data_type;
    uint16_t bit_length;
    uint16_t obj_access;
    uint8_t *data;
    size_t   data_len;
} PACKED ec_coe_sdo_entry_desc_t;

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! read coe sdo 
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param index sdo index
 * \param sub_index sdo sub index
 * \param complete complete access (only if sub_index == 0)
 * \param buf buffer to store answer
 * \param len length of buffer, outputs read length
 * \param abort_code abort_code if we got abort request
 * \return working counter
 */
int ec_coe_sdo_read(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t *len,
        uint32_t *abort_code);

//! write coe sdo 
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param index sdo index
 * \param sub_index sdo sub index
 * \param complete complete access (only if sub_index == 0)
 * \param buf buffer to write to sdo
 * \param len length of buffer, outputs written length
 * \param abort_code abort_code if we got abort request
 * \return working counter
 */
int ec_coe_sdo_write(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t *len,
        uint32_t *abort_code);

//! read coe sdo description
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param index sdo index
 * \param desc buffer to store answer
 * \return working counter
 */
int ec_coe_sdo_desc_read(ec_t *pec, uint16_t slave, uint16_t index, 
        ec_coe_sdo_desc_t *desc);

//! read coe sdo entry description
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param index sdo index
 * \param sub_index sub index
 * \param buf buffer to store answer
 * \param len length of buffer, outputs read length
 * \return working counter
 */
int ec_coe_sdo_entry_desc_read(ec_t *pec, uint16_t slave, uint16_t index,
        uint8_t sub_index, uint8_t value_info, ec_coe_sdo_entry_desc_t *desc);

//! read coe object dictionary list
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param buf buffer to store answer
 * \param len length of buffer, outputs read length
 * \return working counter
 */
int ec_coe_odlist_read(ec_t *pec, uint16_t slave, uint8_t *buf, size_t *len);

//! generate sync manager process data mapping via coe
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \return 0 on success
 */
int ec_coe_generate_mapping(ec_t *pec, uint16_t slave);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_COE_H__

