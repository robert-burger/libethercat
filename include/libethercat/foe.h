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

#ifndef __LIBETHERCAT_FOE_H__
#define __LIBETHERCAT_FOE_H__

#include "libethercat/common.h"

typedef struct ec_foe {
    pool_t *recv_pool;
} ec_foe_t;

#define MAX_FILE_NAME_SIZE 512  //!< file name max size
#define MAX_ERROR_TEXT_SIZE 512 //!< error text max size

//! firmware update 
typedef struct ec_fw_update {
    uint16_t cmd;               //!< firmware update command
    uint16_t size;              //!< size of data
    uint16_t address_low;       //!< destination/source address low WORD
    uint16_t address_high;      //!< destination/source address high WORD
    uint16_t data[(EC_MAX_DATA-8)>>1];
                                //!< firmware data bytes
} ec_fw_update_t;

enum ec_foe_op_code {
    EC_FOE_OP_CODE_READ_REQUEST  = 0x01,    //!< read request
    EC_FOE_OP_CODE_WRITE_REQUEST = 0x02,    //!< write request
    EC_FOE_OP_CODE_DATA_REQUEST  = 0x03,    //!< data request
    EC_FOE_OP_CODE_ACK_REQUEST   = 0x04,    //!< acknowledge request
    EC_FOE_OP_CODE_ERROR_REQUEST = 0x05,    //!< error request
    EC_FOE_OP_CODE_BUSY_REQUEST  = 0x06,    //!< busy request
};

enum ec_foe_error {
    EC_FOE_ERROR_NOT_DEFINED         = 0x8000,  //!< not defined
    EC_FOE_ERROR_NOT_FOUND           = 0x8001,  //!< not found
    EC_FOE_ERROR_ACCESS_DENIED       = 0x8002,  //!< access denied
    EC_FOE_ERROR_DISK_FULL           = 0x8003,  //!< disk full
    EC_FOE_ERROR_ILLEGAL             = 0x8004,  //!< illegal
    EC_FOE_ERROR_PACKET_NUMBER_WRONG = 0x8005,  //!< packed number wrong
    EC_FOE_ERROR_ALREADY_EXISTS      = 0x8006,  //!< already exist
    EC_FOE_ERROR_NO_USER             = 0x8007,  //!< no user
    EC_FOE_ERROR_BOOTSTRAP_ONLY      = 0x8008,  //!< bootstrap access only
    EC_FOE_ERROR_NOT_BOOTSTRAP       = 0x8009,  //!< not in bootstrap
    EC_FOE_ERROR_NO_RIGHTS           = 0x800A,  //!< no access rights
    EC_FOE_ERROR_PROGRAM_ERROR       = 0x800B,  //!< program error
};

enum efw_cmd {
    EFW_CMD_IGNORE                  = 0,    //!< command ignore
    EFW_CMD_MEMORY_TRANSFER         = 1,    //!< command memory transfer
    EFW_CMD_WRCODE                  = 2,    //!< command wrcode
    EFW_CMD_CHK_DEVID               = 3,    //!< command check device id
    EFW_CMD_CHK_DEVICEID            = 3,    //!< command check device id
    EFW_CMD_CHKSUM                  = 4,    //!< command checksum
    EFW_CMD_WRCODECHKSUM            = 5,    //!< command wr code checksum
    EFW_CMD_SET_DEVID               = 6,    //!< command set device id
    EFW_CMD_CHKSUMCHKSUM            = 6,    //!< command checksum checksum
    EFW_CMD_BOOTCHKSUM              = 7,    //!< command boot checksum
    EFW_CMD_SET_EEPROM              = 10,   //!< command set eeprom
};

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! initialize FoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_foe_init(ec_t *pec, uint16_t slave);

//! \brief Wait for FoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] pp_entry  Returns pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_foe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry);

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
void ec_foe_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry);

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
int ec_foe_read(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t **file_data, 
        ssize_t *file_data_len, char **error_message);

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
int ec_foe_write(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t *file_data, 
        ssize_t file_data_len, char **error_message);

#if 0 
{
#endif
#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_COE_H__

