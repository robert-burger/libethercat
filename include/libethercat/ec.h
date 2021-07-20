/**
 * \file ec.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 22 Nov 2016
 *
 * \brief ethercat master functions.
 *
 * These are EtherCAT master specific configuration functions.
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
#include <sys/select.h>

#include "libethercat/common.h"
#include "libethercat/dc.h"
#include "libethercat/slave.h"
#include "libethercat/hw.h"
#include "libethercat/regs.h"
#include "libethercat/idx.h"
#include "libethercat/datagram.h"
#include "libethercat/pool.h"
#include "libethercat/message_pool.h"
#include "libethercat/eeprom.h"

struct ec;
struct ec_slave;
typedef struct ec_slave ec_slave_t;
    
//! process data group structure
typedef struct ec_pd_group {
    uint32_t log;                   //!< locical address
                                    /*!<
                                     * This defines the logical start address
                                     * for the process data group. It is used
                                     * for EtherCAT logical addressing commands
                                     * LRW, LRD, LWR, ...
                                     */

    uint32_t log_len;               //!< byte length at logical address
                                    /*!<
                                     * This defines the byte length at logical 
                                     * start address for the process data 
                                     * group. It is used for EtherCAT logical 
                                     * addressing commands LRW, LRD, LWR, ...
                                     */
    
    uint8_t *pd;                    //!< process data pointer
                                    /*!< 
                                     * This address holds the process data
                                     * of the whole group. At offset 0 the 
                                     * outputs should be set, at offset \link
                                     * pdout_len \endlink, the inputs are 
                                     * filled in by the LRW command.
                                     */

    size_t   pdout_len;             //!< length of process data outputs
    size_t   pdin_len;              //!< length of process data inputs
    size_t   pd_lrw_len;            //!< inputs and outputs length if lrw is used

    int use_lrw;                    //!< LRW flag
                                    /*!
                                     * This flag defines if the EtherCAT master
                                     * should use the LRW command for process
                                     * data exchange
                                     */

    uint16_t wkc_expected;          //!< expected working counter
                                    /*!< 
                                     * This is the expected working counter 
                                     * for the LRW command. The working counter
                                     * will be incremented by every slave that
                                     * reads data by 1, by every slave that 
                                     * writes data by 2 and by every slave that
                                     * reads and writes data by 3.
                                     */
    
    pool_entry_t *p_entry;          //!< EtherCAT datagram from pool
    idx_entry_t *p_idx;             //!< EtherCAT datagram index from pool
} ec_pd_group_t;

//! ethercat master structure
typedef struct ec {
    hw_t *phw;                      //!< pointer to hardware interface
    int tx_sync;                    //!< synchronous call to send frames
                                    /*!<
                                     * This defines if the actual call to 
                                     * hardware interface to send a frame (
                                     * \link hw_tx() \endlink should be done 
                                     * synchronously by the ec_transceive 
                                     * function.
                                     * If not set, the hw_tx() has to be 
                                     * called by the user (e.g. cyclical timer
                                     * loop).
                                     */

    pool_t *pool;                   //!< datagram pool
                                    /*!<
                                     * All EtherCAT datagrams will be pre-
                                     * allocated and available in the datagram
                                     * pool. Theres no need to allocate 
                                     * datagrams at runtime.
                                     */

    idx_queue_t idx_q;              //!< index queue
                                    /*! 
                                     * The index queue holds all available 
                                     * EtherCAT datagram indices. For every
                                     * datagram one index will be taken out of
                                     * the queue and returned to the queue if
                                     * the frame with the datagram is received 
                                     * again by the master.
                                     */

    int slave_cnt;                  //!< count of found EtherCAT slaves
    ec_slave_t *slaves;             //!< array with EtherCAT slaves

    int pd_group_cnt;               //!< count of process data groups
    ec_pd_group_t *pd_groups;       //!< array with process data groups

    ec_dc_info_t dc;                //!< distributed clocks master settings
    ec_async_message_loop_t *async_loop;
                                    //!< asynchronous message loop
                                    /*!<
                                     * This loop receives asynchronous messages
                                     * from the EtherCAT slave mailboxes. This 
                                     * may be e.g. emergency messages...
                                     */
    
    int tun_fd;                     //!< tun device file descriptor
    uint32_t tun_ip;                //!< tun device ip addres
    pthread_t tun_tid;              //!< tun device handler thread id.
    int tun_running;                //!< tun device handler run flag.
    
    int eeprom_log;                 //!< flag whether to log eeprom to stdout
    ec_state_t master_state;        //!< expected EtherCAT master state
    int state_transition_pending;   //!< state transition is currently pending

    int threaded_startup;           //!< running state machine in threads for slave
    
    pool_entry_t *p_de_state;   //!< EtherCAT datagram from pool for ec_state read
    idx_entry_t *p_idx_state;       //!< EtherCAT datagram index from pool for ec_state read

    fd_set mbx_fds;
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
int ec_open(ec_t **ppec, const char *ifname, int prio, int cpumask, 
        int eeprom_log);

//! closes ethercat master
/*!
 * \param pec pointer to ethercat master
 * \return 0 on success 
 */
int ec_close(ec_t *pec);

//! configures tun device of EtherCAT master, used for EoE slaves.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] ip_address    IP address to be set for tun device.
 */
void ec_configure_tun(ec_t *pec, uint8_t ip_address[4]);

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

//! send broadcast read to ec state
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_send_brd_ec_state(ec_t *pec);

//! receive broadcast read to ec_state
/*!
 * \param pec ethercat master pointer
 * \param timeout for waiting for packet
 * \return 0 on success
 */
int ec_receive_brd_ec_state(ec_t *pec, ec_timer_t *timeout);

#ifdef __cplusplus
};
#endif

#define ec_to_adr(ado, adp) \
    ((uint32_t)(adp) << 16) | ((ado) & 0xFFFF)

#define ec_brd(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BRD, ((uint32_t)(ado) << 16), \
            (uint8_t *)(data), (datalen), (wkc))
#define ec_bwr(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BWR, ((uint32_t)(ado) << 16), \
            (uint8_t *)(data), (datalen), (wkc))
#define ec_brw(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BRW, ((uint32_t)(ado) << 16), \
            (uint8_t *)(data), (datalen), (wkc))

#define ec_aprd(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APRD, ((uint32_t)(ado) << 16) | \
            ((adp) & 0xFFFF), (uint8_t *)(data), (datalen), (wkc))
#define ec_apwr(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APWR, ((uint32_t)(ado) << 16) | \
            ((adp) & 0xFFFF), (uint8_t *)(data), (datalen), (wkc))
#define ec_aprw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APRW, ((uint32_t)(ado) << 16) | \
            ((adp) & 0xFFFF), (uint8_t *)(data), (datalen), (wkc))

#define ec_fprd(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPRD, ((uint32_t)(ado) << 16) | \
            ((adp) & 0xFFFF), (uint8_t *)(data), (datalen), (wkc))
#define ec_fpwr(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPWR, ((uint32_t)(ado) << 16) | \
            ((adp) & 0xFFFF), (uint8_t *)(data), (datalen), (wkc))
#define ec_fprw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPRW, ((uint32_t)(ado) << 16) | \
            ((adp) & 0xFFFF), (uint8_t *)(data), (datalen), (wkc))

#define ec_frmw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FRMW, ((uint32_t)(ado) << 16) | \
            ((adp) & 0xFFFF), (uint8_t *)(data), (datalen), (wkc))

#endif // __LIBETHERCAT_EC_H__

