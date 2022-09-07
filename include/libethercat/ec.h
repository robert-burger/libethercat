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

#ifndef LIBETHERCAT_EC_H
#define LIBETHERCAT_EC_H

#include <libosal/types.h>

#include <sys/select.h>

#include "libethercat/common.h"
#include "libethercat/dc.h"
#include "libethercat/slave.h"
#include "libethercat/hw.h"
#include "libethercat/regs.h"
#include "libethercat/idx.h"
#include "libethercat/datagram.h"
#include "libethercat/pool.h"
#include "libethercat/async_loop.h"
#include "libethercat/eeprom.h"

#define LEC_MAX_SLAVES  256
#define LEC_MAX_GROUPS  16

#define EC_SHORT_TIMEOUT_MBX        10000000
#define EC_DEFAULT_TIMEOUT_MBX      1000000000
#define EC_DEFAULT_DELAY            2000000

struct ec;
struct ec_slave;
typedef struct ec_slave ec_slave_t;
    
//! process data group structure
typedef struct ec_pd_group {
    osal_uint32_t log;              //!< logical address
                                    /*!<
                                     * This defines the logical start address
                                     * for the process data group. It is used
                                     * for EtherCAT logical addressing commands
                                     * LRW, LRD, LWR, ...
                                     */

    osal_uint32_t log_len;          //!< byte length at logical address
                                    /*!<
                                     * This defines the byte length at logical 
                                     * start address for the process data 
                                     * group. It is used for EtherCAT logical 
                                     * addressing commands LRW, LRD, LWR, ...
                                     */
    
    osal_uint8_t *pd;               //!< process data pointer
                                    /*!< 
                                     * This address holds the process data
                                     * of the whole group. At offset 0 the 
                                     * outputs should be set, at offset \link
                                     * pdout_len \endlink, the inputs are 
                                     * filled in by the LRW command.
                                     */

    osal_size_t   pdout_len;        //!< length of process data outputs
    osal_size_t   pdin_len;         //!< length of process data inputs
    osal_size_t   pd_lrw_len;       //!< inputs and outputs length if lrw is used

    int use_lrw;                    //!< LRW flag
                                    /*!
                                     * This flag defines if the EtherCAT master
                                     * should use the LRW command for process
                                     * data exchange
                                     */

    osal_uint16_t wkc_expected;     //!< expected working counter
                                    /*!< 
                                     * This is the expected working counter 
                                     * for the LRW command. The working counter
                                     * will be incremented by every slave that
                                     * reads data by 1, by every slave that 
                                     * writes data by 2 and by every slave that
                                     * reads and writes data by 3.
                                     */
    
    int recv_missed;                //!< missed continues ethercat frames 

    pool_entry_t *p_entry;          //!< EtherCAT datagram from pool
    idx_entry_t *p_idx;             //!< EtherCAT datagram index from pool
} ec_pd_group_t;

//! ethercat master structure
typedef struct ec {
    hw_t *phw;                      //!< pointer to hardware interface
    int tx_sync;                    //!< Synchronous call to send frames.
                                    /*!<
                                     * This defines if the actual call to 
                                     * hardware interface to send a frame (
                                     * \link hw_tx() \endlink should be done 
                                     * synchronously by the ec_transceive 
                                     * function.
                                     * If not set, the hw_tx() has to be 
                                     * called by the user (e.g. cyclical timer
                                     * loop).<br>
                                     * Usually this is needed in states BOOT,
                                     * INIT and PREOP. When entering SAFEOP (and
                                     * OP) state the realtime/deterministic mode
                                     * is started.
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

    osal_uint16_t slave_cnt;        //!< count of found EtherCAT slaves
    ec_slave_t slaves[LEC_MAX_SLAVES];             //!< array with EtherCAT slaves

    osal_uint16_t pd_group_cnt;     //!< count of process data groups
    ec_pd_group_t pd_groups[LEC_MAX_GROUPS];       //!< array with process data groups

    ec_dc_info_t dc;                //!< distributed clocks master settings
    ec_async_loop_t *async_loop;
                                    //!< asynchronous message loop
                                    /*!<
                                     * This loop receives asynchronous messages
                                     * from the EtherCAT slave mailboxes. This 
                                     * may be e.g. emergency messages...
                                     */
    
    int tun_fd;                     //!< tun device file descriptor
    osal_uint32_t tun_ip;           //!< tun device ip addres
    osal_task_t tun_tid;            //!< tun device handler thread id.
    int tun_running;                //!< tun device handler run flag.
    
    int eeprom_log;                 //!< flag whether to log eeprom to stdout
    ec_state_t master_state;        //!< expected EtherCAT master state
    int state_transition_pending;   //!< state transition is currently pending

    int threaded_startup;           //!< running state machine in threads for slave
    
    int consecutive_max_miss;       //!< max missed counter for receive frames before falling back to init

    pool_entry_t *p_de_state;       //!< EtherCAT datagram from pool for ec_state read
    idx_entry_t *p_idx_state;       //!< EtherCAT datagram index from pool for ec_state read
} ec_t;

#ifdef __cplusplus
extern "C" {
#endif

extern void *ec_log_func_user;
extern void (*ec_log_func)(int lvl, void *user, const osal_char_t *format, ...);

void ec_log(int lvl, const osal_char_t *pre, const osal_char_t *format, ...);

//! \brief Open ethercat master.
/*!
 * This function is used as initial call to create the EtherCAT master 
 * instance. It configures all needed options with default values. A packet 
 * receive thread is spawned with given priority (@p prio) and affinity (@p affinity). 
 * Ensure that they meet your realtime requirements.
 *
 * After the successfull completion of \link ec_open \endlink an EtherCAT 
 * scan should be performed with \link ec_scan \endlink.
 *
 * \param[out] ppec         Return value for ethercat master pointer.
 * \param[in]  ifname       Ethercat master interface name.
 * \param[in]  prio         Receive thread priority.
 * \param[in]  cpumask      Receive thread cpumask.
 * \param[in]  eeprom_log   Log eeprom to stdout.
 * \return 0 on succes, otherwise error code
 */
int ec_open(ec_t **ppec, const osal_char_t *ifname, int prio, int cpumask, int eeprom_log);

//! \brief Closes ethercat master.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \return 0 on success 
 */
int ec_close(ec_t *pec);

//! \brief Configures tun device of EtherCAT master, used for EoE slaves.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] ip_address    IP address to be set for tun device.
 */
void ec_configure_tun(ec_t *pec, osal_uint8_t ip_address[4]);

//! \brief Create process data groups.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] pd_group_cnt  Number of groups to create.
 * \return 0 on success
 */
int ec_create_pd_groups(ec_t *pec, osal_uint32_t pd_group_cnt);

//! \brief Destroy process data groups.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \return 0 on success
 */
int ec_destroy_pd_groups(ec_t *pec);

//! \brief Syncronous ethercat read/write.
/*!
 * \param[in]  pec          Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in]  cmd          EtherCAT command.
 * \param[in]  adr          32-bit address of slave.
 * \param[in]  data         Data buffer to read/write .
 * \param[in]  datalen      Length of data.
 * \param[out] wkc          Return value for working counter.
 * \return 0 on succes, otherwise error code
 */
int ec_transceive(ec_t *pec, osal_uint8_t cmd, osal_uint32_t adr, 
        osal_uint8_t *data, osal_size_t datalen, osal_uint16_t *wkc);

//! \brief Asyncronous ethercat read/write, answer don't care.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] cmd           Ethercat command.
 * \param[in] adr           32-bit address of slave.
 * \param[in] data          Data buffer to read/write.
 * \param[in] datalen       Length of data.
 * \return 0 on succes, otherwise error code
 */
int ec_transmit_no_reply(ec_t *pec, osal_uint8_t cmd, osal_uint32_t adr, 
        osal_uint8_t *data, osal_size_t datalen);

//! \brief Set state on ethercat bus.
/*! 
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] state         New ethercat state.
 * \return 0 on success
 */
int ec_set_state(ec_t *pec, ec_state_t state);

//! \brief Send process data for specific group with logical commands.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] group         Group number.
 * \return 0 on success
 */
int ec_send_process_data_group(ec_t *pec, int group);

//! \brief Receive process data for specific group with logical commands.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] group         Group number.
 * \param[in] timeout       Timeout for waiting for packet.
 * \return 0 on success
 */
int ec_receive_process_data_group(ec_t *pec, int group, osal_timer_t *timeout);

//! \brief Send distributed clocks sync datagram.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \return 0 on success
 */
int ec_send_distributed_clocks_sync(ec_t *pec);

//! \brief Receive distributed clocks sync datagram.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] timeout       Absolute timeout.
 * \return 0 on success
 */
int ec_receive_distributed_clocks_sync(ec_t *pec, osal_timer_t *timeout);

//! \brief Send broadcast read to ec state.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \return 0 on success
 */
int ec_send_brd_ec_state(ec_t *pec);

//! \brief Receive broadcast read to ec_state.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] timeout       Timeout for waiting for packet.
 * \return 0 on success
 */
int ec_receive_brd_ec_state(ec_t *pec, osal_timer_t *timeout);

//! \brief Return current slave count.
/*!
 * \param[in] pec           Pointer to ethercat master structure.
 * \return cnt current slave count.
 */
int ec_get_slave_count(ec_t *pec);

#ifdef __cplusplus
};
#endif

#define ec_to_adr(ado, adp) \
    ((osal_uint32_t)(adp) << 16u) | ((ado) & 0xFFFFu)

#define ec_brd(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BRD, ((osal_uint32_t)(ado) << 16u), \
            (osal_uint8_t *)(data), (datalen), (wkc))
#define ec_bwr(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BWR, ((osal_uint32_t)(ado) << 16u), \
            (osal_uint8_t *)(data), (datalen), (wkc))
#define ec_brw(pec, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_BRW, ((osal_uint32_t)(ado) << 16u), \
            (osal_uint8_t *)(data), (datalen), (wkc))

#define ec_aprd(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APRD, ((osal_uint32_t)(ado) << 16u) | \
            (*(osal_uint16_t *)&(adp) & 0xFFFFu), (osal_uint8_t *)(data), (datalen), (wkc))
#define ec_apwr(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APWR, ((osal_uint32_t)(ado) << 16u) | \
            (*(osal_uint16_t *)&(adp) & 0xFFFFu), (osal_uint8_t *)(data), (datalen), (wkc))
#define ec_aprw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_APRW, ((osal_uint32_t)(ado) << 16u) | \
            (*(osal_uint16_t *)&(adp) & 0xFFFFu), (osal_uint8_t *)(data), (datalen), (wkc))

#define ec_fprd(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPRD, ((osal_uint32_t)(ado) << 16lu) | \
            (osal_uint32_t)((adp) & 0xFFFFu), (osal_uint8_t *)(data), (datalen), (wkc))
#define ec_fpwr(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPWR, ((osal_uint32_t)(ado) << 16u) | \
            ((adp) & 0xFFFFu), (osal_uint8_t *)(data), (datalen), (wkc))
#define ec_fprw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FPRW, ((osal_uint32_t)(ado) << 16u) | \
            ((adp) & 0xFFFFu), (osal_uint8_t *)(data), (datalen), (wkc))
#define ec_frmw(pec, adp, ado, data, datalen, wkc) \
    ec_transceive((pec), EC_CMD_FRMW, ((osal_uint32_t)(ado) << 16u) | \
            ((adp) & 0xFFFFu), (osal_uint8_t *)(data), (datalen), (wkc))

#define check_ret(fcn, ...) { \
    if (fcn(__VA_ARGS__) != EC_OK) { \
        ec_log(1, __func__, "" #fcn "(" #__VA_ARGS__ ") failed!\n"); \
    } }

#define check_ec_bwr(...)  check_ret(ec_bwr, __VA_ARGS__)
#define check_ec_brd(...)  check_ret(ec_brd, __VA_ARGS__)
#define check_ec_brw(...)  check_ret(ec_brw, __VA_ARGS__)

#define check_ec_apwr(...)  check_ret(ec_apwr, __VA_ARGS__)
#define check_ec_aprd(...)  check_ret(ec_aprd, __VA_ARGS__)
#define check_ec_aprw(...)  check_ret(ec_aprw, __VA_ARGS__)

#define check_ec_fpwr(...)  check_ret(ec_fpwr, __VA_ARGS__)
#define check_ec_fprd(...)  check_ret(ec_fprd, __VA_ARGS__)
#define check_ec_fprw(...)  check_ret(ec_fprw, __VA_ARGS__)
#define check_ec_frmw(...)  check_ret(ec_frmw, __VA_ARGS__)


#endif // LIBETHERCAT_EC_H

