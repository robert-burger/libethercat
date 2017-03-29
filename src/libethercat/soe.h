/**
 * \file soe.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 25 Nov 2016
 *
 * \brief ethercat soe functions.
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

//! soe mailbox structure
typedef struct PACKED ec_soe_header {
    uint8_t op_code    : 3;         //!< op code
    uint8_t incomplete : 1;         //!< incompletion flag
    uint8_t error      : 1;         //!< error flag
    uint8_t atn        : 3;         //!< at number
    uint8_t elements;               //!< servodrive element mask
    union {
        uint16_t idn;               //!< id number
        uint16_t fragments_left;    //!< fragments left
    };
} PACKED ec_soe_header_t;

//! soe idn list 
typedef struct PACKED ec_soe_idn_list {
    uint16_t  cur_len;              //!< currently stored list length in bytes
    uint16_t  max_len;              //!< maximum length of list in bytes
    ec_data_t idn_list;             //!< idn list
} PACKED ec_soe_idn_list_t;

typedef struct PACKED ec_soe_idn_attribute {
    uint32_t evafactor   :16;       //!< evalution factor 
    uint32_t length      :2;        //!< idn length
    uint32_t list        :1;        //!< idn is list
    uint32_t command     :1;        //!< idn is command
    uint32_t datatype    :3;        //!< datatype
    uint32_t reserved1   :1;
    uint32_t decimals    :4;        //!< if float, number of decimals 
    uint32_t wp_preop    :1;        //!< write protect in preop
    uint32_t wp_safeop   :1;        //!< write protect in safeop
    uint32_t wp_op       :1;        //!< write protect in op
    uint32_t reserved2   :1;
} PACKED ec_soe_idn_attribute_t;

//! soe elements
enum {
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

//! read elements of soe id number
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \param atn at number
 * \param idn id number
 * \param element servodrive elements
 * \param buf buffer for answer
 * \param len length of buffer, returns length of answer
 * \return 0 on successs
 */
int ec_soe_read(ec_t *pec, uint16_t slave, uint8_t atn, uint16_t idn, 
        uint8_t elements, uint8_t *buf, size_t *len);

//! write elements of soe id number
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \param atn at number
 * \param idn id number
 * \param element servodrive elements
 * \param buf buffer to write
 * \param len length of buffer
 * \return 0 on successs
 */
int ec_soe_write(ec_t *pec, uint16_t slave, uint8_t atn, uint16_t idn, 
        uint8_t elements, uint8_t *buf, size_t len);

//! generate sync manager process data mapping via soe
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \return 0 on success
 */
int ec_soe_generate_mapping(ec_t *pec, uint16_t slave);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_SOE_H__

