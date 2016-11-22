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

#ifndef __LIBETHERCAT_EC_H__
#define __LIBETHERCAT_EC_H__

#include <pthread.h>
#include <stdint.h>

#include "libethercat/common.h"
#include "libethercat/dc.h"
#include "libethercat/slave.h"
#include "libethercat/hw.h"
#include "libethercat/regs.h"
#include "libethercat/idx.h"
#include "libethercat/datagram.h"
#include "libethercat/datagram_pool.h"
#include "libethercat/message_pool.h"
#include "libethercat/eeprom.h"

struct ec;
    
typedef struct PACKED ec_pd_group {
    uint32_t log;
    uint32_t log_len;
    
    uint8_t *pd;
    size_t   pdout_len;
    size_t   pdin_len;

    uint16_t wkc_expected;
    
    datagram_entry_t *p_de;
    idx_entry_t *p_idx;
} PACKED ec_pd_group_t;


typedef struct ec {
    hw_t *phw;
    int tx_sync;
    datagram_pool_t *pool;

    idx_queue_t idx_q;

    int slave_cnt;
    ec_slave_t *slaves;

    int pd_group_cnt;
    ec_pd_group_t *pd_groups;

    ec_dc_info_t dc;
    ec_async_message_loop_t *async_loop;
    
    int eeprom_log;
    ec_state_t master_state;
} ec_t;

#ifdef __cplusplus
extern "C" {
#endif

extern void *ec_log_func_user;
extern void (*ec_log_func)(int lvl, void *user, const char *format, ...);

void ec_log(int lvl, const char *pre, const char *format, ...);

//! open ethercat master
/*!
 * \param ppec return value for ethercat master pointer
 * \param ifname ethercat master interface name
 * \param prio receive thread priority
 * \param cpumask receive thread cpumask
 * \param eeprom_log log eeprom to stdout
 * \return 0 on succes, otherwise error code
 */
int ec_open(ec_t **ppec, const char *ifname, int prio, int cpumask, int eeprom_log);

//! closes ethercat master
/*!
 * \param pec pointer to ethercat master
 * \return 0 on success 
 */
int ec_close(ec_t *pec);

//! create process data groups
/*!
 * \param pec ethercat master pointer
 * \param pd_group_cnt number of groups to create
 * \return 0 on success
 */
int ec_create_pd_groups(ec_t *pec, int pd_group_cnt);

//! destroy process data groups
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_destroy_pd_groups(ec_t *pec);

//! syncronous ethercat read/write
/*!
 * \param pec pointer to ethercat master
 * \param cmd ethercat command
 * \param adr 32-bit address of slave
 * \param data data buffer to read/write 
 * \param datalen length of data
 * \param wkc return value for working counter
 * \return 0 on succes, otherwise error code
 */
int ec_transceive(ec_t *pec, uint8_t cmd, uint32_t adr, 
        uint8_t *data, size_t datalen, uint16_t *wkc);

//! asyncronous ethercat read/write, answer don't care
/*!
 * \param pec pointer to ethercat master
 * \param cmd ethercat command
 * \param adr 32-bit address of slave
 * \param data data buffer to read/write 
 * \param datalen length of data
 * \return 0 on succes, otherwise error code
 */
int ec_transmit_no_reply(ec_t *pec, uint8_t cmd, uint32_t adr, 
        uint8_t *data, size_t datalen);

//! set state on ethercat bus
/*! 
 * \param pec ethercat master pointer
 * \param state new ethercat state
 * \return 0 on success
 */
int ec_set_state(ec_t *pec, ec_state_t state);

//! send process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \return 0 on success
 */
int ec_send_process_data_group(ec_t *pec, int group);

//! receive process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \param timeout for waiting for packet
 * \return 0 on success
 */
int ec_receive_process_data_group(ec_t *pec, int group, ec_timer_t *timeout);

//! send distributed clocks sync datagram
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_send_distributed_clocks_sync(ec_t *pec);

//! receive distributed clocks sync datagram
/*!
 * \param pec ethercat master pointer
 * \param timeout absolute timeout
 * \return 0 on success
 */
int ec_receive_distributed_clocks_sync(ec_t *pec, ec_timer_t *timeout);

#ifdef __cplusplus
};
#endif

#define ec_to_adr(ado, adp) \
    ((uint32_t)(adp) << 16) | ((ado) & 0xFFFF)

#define ec_brd(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BRD, ((uint32_t)(ado) << 16), (uint8_t *)(data), (datalen), (wkc))
#define ec_bwr(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BWR, ((uint32_t)(ado) << 16), (uint8_t *)(data), (datalen), (wkc))
#define ec_brw(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BRW, ((uint32_t)(ado) << 16), (uint8_t *)(data), (datalen), (wkc))

#define ec_aprd(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APRD, ((uint32_t)(ado) << 16) | ((adp) & 0xFFFF), \
            (uint8_t *)(data), (datalen), (wkc))
#define ec_apwr(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APWR, ((uint32_t)(ado) << 16) | ((adp) & 0xFFFF), \
            (uint8_t *)(data), (datalen), (wkc))
#define ec_aprw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APRW, ((uint32_t)(ado) << 16) | ((adp) & 0xFFFF), \
            (uint8_t *)(data), (datalen), (wkc))

#define ec_fprd(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPRD, ((uint32_t)(ado) << 16) | ((adp) & 0xFFFF), \
            (uint8_t *)(data), (datalen), (wkc))
#define ec_fpwr(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPWR, ((uint32_t)(ado) << 16) | ((adp) & 0xFFFF), \
            (uint8_t *)(data), (datalen), (wkc))
#define ec_fprw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPRW, ((uint32_t)(ado) << 16) | ((adp) & 0xFFFF), \
            (uint8_t *)(data), (datalen), (wkc))

#define ec_frmw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FRMW, ((uint32_t)(ado) << 16) | ((adp) & 0xFFFF), \
            (uint8_t *)(data), (datalen), (wkc))

#endif // __LIBETHERCAT_EC_H__

