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

#include "libethercat/foe.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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

    pthread_mutex_lock(&slv->mbx_lock);

    ec_foe_rw_request_t *write_buf = 
        (ec_foe_rw_request_t *)(slv->mbx_write.buf);

    // calc lengths
    ssize_t foe_max_len = min(slv->sm[1].len, MAX_FILE_NAME_SIZE);
    ssize_t file_name_len = min(strlen(file_name), foe_max_len-6);

    // empty mailbox if anything in
    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0);

    // mailbox header
    ec_mbx_clear(pec, slave, 0);
    write_buf->mbx_hdr.length    = 6 + file_name_len;
    write_buf->mbx_hdr.address   = 0x0000;
    write_buf->mbx_hdr.priority  = 0x00;
    write_buf->mbx_hdr.counter   = 0;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_FOE;

    // foe header (2 Byte)
    write_buf->foe_hdr.op_code   = EC_FOE_OP_CODE_READ_REQUEST;
    write_buf->foe_hdr.reserved  = 0x00;

    // read request (password 4 Byte)
    write_buf->password          = password;
    memcpy(write_buf->file_name, file_name, file_name_len);

    ec_log(10, __func__, "start reading file \"%s\"\n", file_name);

    // send request
    wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, __func__, "error on writing send mailbox\n");
        goto exit;
    }

    ec_foe_ack_request_t *write_buf_ack = 
        (ec_foe_ack_request_t *)(slv->mbx_write.buf);
    ec_foe_data_request_t *read_buf_data = 
        (ec_foe_data_request_t *)(slv->mbx_read.buf);

    *file_data_len = 0;

    while (1) {
        // wait for answer
        ec_mbx_clear(pec, slave, 1);
        wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, __func__, "error on reading receive mailbox\n");
            goto exit;
        }

        if (read_buf_data->mbx_hdr.mbxtype != EC_MBX_FOE) {
            ec_log(10, __func__, "wrong mailbox type 0x%X\n", 
                    read_buf_data->mbx_hdr.mbxtype);
            continue;
        }

        if (read_buf_data->foe_hdr.op_code == EC_FOE_OP_CODE_ERROR_REQUEST) {
            ec_foe_error_request_t *read_buf_error =
                (ec_foe_error_request_t *)(slv->mbx_read.buf);

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
            goto exit;
        }

        if (read_buf_data->foe_hdr.op_code != EC_FOE_OP_CODE_DATA_REQUEST) {
            ec_log(10, __func__, "got foe op_code %X\n", 
                    read_buf_data->foe_hdr.op_code);

            continue;
        }

        size_t len = read_buf_data->mbx_hdr.length - 6;
        *file_data = realloc(*file_data, *file_data_len + len);
        memcpy(*file_data + *file_data_len, 
                &read_buf_data->data.bdata[0], len); 
        *file_data_len += len;

        // everthing is fine, send ack 
        // mailbox header
        ec_mbx_clear(pec, slave, 0);
        write_buf_ack->mbx_hdr.length    = 6; 
        write_buf_ack->mbx_hdr.address   = 0x0000;
        write_buf_ack->mbx_hdr.priority  = 0x00;
        write_buf_ack->mbx_hdr.mbxtype   = EC_MBX_FOE;

        // foe
        write_buf_ack->foe_hdr.op_code   = EC_FOE_OP_CODE_ACK_REQUEST;
        write_buf_ack->foe_hdr.reserved  = 0x00;
        write_buf_ack->packet_nr         = read_buf_data->packet_nr;

        wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, __func__, "error on writing send mailbox\n");
            goto exit;
        }

        // compare length + mbx_hdr_size with mailbox size
        if ((read_buf_data->mbx_hdr.length + 6) < slv->sm[1].len)
            break;
    }

exit:
    ec_log(10, __func__, "reading file \"%s\" finished\n", file_name);

    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);
    return wkc;
}

// write file over foe
int ec_foe_write(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t *file_data, 
        ssize_t file_data_len, char **error_message) {
    int wkc = -1;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_FOE)) {
        ec_log(10, __func__, "no FOE support on slave %d\n", slave);
        return -1;
    }

    pthread_mutex_lock(&slv->mbx_lock);

    ec_foe_rw_request_t *write_buf = 
        (ec_foe_rw_request_t *)(slv->mbx_write.buf);

    // calc lengths
    ssize_t foe_max_len = min(slv->sm[1].len, MAX_FILE_NAME_SIZE);
    ssize_t file_name_len = min(strlen(file_name), foe_max_len-6);

    // empty mailbox if anything in
    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0);

    // mailbox header
    ec_mbx_clear(pec, slave, 0);
    write_buf->mbx_hdr.length    = 6 + file_name_len;
    write_buf->mbx_hdr.address   = 0x0000;
    write_buf->mbx_hdr.priority  = 0x00;
    write_buf->mbx_hdr.counter   = 0;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_FOE;

    // foe header (2 Byte)
    write_buf->foe_hdr.op_code   = EC_FOE_OP_CODE_WRITE_REQUEST;
    write_buf->foe_hdr.reserved  = 0x00;

    // read request (password 4 Byte)
    write_buf->password          = password;
    memcpy(write_buf->file_name, file_name, file_name_len);

    // send request
    wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, __func__, "error on writing send mailbox\n");
        goto exit;
    }

    ec_foe_data_request_t *write_buf_data = 
        (ec_foe_data_request_t *)(slv->mbx_write.buf);
    ec_foe_ack_request_t *read_buf_ack = 
        (ec_foe_ack_request_t *)(slv->mbx_read.buf);
        
    // wait for ack
    int i = 10000000;
    do {
        ec_mbx_clear(pec, slave, 1);
        wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX * 10);

        if (--i == 0)
            break;
    } while (wkc != 1);
    if (!wkc) {
        ec_log(10, __func__, 
                "error on reading receive mailbox wating for ack\n");
        goto exit;
    }

    if (read_buf_ack->mbx_hdr.mbxtype != EC_MBX_FOE) {
        ec_log(10, __func__, "wrong mailbox type 0x%X\n", 
                read_buf_ack->mbx_hdr.mbxtype);
        goto exit;
    }
        
    if (read_buf_ack->foe_hdr.op_code != EC_FOE_OP_CODE_ACK_REQUEST) {
        if (read_buf_ack->foe_hdr.op_code == EC_FOE_OP_CODE_ERROR_REQUEST) {
            ec_foe_error_request_t *read_buf_error =
                (ec_foe_error_request_t *)(slv->mbx_read.buf);

            ec_log(10, __func__, "got foe error code 0x%X\n",
                    read_buf_error->error_code);

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

    // mailbox len - mailbox hdr (6) - foe header (6)
    size_t data_len = slv->sm[1].len - 6 - 6;
    off_t file_offset = 0;

    while (1) {
        int last_pkt = 0;

        // everthing is fine, send data 
        ec_mbx_clear(pec, slave, 0);
    
        int bytes_read = min(file_data_len - file_offset, data_len);
        memcpy(&write_buf_data->data.bdata[0], file_data + file_offset, bytes_read);
        if (bytes_read < data_len)
            last_pkt = 1;

        file_offset += bytes_read;

        // mailbox header
        write_buf_data->mbx_hdr.length    = 6 + bytes_read; 
        write_buf_data->mbx_hdr.address   = 0x0000;
        write_buf_data->mbx_hdr.priority  = 0x00;
        write_buf_data->mbx_hdr.mbxtype   = EC_MBX_FOE;

        // foe
        write_buf_data->foe_hdr.op_code   = EC_FOE_OP_CODE_DATA_REQUEST;
        write_buf_data->foe_hdr.reserved  = 0x00;
        write_buf_data->packet_nr         = read_buf_ack->packet_nr + 1;

        wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, __func__,
                    "error on writing send mailbox with data packet %d\n", 
                    write_buf_data->packet_nr);
            goto exit;
        }

        // wait for ack
        ec_mbx_clear(pec, slave, 1);
        wkc = ec_mbx_receive(pec, slave, 10 * EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, __func__,
                    "error on reading receive mailbox wating for data ack\n");
            goto exit;
        }

        if (read_buf_ack->mbx_hdr.mbxtype != EC_MBX_FOE) {
            ec_log(10, __func__, "wrong mailbox type 0x%X\n", 
                    read_buf_ack->mbx_hdr.mbxtype);
            goto exit;
        }

        if (read_buf_ack->foe_hdr.op_code != EC_FOE_OP_CODE_ACK_REQUEST) {
            ec_log(10, __func__,
                    "got no ack on foe write request, got 0x%X\n", 
                    read_buf_ack->foe_hdr.op_code);
            goto exit;
        }
        
        if (last_pkt)
            break;
    }

exit:
    ec_log(10, __func__, "file download finished\n");

    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);
    return wkc;
}

