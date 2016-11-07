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

#ifndef __LIBETHERCAT_FOE_H__
#define __LIBETHERCAT_FOE_H__

#include "libethercat/mbx.h"

typedef struct ec_foe_header {
    uint8_t op_code;
    uint8_t reserved;
} ec_foe_header_t;

#define MAX_FILE_NAME_SIZE 512
#define MAX_ERROR_TEXT_SIZE 512

typedef struct ec_foe_rw_request {
    ec_mbx_header_t mbx_hdr;
    ec_foe_header_t foe_hdr;
    uint32_t        password;
    char            file_name[MAX_FILE_NAME_SIZE];
} ec_foe_rw_request_t;

typedef struct ec_foe_data_request {
    ec_mbx_header_t mbx_hdr;
    ec_foe_header_t foe_hdr;
    uint32_t        packet_nr;
    ec_data_t       data;
} ec_foe_data_request_t;

typedef struct ec_foe_ack_request {
    ec_mbx_header_t mbx_hdr;
    ec_foe_header_t foe_hdr;
    uint32_t        packet_nr;
} ec_foe_ack_request_t;

typedef struct ec_foe_error_request {
    ec_mbx_header_t mbx_hdr;
    ec_foe_header_t foe_hdr;
    uint32_t        error_code;
    char            error_text[MAX_ERROR_TEXT_SIZE];
} ec_foe_error_request_t;

typedef struct ec_fw_update {
    uint16_t cmd;
    uint16_t size;
    uint16_t address_low;
    uint16_t address_high;
    uint16_t data[(EC_MAX_DATA-8)>>1];
} ec_fw_update_t;

enum {
    EC_FOE_OP_CODE_READ_REQUEST  = 0x01,
    EC_FOE_OP_CODE_WRITE_REQUEST = 0x02,
    EC_FOE_OP_CODE_DATA_REQUEST  = 0x03,
    EC_FOE_OP_CODE_ACK_REQUEST   = 0x04,
    EC_FOE_OP_CODE_ERROR_REQUEST = 0x05,
    EC_FOE_OP_CODE_BUSY_REQUEST  = 0x06,
};

enum {
    EC_FOE_ERROR_NOT_DEFINED         = 0x8000,
    EC_FOE_ERROR_NOT_FOUND           = 0x8001,
    EC_FOE_ERROR_ACCESS_DENIED       = 0x8002,
    EC_FOE_ERROR_DISK_FULL           = 0x8003,
    EC_FOE_ERROR_ILLEGAL             = 0x8004,
    EC_FOE_ERROR_PACKET_NUMBER_WRONG = 0x8005,
    EC_FOE_ERROR_ALREADY_EXISTS      = 0x8006,
    EC_FOE_ERROR_NO_USER             = 0x8007,
    EC_FOE_ERROR_BOOTSTRAP_ONLY      = 0x8008,
    EC_FOE_ERROR_NOT_BOOTSTRAP       = 0x8009,
    EC_FOE_ERROR_NO_RIGHTS           = 0x800A,
    EC_FOE_ERROR_PROGRAM_ERROR       = 0x800B,
};

enum {
    EFW_CMD_IGNORE                  = 0,
    EFW_CMD_MEMORY_TRANSFER         = 1,
    EFW_CMD_WRCODE                  = 2,
    EFW_CMD_CHK_DEVID               = 3,
    EFW_CMD_CHK_DEVICEID            = 3,
    EFW_CMD_CHKSUM                  = 4,
    EFW_CMD_WRCODECHKSUM            = 5,
    EFW_CMD_SET_DEVID               = 6,
    EFW_CMD_CHKSUMCHKSUM            = 6,
    EFW_CMD_BOOTCHKSUM              = 7,
    EFW_CMD_SET_EEPROM              = 10,
};

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! read file over foe
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param password foe password
 * \param remote_file_name file_name to read from
 * \param local_file_name file_name to store file to
 * \return working counter
 */
int ec_foe_read(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t **file_data, 
        ssize_t *file_data_len, char **error_message);

//! write file over foe
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param password foe password
 * \param remote_file_name file_name to read from
 * \param local_file_name file_name to store file to
 * \return working counter
 */
int ec_foe_write(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t *file_data, 
        ssize_t file_data_len, char **error_message);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_COE_H__

