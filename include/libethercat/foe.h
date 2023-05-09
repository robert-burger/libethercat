/**
 * \file foe.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief file over ethercat fuctions
 *
 * These functions are used to gain access to the File-over-EtherCAT 
 * mailbox protocol.
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

#ifndef LIBETHERCAT_FOE_H
#define LIBETHERCAT_FOE_H

#include <libosal/types.h>

#include "libethercat/common.h"

typedef struct ec_foe {
    pool_t recv_pool;
} ec_foe_t;

#define MAX_FILE_NAME_SIZE  512u                    //!< \brief file name max size
#define MAX_ERROR_TEXT_SIZE 512u                    //!< \brief error text max size

//! firmware update 
typedef struct ec_fw_update {
    osal_uint16_t cmd;                              //!< \brief firmware update command
    osal_uint16_t size;                             //!< \brief size of data
    osal_uint16_t address_low;                      //!< \brief destination/source address low WORD
    osal_uint16_t address_high;                     //!< \brief destination/source address high WORD
    osal_uint16_t data[(LEC_MAX_DATA-8u)>>1u];      //!< \brief firmware data bytes
} ec_fw_update_t;

#define EC_FOE_OP_CODE_READ_REQUEST  0x01u          //!< \brief read request
#define EC_FOE_OP_CODE_WRITE_REQUEST 0x02u          //!< \brief write request
#define EC_FOE_OP_CODE_DATA_REQUEST  0x03u          //!< \brief data request
#define EC_FOE_OP_CODE_ACK_REQUEST   0x04u          //!< \brief acknowledge request
#define EC_FOE_OP_CODE_ERROR_REQUEST 0x05u          //!< \brief error request
#define EC_FOE_OP_CODE_BUSY_REQUEST  0x06u          //!< \brief busy request

#define EC_FOE_ERROR_NOT_DEFINED         0x8000u    //!< \brief not defined
#define EC_FOE_ERROR_NOT_FOUND           0x8001u    //!< \brief not found
#define EC_FOE_ERROR_ACCESS_DENIED       0x8002u    //!< \brief access denied
#define EC_FOE_ERROR_DISK_FULL           0x8003u    //!< \brief disk full
#define EC_FOE_ERROR_ILLEGAL             0x8004u    //!< \brief illegal
#define EC_FOE_ERROR_PACKET_NUMBER_WRONG 0x8005u    //!< \brief packed number wrong
#define EC_FOE_ERROR_ALREADY_EXISTS      0x8006u    //!< \brief already exist
#define EC_FOE_ERROR_NO_USER             0x8007u    //!< \brief no user
#define EC_FOE_ERROR_BOOTSTRAP_ONLY      0x8008u    //!< \brief bootstrap access only
#define EC_FOE_ERROR_NOT_BOOTSTRAP       0x8009u    //!< \brief not in bootstrap
#define EC_FOE_ERROR_NO_RIGHTS           0x800Au    //!< \brief no access rights
#define EC_FOE_ERROR_PROGRAM_ERROR       0x800Bu    //!< \brief program error

#define EFW_CMD_IGNORE                  0u          //!< \brief command ignore
#define EFW_CMD_MEMORY_TRANSFER         1u          //!< \brief command memory transfer
#define EFW_CMD_WRCODE                  2u          //!< \brief command wrcode
#define EFW_CMD_CHK_DEVID               3u          //!< \brief command check device id
#define EFW_CMD_CHK_DEVICEID            3u          //!< \brief command check device id
#define EFW_CMD_CHKSUM                  4u          //!< \brief command checksum
#define EFW_CMD_WRCODECHKSUM            5u          //!< \brief command wr code checksum
#define EFW_CMD_SET_DEVID               6u          //!< \brief command set device id
#define EFW_CMD_CHKSUMCHKSUM            6u          //!< \brief command checksum checksum
#define EFW_CMD_BOOTCHKSUM              7u          //!< \brief command boot checksum
#define EFW_CMD_SET_EEPROM              10u         //!< \brief command set eeprom

#ifdef __cplusplus
extern "C" {
#endif

//! initialize FoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_foe_init(ec_t *pec, osal_uint16_t slave);

//! deinitialize FoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_foe_deinit(ec_t *pec, osal_uint16_t slave);

//! \brief Enqueue FoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_foe_enqueue(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry);

//! Read file over FoE.
/*!
 * \param[in] pec               Pointer to ethercat master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of ethercat slave. this depends on 
 *                              the physical order of the ethercat slaves 
 *                              (usually the n'th slave attached).
 * \param[in] password          FoE password for file to read.
 * \param[in] file_name         File name on EtherCAT slave to read from.
 * \param[out] file_data        This will be allocated by the \link ec_foe_read 
 *                              \endlink call and return the content of the EtherCAT
 *                              slaves file. The caller must ensure to free the
 *                              allocated memory.
 * \param[out] file_data_len    The length of the file and the allocated buffer 
 *                              \p file_data
 * \param[out] error_message    In error cases this will return the error message
 *                              set by the EtherCAT slave. If any error has occured,
 *                              this has to be freed by the caller.
 *
 * \return Working counter of the get state command, should be 1 if it was successfull.
 */
int ec_foe_read(ec_t *pec, osal_uint16_t slave, osal_uint32_t password,
        osal_char_t file_name[MAX_FILE_NAME_SIZE], osal_uint8_t **file_data, 
        osal_size_t *file_data_len, const osal_char_t **error_message);

//! Write file over FoE.
/*!
 * \param[in] pec               Pointer to ethercat master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of ethercat slave. this depends on 
 *                              the physical order of the ethercat slaves 
 *                              (usually the n'th slave attached).
 * \param[in] password          FoE password for file to write.
 * \param[in] file_name         File name on EtherCAT slave to write to.
 * \param[out] file_data        The caller has to provide the data which will 
 *                              be written as content of the file to the EtherCAT
 *                              slave.
 * \param[out] file_data_len    The length of the \p file_data.
 * \param[out] error_message    In error cases this will return the error message
 *                              set by the EtherCAT slave. If any error has occured,
 *                              this has to be freed by the caller.
 *
 * \return Working counter of the get state command, should be 1 if it was successfull.
 */
int ec_foe_write(ec_t *pec, osal_uint16_t slave, osal_uint32_t password,
        osal_char_t file_name[MAX_FILE_NAME_SIZE], osal_uint8_t *file_data, 
        osal_size_t file_data_len, const osal_char_t **error_message);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_COE_H

