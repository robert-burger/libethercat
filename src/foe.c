/**
 * \file foe.c
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

#include "libethercat/ec.h"
#include "libethercat/foe.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

//! FoE header
typedef struct ec_foe_header {
    uint8_t op_code;            //!< FoE op code
    uint8_t reserved;           //!< FoE reserved 
} ec_foe_header_t;

//! read/write request
typedef struct ec_foe_rw_request {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_foe_header_t foe_hdr;    //!< FoE header
    uint32_t        password;   //!< FoE password
    char            file_name[MAX_FILE_NAME_SIZE];
                                //!< FoE filename to read/write
} ec_foe_rw_request_t;

//! data packet
typedef struct ec_foe_data_request {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_foe_header_t foe_hdr;    //!< FoE header
    uint32_t        packet_nr;  //!< FoE segmented packet number
    ec_data_t       data;       //!< FoE segmented packet data
} ec_foe_data_request_t;

//! acknowledge data packet
typedef struct ec_foe_ack_request {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_foe_header_t foe_hdr;    //!< FoE header
    uint32_t        packet_nr;  //!< FoE segmented packet number
} ec_foe_ack_request_t;

//! error request
typedef struct ec_foe_error_request {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_foe_header_t foe_hdr;    //!< FoE header
    uint32_t        error_code; //!< error code
    char            error_text[MAX_ERROR_TEXT_SIZE];
                                //!< error text
} ec_foe_error_request_t;

//! initialize FoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_foe_init(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_open(&slv->mbx.foe.recv_pool, 0, 1518);
}

//! deinitialize FoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_foe_deinit(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_close(slv->mbx.foe.recv_pool);
}

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
void ec_foe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    ec_mbx_sched_read(pec, slave);

    ec_timer_t timeout;
    ec_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    pool_get(slv->mbx.foe.recv_pool, pp_entry, &timeout);
}

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
void ec_foe_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_put(slv->mbx.foe.recv_pool, p_entry);
}

// read file over foe
int ec_foe_read(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t **file_data, 
        ssize_t *file_data_len, char **error_message) {
    int wkc = -1;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_FOE)) {
        ec_log(10, __func__, "no FOE support on slave %d\n", slave);
        return -1;
    }

    pthread_mutex_lock(&slv->mbx.lock);

    pool_entry_t *p_entry;
    pool_get(slv->mbx.message_pool_free, &p_entry, NULL);
    memset(p_entry->data, 0, p_entry->data_size);
    MESSAGE_POOL_DEBUG(free);

    ec_foe_rw_request_t *write_buf = (ec_foe_rw_request_t *)(p_entry->data);

    // calc lengths
    ssize_t foe_max_len = min(slv->sm[1].len, MAX_FILE_NAME_SIZE);
    ssize_t file_name_len = min(strlen(file_name), foe_max_len-6);

    // mailbox header
    write_buf->mbx_hdr.length    = 6 + file_name_len;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_FOE;
    // foe header (2 Byte)
    write_buf->foe_hdr.op_code   = EC_FOE_OP_CODE_READ_REQUEST;
    // read request (password 4 Byte)
    write_buf->password          = password;
    memcpy(write_buf->file_name, file_name, file_name_len);

    ec_log(10, __func__, "start reading file \"%s\"\n", file_name);

    // send request
    ec_mbx_enqueue(pec, slave, p_entry);

    *file_data_len = 0;

    while (1) {
        // wait for answer
        for (p_entry = NULL; !p_entry; ec_foe_wait(pec, slave, &p_entry)) {}
        ec_foe_data_request_t *read_buf_data = (ec_foe_data_request_t *)(p_entry->data);

        if (read_buf_data->foe_hdr.op_code == EC_FOE_OP_CODE_ERROR_REQUEST) {
            ec_foe_error_request_t *read_buf_error =
                (ec_foe_error_request_t *)(p_entry->data);

            ec_log(100, __func__, "got foe error code 0x%X\n", 
                    read_buf_error->error_code);

            ssize_t text_len = (read_buf_data->mbx_hdr.length - 6);
            if (text_len > 0) {
                *error_message = strndup(read_buf_error->error_text, text_len);
            } else {
                if (read_buf_error->error_code == 0x800D) {
                    *error_message = strdup("file not found!");
                }
            }

            wkc = -1;
            pool_put(slv->mbx.message_pool_free, p_entry);
            MESSAGE_POOL_DEBUG(free);
            goto exit;
        }

        if (read_buf_data->foe_hdr.op_code != EC_FOE_OP_CODE_DATA_REQUEST) {
            ec_log(10, __func__, "got foe op_code %X\n", read_buf_data->foe_hdr.op_code);
            pool_put(slv->mbx.message_pool_free, p_entry);
            MESSAGE_POOL_DEBUG(free);
            continue;
        }

        size_t len = read_buf_data->mbx_hdr.length - 6;
        *file_data = realloc(*file_data, *file_data_len + len);
        memcpy(*file_data + *file_data_len, 
                &read_buf_data->data.bdata[0], len); 
        *file_data_len += len;

        int packet_nr = read_buf_data->packet_nr;
        int read_data_length = read_buf_data->mbx_hdr.length;
    
        memset(p_entry->data, 0, p_entry->data_size);
        ec_foe_ack_request_t *write_buf_ack = (ec_foe_ack_request_t *)(p_entry->data);

        // everthing is fine, send ack 
        // mailbox header
        write_buf_ack->mbx_hdr.length    = 6; 
        write_buf_ack->mbx_hdr.mbxtype   = EC_MBX_FOE;
        // foe
        write_buf_ack->foe_hdr.op_code   = EC_FOE_OP_CODE_ACK_REQUEST;
        write_buf_ack->packet_nr         = packet_nr;

        // send request
        ec_mbx_enqueue(pec, slave, p_entry);

        // compare length + mbx_hdr_size with mailbox size
        if ((read_data_length + 6) < slv->sm[1].len) {
            pool_put(slv->mbx.message_pool_free, p_entry);
            break;
        }
    }

exit:
    ec_log(10, __func__, "reading file \"%s\" finished\n", file_name);

    pthread_mutex_unlock(&slv->mbx.lock);
    return wkc;
}

// write file over foe
int ec_foe_write(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t *file_data, 
        ssize_t file_data_len, char **error_message) {
    int ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_FOE)) {
        ec_log(10, __func__, "no FOE support on slave %d\n", slave);
        return -1;
    }

    pthread_mutex_lock(&slv->mbx.lock);

    pool_entry_t *p_entry;
    pool_get(slv->mbx.message_pool_free, &p_entry, NULL);
    memset(p_entry->data, 0, p_entry->data_size);
    MESSAGE_POOL_DEBUG(free);

    ec_foe_rw_request_t *write_buf = (ec_foe_rw_request_t *)(p_entry->data);

    // calc lengths
    ssize_t foe_max_len = min(slv->sm[1].len, MAX_FILE_NAME_SIZE);
    ssize_t file_name_len = min(strlen(file_name), foe_max_len-6);

    // mailbox header
    write_buf->mbx_hdr.length    = 6 + file_name_len;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_FOE;
    // foe header (2 Byte)
    write_buf->foe_hdr.op_code   = EC_FOE_OP_CODE_WRITE_REQUEST;
    // read request (password 4 Byte)
    write_buf->password          = password;
    memcpy(write_buf->file_name, file_name, file_name_len);

    // send request
    ec_mbx_enqueue(pec, slave, p_entry);

    // wait for answer
    for (p_entry = NULL; !p_entry; ec_foe_wait(pec, slave, &p_entry)) {}
    ec_foe_ack_request_t *read_buf_ack = (ec_foe_ack_request_t *)(p_entry->data);

    if (read_buf_ack->foe_hdr.op_code != EC_FOE_OP_CODE_ACK_REQUEST) {
        if (read_buf_ack->foe_hdr.op_code == EC_FOE_OP_CODE_ERROR_REQUEST) {
            ec_foe_error_request_t *read_buf_error = (ec_foe_error_request_t *)(p_entry->data);

            ec_log(10, __func__, "got foe error code 0x%X\n", read_buf_error->error_code);

            ssize_t text_len = (read_buf_ack->mbx_hdr.length - 6);
            if (text_len > 0) {
                char *error_text = malloc(text_len + 1);
                strncpy(error_text, read_buf_error->error_text, text_len);
                error_text[text_len] = '\0';
                ec_log(10, __func__, "error_text: %s\n", error_text);
            }
        } else
            ec_log(10, __func__, "got no ack on foe write request, got 0x%X\n", 
                    read_buf_ack->foe_hdr.op_code);
        goto exit;
    }
    
    // returning ack message
    pool_put(slv->mbx.message_pool_free, p_entry);

    // mailbox len - mailbox hdr (6) - foe header (6)
    size_t data_len = slv->sm[1].len - 6 - 6;
    off_t file_offset = 0;
    int packet_nr = 0;

    while (1) {
        pool_get(slv->mbx.message_pool_free, &p_entry, NULL);
        memset(p_entry->data, 0, p_entry->data_size);

        ec_foe_data_request_t *write_buf_data = (ec_foe_data_request_t *)p_entry->data;

        int last_pkt = 0;

        int bytes_read = min(file_data_len - file_offset, data_len);
        memcpy(&write_buf_data->data.bdata[0], file_data + file_offset, bytes_read);
        if (bytes_read < data_len) {
            last_pkt = 1;
        }

        ec_log(10, __func__, "slave %2d: sending file offset %d, bytes %d\n", 
                slave, file_offset, bytes_read);

        file_offset += bytes_read;

        // mailbox header
        write_buf_data->mbx_hdr.length    = 6 + bytes_read; 
        write_buf_data->mbx_hdr.mbxtype   = EC_MBX_FOE;
        // foe
        write_buf_data->foe_hdr.op_code   = EC_FOE_OP_CODE_DATA_REQUEST;
        write_buf_data->packet_nr         = ++packet_nr;

        // send request
        ec_mbx_enqueue(pec, slave, p_entry);

        // wait for answer
        for (ec_foe_wait(pec, slave, &p_entry); p_entry; ec_foe_wait(pec, slave, &p_entry)) {
            ec_foe_ack_request_t *read_buf_ack = (ec_foe_ack_request_t *)(p_entry->data);

            if (read_buf_ack->foe_hdr.op_code != EC_FOE_OP_CODE_ACK_REQUEST) {
                ec_log(10, __func__,
                        "got no ack on foe write request, got 0x%X, last_pkt %d, bytes_read %d, data_len %d, packet_nr %d\n", 
                        read_buf_ack->foe_hdr.op_code, last_pkt, bytes_read, data_len, packet_nr);
                pool_put(slv->mbx.message_pool_free, p_entry);
                goto exit;
            }

            break;
        }

        if (!p_entry) {
            ec_log(10, __func__,
                    "got no ack on foe write request, last_pkt %d, bytes_read %d, data_len %d\n", 
                    last_pkt, bytes_read, data_len);
            ret = -1;
            goto exit;
        }
            
        pool_put(slv->mbx.message_pool_free, p_entry);
        
        if (last_pkt) {
            break;
        }
    }

exit:
    if (ret == 0) {
        ec_log(10, __func__, "file download finished\n");
    } else {
        ec_log(1, __func__, "file download FAILED: error %d\n", ret);
    }

    pthread_mutex_unlock(&slv->mbx.lock);
    return ret;
}

