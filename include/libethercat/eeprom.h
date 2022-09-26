/**
 * \file eeprom.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief EtherCAT eeprom access fuctions
 *
 * These functions are used to ensure access to the EtherCAT
 * slaves EEPROM.
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

#ifndef LIBETHERCAT_EEPROM_H
#define LIBETHERCAT_EEPROM_H

#include <libosal/queue.h>
#include <libosal/types.h>

#include "libethercat/common.h"

//------------------ Category General ---------------

typedef struct PACKED ec_eeprom_cat_general {
    osal_uint8_t group_idx;          //!< group information, index to STRING
    osal_uint8_t img_idx;            //!< image name, index to STRING
    osal_uint8_t order_idx;          //!< device order number, index to STRING
    osal_uint8_t name_idx;           //!< device name, index to STRING
    osal_uint8_t physical_layer;     //!< physical layer, 0 e-bus, 1, 100base-tx
    osal_uint8_t can_open;           //!< coe support
    osal_uint8_t file_access;        //!< foe support
    osal_uint8_t ethernet;           //!< eoe support
    osal_uint8_t soe_channels;       //!< supported soe channels
    osal_uint8_t ds402_channels;     //!< supported ds402 channels
    osal_uint8_t sysman_class;       //!< sys man ?
    osal_uint8_t flags;              //!< eeprom flags
    osal_uint16_t current_on_ebus;   //!< ebus current in [mA], negative = feed-in
} ec_eeprom_cat_general_t;
    
//------------------ Category PDO -------------------

#define LEC_EEPROM_CAT_PDO_MAX          128
#define LEC_EEPROM_CAT_PDO_ENTRIES_MAX   32

typedef struct PACKED ec_eeprom_cat_pdo_entry {
    osal_uint16_t entry_index;       //!< PDO entry index (CoE)
    osal_uint8_t sub_index;          //!< PDO entry subindex 
    osal_uint8_t entry_name_idx;     //!< name index in eeprom strings
    osal_uint8_t data_type;          //!< data type
    osal_uint8_t bit_len;            //!< length in bits
    osal_uint16_t flags;             //!< PDO entry flags
} ec_eeprom_cat_pdo_entry_t;

typedef struct ec_eeprom_cat_pdo {
    struct PACKED {
        osal_uint16_t pdo_index;         //!< PDO index
        osal_uint8_t n_entry;            //!< number of PDO entries
        osal_uint8_t sm_nr;              //!< assigned sync manager
        osal_uint8_t dc_sync;            //!< use distributed clocks
        osal_uint8_t name_idx;           //!< name index in eeprom strings
        osal_uint16_t flags;             //!< PDO flags

#define EC_EEPROM_CAT_PDO_LEN   (osal_size_t)8u
    };

    ec_eeprom_cat_pdo_entry_t entries[LEC_EEPROM_CAT_PDO_ENTRIES_MAX];
                                //!< PDO entries, (n_entry count)
    
    TAILQ_ENTRY(ec_eeprom_cat_pdo) qh;
                                //!< queue handle for PDO queue
} ec_eeprom_cat_pdo_t;

//! head to PDO queue
TAILQ_HEAD(ec_eeprom_cat_pdo_queue, ec_eeprom_cat_pdo);

//------------------ Category SM --------------------

//! eeprom sync manager settings
typedef struct PACKED ec_eeprom_cat_sm {
    osal_uint16_t adr;               //!< physical start addres
    osal_uint16_t len;               //!< length at physical start address
    osal_uint8_t  ctrl_reg;          //!< control register init value
    osal_uint8_t  status_reg;        //!< status register init value
    osal_uint8_t  activate;          //!< activation flags
    osal_uint8_t  pdi_ctrl;          //!< pdi control register
} PACKED ec_eeprom_cat_sm_t;

//------------------ Category DC --------------------

//! eeprom distributed clocks settings
typedef struct PACKED ec_eeprom_cat_dc {
    osal_uint32_t cycle_time_0;         //!< cycle time sync0
    osal_uint32_t shift_time_0;         //!< shift time sync0
    osal_uint32_t shift_time_1;         //!< shift time sync1
    osal_int16_t  sync_1_cycle_factor;  //!< cycle factor sync1
    osal_uint16_t assign_active;        //!< activation flags
    osal_int16_t  sync_0_cycle_factor;  //!< cycle factor sync0
    osal_uint8_t  name_idx;             //!< name index in eeprom strings
    osal_uint8_t  desc_idx;             //!< description index
    osal_uint8_t  reserved[4];          //!< funny reserved bytes
#define EC_EEPROM_CAT_DC_LEN    (osal_size_t)25u
} PACKED ec_eeprom_cat_dc_t;

//------------------ Category FMMU ------------------

//! eeporm fmmu description
typedef struct PACKED ec_eeprom_cat_fmmu {
    osal_uint8_t type;                      //!< fmmu type
} PACKED ec_eeprom_cat_fmmu_t;
    
typedef struct eeprom_info {
    int read_eeprom;                        //!< read eeprom while reaching PREOP state

    osal_uint32_t vendor_id;                //!< vendor id
    osal_uint32_t product_code;             //!< product code
    osal_uint16_t mbx_supported;            //!< mailbox supported by slave

    osal_uint16_t mbx_receive_offset;       //!< default mailbox receive offset
    osal_uint16_t mbx_receive_size;         //!< default mailbox receive size
    osal_uint16_t mbx_send_offset;          //!< default mailbox send offset
    osal_uint16_t mbx_send_size;            //!< default mailbox send size
    
    osal_uint16_t boot_mbx_receive_offset;  //!< boot mailbox receive offset
    osal_uint16_t boot_mbx_receive_size;    //!< boot mailbox receive size
    osal_uint16_t boot_mbx_send_offset;     //!< boot mailbox send offset
    osal_uint16_t boot_mbx_send_size;       //!< boot mailbox send size

    ec_eeprom_cat_general_t general;        //!< general category

    osal_uint8_t strings_cnt;               //!< count of strings
    osal_char_t **strings;                  //!< array of strings 

    osal_uint8_t sms_cnt;                   //!< count of sync manager settings
    ec_eeprom_cat_sm_t *sms;                //!< array of sync manager settings

    osal_uint8_t fmmus_cnt;                 //!< count of fmmu settings    
    ec_eeprom_cat_fmmu_t *fmmus;            //!< array of fmmu settings

    ec_eeprom_cat_pdo_t free_pdos[LEC_EEPROM_CAT_PDO_MAX];
    struct ec_eeprom_cat_pdo_queue free_pdo_queue;

    struct ec_eeprom_cat_pdo_queue txpdos;  //!< queue with TXPDOs
    struct ec_eeprom_cat_pdo_queue rxpdos;  //!< queue with RXPDOs

    osal_uint8_t dcs_cnt;                   //!< count of distributed clocks settings                            
    ec_eeprom_cat_dc_t *dcs;                //!< array of distributed clocks settings
} eeprom_info_t;

#define EC_EEPROM_MBX_AOE                   (0x01u)     //!< \brief AoE mailbox support
#define EC_EEPROM_MBX_EOE                   (0x02u)     //!< \brief EoE mailbox support
#define EC_EEPROM_MBX_COE                   (0x04u)     //!< \brief CoE mailbox support
#define EC_EEPROM_MBX_FOE                   (0x08u)     //!< \brief FoE mailbox support
#define EC_EEPROM_MBX_SOE                   (0x10u)     //!< \brief SoE mailbox support
#define EC_EEPROM_MBX_VOE                   (0x20u)     //!< \brief VoE mailbox support

#define EC_EEPROM_ADR_VENDOR_ID             (0x0008u)   //!< \brief offset vendor id
#define EC_EEPROM_ADR_PRODUCT_CODE          (0x000Au)   //!< \brief offset product code
#define EC_EEPROM_ADR_BOOT_MBX_RECV_OFF     (0x0014u)   //!< \brief offset mbx receive off
#define EC_EEPROM_ADR_BOOT_MBX_RECV_SIZE    (0x0015u)   //!< \brief offset mbx receive size
#define EC_EEPROM_ADR_BOOT_MBX_SEND_OFF     (0x0016u)   //!< \brief offset mbx send off
#define EC_EEPROM_ADR_BOOT_MBX_SEND_SIZE    (0x0017u)   //!< \brief offset mbx send size
#define EC_EEPROM_ADR_STD_MBX_RECV_OFF      (0x0018u)   //!< \brief offset boot mbx rcv off
#define EC_EEPROM_ADR_STD_MBX_RECV_SIZE     (0x0019u)   //!< \brief offset boot mbx rcv size
#define EC_EEPROM_ADR_STD_MBX_SEND_OFF      (0x001Au)   //!< \brief offset boot mbx send off
#define EC_EEPROM_ADR_STD_MBX_SEND_SIZE     (0x001Bu)   //!< \brief offset boot mbx send size
#define EC_EEPROM_ADR_MBX_SUPPORTED         (0x001Cu)   //!< \brief offset mailbox supported
#define EC_EEPROM_ADR_SIZE                  (0x003Eu)   //!< \brief offset eeprom size
#define EC_EEPROM_ADR_CAT_OFFSET            (0x0040u)   //!< \brief offset start of categories

#define EC_EEPROM_CAT_NOP                   (     0u)   //!< \brief category do nothing
#define EC_EEPROM_CAT_STRINGS               (    10u)   //!< \brief category strings
#define EC_EEPROM_CAT_DATATYPES             (    20u)   //!< \brief category datatypes
#define EC_EEPROM_CAT_GENERAL               (    30u)   //!< \brief category general
#define EC_EEPROM_CAT_FMMU                  (    40u)   //!< \brief category fmmus
#define EC_EEPROM_CAT_SM                    (    41u)   //!< \brief category sync managers
#define EC_EEPROM_CAT_TXPDO                 (    50u)   //!< \brief category TXPDOs
#define EC_EEPROM_CAT_RXPDO                 (    51u)   //!< \brief category RXPDOs
#define EC_EEPROM_CAT_DC                    (    60u)   //!< \brief category distributed clocks
#define EC_EEPROM_CAT_END                   (0xFFFFu)   //!< \brief category end identifier

#ifdef __cplusplus
extern "C" {
#endif

// forward decl
struct ec;

//! Set eeprom control to pdi.
/*!
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 *
 * \retval 0    On success
 */
int ec_eeprom_to_pdi(struct ec *pec, osal_uint16_t slave);

//! Set eeprom control to ec.
/*!
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 *
 * \retval 0    On success
 */
int ec_eeprom_to_ec(struct ec *pec, osal_uint16_t slave);

//! Read 32-bit word of eeprom.
/*!
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 * \param[in] eepadr            Address in eeprom where to read data.
 * \param[out] data             Returns read 32-bit data value.
 *
 * \retval 0    On success
 */
int ec_eepromread(struct ec *pec, osal_uint16_t slave, 
        osal_uint32_t eepadr, osal_uint32_t *data);

//! Write 32-bit word to eeprom
/*!
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 * \param[in] eepadr            Address in eeprom where to write data.
 * \param[out] data             32-bit data value which will be written.
 *
 * \retval 0    On success
 */
int ec_eepromwrite(struct ec *pec, osal_uint16_t slave, 
        osal_uint32_t eepadr, osal_uint16_t *data);

//! Read a burst of eeprom data
/*!
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 * \param[in] eepadr            Address in eeprom from where to read the data.
 * \param[out] buf              Data buffer where the read data will be copied.
 * \param[in] buflen            Length of data buffer provided by user.
 *
 * \retval 0    On success
 */
int ec_eepromread_len(struct ec *pec, osal_uint16_t slave, 
        osal_uint32_t eepadr, osal_uint8_t *buf, osal_size_t buflen);

//! Write a burst of eeprom data.
/*!
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 * \param[in] eepadr            Address in eeprom from where to read the data.
 * \param[in] buf               Data buffer with provided data to write to 
 *                              EtherCAT slave's eeprom.
 * \param[in] buflen            Length of data buffer provided by user.
 *
 * \retval 0    On success
 */
int ec_eepromwrite_len(struct ec *pec, osal_uint16_t slave, 
        osal_uint32_t eepadr, const osal_uint8_t *buf, osal_size_t buflen);

//! Read out whole eeprom and categories and store in EtherCAT master structure.
/*!
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 */
void ec_eeprom_dump(struct ec *pec, osal_uint16_t slave);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_EEPROM_H

