/*
 * \file mbx.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Nov 2016
 *
 * \brief ethercat mailbox common access functions
 *
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

#include "libethercat/mbx.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/eventfd.h>  

#ifndef max
#define max(a, b)  ((a) > (b) ? (a) : (b))
#endif

#define MBX_HANDLER_FLAGS_SEND  ((osal_uint32_t)0x00000001u)
#define MBX_HANDLER_FLAGS_RECV  ((osal_uint32_t)0x00000002u)

// forward declarations
static int ec_mbx_send(ec_t *pec, osal_uint16_t slave, osal_uint8_t *buf, osal_size_t buf_len, osal_uint32_t nsec);
static int ec_mbx_receive(ec_t *pec, osal_uint16_t slave, osal_uint8_t *buf, osal_size_t buf_len, osal_uint32_t nsec);
static void ec_mbx_handler(ec_t *pec, osal_uint16_t slave);
static int ec_mbx_is_empty(ec_t *pec, osal_uint16_t slave, osal_uint8_t mbx_nr, osal_uint32_t nsec);
static int ec_mbx_is_full(ec_t *pec, osal_uint16_t slave, osal_uint8_t mbx_nr, osal_uint32_t nsec);

//! \brief Mailbox handler thread wrapper
static void *ec_mbx_handler_thread(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    ec_mbx_t *pmbx = (ec_mbx_t *)arg;
    ec_mbx_handler(pmbx->pec, pmbx->slave);
    return NULL;
}

//! \brief Initialize mailbox structure.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_mbx_init(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    if (slv->mbx.handler_running == 0) {
        ec_log(10, __func__, "slave %2d: initializing mailbox\n", slave);

        (void)pool_open(&slv->mbx.message_pool_recv_free, 1024, slv->sm[MAILBOX_READ].len);
        (void)pool_open(&slv->mbx.message_pool_send_free, 1024, slv->sm[MAILBOX_WRITE].len);
        (void)pool_open(&slv->mbx.message_pool_send_queued, 0, slv->sm[MAILBOX_WRITE].len);

        osal_mutex_init(&slv->mbx.sync_mutex, NULL);
        osal_binary_semaphore_init(&slv->mbx.sync_sem, NULL);
        osal_mutex_init(&slv->mbx.lock, NULL);

        if (0u != (slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE)) {
            ec_coe_init(pec, slave);
        }
        if (0u != (slv->eeprom.mbx_supported & EC_EEPROM_MBX_SOE)) {
            ec_soe_init(pec, slave);
        }
        if (0u != (slv->eeprom.mbx_supported & EC_EEPROM_MBX_FOE)) {
            ec_foe_init(pec, slave);
        }
        if (0u != (slv->eeprom.mbx_supported & EC_EEPROM_MBX_EOE)) {
            ec_eoe_init(pec, slave);
        }

        // start mailbox handler thread
        slv->mbx.handler_running = 1;
        slv->mbx.pec = pec;
        slv->mbx.slave = slave;

        osal_task_attr_t attr;
        attr.priority = 0;
        attr.affinity = 0xFF;
        snprintf(&attr.task_name[0], TASK_NAME_LEN, "ecat.mbx%d", slave);
        osal_task_create(&slv->mbx.handler_tid, &attr, ec_mbx_handler_thread, &slv->mbx);
    }
}

//! \brief Deinit mailbox structure
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_mbx_deinit(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    if (slv->mbx.handler_running != 0) {
        ec_log(10, __func__, "slave %2d: deinitilizing mailbox\n", slave);

        slv->mbx.handler_running = 0;
        osal_task_join(&slv->mbx.handler_tid, NULL);

        if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_COE) == EC_OK) {
            ec_coe_deinit(pec, slave);
        }
        if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_SOE) == EC_OK) {
            ec_soe_deinit(pec, slave);
        }
        if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_FOE) == EC_OK) {
            ec_foe_deinit(pec, slave);
        }
        if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_EOE) == EC_OK) {
            ec_eoe_deinit(pec, slave);
        }

        osal_mutex_destroy(&slv->mbx.lock);
        osal_binary_semaphore_destroy(&slv->mbx.sync_sem);
        osal_mutex_destroy(&slv->mbx.sync_mutex);

        (void)pool_close(slv->mbx.message_pool_recv_free);
        (void)pool_close(slv->mbx.message_pool_send_free);
        (void)pool_close(slv->mbx.message_pool_send_queued);
    }
}

//! \brief Return string repr of mailbox protocol.
/*!
 * \param[in] mbx_flag  Mailbox protocol flag to be checked
 *
 * \return Mailbox protocol string repr.
 */
static const osal_char_t *ec_mbx_get_protocol_string(osal_uint16_t mbx_flag) {
    static const osal_char_t *MBX_PROTOCOL_STRING_COE = "CoE";
    static const osal_char_t *MBX_PROTOCOL_STRING_SOE = "SoE";
    static const osal_char_t *MBX_PROTOCOL_STRING_FOE = "FoE";
    static const osal_char_t *MBX_PROTOCOL_STRING_EOE = "EoE";
    static const osal_char_t *MBX_PROTOCOL_STRING_UNKNOWN = "Unknown";

    const osal_char_t *ret;

    if (mbx_flag == EC_EEPROM_MBX_COE) {
        ret = MBX_PROTOCOL_STRING_COE;
    } else if (mbx_flag == EC_EEPROM_MBX_SOE) {
        ret = MBX_PROTOCOL_STRING_SOE;
    } else if (mbx_flag == EC_EEPROM_MBX_EOE) {
        ret = MBX_PROTOCOL_STRING_EOE;
    } else if (mbx_flag == EC_EEPROM_MBX_FOE) {
        ret = MBX_PROTOCOL_STRING_FOE;
    } else {
        ret = MBX_PROTOCOL_STRING_UNKNOWN;
    } 

    return ret;
}

//! \brief Checks if mailbox protocol is supported by slave
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] mbx_flag  Mailbox protocol flag to be checked
 *
 * \return 1 if supported, 0 otherwise
 */
int ec_mbx_check(ec_t *pec, int slave, osal_uint16_t mbx_flag) {
    int ret = EC_OK;

    if (!(pec->slaves[slave].eeprom.mbx_supported & (mbx_flag))) {
        ec_log(100, __func__, "no %s support on slave %d\n", 
                ec_mbx_get_protocol_string(mbx_flag), slave); 
        ret = EC_ERROR_MAILBOX_MASK | (int32_t)mbx_flag;
    }

    return ret;
}

//! \brief Check if mailbox is full.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] mbx_nr    Number of mailbox to be checked.
 * \param[in] nsec      Timeout in nanoseconds.
 * \return full (EC_OK) or empty (EC_ERROR_MAILBOX_TIMEOUT)
 */
int ec_mbx_is_full(ec_t *pec, osal_uint16_t slave, osal_uint8_t mbx_nr, osal_uint32_t nsec) {
    osal_uint16_t wkc = 0;
    osal_uint8_t sm_state = 0;
    int ret = EC_ERROR_MAILBOX_TIMEOUT;

    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    osal_uint64_t timeout = nsec;
    osal_timer_t timer;
    osal_timer_init(&timer, timeout);

    do {
        (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 
                (osal_uint16_t)(EC_REG_SM0STAT + ((osal_uint16_t)mbx_nr << 3u)), 
                &sm_state, sizeof(sm_state), &wkc);

        if (wkc && ((sm_state & 0x08u) == 0x08u)) {
            ret = EC_OK;
            break;
        }

        osal_sleep(EC_DEFAULT_DELAY);
    } while (osal_timer_expired(&timer) != OSAL_ERR_TIMEOUT);

    return ret;
}

//! \brief Check if mailbox is empty.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] mbx_nr    Number of mailbox to be checked.
 * \param[in] nsec      Timeout in nanoseconds.
 * \return full (EC_ERROR_MAILBOX_TIMEOUT) or empty (EC_OK)
 */
int ec_mbx_is_empty(ec_t *pec, osal_uint16_t slave, osal_uint8_t mbx_nr, osal_uint32_t nsec) {
    osal_uint16_t wkc = 0;
    osal_uint8_t sm_state = 0;
    int ret = EC_ERROR_MAILBOX_TIMEOUT;

    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    osal_uint64_t timeout = nsec;
    osal_timer_t timer;
    osal_timer_init(&timer, timeout);

    do {
        (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 
                (osal_uint16_t)(EC_REG_SM0STAT + ((osal_uint16_t)mbx_nr << 3u)), 
                &sm_state, sizeof(sm_state), &wkc);

        if (wkc && ((sm_state & 0x08u) == 0x00u)) {
            ret = EC_OK;
            break;
        }

        osal_sleep(EC_DEFAULT_DELAY);
    } while (osal_timer_expired(&timer) != OSAL_ERR_TIMEOUT);

    return ret;
}

//! \brief write mailbox to slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] nsec      Timeout in nanoseconds.
 * \return working counter
 */
int ec_mbx_send(ec_t *pec, osal_uint16_t slave, osal_uint8_t *buf, osal_size_t buf_len, osal_uint32_t nsec) {
    osal_uint16_t wkc = 0;
    int ret = EC_OK;
    ec_slave_ptr(slv, pec, slave);

    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(buf != NULL);

    osal_uint64_t timeout = nsec;
    osal_timer_t timer;
    osal_timer_init(&timer, timeout);

    // wait for send mailbox available 
    ret = ec_mbx_is_empty(pec, slave, MAILBOX_WRITE, nsec);
    if (ret != EC_OK) {
        ec_log(1, __func__, "slave %d: waiting for empty send mailbox failed!\n", slave);
    } else {
        ret = EC_ERROR_MAILBOX_TIMEOUT;

        // send request
        do {
            ec_log(100, __func__, "slave %d: writing mailbox\n", slave);
            (void)ec_fpwr(pec, slv->fixed_address, slv->sm[MAILBOX_WRITE].adr, buf, buf_len, &wkc);

            if (wkc != 0u) {
                ret = EC_OK;
                break;
            }

            osal_sleep(EC_DEFAULT_DELAY);
        } while (osal_timer_expired(&timer) != OSAL_ERR_TIMEOUT);

        if (ret != EC_OK) {
            ec_log(1, __func__, "slave %d: did not respond on writing to write mailbox\n", slave);
        }
    }

    return ret;
}

//! \brief Read mailbox from slave.
/*!
 *
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] nsec      Timeout in nanoseconds.
 * \return working counter
 */
int ec_mbx_receive(ec_t *pec, osal_uint16_t slave, osal_uint8_t *buf, osal_size_t buf_len, osal_uint32_t nsec) {
    osal_uint16_t wkc = 0;
    int ret = EC_ERROR_MAILBOX_READ;
    ec_slave_ptr(slv, pec, slave);

    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(buf != NULL);
    
    osal_timer_t timer;
    osal_timer_init(&timer, EC_DEFAULT_TIMEOUT_MBX);

    // wait for receive mailbox available 
    if (ec_mbx_is_full(pec, slave, MAILBOX_READ, nsec) == EC_OK) {
        // receive answer
        do {
            (void)ec_fprd(pec, slv->fixed_address, slv->sm[MAILBOX_READ].adr,
                        buf, buf_len, &wkc);

            if (wkc != 0u) {
                // reset mailbox state 
                if (slv->mbx.sm_state != NULL) {
                    *slv->mbx.sm_state = 0u;
                }

                ret = EC_OK;
            } else {
                // lost receive mailbox ?
                ec_log(10, __func__, "slave %d: lost receive mailbox ?\n", slave);

                osal_uint16_t sm_status = 0u;
                osal_uint8_t sm_control = 0u;

                (void)ec_fprd(pec, slv->fixed_address, 
                            (osal_uint16_t)(EC_REG_SM0STAT + (MAILBOX_READ << 3u)),
                            &sm_status, sizeof(sm_status), &wkc);

                sm_status ^= 0x0200; // toggle repeat request

                (void)ec_fpwr(pec, slv->fixed_address, 
                            (osal_uint16_t)(EC_REG_SM0STAT + (MAILBOX_READ << 3u)),
                            &sm_status, sizeof(sm_status), &wkc);

                do { 
                    // wait for toggle ack
                    (void)ec_fprd(pec, slv->fixed_address, 
                                (osal_uint16_t)(EC_REG_SM0CONTR + (MAILBOX_READ << 3u)),
                                &sm_control, sizeof(sm_control), &wkc);

                    if (wkc && ((sm_control & 0x02u) == 
                                ((sm_status & 0x0200u) >> 8u))) {
                        break;
                    }
                } while ((osal_timer_expired(&timer) != OSAL_ERR_TIMEOUT) && !wkc);

                if (osal_timer_expired(&timer) == OSAL_ERR_TIMEOUT) {
                    ec_log(1, __func__, "slave %d timeout waiting for toggle ack\n", slave);
                    ret = EC_ERROR_MAILBOX_TIMEOUT;
                } else if (ec_mbx_is_full(pec, slave, MAILBOX_READ, nsec) != EC_OK) { // wait for receive mailbox available 
                    ec_log(1, __func__, "slave %d waiting for full receive "
                            "mailbox failed!\n", slave);
                    ret = EC_ERROR_MAILBOX_READ_EMPTY;
                } else {}
            }

            if (    (ret == EC_OK) ||
                    (ret == EC_ERROR_MAILBOX_READ_EMPTY) ||
                    (ret == EC_ERROR_MAILBOX_TIMEOUT)) {
                break;
            }

            osal_sleep(EC_DEFAULT_DELAY);
        } while (osal_timer_expired(&timer) != OSAL_ERR_TIMEOUT);

        if (osal_timer_expired(&timer) == OSAL_ERR_TIMEOUT) {
            ec_log(1, __func__, "slave %d did not respond "
                    "on reading from receive mailbox\n", slave);
        }
    } else {
        ret = EC_ERROR_MAILBOX_READ_EMPTY;
    }

    return ret;
}

//! \brief Enqueue mailbox message to send queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue_head(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(p_entry != NULL);

    ec_slave_ptr(slv, pec, slave);

    pool_put_head(slv->mbx.message_pool_send_queued, p_entry);
    
    osal_mutex_lock(&pec->slaves[slave].mbx.sync_mutex);
    slv->mbx.handler_flags |= MBX_HANDLER_FLAGS_SEND;
    
    osal_binary_semaphore_post(&slv->mbx.sync_sem);
    osal_mutex_unlock(&pec->slaves[slave].mbx.sync_mutex);
}

//! \brief Enqueue mailbox message to send queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue_tail(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(p_entry != NULL);

    ec_slave_ptr(slv, pec, slave);

    pool_put(slv->mbx.message_pool_send_queued, p_entry);
    
    osal_mutex_lock(&pec->slaves[slave].mbx.sync_mutex);
    slv->mbx.handler_flags |= MBX_HANDLER_FLAGS_SEND;
    
    osal_binary_semaphore_post(&slv->mbx.sync_sem);
    osal_mutex_unlock(&pec->slaves[slave].mbx.sync_mutex);
}

//! \brief Trigger read of mailbox.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
void ec_mbx_sched_read(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    
    osal_mutex_lock(&pec->slaves[slave].mbx.sync_mutex);
    slv->mbx.handler_flags |= MBX_HANDLER_FLAGS_RECV;

    osal_binary_semaphore_post(&slv->mbx.sync_sem);
    osal_mutex_unlock(&pec->slaves[slave].mbx.sync_mutex);
}

//! \brief Handle slaves mailbox.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
static void ec_mbx_do_handle(ec_t *pec, uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    pool_entry_t *p_entry = NULL;
    int ret;
    
    osal_mutex_lock(&pec->slaves[slave].mbx.sync_mutex);
    uint32_t flags = slv->mbx.handler_flags;
    slv->mbx.handler_flags = 0;
    osal_mutex_unlock(&pec->slaves[slave].mbx.sync_mutex);

    // check event
    if ((flags & MBX_HANDLER_FLAGS_RECV) != 0u) {
        flags &= ~MBX_HANDLER_FLAGS_RECV;

        ec_log(100, __func__, "slave %2d: mailbox needs to be read\n", slave);

        do {
            (void)pool_get(slv->mbx.message_pool_recv_free, &p_entry, NULL);
            if (!p_entry) {
                ec_log(1, __func__, "slave %2d: out of mailbox buffers\n", slave);
                break;
            }
            (void)memset(p_entry->data, 0, p_entry->data_size);

            if (ec_mbx_receive(pec, slave, p_entry->data, 
                        min(p_entry->data_size, (osal_size_t)slv->sm[MAILBOX_READ].len), 0) == EC_OK) {
                // cppcheck-suppress misra-c2012-11.3
                ec_mbx_header_t *hdr = (ec_mbx_header_t *)(p_entry->data);
                ec_log(100, __func__, "slave %2d: got one mailbox message: %0X\n", slave, hdr->mbxtype);

                switch (hdr->mbxtype) {
                    case EC_MBX_COE:
                        if (0u != (slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE)) {
                            ec_coe_enqueue(pec, slave, p_entry);
                            p_entry = NULL;
                        } else {
                            ec_log(1, __func__, "slave %2d: got CoE frame, but slave has no support!\n", slave);
                        }
                        break;
                    case EC_MBX_SOE:
                        if (0u != (slv->eeprom.mbx_supported & EC_EEPROM_MBX_SOE)) {
                            ec_soe_enqueue(pec, slave, p_entry);
                            p_entry = NULL;
                        } else {
                            ec_log(1, __func__, "slave %2d: got SoE frame, but slave has no support!\n", slave);
                        }
                        break;
                    case EC_MBX_FOE:
                        if (0u != (slv->eeprom.mbx_supported & EC_EEPROM_MBX_FOE)) {
                            ec_foe_enqueue(pec, slave, p_entry);
                            p_entry = NULL;
                        } else {
                            ec_log(1, __func__, "slave %2d: got FoE frame, but slave has no support!\n", slave);
                        }
                        break;
                    case EC_MBX_EOE:
                        if (0u != (slv->eeprom.mbx_supported & EC_EEPROM_MBX_EOE)) {
                            ec_eoe_enqueue(pec, slave, p_entry);
                            p_entry = NULL;
                        } else {
                            ec_log(1, __func__, "slave %2d: got EoE frame, but slave has no support!\n", slave);
                        }
                        break;
                    default:
                        break;
                }
            }

            if (NULL != p_entry) {
                // returning to free pool
                ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
            }

        } while (ec_mbx_is_full(pec, slave, MAILBOX_READ, 0) == EC_OK);
    }

    if ((flags & MBX_HANDLER_FLAGS_SEND) != 0u) {
        flags &= ~MBX_HANDLER_FLAGS_SEND;

        // need to send a message to write mailbox
        ec_log(100, __func__, "slave %2d: mailbox needs to be written\n", slave);
        (void)pool_get(slv->mbx.message_pool_send_queued, &p_entry, NULL);

        if (p_entry != NULL) {
            ec_log(100, __func__, "slave %2d: got mailbox buffer to write\n", slave);
            int retry_cnt = 10;

            do {
                ret = ec_mbx_send(pec, slave, p_entry->data, 
                        min(p_entry->data_size, slv->sm[MAILBOX_WRITE].len), EC_DEFAULT_TIMEOUT_MBX);
                --retry_cnt;
            } while ((ret != EC_OK) && (retry_cnt > 0));

            if (ret != EC_OK) {
                ec_log(1, __func__, "slave %2d: error on writing send mailbox -> requeue\n", slave);
                ec_mbx_enqueue_head(pec, slave, p_entry);
            } else {                    
                // all done
                if (p_entry->user_cb != NULL) {
                    (*p_entry->user_cb)(p_entry->user_arg, p_entry);

                    p_entry->user_cb = NULL;
                    p_entry->user_arg = NULL;
                }

                ec_mbx_return_free_send_buffer(pec, slave, p_entry);
            }
        }
    } 
}

//! \brief Mailbox handler for one slave
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
void ec_mbx_handler(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    ec_log(10, __func__, "slave %2d: started mailbox handler\n", slave);

    while (slv->mbx.handler_running != 0) {
        int ret;
        osal_timer_t to;
        osal_timer_init(&to, 10000000);

        // wait for mailbox event
        ret = osal_binary_semaphore_timedwait(&slv->mbx.sync_sem, &to);

        if (ret != OSAL_OK) {
            if (ret == OSAL_ERR_TIMEOUT) {
                osal_mutex_lock(&pec->slaves[slave].mbx.sync_mutex);
                slv->mbx.handler_flags |= MBX_HANDLER_FLAGS_SEND;

                // check receive mailbox on timeout if PREOP or lower
                if (slv->act_state != EC_STATE_OP) {
                    slv->mbx.handler_flags |= MBX_HANDLER_FLAGS_RECV;
                }
                osal_mutex_unlock(&pec->slaves[slave].mbx.sync_mutex);
            } else {
                continue;
            }
        }
       
        ec_mbx_do_handle(pec, slave);
    }

    ec_log(10, __func__, "slave %2d: stopped mailbox handler\n", slave);
}

//! \brief Get free mailbox send buffer from slaves send message pool.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[out] pp_entry Pointer to pool entry pointer where buffer 
 *                      is returned.
 * \param[in] timeout   Pointer to timeout or NULL.
 *
 * \return EC_OK on success, otherwise EC_ERROR_MAILBOX_* code.
 */
int ec_mbx_get_free_send_buffer(ec_t *pec, osal_uint16_t slave, pool_entry_t **pp_entry, osal_timer_t *timeout) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(pp_entry != NULL);

    int ret = pool_get(pec->slaves[slave].mbx.message_pool_send_free, pp_entry, timeout);
    if (ret == EC_OK) {
        (*pp_entry)->user_cb = NULL;
        (*pp_entry)->user_arg = NULL;
        (void)memset((*pp_entry)->data, 0, (*pp_entry)->data_size);
    }

    return ret;
}

