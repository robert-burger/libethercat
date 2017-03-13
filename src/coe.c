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

#include "libethercat/mbx.h"
#include "libethercat/coe.h"
#include "libethercat/timer.h"
#include "libethercat/error_codes.h"

#include <stdio.h>
#include <string.h>

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
        uint8_t sub_index, int complete, uint8_t **buf, size_t *len, 
        uint32_t *abort_code) {
    int wkc, ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    // default error return
    (*abort_code) = 0;

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE))
        return EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    if (!(slv->mbx_write.buf))
        return EC_ERROR_MAILBOX_WRITE_IS_NULL;
    if (!(slv->mbx_read.buf))
        return EC_ERROR_MAILBOX_READ_IS_NULL;

    pthread_mutex_lock(&slv->mbx_lock);

    ec_sdo_normal_upload_req_t *write_buf = 
        (ec_sdo_normal_upload_req_t *)(slv->mbx_write.buf);

    // empty mailbox if anything in
    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0);

    // mailbox header
    // (mbxhdr (6) - mbxhdr.length (2)) + coehdr (2) + sdohdr (4)
    ec_mbx_clear(pec, slave, 0);
    write_buf->mbx_hdr.length    = EC_SDO_NORMAL_HDR_LEN; 

    write_buf->mbx_hdr.address   = 0x0000;
    write_buf->mbx_hdr.priority  = 0x00;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_COE;

    // coe header
    write_buf->coe_hdr.service   = EC_COE_SDOREQ;
    write_buf->coe_hdr.number    = 0x00;

    // sdo header
    write_buf->sdo_hdr.command   = EC_COE_SDO_UPLOAD_REQ;
    write_buf->sdo_hdr.complete  = complete;
    write_buf->sdo_hdr.index     = index;
    write_buf->sdo_hdr.sub_index = sub_index;

    // send request
    wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, "ec_coe_sdo_write", "error on writing send mailbox\n");
        ret = EC_ERROR_MAILBOX_WRITE;
        goto exit;
    }

    // wait for answer
    ec_mbx_clear(pec, slave, 1);
    wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, "ec_coe_sdo_write", "error on reading receive mailbox\n");
        ret = EC_ERROR_MAILBOX_READ;
        goto exit;
    }

    ec_sdo_normal_upload_resp_t *read_buf  = 
        (ec_sdo_normal_upload_resp_t *)(slv->mbx_read.buf); 

    if (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ) {
        ec_sdo_abort_request_t *abort_buf = 
            (ec_sdo_abort_request_t *)(slv->mbx_read.buf); 

        ec_log(100, "ec_coe_sdo_write", "got sdo abort request on idx %#X, "
                "subidx %d, abortcode %#X\n", index, sub_index, 
                abort_buf->abort_code);

        *abort_code = abort_buf->abort_code;

        ret = EC_ERROR_MAILBOX_ABORT;
        goto exit;
    }

    // everthing is fine
    *abort_code = 0;

    if (read_buf->sdo_hdr.transfer_type) {
        if (*len) 
            (*len) = min(*len, 4 - read_buf->sdo_hdr.data_set_size);
        else 
            (*len) = 4 - read_buf->sdo_hdr.data_set_size;

        if (!(*buf))
            (*buf) = malloc(*len);

        ec_sdo_expedited_upload_resp_t *exp_read_buf = 
            (ec_sdo_expedited_upload_resp_t *)(slv->mbx_read.buf);
        memcpy(*buf, exp_read_buf->sdo_data.bdata, *len);
    } else {
        if (*len) 
            (*len) = min(*len, read_buf->complete_size);
        else 
            (*len) = read_buf->complete_size;

        if (!(*buf))
            (*buf) = malloc(*len);

        memcpy(*buf, read_buf->sdo_data.bdata, *len);
    }

exit:
    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);

    return ret;
}

//! write coe sdo 
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param index sdo index
 * \param sub_index sdo sub index
 * \param complete complete access (only if sub_index == 0)
 * \param buf buffer to write to sdo
 * \param len length of buffer, outputs written length
 * \return working counter
 */
int ec_coe_sdo_write(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t len,
        uint32_t *abort_code) {
    int wkc, ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    // default error return
    (*abort_code) = 0;

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE))
        return EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    if (!(slv->mbx_write.buf))
        return EC_ERROR_MAILBOX_WRITE_IS_NULL;
    if (!(slv->mbx_read.buf))
        return EC_ERROR_MAILBOX_READ_IS_NULL;

    pthread_mutex_lock(&slv->mbx_lock);

    ec_sdo_normal_download_req_t *write_buf = 
        (ec_sdo_normal_download_req_t *)(slv->mbx_write.buf);

    // empty mailbox if anything pending
    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0);

    size_t max_len = slv->sm[0].len - 0x10,
           rest_len = len,
           seg_len = rest_len > max_len ? max_len : rest_len;

    // mailbox header
    // (mbxhdr (6) - mbxhdr.length (2)) + coehdr (2) + sdohdr (4)
    ec_mbx_clear(pec, slave, 0);
    write_buf->mbx_hdr.length           = EC_SDO_NORMAL_HDR_LEN + seg_len; 
    write_buf->mbx_hdr.address          = 0x0000;
    write_buf->mbx_hdr.priority         = 0x00;
    write_buf->mbx_hdr.mbxtype          = EC_MBX_COE;

    // coe header
    write_buf->coe_hdr.service          = EC_COE_SDOREQ;
    write_buf->coe_hdr.number           = 0x00;

    // sdo header
    write_buf->sdo_hdr.size_indicator   = 1;
    write_buf->sdo_hdr.command          = EC_COE_SDO_DOWNLOAD_REQ;
    write_buf->sdo_hdr.transfer_type    = 0;
    write_buf->sdo_hdr.data_set_size    = 0;
    write_buf->sdo_hdr.complete         = complete;
    write_buf->sdo_hdr.index            = index;
    write_buf->sdo_hdr.sub_index        = sub_index;

    if (len <= 4 && !complete) {
        ec_sdo_expedited_download_req_t *exp_write_buf = 
            (ec_sdo_expedited_download_req_t *)(slv->mbx_write.buf);

        exp_write_buf->mbx_hdr.length        = EC_SDO_NORMAL_HDR_LEN; 
        exp_write_buf->sdo_hdr.transfer_type = 1;
        exp_write_buf->sdo_hdr.data_set_size = 4 - len;
        memcpy(&exp_write_buf->sdo_data.ldata[0], buf, len);

        // send request
        wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, "ec_coe_sdo_write", "error on writing send mailbox\n");
            ret = EC_ERROR_MAILBOX_WRITE;
            goto exit;
        }

        // wait for answer
        ec_mbx_clear(pec, slave, 1);
        wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, "ec_coe_sdo_write", "error on reading receive mailbox\n");
            ret = EC_ERROR_MAILBOX_READ;
            goto exit;
        }

        ec_sdo_expedited_upload_resp_t *read_buf  = 
            (ec_sdo_expedited_upload_resp_t *)(slv->mbx_read.buf); 

        if (!(read_buf->mbx_hdr.mbxtype == EC_MBX_COE))
            ec_log(10, "ec_coe_sdo_write", "error on reading receive mailbox: answer is not COE\n");

        goto exit;
    } 

    uint8_t *tmp = buf;
    write_buf->complete_size = len;
    memcpy(&write_buf->sdo_data.ldata[0], tmp, seg_len);
    rest_len -= seg_len;
    tmp += seg_len;

    // send request
    wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, "ec_coe_sdo_write", "error on writing send mailbox\n");
        ret = EC_ERROR_MAILBOX_WRITE;
        goto exit;
    }

    // wait for answer
    ec_mbx_clear(pec, slave, 1);
    wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, "ec_coe_sdo_write", "error on reading receive mailbox\n");
        ret = EC_ERROR_MAILBOX_READ;
        goto exit;
    }

    ec_sdo_normal_upload_resp_t *read_buf  = 
        (ec_sdo_normal_upload_resp_t *)(slv->mbx_read.buf); 

    if (!(read_buf->mbx_hdr.mbxtype == EC_MBX_COE))
        ec_log(10, "ec_coe_sdo_write", "error on reading receive mailbox: answer is not "
                "COE is 0x%X need 0x%X\n", read_buf->mbx_hdr.mbxtype, EC_MBX_COE);

    seg_len += (EC_SDO_NORMAL_HDR_LEN - EC_SDO_SEG_HDR_LEN);

    ec_sdo_seg_download_req_t *seg_write_buf = 
        (ec_sdo_seg_download_req_t *)(slv->mbx_write.buf);
    ec_sdo_seg_upload_resp_t *seg_read_buf  = 
        (ec_sdo_seg_upload_resp_t *)(slv->mbx_read.buf); 
    seg_write_buf->sdo_hdr.toggle = 1;

    while (rest_len) {
        // need to send more segments
        seg_write_buf->sdo_hdr.command = 0;
        seg_write_buf->sdo_hdr.toggle = !seg_write_buf->sdo_hdr.toggle;
        seg_write_buf->sdo_hdr.seg_data_size = 0;
        seg_write_buf->mbx_hdr.length = EC_SDO_SEG_HDR_LEN + seg_len;

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
        wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, "ec_coe_sdo_write", "error on writing send mailbox\n");
            ret = EC_ERROR_MAILBOX_WRITE;
            goto exit;
        }

        // wait for answer
        ec_mbx_clear(pec, slave, 1);
        wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, "ec_coe_sdo_write", "error on reading receive mailbox\n");
            ret = EC_ERROR_MAILBOX_READ;
            goto exit;
        }

        if (!(seg_read_buf->mbx_hdr.mbxtype == EC_MBX_COE))
            ec_log(10, "ec_coe_sdo_write", "error on reading receive mailbox: answer is not COE\n");

    }

exit:
    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);
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

//! read coe object dictionary list
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param buf buffer to store answer
 * \param len length of buffer, outputs read length
 * \return working counter
 */
int ec_coe_odlist_read(ec_t *pec, uint16_t slave, uint8_t **buf, size_t *len) {
    int wkc, ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE))
        return EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    if (!(slv->mbx_write.buf))
        return EC_ERROR_MAILBOX_WRITE_IS_NULL;
    if (!(slv->mbx_read.buf))
        return EC_ERROR_MAILBOX_READ_IS_NULL;

    pthread_mutex_lock(&slv->mbx_lock);

    ec_sdo_odlist_req_t *write_buf = 
        (ec_sdo_odlist_req_t *)(pec->slaves[slave].mbx_write.buf);
    ec_sdo_odlist_resp_t *read_buf = 
        (ec_sdo_odlist_resp_t *)(pec->slaves[slave].mbx_read.buf); 

    ec_mbx_clear(pec, slave, 1);
    while (ec_mbx_receive(pec, slave, 0) > 0)
        ; // empty mailbox if anything pending

    ec_mbx_clear(pec, slave, 0);

    // mailbox header
    write_buf->mbx_hdr.length       = 8;//12; // (mbxhdr (6) - length (2)) + coehdr (2) + sdoinfohdr (4)
    write_buf->mbx_hdr.address      = 0x0000;
    write_buf->mbx_hdr.priority     = 0x00;
    write_buf->mbx_hdr.mbxtype      = EC_MBX_COE;

    // coe header
    write_buf->coe_hdr.service      = EC_COE_SDOINFO;
    write_buf->coe_hdr.number       = 0x00;

    // sdo header
    write_buf->sdo_info_hdr.opcode  = EC_COE_SDO_INFO_ODLIST_REQ;
    write_buf->list_type            = 0x01;

    // send request
    wkc = ec_mbx_send(pec, slave, 10 * EC_DEFAULT_TIMEOUT_MBX);
    if (wkc != 1) {
        ec_log(10, __func__, "send mailbox failed\n");
        ret = EC_ERROR_MAILBOX_WRITE;
    }

    int val = 0, errors = 0;

    do {
        // wait for answer
        ec_mbx_clear(pec, slave, 1);
        wkc = ec_mbx_receive(pec, slave, 10 * EC_DEFAULT_TIMEOUT_MBX);
        if (wkc != 1) {
            ec_log(10, __func__, "receive mailbox failed\n");
            ret = EC_ERROR_MAILBOX_READ;
            continue;
        }

        if (read_buf->mbx_hdr.mbxtype != EC_MBX_COE) {
            if (++errors == 10) {
                ec_log(10, __func__, "receive mailbox got more than 10 errors...\n");
                return EC_ERROR_MAILBOX_READ;
            }

            continue;
        }

        if (val == 0) {
            // first fragment, allocate buffer if not passed
            size_t od_len = (read_buf->mbx_hdr.length - 10) +                             // first fragment
                (read_buf->sdo_info_hdr.fragments_left * (read_buf->mbx_hdr.length - 6)); // following fragments

            if (*len) 
                (*len) = min(*len, od_len);
            else 
                (*len) = od_len;

            if (!(*buf))
                (*buf) = malloc(*len);
        }

        uint8_t *from = val == 0 ? &read_buf->sdo_info_data.bdata[4] : 
            &read_buf->sdo_info_data.bdata[0];
        size_t act_len = val == 0 ? (read_buf->mbx_hdr.length - 10) : (read_buf->mbx_hdr.length - 6);

        if ((val + act_len) > (*len))
            act_len = (*len) - val;

        if (act_len) {
            memcpy((*buf) + val, from, act_len);
            val += act_len;
        }
    } while (read_buf->sdo_info_hdr.fragments_left);

    *len = val;

    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);
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

//! read coe sdo description
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param index sdo index
 * \param buf buffer to store answer
 * \param len length of buffer, outputs read length
 * \return working counter
 */
int ec_coe_sdo_desc_read(ec_t *pec, uint16_t slave, uint16_t index,
        ec_coe_sdo_desc_t *desc) {
    int wkc, ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE))
        return EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    if (!(slv->mbx_write.buf))
        return EC_ERROR_MAILBOX_WRITE_IS_NULL;
    if (!(slv->mbx_read.buf))
        return EC_ERROR_MAILBOX_READ_IS_NULL;

    pthread_mutex_lock(&slv->mbx_lock);

    ec_sdo_desc_req_t *write_buf = (ec_sdo_desc_req_t *)(slv->mbx_write.buf);
    ec_sdo_desc_resp_t *read_buf = (ec_sdo_desc_resp_t *)(slv->mbx_read.buf); 
    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0); // empty mailbox if anything pending

    ec_mbx_clear(pec, slave, 0);

    // mailbox header
    write_buf->mbx_hdr.length       = 12; // (mbxhdr - length) + coehdr + sdohdr
    write_buf->mbx_hdr.address      = 0x0000;
    write_buf->mbx_hdr.priority     = 0x00;
    write_buf->mbx_hdr.mbxtype      = EC_MBX_COE;

    // coe header
    write_buf->coe_hdr.service      = EC_COE_SDOINFO;
    write_buf->coe_hdr.number       = 0x00;

    // sdo header
    write_buf->sdo_info_hdr.opcode  = EC_COE_SDO_INFO_GET_OBJECT_DESC_REQ;
    write_buf->index                = index;

    // send request
    wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (wkc != 1) {
        ec_log(10, __func__, "send mailbox failed\n");
        ret = EC_ERROR_MAILBOX_WRITE;
    }

    // wait for answer
    ec_mbx_clear(pec, slave, 1);
    wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (wkc != 1) {
        ec_log(10, __func__, "receive mailbox failed\n");
        ret = EC_ERROR_MAILBOX_READ;
    }

    if (read_buf->coe_hdr.service == EC_COE_SDOINFO) {
        if (read_buf->sdo_info_hdr.opcode == EC_COE_SDO_INFO_GET_OBJECT_DESC_RESP) {
            // transfer was successfull
            desc->data_type         = read_buf->sdo_info_data.wdata[1];
            desc->max_subindices    = read_buf->sdo_info_data.bdata[4];
            desc->obj_code          = read_buf->sdo_info_data.bdata[5];

            desc->name_len = read_buf->mbx_hdr.length - 6 - 6;
            desc->name = (char *)malloc(desc->name_len); // must be freed by caller
            memcpy(desc->name, &read_buf->sdo_info_data.bdata[6], desc->name_len);
        }
    } else if (read_buf->coe_hdr.service == EC_COE_SDOREQ) {
        desc->data_type         = 0;
        desc->obj_code          = 0;
        desc->max_subindices    = 0;
        desc->name[0] = '\0';

        wkc = -1;
    }

    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);
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

//! read coe sdo entry description
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param index sdo index
 * \param buf buffer to store answer
 * \param len length of buffer, outputs read length
 * \return working counter
 */
int ec_coe_sdo_entry_desc_read(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, uint8_t value_info, ec_coe_sdo_entry_desc_t *desc) {
    int wkc, ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    
    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE))
        return EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    if (!(slv->mbx_write.buf))
        return EC_ERROR_MAILBOX_WRITE_IS_NULL;
    if (!(slv->mbx_read.buf))
        return EC_ERROR_MAILBOX_READ_IS_NULL;

    pthread_mutex_lock(&slv->mbx_lock);

    ec_sdo_entry_desc_req_t *write_buf = 
        (ec_sdo_entry_desc_req_t *)(slv->mbx_write.buf);
    ec_sdo_entry_desc_resp_t *read_buf = 
        (ec_sdo_entry_desc_resp_t *)(slv->mbx_read.buf); 

    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0); // empty mailbox if anything pending

    ec_mbx_clear(pec, slave, 0);

    // mailbox header
    write_buf->mbx_hdr.length       = 10; // (mbxhdr - length) + coehdr + sdohdr
    write_buf->mbx_hdr.address      = 0x0000;
    write_buf->mbx_hdr.priority     = 0x00;
    write_buf->mbx_hdr.mbxtype      = EC_MBX_COE;

    // coe header
    write_buf->coe_hdr.service      = EC_COE_SDOINFO;
    write_buf->coe_hdr.number       = 0x00;

    // sdo header
    write_buf->sdo_info_hdr.opcode  = EC_COE_SDO_INFO_GET_ENTRY_DESC_REQ;
    write_buf->index                = index;
    write_buf->sub_index            = sub_index;
    write_buf->value_info           = value_info;

    // send request
    wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (wkc != 1) {
        ec_log(10, __func__, "send mailbox failed\n");
        ret = EC_ERROR_MAILBOX_WRITE;
    }

    // wait for answer
    ec_mbx_clear(pec, slave, 1);
    wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (wkc != 1) {
        ec_log(10, __func__, "receive mailbox failed\n");
        ret = EC_ERROR_MAILBOX_READ;
    }

    if (read_buf->coe_hdr.service == EC_COE_SDOINFO) {
        if (read_buf->sdo_info_hdr.opcode == 
                EC_COE_SDO_INFO_GET_ENTRY_DESC_RESP) {
            // transfer was successfull
            desc->data_type     = read_buf->data_type;
            desc->bit_length    = read_buf->bit_length;
            desc->obj_access    = read_buf->obj_access;
            desc->data_len      = read_buf->mbx_hdr.length - 6 - 10;

            if (desc->data) {
                memcpy(desc->data, read_buf->desc_data.bdata, desc->data_len);
                //                int h;
                //                for (h = 0; h < desc->data_len; ++h) 
                //                    printf("%02X ", desc->data[h]);
                //                printf("\n");
            }

        }
    } else if (read_buf->coe_hdr.service == EC_COE_SDOREQ) {
        desc->data_type         = 0;
        desc->bit_length        = 0;
        desc->obj_access        = 0;
        desc->data_len          = 0;
        ret = EC_ERROR_MAILBOX_READ;
    }

    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);
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
        if (ec_coe_sdo_read(pec, slave, idx, 0, 0, &buf, 
                &entry_cnt_size, &abort_code) != 0) {
            ec_log(10, "GENERATE_MAPPING COE", "slave %2d: sm%d reading "
                    "0x%04X/%d failed\n", slave, sm_idx, idx, 0);
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
            if (!ec_coe_sdo_read(pec, slave, idx, i, 0, 
                    &buf, &entry_size, &abort_code)) {
                ec_log(10, "GENERATE_MAPPING COE", "            "
                        "pdo: reading 0x%04X/%d failed\n", idx, i);
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
            if (!ec_coe_sdo_read(pec, slave, entry_idx, 0, 0, 
                    &buf, &entry_cnt_size, &abort_code)) {
                ec_log(10, "GENERATE_MAPPING COE", "             "
                        "pdo: reading 0x%04X/%d failed\n", entry_idx, 0);
                continue;
            }

            ec_log(100, "GENERATE_MAPPING COE", "             "
                    "pdo: 0x%04X count %d\n", entry_idx, entry_cnt_2); 

            for (int j = 1; j <= entry_cnt_2; ++j) {
                uint32_t entry;
                size_t entry_size = sizeof(entry);
                buf = (uint8_t *)&entry;
                if (!ec_coe_sdo_read(pec, slave, entry_idx, j, 0, &buf,
                            &entry_size, &abort_code)) {
                    ec_log(10, "GENERATE_MAPPING COE", "                "
                            "reading 0x%04X/%d failed\n", 
                            entry_idx, j);
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

