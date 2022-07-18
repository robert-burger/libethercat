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
 * If not, see <www.gnu.org/licenses/>.
 */

#include "config.h"

#include "libethercat/ec.h"
#include "libethercat/mbx.h"
#include "libethercat/coe.h"
#include "libethercat/timer.h"
#include "libethercat/error_codes.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

const char *get_sdo_info_error_string(uint32_t errorcode) {
    static const char *sdo_info_error_0x05030000 = "Toggle bit not changed";
    static const char *sdo_info_error_0x05040000 = "SDO protocol timeout";
    static const char *sdo_info_error_0x05040001 = "Client/Server command specifier not valid or unknown";
    static const char *sdo_info_error_0x05040005 = "Out of memory";
    static const char *sdo_info_error_0x06010000 = "Unsupported access to an object";
    static const char *sdo_info_error_0x06010001 = "Attempt to read to a write only object";
    static const char *sdo_info_error_0x06010002 = "Attempt to write to a read only object";
    static const char *sdo_info_error_0x06010003 = "Subindex cannot be written, SI0 must be 0 for write access";
    static const char *sdo_info_error_0x06010004 = "SDO Complete access not supported for objects of variable length such as ENUM object types";
    static const char *sdo_info_error_0x06010005 = "Object length exceeds mailbox size";
    static const char *sdo_info_error_0x06010006 = "Object mapped to RxPDO, SDO Download blocked";
    static const char *sdo_info_error_0x06020000 = "The object does not exist in the object directory";
    static const char *sdo_info_error_0x06040041 = "The object can not be mapped into the PDO";
    static const char *sdo_info_error_0x06040042 = "The number and length of the objects to be mapped would exceed the PDO length";
    static const char *sdo_info_error_0x06040043 = "General parameter incompatibility reason";
    static const char *sdo_info_error_0x06040047 = "General internal incompatibility in the device";
    static const char *sdo_info_error_0x06060000 = "Access failed due to a hardware error";
    static const char *sdo_info_error_0x06070010 = "Data type does not match, length of service parameter does not match";
    static const char *sdo_info_error_0x06070012 = "Data type does not match, length of service parameter too high";
    static const char *sdo_info_error_0x06070013 = "Data type does not match, length of service parameter too low";
    static const char *sdo_info_error_0x06090011 = "Subindex does not exist";
    static const char *sdo_info_error_0x06090030 = "Value range of parameter exceeded (only for write access)";
    static const char *sdo_info_error_0x06090031 = "Value of parameter written too high";
    static const char *sdo_info_error_0x06090032 = "Value of parameter written too low";
    static const char *sdo_info_error_0x06090036 = "Maximum value is less than minimum value";
    static const char *sdo_info_error_0x08000000 = "General error";
    static const char *sdo_info_error_0x08000020 = "Data cannot be transferred or stored to the application";
    static const char *sdo_info_error_0x08000021 = "Data cannot be transferred or stored to the application because of local control";
    static const char *sdo_info_error_0x08000022 = "Data cannot be transferred or stored to the application because of the present device state";
    static const char *sdo_info_error_0x08000023 = "Object dictionary dynamic generation fails or no object dictionary is present";
    static const char *sdo_info_error_unknown    = "UNKNOWN ERROR";

    const char *retval = sdo_info_error_unknown;

    switch (errorcode) {
        default:
            break;
#define ADD_CASE(x) \
        case (x): \
            retval = sdo_info_error_##x; \
            break;
        ADD_CASE(0x05030000)
        ADD_CASE(0x05040000)
        ADD_CASE(0x05040001)
        ADD_CASE(0x05040005)
        ADD_CASE(0x06010000)
        ADD_CASE(0x06010001)
        ADD_CASE(0x06010002)
        ADD_CASE(0x06010003)
        ADD_CASE(0x06010004)
        ADD_CASE(0x06010005)
        ADD_CASE(0x06010006)
        ADD_CASE(0x06020000)
        ADD_CASE(0x06040041)
        ADD_CASE(0x06040042)
        ADD_CASE(0x06040043)
        ADD_CASE(0x06040047)
        ADD_CASE(0x06060000)
        ADD_CASE(0x06070010)
        ADD_CASE(0x06070012)
        ADD_CASE(0x06070013)
        ADD_CASE(0x06090011)
        ADD_CASE(0x06090030)
        ADD_CASE(0x06090031)
        ADD_CASE(0x06090032)
        ADD_CASE(0x06090036)
        ADD_CASE(0x08000000)
        ADD_CASE(0x08000020)
        ADD_CASE(0x08000021)
        ADD_CASE(0x08000022)
        ADD_CASE(0x08000023)
    }

    return retval;
}

typedef struct {
    uint16_t number   : 9;
    uint16_t reserved : 3;
    uint16_t service  : 4;
} PACKED ec_coe_header_t;

typedef struct {
    uint8_t size_indicator     : 1;
    uint8_t transfer_type      : 1;
    uint8_t data_set_size      : 2;
    uint8_t complete           : 1;
    uint8_t command            : 3;
    uint16_t index;
    uint8_t  sub_index;
} PACKED ec_sdo_init_download_header_t;

typedef struct {
    uint8_t more_follows       : 1;
    uint8_t seg_data_size      : 3;
    uint8_t toggle             : 1;
    uint8_t command            : 3;
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

#define MSG_BUF_LEN     256u

static void ec_coe_print_msg(int level, const char *ctx, int slave, const char *msg, uint8_t *buf, size_t buflen) {
    static char msg_buf[MSG_BUF_LEN];

    char *tmp = msg_buf;
    size_t pos = 0;
    size_t max_pos = min(MSG_BUF_LEN, buflen);
    for (uint32_t u = 0; (u < max_pos) && (MSG_BUF_LEN > pos); ++u) {
        int ret = snprintf(&tmp[pos], MSG_BUF_LEN - pos, "%02X ", buf[u]);

        if (ret < 0) {
            break;
        } 

        pos += (uint32_t)ret;
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
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    check_ret(pool_open(&slv->mbx.coe.recv_pool, 0, 1518));
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
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    
    pthread_mutex_destroy(&slv->mbx.coe.lock);
    check_ret(pool_close(slv->mbx.coe.recv_pool));
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
static void ec_coe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry) {
    assert(pec != NULL);
    assert(pp_entry != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    ec_mbx_sched_read(pec, slave);

    ec_timer_t timeout;
    ec_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    check_ret(pool_get(slv->mbx.coe.recv_pool, pp_entry, &timeout));
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
    assert(pec != NULL);
    assert(p_entry != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    // cppcheck-suppress misra-c2012-11.3
    ec_coe_header_t *coe_hdr = (ec_coe_header_t *)(&(p_entry->data[sizeof(ec_mbx_header_t)]));

    ec_log(100, __func__, "got service: %0X\n", coe_hdr->service);

    if (coe_hdr->service == EC_COE_EMERGENCY) {
        ec_coe_emergency_enqueue(pec, slave, p_entry);
    } else if ((coe_hdr->service >= 0x01u) && (coe_hdr->service <= 0x08u)) {
        pool_put(slv->mbx.coe.recv_pool, p_entry);
    } else {
        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
    }
}

// read coe sdo 
int ec_coe_sdo_read(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t *len, 
        uint32_t *abort_code) 
{ 
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);
    assert(slave < pec->slave_cnt);

    pool_entry_t *p_entry = NULL;
    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    ec_slave_ptr(slv, pec, slave);
        
    // default error return
    (*abort_code) = 0;

    // getting index
    pthread_mutex_lock(&slv->mbx.coe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_COE) == 0) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock) != 0) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else { 
        assert(p_entry != NULL);

        // cppcheck-suppress misra-c2012-11.3
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
        ec_coe_wait(pec, slave, &p_entry);
        while (p_entry != NULL) {
            ec_sdo_normal_upload_resp_t *read_buf = (void *)(p_entry->data);

            if (    (read_buf->coe_hdr.service == EC_COE_SDOREQ) &&
                    (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ)) 
            {
                // cppcheck-suppress misra-c2012-11.3
                ec_sdo_abort_request_t *abort_buf = (ec_sdo_abort_request_t *)(p_entry->data); 

                ec_log(100, __func__, "slave %2d: got sdo abort request on idx %#X, subidx %d, "
                        "abortcode %#X\n", slave, index, sub_index, abort_buf->abort_code);

                *abort_code = abort_buf->abort_code;
                ret = EC_ERROR_MAILBOX_ABORT;
            } else if (read_buf->coe_hdr.service == EC_COE_SDORES) {
                // everthing is fine
                *abort_code = 0;

                ret = EC_OK;

                if (read_buf->sdo_hdr.transfer_type) {
                    ec_sdo_expedited_upload_resp_t *exp_read_buf = (void *)(p_entry->data);

                    if ((*len) < (4u - read_buf->sdo_hdr.data_set_size)) {
                        ret = EC_ERROR_MAILBOX_BUFFER_TOO_SMALL;
                    }

                    (*len) = 4u - read_buf->sdo_hdr.data_set_size;

                    if (ret == EC_OK) {
                        (void)memcpy(buf, exp_read_buf->sdo_data, *len);
                    }
                } else {
                    if ((*len) < read_buf->complete_size) {
                        ret = EC_ERROR_MAILBOX_BUFFER_TOO_SMALL;
                    } 

                    (*len) = read_buf->complete_size;

                    if (ret == EC_OK) {
                        (void)memcpy(buf, read_buf->sdo_data, *len);
                    }
                }
            } else {
                ec_coe_print_msg(1, __func__, slave, "got unexpected mailbox message", 
                        (uint8_t *)(p_entry->data), 6u + read_buf->mbx_hdr.length);
                ret = EC_ERROR_MAILBOX_READ;
            }

            ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
            p_entry = NULL;

            if ((ret == EC_OK) || (ret == EC_ERROR_MAILBOX_ABORT)) {
                break;
            }

            ec_coe_wait(pec, slave, &p_entry);
        }
    }

    // returning index and ulock 
    pthread_mutex_unlock(&slv->mbx.coe.lock);

    return ret;
}

static int ec_coe_sdo_write_expedited(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t len,
        uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(buf != NULL);
    assert(slave < pec->slave_cnt);

    pool_entry_t *p_entry = NULL;
    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    ec_slave_ptr(slv, pec, slave);

    // default error return
    (*abort_code) = 0;

    pthread_mutex_lock(&slv->mbx.coe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_COE) == 0) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock) != 0) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else { 
        assert(p_entry != NULL);

        // cppcheck-suppress misra-c2012-11.3
        ec_sdo_expedited_download_req_t *exp_write_buf = (void *)(p_entry->data);

        // mailbox header
        // (mbxhdr (6) - mbxhdr.length (2)) + coehdr (2) + sdohdr (4)
        exp_write_buf->mbx_hdr.length           = EC_SDO_NORMAL_HDR_LEN;
        exp_write_buf->mbx_hdr.mbxtype          = EC_MBX_COE;
        // coe header
        exp_write_buf->coe_hdr.service          = EC_COE_SDOREQ;
        // sdo header
        exp_write_buf->sdo_hdr.transfer_type    = 1u;
        exp_write_buf->sdo_hdr.data_set_size    = 4u - len;
        exp_write_buf->sdo_hdr.size_indicator   = 1;
        exp_write_buf->sdo_hdr.command          = EC_COE_SDO_DOWNLOAD_REQ;
        exp_write_buf->sdo_hdr.complete         = complete;
        exp_write_buf->sdo_hdr.index            = index;
        exp_write_buf->sdo_hdr.sub_index        = sub_index;

        // data
        (void)memcpy(&exp_write_buf->sdo_data[0], buf, len);

        // send request
        ec_mbx_enqueue_head(pec, slave, p_entry);

        // wait for answer
        ec_coe_wait(pec, slave, &p_entry);
        while (p_entry != NULL) {
            ec_sdo_expedited_download_resp_t *read_buf = (void *)(p_entry->data);

            if (    (read_buf->coe_hdr.service == EC_COE_SDOREQ) &&
                    (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ)) 
            {
                // cppcheck-suppress misra-c2012-11.3
                ec_sdo_abort_request_t *abort_buf = (ec_sdo_abort_request_t *)(p_entry->data); 

                ec_log(10, __func__, "slave %d: got sdo abort request on idx %#X, subidx %d, "
                        "abortcode %#X\n", slave, index, sub_index, abort_buf->abort_code);

                *abort_code = abort_buf->abort_code;
                ret = EC_ERROR_MAILBOX_ABORT;
            } else if (read_buf->coe_hdr.service == EC_COE_SDORES) {
                // everthing is fine
                ret = EC_OK;
            } else {
                ec_coe_print_msg(5, __func__, slave, "got unexpected mailbox message", 
                        (uint8_t *)(p_entry->data), 6u + read_buf->mbx_hdr.length);
                ret = EC_ERROR_MAILBOX_READ;
            }
                
            ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
            p_entry = NULL;

            if ((ret == EC_OK) || (ret == EC_ERROR_MAILBOX_ABORT)) {
                break;
            }

            ec_coe_wait(pec, slave, &p_entry);
        }
    }

    pthread_mutex_unlock(&slv->mbx.coe.lock);
    return ret;
}

static int ec_coe_sdo_write_normal(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t len,
        uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(buf != NULL);
    assert(slave < pec->slave_cnt);

    pool_entry_t *p_entry = NULL;
    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    ec_slave_ptr(slv, pec, slave);

    // default error return
    (*abort_code) = 0;

    pthread_mutex_lock(&slv->mbx.coe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_COE) == 0) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock) != 0) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else { 
        assert(p_entry != NULL);

        // cppcheck-suppress misra-c2012-11.3
        ec_sdo_normal_download_req_t *write_buf = (ec_sdo_normal_download_req_t *)(p_entry->data);

        size_t max_len = slv->sm[0].len - 0x10u;
        size_t rest_len = len;
        size_t seg_len = (rest_len > max_len) ? max_len : rest_len;

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

        // normal download
        uint8_t *tmp = buf;
        off_t tmp_pos = 0;
        write_buf->complete_size = len;
        (void)memcpy(&write_buf->sdo_data[0], &tmp[tmp_pos], seg_len);
        rest_len -= seg_len;
        tmp_pos += seg_len;

        // send request
        ec_mbx_enqueue_head(pec, slave, p_entry);

        // wait for answer
        ec_coe_wait(pec, slave, &p_entry);
        while (p_entry != NULL) {
            ec_sdo_normal_download_resp_t *read_buf = (void *)(p_entry->data);

            if (    (read_buf->coe_hdr.service == EC_COE_SDOREQ) &&
                    (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ)) 
            {
                // cppcheck-suppress misra-c2012-11.3
                ec_sdo_abort_request_t *abort_buf = (ec_sdo_abort_request_t *)(p_entry->data); 

                ec_log(10, __func__, "slave %d: got sdo abort request on idx %#X, subidx %d, "
                        "abortcode %#X\n", slave, index, sub_index, abort_buf->abort_code);

                *abort_code = abort_buf->abort_code;
                ret = EC_ERROR_MAILBOX_ABORT;
            } else if (read_buf->coe_hdr.service == EC_COE_SDORES) {
                // everthing is fine
                seg_len += (EC_SDO_NORMAL_HDR_LEN - EC_SDO_SEG_HDR_LEN);
                ret = EC_OK;
            } else {
                ec_coe_print_msg(5, __func__, slave, "got unexpected mailbox message", 
                        (uint8_t *)(p_entry->data), 6u + read_buf->mbx_hdr.length);
                ret = EC_ERROR_MAILBOX_READ;
            }
            
            ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
            p_entry = NULL;

            if ((ret == EC_OK) || (ret == EC_ERROR_MAILBOX_ABORT)) {
                break;
            }

            ec_coe_wait(pec, slave, &p_entry);
        }

        if (ret == EC_OK) {
            ret = EC_ERROR_MAILBOX_TIMEOUT;

            uint8_t toggle = 1u;

            while (rest_len != 0u) {
                if (ec_mbx_get_free_send_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock) != 0) {
                    ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
                    break;
                } else { 
                    ec_sdo_seg_download_req_t *seg_write_buf = (void *)(p_entry->data);

                    // need to send more segments
                    seg_write_buf->mbx_hdr.length           = EC_SDO_SEG_HDR_LEN + seg_len;
                    seg_write_buf->mbx_hdr.mbxtype          = EC_MBX_COE;
                    // coe header
                    seg_write_buf->coe_hdr.service          = EC_COE_SDOREQ;
                    // sdo header
                    seg_write_buf->sdo_hdr.toggle = toggle;
                    toggle = (toggle == 1u) ? 0u : 1u;

                    if (rest_len < seg_len) {
                        seg_len = rest_len;
                        seg_write_buf->sdo_hdr.command = EC_COE_SDO_DOWNLOAD_SEQ_REQ;

                        if (rest_len < 7u) {
                            seg_write_buf->mbx_hdr.length = EC_SDO_NORMAL_HDR_LEN;
                            seg_write_buf->sdo_hdr.seg_data_size = 7u - rest_len;
                        } else {
                            seg_write_buf->mbx_hdr.length = EC_SDO_SEG_HDR_LEN + rest_len;
                        }
                    }

                    (void)memcpy(&seg_write_buf->sdo_data, &tmp[tmp_pos], seg_len);
                    rest_len -= seg_len;
                    tmp_pos += seg_len;

                    // send request
                    ec_mbx_enqueue_head(pec, slave, p_entry);

                    // wait for answer
                    ec_coe_wait(pec, slave, &p_entry);
                    while (p_entry != NULL) {
                        ec_sdo_seg_download_resp_t *read_buf = (void *)(p_entry->data);

                        if (    (read_buf->coe_hdr.service == EC_COE_SDOREQ) &&
                                (read_buf->sdo_hdr.command == EC_COE_SDO_ABORT_REQ)) 
                        {
                            // cppcheck-suppress misra-c2012-11.3
                            ec_sdo_abort_request_t *abort_buf = (ec_sdo_abort_request_t *)(p_entry->data); 

                            ec_log(10, __func__, "slave %d: got sdo abort request on idx %#X, subidx %d, "
                                    "abortcode %#X\n", slave, index, sub_index, abort_buf->abort_code);

                            *abort_code = abort_buf->abort_code;
                            ret = EC_ERROR_MAILBOX_ABORT;
                        } else if (read_buf->coe_hdr.service == EC_COE_SDORES) {
                            // everthing is fine
                            ret = EC_OK;
                        } else {
                            ec_coe_print_msg(5, __func__, slave, "got unexpected mailbox message", 
                                    (uint8_t *)(p_entry->data), 6u + read_buf->mbx_hdr.length);
                            ret = EC_ERROR_MAILBOX_READ;
                        }

                        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
                        p_entry = NULL;

                        if ((ret == EC_OK) || (ret == EC_ERROR_MAILBOX_ABORT)) {
                            break;
                        }

                        ec_coe_wait(pec, slave, &p_entry);
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&slv->mbx.coe.lock);
    return ret;
}

// write coe sdo 
int ec_coe_sdo_write(ec_t *pec, uint16_t slave, uint16_t index, 
        uint8_t sub_index, int complete, uint8_t *buf, size_t len,
        uint32_t *abort_code) 
{
    int ret = 0;
    if ((len <= 4u) && (complete == 0)) {
        ret = ec_coe_sdo_write_expedited(pec, slave, index, sub_index, complete, buf, len, abort_code); 
    } else {
        ret = ec_coe_sdo_write_normal(pec, slave, index, sub_index, complete, buf, len, abort_code);
    }

    return ret;
}

typedef struct PACKED ec_sdoinfoheader {
    uint16_t opcode     : 7;
    uint16_t incomplete : 1;
    uint16_t reserved   : 8;
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
int ec_coe_odlist_read(ec_t *pec, uint16_t slave, uint8_t *buf, size_t *len) {
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);
    assert(slave < pec->slave_cnt);

    pool_entry_t *p_entry = NULL;
    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    ec_slave_ptr(slv, pec, slave);

    pthread_mutex_lock(&slv->mbx.coe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_COE) == 0) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock) != 0) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else { 
        assert(p_entry != NULL);

        // cppcheck-suppress misra-c2012-11.3
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

        size_t val = 0;
        int frag_left = 0;

        do {
            // wait for answer
            ec_coe_wait(pec, slave, &p_entry);
            while (p_entry != NULL) {
                ec_sdo_odlist_resp_t *read_buf = (void *)(p_entry->data); 

                if (val == 0u) {
                    // first fragment, allocate buffer if not passed
                    size_t od_len = (read_buf->mbx_hdr.length - 8u) +                             // first fragment
                        (read_buf->sdo_info_hdr.fragments_left * (read_buf->mbx_hdr.length - 6u)); // following fragments

                    if ((*len) < od_len) {
                        ret = EC_ERROR_MAILBOX_BUFFER_TOO_SMALL;
                    } else {
                        ret = EC_OK;
                    }

                    (*len) = od_len;
                }

                uint8_t *from = (val == 0u) ? &read_buf->sdo_info_data[2] : &read_buf->sdo_info_data[0];
                size_t act_len = (val == 0u) ? (read_buf->mbx_hdr.length - 8u) : (read_buf->mbx_hdr.length - 6u);

                if ((val + act_len) > (*len)) {
                    act_len = (*len) - val;
                }

                if ((ret == EC_OK) && (act_len != 0u)) {
                    (void)memcpy(&buf[val], from, act_len);
                    val += act_len;
                }

                frag_left = read_buf->sdo_info_hdr.fragments_left;

                ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
                p_entry = NULL;
            }
        } while (frag_left != 0);

        *len = val;
    }

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

typedef struct PACKED ec_sdo_info_error_resp {
    ec_mbx_header_t      mbx_hdr;
    ec_coe_header_t      coe_hdr;
    ec_sdoinfoheader_t  sdo_info_hdr;
    ec_data_t           sdo_info_data;
} PACKED ec_sdo_info_error_resp_t;

// read coe sdo description
int ec_coe_sdo_desc_read(ec_t *pec, uint16_t slave, uint16_t index,
        ec_coe_sdo_desc_t *desc, uint32_t *error_code) {
    assert(pec != NULL);
    assert(desc != NULL);
    assert(slave < pec->slave_cnt);

    pool_entry_t *p_entry = NULL;
    int ret = 0;
    ec_slave_ptr(slv, pec, slave);

    pthread_mutex_lock(&slv->mbx.coe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_COE) == 0) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock) != 0) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else { 
        assert(p_entry != NULL);

        // cppcheck-suppress misra-c2012-11.3
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
        ec_coe_wait(pec, slave, &p_entry);
        while (p_entry != NULL) {
            ec_sdo_desc_resp_t *read_buf = (void *)(p_entry->data); 
            if (read_buf->coe_hdr.service == EC_COE_SDOINFO) {
                if  (read_buf->sdo_info_hdr.opcode == EC_COE_SDO_INFO_GET_OBJECT_DESC_RESP) {
                    // transfer was successfull
                    (void)memcpy(&desc->data_type, (void *)&read_buf->sdo_info_data[2], sizeof(desc->data_type));
                    desc->max_subindices    = read_buf->sdo_info_data[4];
                    desc->obj_code          = read_buf->sdo_info_data[5];
                    desc->name_len          = min(CANOPEN_MAXNAME, read_buf->mbx_hdr.length - 6u - 6u);
                    (void)memcpy(desc->name, (void *)&read_buf->sdo_info_data[6], desc->name_len);

                    ret = EC_OK;
                } else if (read_buf->sdo_info_hdr.opcode == EC_COE_SDO_INFO_ERROR_REQUEST) {
                    uint32_t ecode = read_buf->sdo_info_data[0];

                    ec_log(5, __func__, "slave %2d: got sdo info error request on idx %#X, "
                            "error_code %X, message %s\n", slave, index, ecode, get_sdo_info_error_string(ecode));

                    if (error_code != NULL) {
                        *error_code = ecode;
                    }

                    ret = EC_ERROR_MAILBOX_ABORT;
                } else {}
            } else {
                // not our answer, print out this message
                ec_coe_print_msg(5, __func__, slave, "unexpected coe answer", 
                        (uint8_t *)read_buf, 6u + read_buf->mbx_hdr.length);
                (void)memset(desc, 0, sizeof(ec_coe_sdo_desc_t));
                ret = EC_ERROR_MAILBOX_READ;
            }

            ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
            p_entry = NULL;

            if ((ret == EC_OK) || (ret == EC_ERROR_MAILBOX_ABORT)) {
                break;
            }

            ec_coe_wait(pec, slave, &p_entry);
        }
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
        uint8_t sub_index, uint8_t value_info, ec_coe_sdo_entry_desc_t *desc, 
        uint32_t *error_code) 
{
    assert(pec != NULL);
    assert(desc != NULL);
    assert(slave < pec->slave_cnt);

    pool_entry_t *p_entry = NULL;
    int ret = EC_ERROR_MAILBOX_READ;
    ec_slave_ptr(slv, pec, slave);

    pthread_mutex_lock(&slv->mbx.coe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_COE) == 0) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, p_entry, NULL, &slv->mbx.coe.lock) != 0) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else { 
        assert(p_entry != NULL);

        // cppcheck-suppress misra-c2012-11.3
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
        ec_coe_wait(pec, slave, &p_entry);
        while (p_entry != NULL) {
            ec_sdo_entry_desc_resp_t *read_buf = (void *)(p_entry->data); 

            if (read_buf->coe_hdr.service == EC_COE_SDOINFO) {
                if (read_buf->sdo_info_hdr.opcode == EC_COE_SDO_INFO_GET_ENTRY_DESC_RESP) {
                    // transfer was successfull
                    desc->value_info    = read_buf->value_info;
                    desc->data_type     = read_buf->data_type;
                    desc->bit_length    = read_buf->bit_length;
                    desc->obj_access    = read_buf->obj_access;
                    desc->name_len      = min(CANOPEN_MAXNAME, read_buf->mbx_hdr.length - 6u - 10u);

                    (void)memcpy(desc->name, read_buf->desc_data, desc->name_len);
                    ret = EC_OK;
                } else if (read_buf->sdo_info_hdr.opcode == EC_COE_SDO_INFO_ERROR_REQUEST) {
                    ec_sdo_info_error_resp_t *read_buf_error = (void *)(p_entry->data);

                    uint32_t ecode = read_buf_error->sdo_info_data[0];

                    ec_log(5, __func__, "slave %2d: got sdo info error request on idx %#X, "
                            "error_code %X, message: %s\n", slave, index, ecode, get_sdo_info_error_string(ecode));

                    if (error_code != NULL) {
                        *error_code = ecode;
                    }

                    ret = EC_ERROR_MAILBOX_ABORT;
                } else {}
            } else {
                // not our answer, print out this message
                ec_coe_print_msg(5, __func__, slave, "unexpected coe answer", 
                        (uint8_t *)read_buf, 6u + read_buf->mbx_hdr.length);
                (void)memset(desc, 0, sizeof(ec_coe_sdo_entry_desc_t));
                ret = EC_ERROR_MAILBOX_READ;
            }

            ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
            p_entry = NULL;
            
            if ((ret == EC_OK) || (ret == EC_ERROR_MAILBOX_ABORT)) {
                break;
            }

            ec_coe_wait(pec, slave, &p_entry);
        }
    }

    pthread_mutex_unlock(&slv->mbx.coe.lock);

    return ret;
}

int ec_coe_generate_mapping(ec_t *pec, uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    uint8_t *buf = NULL;
    uint16_t start_adr; 
    ec_slave_ptr(slv, pec, slave);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_COE) == 0) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_COE;
    } else {
        if (slv->sm[0].adr > slv->sm[1].adr) {
            start_adr = slv->sm[0].adr + slv->sm[0].len;
        } else {
            start_adr = slv->sm[1].adr + slv->sm[1].len;
        }

        for (uint32_t sm_idx = 2u; sm_idx <= 3u; ++sm_idx) {
            uint32_t bit_len = 0u;
            uint32_t idx = 0x1c10u + sm_idx;
            uint8_t entry_cnt = 0u;
            uint8_t entry_cnt_2 = 0u;
            size_t entry_cnt_size = sizeof(entry_cnt);
            uint32_t abort_code = 0u;

            // read count of mapping entries, stored in subindex 0
            // mapped entreis are stored at 0x1c12 and 0x1c13 and should usually be
            // written in state preop with an init command
            buf = (uint8_t *)&entry_cnt;
            ret = ec_coe_sdo_read(pec, slave, idx, 0, 0, buf, &entry_cnt_size, &abort_code);
            if (ret != 0) {
                ec_log(5, "GENERATE_MAPPING COE", "slave %2d: sm%u reading "
                        "0x%04X/%d failed, error code 0x%X\n", slave, sm_idx, idx, 0, ret);
                continue;
            }

            ec_log(100, "GENERATE_MAPPING COE", "slave %2d: sm%u 0x%04X "
                    "count %d\n", slave, sm_idx, idx, entry_cnt); 

            // now read all mapped pdo's to retreave the mapped object lengths
            for (uint8_t i = 1u; i <= entry_cnt; ++i) {
                uint16_t entry_idx;
                size_t entry_size = sizeof(entry_idx);

                // read entry subindex with mapped value
                buf = (uint8_t *)&entry_idx;
                ret = ec_coe_sdo_read(pec, slave, idx, i, 0, buf, &entry_size, &abort_code);
                if (ret != 0) {
                    ec_log(5, "GENERATE_MAPPING COE", "            "
                            "pdo: reading 0x%04X/%d failed, error code 0x%X\n", 
                            idx, i, ret);
                    continue;
                }

                // read entry subindex with mapped value
                if (entry_idx == 0u) {
                    ec_log(100, "GENERATE_MAPPING COE", "            "
                            "pdo: entry_idx is 0\n");
                    continue;
                }

                entry_cnt_size = sizeof(entry_cnt_2);

                // read count of entries of mapped value
                buf = (uint8_t *)&entry_cnt_2;
                ret = ec_coe_sdo_read(pec, slave, entry_idx, 0, 0, buf, &entry_cnt_size, &abort_code);
                if (ret != 0) {
                    ec_log(5, "GENERATE_MAPPING COE", "             "
                            "pdo: reading 0x%04X/%d failed, error code 0x%X\n", 
                            entry_idx, 0, ret);
                    continue;
                }

                ec_log(100, "GENERATE_MAPPING COE", "             "
                        "pdo: 0x%04X count %d\n", entry_idx, entry_cnt_2); 

                for (uint8_t j = 1u; j <= entry_cnt_2; ++j) {
                    uint32_t entry;
                    size_t entry_size = sizeof(entry);
                    buf = (uint8_t *)&entry;
                    ret = ec_coe_sdo_read(pec, slave, entry_idx, j, 0, buf, &entry_size, &abort_code);
                    if (ret != 0) {
                        ec_log(5, "GENERATE_MAPPING COE", "                "
                                "reading 0x%04X/%d failed, error code 0x%X\n", 
                                entry_idx, j, ret);
                        continue;
                    }

                    bit_len += entry & 0x000000FFu;

                    ec_log(100, "GENERATE_MAPPING COE", "                "
                            "mapped entry 0x%04X / %d -> %d bits\n",
                            (entry & 0xFFFF0000u) >> 16u, 
                            (entry & 0x0000FF00u) >> 8u, 
                            (entry & 0x000000FFu));
                }                        
            }

            // store sync manager settings if we have at least 1 bit mapped
            if (bit_len != 0u) {
                ec_log(100, "GENERATE_MAPPING COE", 
                        "slave %2d: sm%u length bits %d, bytes %d\n", 
                        slave, sm_idx, bit_len, (bit_len + 7u) / 8u);

                if (slv->sm && (slv->sm_ch > sm_idx)) {
                    slv->sm[sm_idx].len = (bit_len + 7u) / 8u;

                    // only set a new address if not previously set by
                    // user or eeprom. some slave require the sm address to be
                    // exactly the address stored in eeprom.
                    if (!slv->sm[sm_idx].adr) {
                        slv->sm[sm_idx].adr = start_adr;
                        start_adr += slv->sm[sm_idx].len * 3u;
                    }

                    slv->sm[sm_idx].flags = (sm_idx == 2u) ? 0x10064u : 0x10020u;
                }
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
    assert(pec != NULL);
    assert(p_entry != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    // get length and queue this message    
    // cppcheck-suppress misra-c2012-11.3
    ec_mbx_header_t *hdr = (ec_mbx_header_t *)(p_entry->data);

    // don't copy any headers, we already know that we have a coe emergency
    size_t msg_len = hdr->length - 2u;

    ec_coe_emergency_message_entry_t *qmsg = (ec_coe_emergency_message_entry_t *)
        malloc(sizeof(ec_coe_emergency_message_entry_t) + msg_len);

    // skip mbx header and coe header
    (void)memcpy(qmsg->msg, (uint8_t *)&(p_entry->data[6 + 2]), msg_len);

    qmsg->msg_len = msg_len;
    check_ret(ec_timer_gettime(&qmsg->timestamp));
    TAILQ_INSERT_TAIL(&slv->mbx.coe.emergencies, qmsg, qh);

    ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
}

