//! ethercat canopen over ethercat mailbox handling
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

#include "libethercat/ec.h"
#include "libethercat/mbx.h"
#include "libethercat/coe.h"
#include "libethercat/timer.h"
#include "libethercat/error_codes.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

typedef struct {
    unsigned number   : 9;
    unsigned reserved : 3;
    unsigned service  : 4;
} PACKED ec_coe_header_t;

typedef struct {
    unsigned size_indicator     : 1;
    unsigned transfer_type      : 1;
    unsigned data_set_size      : 2;
    unsigned complete           : 1;
    unsigned command            : 3;
    uint16_t index;
    uint8_t  sub_index;
} PACKED ec_sdo_init_download_header_t;

typedef struct {
    unsigned more_follows       : 1;
    unsigned seg_data_size      : 3;
    unsigned toggle             : 1;
    unsigned command            : 3;
} PACKED ec_sdo_seg_download_req_header_t;

// ------------------------ EXPEDITED --------------------------

//! expedited download request/upload response
typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_coe_header_t coe_hdr;
    ec_sdo_init_download_header_t sdo_hdr;
    ec_data_t sdo_data;
} PACKED ec_sdo_expedited_download_req_t, ec_sdo_expedited_upload_resp_t;

//! expedited download response/upload request
typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_coe_header_t coe_hdr;
    ec_sdo_init_download_header_t sdo_hdr;
} PACKED ec_sdo_expedited_download_resp_t, ec_sdo_expedited_upload_req_t;

#define EC_SDO_EXPEDITED_HDR_LEN \
    ((sizeof(ec_coe_header_t) + sizeof(ec_sdo_header_t)))

// ------------------------ SEGMENTED --------------------------

//! segmented download request/upload response
typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_coe_header_t coe_hdr;
    ec_sdo_seg_download_req_header_t sdo_hdr;
    ec_data_t sdo_data;
} PACKED ec_sdo_seg_download_req_t, ec_sdo_seg_upload_resp_t;

//! segmented download response/upload request
typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_coe_header_t coe_hdr;
    ec_sdo_init_download_header_t sdo_hdr;
} PACKED ec_sdo_seg_download_resp_t, ec_sdo_seg_upload_req_t;

#define EC_SDO_SEG_HDR_LEN \
    ((sizeof(ec_coe_header_t) + sizeof(ec_sdo_seg_download_req_header_t)))

// ------------------------ NORMAL --------------------------

//! normal download request/upload response
typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_coe_header_t coe_hdr;
    ec_sdo_init_download_header_t sdo_hdr;
    uint32_t complete_size;
    ec_data_t sdo_data;
} PACKED ec_sdo_normal_download_req_t, ec_sdo_normal_upload_resp_t;

//! normal download response/upload request
typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_coe_header_t coe_hdr;
    ec_sdo_init_download_header_t sdo_hdr;
} PACKED ec_sdo_normal_download_resp_t, ec_sdo_normal_upload_req_t;

#define EC_SDO_NORMAL_HDR_LEN \
    ((sizeof(ec_coe_header_t) + sizeof(ec_sdo_init_download_header_t) + sizeof(uint32_t)))

// ------------------------ ABORT --------------------------

typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_coe_header_t coe_hdr;
    ec_sdo_init_download_header_t sdo_hdr;
    uint32_t abort_code;
} PACKED ec_sdo_abort_request_t;

#define MSG_BUF_LEN     256
char msg_buf[MSG_BUF_LEN];

void ec_coe_print_msg(int level, const char *ctx, int slave, const char *msg, uint8_t *buf, size_t buflen) {
    char *tmp = msg_buf;
    size_t pos = 0;
    size_t max_pos = min(MSG_BUF_LEN, buflen);
    for (int u = 0; (u < max_pos) && ((MSG_BUF_LEN-pos) > 0); ++u) {
        pos += snprintf(tmp + pos, MSG_BUF_LEN - pos, "%02X ", buf[u]);
    }

    ec_log(level, ctx, "slave %d: %s - %s\n", slave, msg, msg_buf);
}


//! initialize CoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_coe_init(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_open(&slv->mbx.coe.recv_pool, 0, 1518);
    pthread_mutex_init(&slv->mbx.coe.lock, NULL);
                
    TAILQ_INIT(&slv->mbx.coe.emergencies);
}

//! deinitialize CoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_coe_deinit(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    
    pthread_mutex_destroy(&slv->mbx.coe.lock);
    pool_close(slv->mbx.coe.recv_pool);
}

//! \brief Wait for CoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] pp_entry  Returns pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_coe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    ec_mbx_sched_read(pec, slave);

    ec_timer_t timeout;
    ec_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    pool_get(slv->mbx.coe.recv_pool, pp_entry, &timeout);
}

//! \brief Enqueue CoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_coe_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    ec_coe_header_t *coe_hdr = (ec_coe_header_t *)(p_entry->data);

    if (coe_hdr->service == EC_COE_EMERGENCY) {
        ec_coe_emergency_enqueue(pec, slave, p_entry);
    } else {
        pool_put(slv->mbx.coe.recv_pool, p_entry);
    }
}

// read coe sdo 
int ec_coe_sdo_read(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t **buf, size_t *len, 
        uint32_t *abort_code) 
{
    int ret = -1;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    // default error return
    (*abort_code) = 0;

    // getting index
    pthread_mutex_lock(&slv->mbx.coe.lock);

    pool_entry_t *p_entry;
    ec_mbx_get_free_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock);

    ec_sdo_normal_upload_req_t *write_buf = (ec_sdo_normal_upload_req_t *)(p_entry->data);

    // mailbox header
    // (mbxhdr (6) - mbxhdr.length (2)) + coehdr (2) + sdohdr (4)
    write_buf->mbx_hdr.length    = EC_SDO_NORMAL_HDR_LEN; 
    write_buf->mbx_hdr.mbxtype   = EC_MBX_COE;
    // coe header
    write_buf->coe_hdr.service   = EC_COE_SDOREQ;
    // sdo header
    write_buf->sdo_hdr.command   = EC_COE_SDO_UPLOAD_REQ;
    write_buf->sdo_hdr.complete  = complete;
    write_buf->sdo_hdr.index     = index;
    write_buf->sdo_hdr.sub_index = sub_index;

    // send request
    ec_mbx_enqueue_head(pec, slave, p_entry);

    // wait for answer
    for (ec_coe_wait(pec, slave, &p_entry); p_entry; ec_coe_wait(pec, slave, &p_entry)) {
        ec_sdo_normal_upload_resp_t *read_buf = (void *)(p_entry->data);

        if (    (read_buf->coe_hdr.service == EC_COE_SDOREQ) &&
                (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ)) 
        {
            ec_sdo_abort_request_t *abort_buf = (ec_sdo_abort_request_t *)(p_entry->data); 

            ec_log(100, __func__, "slave %2d: got sdo abort request on idx %#X, subidx %d, "
                    "abortcode %#X\n", slave, index, sub_index, abort_buf->abort_code);

            *abort_code = abort_buf->abort_code;
            ret = EC_ERROR_MAILBOX_ABORT;
            break;
        } else if (read_buf->coe_hdr.service == EC_COE_SDORES) {
            // everthing is fine
            *abort_code = 0;

            if (read_buf->sdo_hdr.transfer_type) {
                ec_sdo_expedited_upload_resp_t *exp_read_buf = (void *)(p_entry->data);
                if (*len)    { (*len) = min(*len, 4 - read_buf->sdo_hdr.data_set_size); }
                else         { (*len) = 4 - read_buf->sdo_hdr.data_set_size; }

                if (!(*buf)) { (*buf) = malloc(*len); }

                memcpy(*buf, exp_read_buf->sdo_data.bdata, *len);
            } else {
                if (*len)    { (*len) = min(*len, read_buf->complete_size); }
                else         { (*len) = read_buf->complete_size; }

                if (!(*buf)) { (*buf) = malloc(*len); }

                memcpy(*buf, read_buf->sdo_data.bdata, *len);
            }

            ret = 0;
            break;
        } else {
            ec_coe_print_msg(1, __func__, slave, "got unexpected mailbox message", 
                    (uint8_t *)(p_entry->data), 6 + read_buf->mbx_hdr.length);
            ret = EC_ERROR_MAILBOX_READ;
        
            ec_mbx_return_free_buffer(pec, slave, p_entry);
            p_entry = NULL;
        }
    }

    if (p_entry) {
        ec_mbx_return_free_buffer(pec, slave, p_entry);
    }

    // returning index and ulock 
    pthread_mutex_unlock(&slv->mbx.coe.lock);

    return ret;
}

// write coe sdo 
int ec_coe_sdo_write(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t len,
        uint32_t *abort_code) 
{
    int ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    // default error return
    (*abort_code) = 0;

    pthread_mutex_lock(&slv->mbx.coe.lock);

    pool_entry_t *p_entry;
    ec_mbx_get_free_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock);

    ec_sdo_normal_download_req_t *write_buf = (ec_sdo_normal_download_req_t *)(p_entry->data);

    size_t max_len = slv->sm[0].len - 0x10,
           rest_len = len,
           seg_len = rest_len > max_len ? max_len : rest_len;

    // mailbox header
    // (mbxhdr (6) - mbxhdr.length (2)) + coehdr (2) + sdohdr (4)
    write_buf->mbx_hdr.length           = EC_SDO_NORMAL_HDR_LEN + seg_len; 
    write_buf->mbx_hdr.mbxtype          = EC_MBX_COE;
    // coe header
    write_buf->coe_hdr.service          = EC_COE_SDOREQ;
    // sdo header
    write_buf->sdo_hdr.size_indicator   = 1;
    write_buf->sdo_hdr.command          = EC_COE_SDO_DOWNLOAD_REQ;
    write_buf->sdo_hdr.complete         = complete;
    write_buf->sdo_hdr.index            = index;
    write_buf->sdo_hdr.sub_index        = sub_index;

    if (len <= 4 && !complete) {
        ec_sdo_expedited_download_req_t *exp_write_buf = (void *)(p_entry->data);

        exp_write_buf->mbx_hdr.length        = EC_SDO_NORMAL_HDR_LEN; 
        exp_write_buf->sdo_hdr.transfer_type = 1;
        exp_write_buf->sdo_hdr.data_set_size = 4 - len;
        memcpy(&exp_write_buf->sdo_data.ldata[0], buf, len);

        // send request
        ec_mbx_enqueue_head(pec, slave, p_entry);

        // wait for answer
        for (ec_coe_wait(pec, slave, &p_entry); p_entry; ec_coe_wait(pec, slave, &p_entry)) {
            ec_sdo_expedited_download_resp_t *read_buf = (void *)(p_entry->data);

            if (    (read_buf->coe_hdr.service == EC_COE_SDOREQ) &&
                    (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ)) 
            {
                ec_sdo_abort_request_t *abort_buf = (ec_sdo_abort_request_t *)(p_entry->data); 

                ec_log(100, __func__, "slave %d: got sdo abort request on idx %#X, subidx %d, "
                        "abortcode %#X\n", slave, index, sub_index, abort_buf->abort_code);

                *abort_code = abort_buf->abort_code;
                ret = EC_ERROR_MAILBOX_ABORT;
                goto exit; 
            } else if (read_buf->coe_hdr.service == EC_COE_SDORES) {
                // everthing is fine
                ec_mbx_return_free_buffer(pec, slave, p_entry);
                p_entry = NULL;
                goto exit;
            } else {
                ec_coe_print_msg(1, __func__, slave, "got unexpected mailbox message", 
                        (uint8_t *)(p_entry->data), 6 + read_buf->mbx_hdr.length);
                ret = EC_ERROR_MAILBOX_READ;

                ec_mbx_return_free_buffer(pec, slave, p_entry);
                p_entry = NULL;
            }
        }
    } 

    uint8_t *tmp = buf;
    write_buf->complete_size = len;
    memcpy(&write_buf->sdo_data.ldata[0], tmp, seg_len);
    rest_len -= seg_len;
    tmp += seg_len;

    // send request
    ec_mbx_enqueue_head(pec, slave, p_entry);

    // wait for answer
    for (ec_coe_wait(pec, slave, &p_entry); p_entry; ec_coe_wait(pec, slave, &p_entry)) {
        ec_sdo_normal_download_resp_t *read_buf = (void *)(p_entry->data);

        if (    (read_buf->coe_hdr.service == EC_COE_SDOREQ) &&
                (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ)) 
        {
            ec_sdo_abort_request_t *abort_buf = (ec_sdo_abort_request_t *)(p_entry->data); 

            ec_log(100, __func__, "slave %d: got sdo abort request on idx %#X, subidx %d, "
                    "abortcode %#X\n", slave, index, sub_index, abort_buf->abort_code);

            *abort_code = abort_buf->abort_code;
            ret = EC_ERROR_MAILBOX_ABORT;
            goto exit; 
        } else if (read_buf->coe_hdr.service == EC_COE_SDORES) {
            // everthing is fine
            ec_mbx_return_free_buffer(pec, slave, p_entry);
            seg_len += (EC_SDO_NORMAL_HDR_LEN - EC_SDO_SEG_HDR_LEN);
            break;
        } else {
            ec_coe_print_msg(1, __func__, slave, "got unexpected mailbox message", 
                    (uint8_t *)(p_entry->data), 6 + read_buf->mbx_hdr.length);
            ret = EC_ERROR_MAILBOX_READ;
        
            ec_mbx_return_free_buffer(pec, slave, p_entry);
            p_entry = NULL;
        }
    }

    uint8_t toggle = 1;

    while (rest_len) {
        ec_mbx_get_free_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock);
        MESSAGE_POOL_DEBUG(free);
        ec_sdo_seg_download_req_t *seg_write_buf = (void *)(p_entry->data);

        // need to send more segments
        seg_write_buf->mbx_hdr.length           = EC_SDO_SEG_HDR_LEN + seg_len;
        seg_write_buf->mbx_hdr.mbxtype          = EC_MBX_COE;
        // coe header
        seg_write_buf->coe_hdr.service          = EC_COE_SDOREQ;
        // sdo header
        seg_write_buf->sdo_hdr.toggle = toggle;
        toggle = !toggle;

        if (rest_len < seg_len) {
            seg_len = rest_len;
            seg_write_buf->sdo_hdr.command = EC_COE_SDO_DOWNLOAD_SEQ_REQ;

            if (rest_len < 7) {
                seg_write_buf->mbx_hdr.length = EC_SDO_NORMAL_HDR_LEN;
                seg_write_buf->sdo_hdr.seg_data_size = 7 - rest_len;
            } else
                seg_write_buf->mbx_hdr.length = EC_SDO_SEG_HDR_LEN + rest_len;
        }

        memcpy(&seg_write_buf->sdo_data, tmp, seg_len);
        rest_len -= seg_len;
        tmp += seg_len;

        // send request
        ec_mbx_enqueue_head(pec, slave, p_entry);

        // wait for answer
        for (ec_coe_wait(pec, slave, &p_entry); p_entry; ec_coe_wait(pec, slave, &p_entry)) {
            ec_sdo_seg_download_resp_t *read_buf = (void *)(p_entry->data);

            if (    (read_buf->coe_hdr.service == EC_COE_SDOREQ) &&
                    (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ)) 
            {
                ec_sdo_abort_request_t *abort_buf = (ec_sdo_abort_request_t *)(p_entry->data); 

                ec_log(100, __func__, "slave %d: got sdo abort request on idx %#X, subidx %d, "
                        "abortcode %#X\n", slave, index, sub_index, abort_buf->abort_code);

                *abort_code = abort_buf->abort_code;
                ret = EC_ERROR_MAILBOX_ABORT;
                goto exit; 
            } else if (read_buf->coe_hdr.service == EC_COE_SDORES) {
                // everthing is fine
                ec_mbx_return_free_buffer(pec, slave, p_entry);
                p_entry = NULL;
                break;
            } else {
                ec_coe_print_msg(1, __func__, slave, "got unexpected mailbox message", 
                        (uint8_t *)(p_entry->data), 6 + read_buf->mbx_hdr.length);
                ret = EC_ERROR_MAILBOX_READ;

                ec_mbx_return_free_buffer(pec, slave, p_entry);
                p_entry = NULL;
            }
        }
    }

exit:
    if (p_entry) {
        ec_mbx_return_free_buffer(pec, slave, p_entry);
    }

    pthread_mutex_unlock(&slv->mbx.coe.lock);
    return ret;
}

typedef struct PACKED ec_sdoinfoheader {
    unsigned opcode     : 7;
    unsigned incomplete : 1;
    unsigned reserved   : 8;
    uint16_t fragments_left;
} PACKED ec_sdoinfoheader_t;

typedef struct PACKED ec_sdo_odlist_req {
    ec_mbx_header_t      mbx_hdr;
    ec_coe_header_t      coe_hdr;
    ec_sdoinfoheader_t  sdo_info_hdr;
    uint16_t            list_type;
} PACKED ec_sdo_odlist_req_t;

typedef struct PACKED ec_sdo_odlist_resp {
    ec_mbx_header_t      mbx_hdr;
    ec_coe_header_t      coe_hdr;
    ec_sdoinfoheader_t  sdo_info_hdr;
    ec_data_t           sdo_info_data;
} PACKED ec_sdo_odlist_resp_t;

// read coe object dictionary list
int ec_coe_odlist_read(ec_t *pec, uint16_t slave, uint8_t **buf, size_t *len) {
    int ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    pthread_mutex_lock(&slv->mbx.coe.lock);

    pool_entry_t *p_entry;
    ec_mbx_get_free_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock);

    ec_sdo_odlist_req_t *write_buf = (ec_sdo_odlist_req_t *)(p_entry->data);

    // mailbox header
    write_buf->mbx_hdr.length       = 8;//12; // (mbxhdr (6) - length (2)) + coehdr (2) + sdoinfohdr (4)
    write_buf->mbx_hdr.mbxtype      = EC_MBX_COE;
    // coe header
    write_buf->coe_hdr.service      = EC_COE_SDOINFO;
    // sdo header
    write_buf->sdo_info_hdr.opcode  = EC_COE_SDO_INFO_ODLIST_REQ;
    write_buf->list_type            = 0x01;

    // send request
    ec_mbx_enqueue_head(pec, slave, p_entry);

    int val = 0, frag_left = 0;

    do {
        // wait for answer
        for (ec_coe_wait(pec, slave, &p_entry); p_entry; ec_coe_wait(pec, slave, &p_entry)) {
            ec_sdo_odlist_resp_t *read_buf = (void *)(p_entry->data); 

            if (val == 0) {
                // first fragment, allocate buffer if not passed
                size_t od_len = (read_buf->mbx_hdr.length - 8) +                              // first fragment
                    (read_buf->sdo_info_hdr.fragments_left * (read_buf->mbx_hdr.length - 6)); // following fragments

                if (*len)    { (*len) = min(*len, od_len); }
                else         { (*len) = od_len;            }

                if (!(*buf)) { (*buf) = malloc(*len);      }
            }

            uint8_t *from = val == 0 ? &read_buf->sdo_info_data.bdata[2] : &read_buf->sdo_info_data.bdata[0];
            size_t act_len = val == 0 ? (read_buf->mbx_hdr.length - 8) : (read_buf->mbx_hdr.length - 6);

            if ((val + act_len) > (*len)) {
                act_len = (*len) - val;
            }

            if (act_len) {
                memcpy((*buf) + val, from, act_len);
                val += act_len;
            }

            frag_left = read_buf->sdo_info_hdr.fragments_left;

            ec_mbx_return_free_buffer(pec, slave, p_entry);
            p_entry = NULL;
            break;
        }
    } while (frag_left != 0);

    *len = val;

    pthread_mutex_unlock(&slv->mbx.coe.lock);

    return ret;
}

typedef struct PACKED ec_sdo_desc_req {
    ec_mbx_header_t      mbx_hdr;
    ec_coe_header_t      coe_hdr;
    ec_sdoinfoheader_t  sdo_info_hdr;
    uint16_t            index;
} PACKED ec_sdo_desc_req_t;

typedef struct PACKED ec_sdo_desc_resp {
    ec_mbx_header_t      mbx_hdr;
    ec_coe_header_t      coe_hdr;
    ec_sdoinfoheader_t  sdo_info_hdr;
    ec_data_t           sdo_info_data;
} PACKED ec_sdo_desc_resp_t;

// read coe sdo description
int ec_coe_sdo_desc_read(ec_t *pec, uint16_t slave, uint16_t index,
        ec_coe_sdo_desc_t *desc) {
    int ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    pthread_mutex_lock(&slv->mbx.coe.lock);
    
    pool_entry_t *p_entry;
    ec_mbx_get_free_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock);

    ec_sdo_desc_req_t *write_buf = (ec_sdo_desc_req_t *)(p_entry->data);

    // mailbox header
    write_buf->mbx_hdr.length       = 12; // (mbxhdr - length) + coehdr + sdohdr
    write_buf->mbx_hdr.mbxtype      = EC_MBX_COE;
    // coe header
    write_buf->coe_hdr.service      = EC_COE_SDOINFO;
    // sdo header
    write_buf->sdo_info_hdr.opcode  = EC_COE_SDO_INFO_GET_OBJECT_DESC_REQ;
    write_buf->index                = index;

    // send request
    ec_mbx_enqueue_head(pec, slave, p_entry);

    // wait for answer
    for (ec_coe_wait(pec, slave, &p_entry); p_entry; ec_coe_wait(pec, slave, &p_entry)) {
        ec_sdo_desc_resp_t *read_buf = (void *)(p_entry->data); 
        if (    (read_buf->coe_hdr.service == EC_COE_SDOINFO) &&
                (read_buf->sdo_info_hdr.opcode == EC_COE_SDO_INFO_GET_OBJECT_DESC_RESP)) {
            // transfer was successfull
            desc->data_type         = read_buf->sdo_info_data.wdata[1];
            desc->max_subindices    = read_buf->sdo_info_data.bdata[4];
            desc->obj_code          = read_buf->sdo_info_data.bdata[5];
            desc->name_len          = read_buf->mbx_hdr.length - 6 - 6;
            desc->name              = (char *)malloc(desc->name_len); // must be freed by caller
            memcpy(desc->name, &read_buf->sdo_info_data.bdata[6], desc->name_len);
        } else {
            // not our answer, print out this message
            ec_coe_print_msg(1, __func__, slave, "unexpected coe answer", 
                    (uint8_t *)read_buf, 6 + read_buf->mbx_hdr.length);
            memset(desc, 0, sizeof(ec_coe_sdo_desc_t));
            ret = EC_ERROR_MAILBOX_READ;
        }
            
        break;
    }

    if (p_entry) {
        ec_mbx_return_free_buffer(pec, slave, p_entry);
    }

    pthread_mutex_unlock(&slv->mbx.coe.lock);
    return ret;
}

typedef struct PACKED ec_sdo_entry_desc_req {
    ec_mbx_header_t      mbx_hdr;
    ec_coe_header_t      coe_hdr;
    ec_sdoinfoheader_t  sdo_info_hdr;
    uint16_t            index;
    uint8_t             sub_index;
    uint8_t             value_info;
} PACKED ec_sdo_entry_desc_req_t;

typedef struct PACKED ec_sdo_entry_desc_resp {
    ec_mbx_header_t      mbx_hdr;
    ec_coe_header_t      coe_hdr;
    ec_sdoinfoheader_t  sdo_info_hdr;
    uint16_t            index;
    uint8_t             sub_index;
    uint8_t             value_info;
    uint16_t            data_type;
    uint16_t            bit_length;
    uint16_t            obj_access;
    ec_data_t           desc_data;
} PACKED ec_sdo_entry_desc_resp_t;
        
// read coe sdo entry description
int ec_coe_sdo_entry_desc_read(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, uint8_t value_info, ec_coe_sdo_entry_desc_t *desc) {
    int ret = EC_ERROR_MAILBOX_READ;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    
    pthread_mutex_lock(&slv->mbx.coe.lock);

    pool_entry_t *p_entry;
    ec_mbx_get_free_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock);

    ec_sdo_entry_desc_req_t *write_buf = (ec_sdo_entry_desc_req_t *)(p_entry->data);

    // mailbox header
    write_buf->mbx_hdr.length       = 10; // (mbxhdr - length) + coehdr + sdohdr
    write_buf->mbx_hdr.mbxtype      = EC_MBX_COE;
    // coe header
    write_buf->coe_hdr.service      = EC_COE_SDOINFO;
    // sdo header
    write_buf->sdo_info_hdr.opcode  = EC_COE_SDO_INFO_GET_ENTRY_DESC_REQ;
    write_buf->index                = index;
    write_buf->sub_index            = sub_index;
    write_buf->value_info           = value_info;

    // send request
    ec_mbx_enqueue_head(pec, slave, p_entry);

    // wait for answer
    for (ec_coe_wait(pec, slave, &p_entry); p_entry; ec_coe_wait(pec, slave, &p_entry)) {
        ec_sdo_entry_desc_resp_t *read_buf = (void *)(p_entry->data); 

        if (    (read_buf->coe_hdr.service == EC_COE_SDOINFO) &&
                (read_buf->sdo_info_hdr.opcode == EC_COE_SDO_INFO_GET_ENTRY_DESC_RESP)) {
            // transfer was successfull
            desc->value_info    = read_buf->value_info;
            desc->data_type     = read_buf->data_type;
            desc->bit_length    = read_buf->bit_length;
            desc->obj_access    = read_buf->obj_access;
            desc->data_len      = read_buf->mbx_hdr.length - 6 - 10;

            if (!desc->data) { desc->data = malloc(desc->data_len); }
            memcpy(desc->data, read_buf->desc_data.bdata, desc->data_len);
            ret = 0;
        } else {
            // not our answer, print out this message
            ec_coe_print_msg(1, __func__, slave, "unexpected coe answer", 
                    (uint8_t *)read_buf, 6 + read_buf->mbx_hdr.length);
            memset(desc, 0, sizeof(ec_coe_sdo_entry_desc_t));
            ret = EC_ERROR_MAILBOX_READ;
        }

        break;
    }

    if (p_entry) {        
        ec_mbx_return_free_buffer(pec, slave, p_entry);
    }

    pthread_mutex_unlock(&slv->mbx.coe.lock);
    return ret;
}

int ec_coe_generate_mapping(ec_t *pec, uint16_t slave) {
    int ret = 0;
    uint8_t *buf = NULL;
    uint16_t start_adr; 
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (slv->sm[0].adr > slv->sm[1].adr)
        start_adr = slv->sm[0].adr + slv->sm[0].len;
    else 
        start_adr = slv->sm[1].adr + slv->sm[1].len;

    for (int sm_idx = 2; sm_idx <= 3; ++sm_idx) {
        int bit_len = 0, idx = 0x1c10 + sm_idx;
        uint8_t entry_cnt = 0, entry_cnt_2;
        size_t entry_cnt_size = sizeof(entry_cnt);
        uint32_t abort_code = 0;
        
        // read count of mapping entries, stored in subindex 0
        // mapped entreis are stored at 0x1c12 and 0x1c13 and should usually be
        // written in state preop with an init command
        buf = (uint8_t *)&entry_cnt;
        if ((ret = ec_coe_sdo_read(pec, slave, idx, 0, 0, &buf, 
                &entry_cnt_size, &abort_code)) != 0) {
            ec_log(1, "GENERATE_MAPPING COE", "slave %2d: sm%d reading "
                    "0x%04X/%d failed, error code 0x%X\n", slave, sm_idx, idx, 0, ret);
            continue;
        }

        ec_log(100, "GENERATE_MAPPING COE", "slave %2d: sm%d 0x%04X "
                "count %d\n", slave, sm_idx, idx, entry_cnt); 

        // now read all mapped pdo's to retreave the mapped object lengths
        for (int i = 1; i <= entry_cnt; ++i) {
            uint16_t entry_idx;
            size_t entry_size = sizeof(entry_idx);
            
            // read entry subindex with mapped value
            buf = (uint8_t *)&entry_idx;
            if ((ret = ec_coe_sdo_read(pec, slave, idx, i, 0, 
                    &buf, &entry_size, &abort_code)) != 0) {
                ec_log(1, "GENERATE_MAPPING COE", "            "
                        "pdo: reading 0x%04X/%d failed, error code 0x%X\n", 
                        idx, i, ret);
                continue;
            }
            
            // read entry subindex with mapped value
            if (entry_idx == 0) {
                ec_log(100, "GENERATE_MAPPING COE", "            "
                        "pdo: entry_idx is 0\n");
                continue;
            }

            entry_cnt_size = sizeof(entry_cnt_2);

            // read count of entries of mapped value
            buf = (uint8_t *)&entry_cnt_2;
            if ((ret = ec_coe_sdo_read(pec, slave, entry_idx, 0, 0, 
                    &buf, &entry_cnt_size, &abort_code)) != 0) {
                ec_log(1, "GENERATE_MAPPING COE", "             "
                        "pdo: reading 0x%04X/%d failed, error code 0x%X\n", 
                        entry_idx, 0, ret);
                continue;
            }

            ec_log(100, "GENERATE_MAPPING COE", "             "
                    "pdo: 0x%04X count %d\n", entry_idx, entry_cnt_2); 

            for (int j = 1; j <= entry_cnt_2; ++j) {
                uint32_t entry;
                size_t entry_size = sizeof(entry);
                buf = (uint8_t *)&entry;
                if ((ret = ec_coe_sdo_read(pec, slave, entry_idx, j, 0, &buf,
                            &entry_size, &abort_code)) != 0) {
                    ec_log(1, "GENERATE_MAPPING COE", "                "
                            "reading 0x%04X/%d failed, error code 0x%X\n", 
                            entry_idx, j, ret);
                    continue;
                }

                bit_len += entry & 0x000000FF;
                
                ec_log(100, "GENERATE_MAPPING COE", "                "
                        "mapped entry 0x%04X / %d -> %d bits\n",
                        (entry & 0xFFFF0000) >> 16, 
                        (entry & 0x0000FF00) >> 8, 
                        (entry & 0x000000FF));
            }                        
        }

        // store sync manager settings if we have at least 1 bit mapped
        if (bit_len) {
            ec_log(100, "GENERATE_MAPPING COE", 
                    "slave %2d: sm%d length bits %d, bytes %d\n", 
                    slave, sm_idx, bit_len, (bit_len + 7) / 8);

            if (slv->sm && slv->sm_ch > sm_idx) {
                slv->sm[sm_idx].len = (bit_len + 7) / 8;

                // only set a new address if not previously set by
                // user or eeprom. some slave require the sm address to be
                // exactly the address stored in eeprom.
                if (!slv->sm[sm_idx].adr) {
                    slv->sm[sm_idx].adr = start_adr;
                    start_adr += slv->sm[sm_idx].len * 3;
                }

                slv->sm[sm_idx].flags = sm_idx == 2 ? 0x10064 : 0x10020;
            }
        }
    }

    return ret;
}

//! queue read mailbox content
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 */
void ec_coe_emergency_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    // get length and queue this message    
    ec_mbx_header_t *hdr = (ec_mbx_header_t *)(p_entry->data);

    // don't copy any headers, we already know that we have a coe emergency
    size_t msg_len = hdr->length - 2;

    ec_coe_emergency_message_entry_t *qmsg = (ec_coe_emergency_message_entry_t *)
        malloc(sizeof(ec_coe_emergency_message_entry_t) + msg_len);

    // skip mbx header and coe header
    memcpy(qmsg->msg, (uint8_t *)(p_entry->data) + 6 + 2, msg_len);

    qmsg->msg_len = msg_len;
    ec_timer_gettime(&qmsg->timestamp);
    TAILQ_INSERT_TAIL(&slv->mbx.coe.emergencies, qmsg, qh);

    ec_mbx_return_free_buffer(pec, slave, p_entry);
}

