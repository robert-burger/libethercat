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
 * If not, see <www.gnu.org/licenses/>.
 */

#include "config.h"

#include "libethercat/ec.h"
#include "libethercat/foe.h"
#include "libethercat/error_codes.h"

// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

//! FoE header
typedef struct PACKED ec_foe_header {
    uint8_t op_code;            //!< FoE op code
    // cppcheck-suppress unusedStructMember
    uint8_t reserved;           //!< FoE reserved 
} PACKED ec_foe_header_t;

//! read/write request
typedef struct PACKED ec_foe_rw_request {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_foe_header_t foe_hdr;    //!< FoE header
    uint32_t        password;   //!< FoE password
    char            file_name[MAX_FILE_NAME_SIZE];
                                //!< FoE filename to read/write
} PACKED ec_foe_rw_request_t;

//! data packet
typedef struct PACKED ec_foe_data_request {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_foe_header_t foe_hdr;    //!< FoE header
    uint32_t        packet_nr;  //!< FoE segmented packet number
    ec_data_t       data;       //!< FoE segmented packet data
} PACKED ec_foe_data_request_t;

//! acknowledge data packet
typedef struct PACKED ec_foe_ack_request {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_foe_header_t foe_hdr;    //!< FoE header
    uint32_t        packet_nr;  //!< FoE segmented packet number
} PACKED ec_foe_ack_request_t;

//! error request
typedef struct PACKED ec_foe_error_request {
    ec_mbx_header_t mbx_hdr;    //!< mailbox header
    ec_foe_header_t foe_hdr;    //!< FoE header
    uint32_t        error_code; //!< error code
    char            error_text[MAX_ERROR_TEXT_SIZE];
                                //!< error text
} PACKED ec_foe_error_request_t;

//! initialize FoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_foe_init(ec_t *pec, uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    if (pool_open(&slv->mbx.foe.recv_pool, 0, 1518) != EC_OK) {
        ec_log(1, __func__, "opening FoE receive pool failed!\n");
    }
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
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    if (pool_close(slv->mbx.foe.recv_pool) != EC_OK) {
        ec_log(1, __func__, "closing FoE receive pool failed!\n");
    }
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
static void ec_foe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(pp_entry != NULL);

    ec_slave_ptr(slv, pec, slave);

    ec_mbx_sched_read(pec, slave);

    ec_timer_t timeout;
    ec_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    // ignore return value here, may fail if no new message are currently available
    (void)pool_get(slv->mbx.foe.recv_pool, pp_entry, &timeout);
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
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    pool_put(slv->mbx.foe.recv_pool, p_entry);
}

#define MSG_BUF_LEN     256u
static void ec_foe_print_msg(int level, const char *ctx, int slave, const char *msg, uint8_t *buf, size_t buflen) {
    static char msg_buf[MSG_BUF_LEN];

    char *tmp = msg_buf;
    size_t pos = 0;
    size_t max_pos = min(MSG_BUF_LEN, buflen);
    for (uint32_t u = 0u; (u < max_pos) && ((MSG_BUF_LEN-pos) > 0u); ++u) {
        int local_ret = snprintf(&tmp[pos], MSG_BUF_LEN - pos, "%02X ", buf[u]);
        if (local_ret < 0) {
            ec_log(1, ctx, "slave %2d: snprintf failed with %d\n", slave, local_ret);
            break;
        } 

        pos += (size_t)local_ret;
    }

    ec_log(level, ctx, "slave %d: %s - %s\n", slave, msg, msg_buf);
}

static const char *dump_foe_error_request(int slave, ec_foe_error_request_t *read_buf_error) {
    static const char *EC_MAILBOX_FOE_ERROR_MESSAGE_FILE_NOT_FOUND = "File not found!";

    const char *ret = NULL;
    ec_log(10, __func__, "got foe error code 0x%X\n", read_buf_error->error_code);

    size_t text_len = (read_buf_error->mbx_hdr.length - 6u);
    if (text_len > 0u) {
        // cppcheck-suppress misra-c2012-21.3
        char *error_text = (char *)malloc(text_len + 1u);
        (void)strncpy(error_text, read_buf_error->error_text, text_len);
        error_text[text_len] = '\0';
        ec_log(10, __func__, "error_text: %s\n", error_text);
    } else {
        if (read_buf_error->error_code == 0x800Du) {
            ret = EC_MAILBOX_FOE_ERROR_MESSAGE_FILE_NOT_FOUND;
        }
    }

    ec_foe_print_msg(1, __func__, slave, "got message: ", (void *)read_buf_error, read_buf_error->mbx_hdr.length + 6u);

    return ret;
}

// read file over foe
int ec_foe_read(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t **file_data, 
        size_t *file_data_len, const char **error_message) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(file_data != NULL);
    assert(file_data_len != NULL);

    int ret = EC_OK;
    ec_slave_ptr(slv, pec, slave);
    pool_entry_t *p_entry_send;

    pthread_mutex_lock(&slv->mbx.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_FOE) != EC_OK) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_FOE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, &p_entry_send, NULL) != EC_OK) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else {
        // cppcheck-suppress misra-c2012-11.3
        ec_foe_rw_request_t *write_buf = (ec_foe_rw_request_t *)(p_entry_send->data);

        // calc lengths
        size_t foe_max_len = min(slv->sm[1].len, MAX_FILE_NAME_SIZE);
        size_t file_name_len = min(strlen(file_name), foe_max_len-6u);

        // mailbox header
        write_buf->mbx_hdr.length    = 6u + file_name_len;
        write_buf->mbx_hdr.mbxtype   = EC_MBX_FOE;
        // foe header (2 Byte)
        write_buf->foe_hdr.op_code   = EC_FOE_OP_CODE_READ_REQUEST;
        // read request (password 4 Byte)
        write_buf->password          = password;
        (void)memcpy(write_buf->file_name, file_name, file_name_len);

        ec_log(10, __func__, "start reading file \"%s\"\n", file_name);

        // send request
        ec_mbx_enqueue_head(pec, slave, p_entry_send);

        *file_data_len = 0;

        do { 
            ret = EC_ERROR_MAILBOX_FOE_AGAIN;

            // wait for answer
            pool_entry_t *p_entry_recv;
            ec_foe_wait(pec, slave, &p_entry_recv);

            if (p_entry_recv == NULL) {
                ret = EC_ERROR_MAILBOX_TIMEOUT;
            } else {
                // cppcheck-suppress misra-c2012-11.3
                ec_foe_data_request_t *read_buf_data = (ec_foe_data_request_t *)(p_entry_recv->data);

                if (read_buf_data->foe_hdr.op_code == EC_FOE_OP_CODE_ERROR_REQUEST) {
                    // cppcheck-suppress misra-c2012-11.3
                    ec_foe_error_request_t *read_buf_error = (ec_foe_error_request_t *)(p_entry_recv->data);
                    *error_message = dump_foe_error_request(slave, read_buf_error);
                    ret = EC_ERROR_MAILBOX_FOE_ERROR_REQ;
                } else if (read_buf_data->foe_hdr.op_code != EC_FOE_OP_CODE_DATA_REQUEST) {
                    ec_log(10, __func__, "got foe op_code %X\n", read_buf_data->foe_hdr.op_code);
                } else {
                    size_t len = read_buf_data->mbx_hdr.length - 6u;
                    // cppcheck-suppress misra-c2012-21.3
                    *file_data = (uint8_t *)realloc(*file_data, *file_data_len + len);
                    (void)memcpy(&(*file_data)[*file_data_len], &read_buf_data->data[0], len); 
                    *file_data_len += len;

                    int packet_nr = read_buf_data->packet_nr;
                    uint32_t read_data_length = read_buf_data->mbx_hdr.length;

                    if (ec_mbx_get_free_send_buffer(pec, slave, &p_entry_send, NULL) != EC_OK) {
                        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
                    } else {
                        // cppcheck-suppress misra-c2012-11.3
                        ec_foe_ack_request_t *write_buf_ack = (ec_foe_ack_request_t *)(p_entry_send->data);

                        // everthing is fine, send ack 
                        // mailbox header
                        write_buf_ack->mbx_hdr.length    = 6; 
                        write_buf_ack->mbx_hdr.mbxtype   = EC_MBX_FOE;
                        // foe
                        write_buf_ack->foe_hdr.op_code   = EC_FOE_OP_CODE_ACK_REQUEST;
                        write_buf_ack->packet_nr         = packet_nr;

                        // send request
                        ec_mbx_enqueue_head(pec, slave, p_entry_send);

                        // compare length + mbx_hdr_size with mailbox size
                        if ((read_data_length + 6u) < slv->sm[1].len) {
                            // finished here
                            ret = EC_OK;
                        }
                    }
                }

                ec_mbx_return_free_recv_buffer(pec, slave, p_entry_recv);
            }
        } while (ret == EC_ERROR_MAILBOX_FOE_AGAIN);

        ec_log(10, __func__, "reading file \"%s\" finished\n", file_name);
    }

    pthread_mutex_unlock(&slv->mbx.lock);
    return ret;
}

// write file over foe
int ec_foe_write(ec_t *pec, uint16_t slave, uint32_t password,
        char file_name[MAX_FILE_NAME_SIZE], uint8_t *file_data, 
        size_t file_data_len, const char **error_message) 
{
    (void)error_message;
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(file_data != NULL);

    int ret = EC_OK;
    ec_slave_ptr(slv, pec, slave);
    pool_entry_t *p_entry;

    pthread_mutex_lock(&slv->mbx.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_FOE) != EC_OK) { 
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_FOE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, &p_entry, NULL) != EC_OK) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else {
        // cppcheck-suppress misra-c2012-11.3
        ec_foe_rw_request_t *write_buf = (ec_foe_rw_request_t *)(p_entry->data);

        // calc lengths
        size_t foe_max_len = min(slv->sm[1].len, MAX_FILE_NAME_SIZE);
        size_t file_name_len = min(strlen(file_name), foe_max_len-6u);

        // mailbox header
        write_buf->mbx_hdr.length    = 6u + file_name_len;
        write_buf->mbx_hdr.mbxtype   = EC_MBX_FOE;
        // foe header (2 Byte)
        write_buf->foe_hdr.op_code   = EC_FOE_OP_CODE_WRITE_REQUEST;
        // read request (password 4 Byte)
        write_buf->password          = password;
        (void)memcpy(write_buf->file_name, file_name, file_name_len);

        // send request
        ec_mbx_enqueue_head(pec, slave, p_entry);

        // wait for answer
        ec_foe_wait(pec, slave, &p_entry);
        
        if (p_entry != NULL) {
            // cppcheck-suppress misra-c2012-11.3
            ec_foe_ack_request_t *read_buf_ack = (ec_foe_ack_request_t *)(p_entry->data);

            if (read_buf_ack->foe_hdr.op_code != EC_FOE_OP_CODE_ACK_REQUEST) {
                if (read_buf_ack->foe_hdr.op_code == EC_FOE_OP_CODE_ERROR_REQUEST) {
                    // cppcheck-suppress misra-c2012-11.3
                    ec_foe_error_request_t *read_buf_error = (ec_foe_error_request_t *)(p_entry->data);
                    *error_message = dump_foe_error_request(slave, read_buf_error);
                    ret = EC_ERROR_MAILBOX_FOE_ERROR_REQ;
                } else {
                    ec_log(10, __func__, "got no ack on foe write request, got 0x%X\n", 
                            read_buf_ack->foe_hdr.op_code);

                    ret = EC_ERROR_MAILBOX_FOE_NO_ACK;
                }
            } else {
                ret = EC_OK;
            }

            // returning ack message
            ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
        } else {
            ret = EC_ERROR_MAILBOX_TIMEOUT;
        }
    }

    if (ret == EC_OK) {
        // mailbox len - mailbox hdr (6) - foe header (6)
        size_t data_len = slv->sm[1].len - 6u - 6u;
        off_t file_offset = 0;
        int packet_nr = 0;
        int last_pkt = 0;

        do {
            ret = ec_mbx_get_free_send_buffer(pec, slave, &p_entry, NULL);
            if (ret == EC_OK) {
                // cppcheck-suppress misra-c2012-11.3
                ec_foe_data_request_t *write_buf_data = (ec_foe_data_request_t *)p_entry->data;

                size_t rest_len = file_data_len - file_offset;
                size_t bytes_read = min(rest_len, data_len);
                (void)memcpy(&write_buf_data->data[0], &file_data[file_offset], bytes_read);
                if (bytes_read < data_len) {
                    last_pkt = 1;
                }

                ec_log(10, __func__, "slave %2d: sending file offset %d, bytes %d\n", 
                        slave, file_offset, bytes_read);

                file_offset += bytes_read;

                packet_nr++;
                // mailbox header
                write_buf_data->mbx_hdr.length    = 6u + bytes_read; 
                write_buf_data->mbx_hdr.mbxtype   = EC_MBX_FOE;
                // foe
                write_buf_data->foe_hdr.op_code   = EC_FOE_OP_CODE_DATA_REQUEST;
                write_buf_data->packet_nr         = packet_nr;

                // send request
                ec_mbx_enqueue_head(pec, slave, p_entry);

                // wait for answer
                ec_foe_wait(pec, slave, &p_entry);
                if (p_entry != NULL) {
                    // cppcheck-suppress misra-c2012-11.3
                    ec_foe_ack_request_t *read_buf_ack = (ec_foe_ack_request_t *)(p_entry->data);

                    if (read_buf_ack->foe_hdr.op_code != EC_FOE_OP_CODE_ACK_REQUEST) {
                        if (read_buf_ack->foe_hdr.op_code == EC_FOE_OP_CODE_ERROR_REQUEST) {
                            // cppcheck-suppress misra-c2012-11.3
                            ec_foe_error_request_t *read_buf_error = (ec_foe_error_request_t *)(p_entry->data);
                            *error_message = dump_foe_error_request(slave, read_buf_error);
                            ret = EC_ERROR_MAILBOX_FOE_ERROR_REQ;
                        } else {
                            ec_log(10, __func__,
                                    "got no ack on foe write request, got 0x%X, last_pkt %d, bytes_read %d, data_len %d, packet_nr %d\n", 
                                    read_buf_ack->foe_hdr.op_code, last_pkt, bytes_read, data_len, packet_nr);

                            ret = EC_ERROR_MAILBOX_FOE_NO_ACK;
                        }
                    } else {
                        ret = EC_OK;
                    }

                    ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
                } else {
                    ec_log(10, __func__,
                            "got no ack on foe write request, last_pkt %d, bytes_read %d, data_len %d\n", 
                            last_pkt, bytes_read, data_len);
                    ret = EC_ERROR_MAILBOX_FOE_NO_ACK;
                }
            }
        } while ((last_pkt != 0) && (ret == EC_OK));

        if (ret == EC_OK) {
            ec_log(10, __func__, "file download finished\n");
        } else {
            ec_log(1, __func__, "file download FAILED: error %d\n", ret);
        }
    }

    pthread_mutex_unlock(&slv->mbx.lock);

    return ret;
}

