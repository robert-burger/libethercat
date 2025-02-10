/**
 * \file slave.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 21 Nov 2016
 *
 * \brief EtherCAT slave functions.
 *
 * These are EtherCAT slave specific configuration functions.
 */

/*
 * This file is part of libethercat.
 *
 * libethercat is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * libethercat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public 
 * License along with libethercat (LICENSE.LGPL-V3); if not, write 
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth 
 * Floor, Boston, MA  02110-1301, USA.
 * 
 * Please note that the use of the EtherCAT technology, the EtherCAT 
 * brand name and the EtherCAT logo is only permitted if the property 
 * rights of Beckhoff Automation GmbH are observed. For further 
 * information please contact Beckhoff Automation GmbH & Co. KG, 
 * Hülshorstweg 20, D-33415 Verl, Germany (www.beckhoff.com) or the 
 * EtherCAT Technology Group, Ostendstraße 196, D-90482 Nuremberg, 
 * Germany (ETG, www.ethercat.org).
 *
 */

#ifndef LIBETHERCAT_SLAVE_H
#define LIBETHERCAT_SLAVE_H

#include <libosal/types.h>

#include "libethercat/config.h"
#include "libethercat/common.h"
#include "libethercat/eeprom.h"
#include "libethercat/dc.h"
#include "libethercat/mbx.h"

/** \defgroup slave_group Slave
 *
 * This modules contains main EtherCAT slave functions.
 *
 * @{
 */

//! EtherCAT slave state transitions
#define BOOT_2_BOOT      0x0303u  //!< \brief BOOT to BOOT state transition
#define BOOT_2_INIT      0x0301u  //!< \brief BOOT to INIT state transition
#define BOOT_2_PREOP     0x0302u  //!< \brief BOOT to PREOP state transition,
#define BOOT_2_SAFEOP    0x0304u  //!< \brief BOOT to SAFEOP state transition,
#define BOOT_2_OP        0x0308u  //!< \brief BOOT to OP state transition,
#define UNKNOWN_2_BOOT   0x0003u  //!< \brief UNKNOWN to BOOT state transition
#define UNKNOWN_2_INIT   0x0001u  //!< \brief UNKNOWN to INIT state transition,
#define UNKNOWN_2_PREOP  0x0002u  //!< \brief UNKNOWN to PREOP state transition,
#define UNKNOWN_2_SAFEOP 0x0004u  //!< \brief UNKNOWN to SAFEOP state transition,
#define UNKNOWN_2_OP     0x0008u  //!< \brief UNKNOWN to OP state transition,
#define INIT_2_BOOT      0x0103u  //!< \brief INIT to BOOT state transition
#define INIT_2_INIT      0x0101u  //!< \brief INIT to INIT state transition,
#define INIT_2_PREOP     0x0102u  //!< \brief INIT to PREOP state transition,
#define INIT_2_SAFEOP    0x0104u  //!< \brief INIT to SAFEOP state transition,
#define INIT_2_OP        0x0108u  //!< \brief INIT to OP state transition,
#define PREOP_2_BOOT     0x0203u  //!< \brief PREOP to BOOT state transition,
#define PREOP_2_INIT     0x0201u  //!< \brief PREOP to INIT state transition,
#define PREOP_2_PREOP    0x0202u  //!< \brief PREOP to PREOP state transition,
#define PREOP_2_SAFEOP   0x0204u  //!< \brief PREOP to SAFEOP state transition,
#define PREOP_2_OP       0x0208u  //!< \brief PREOP to OP state transition,
#define SAFEOP_2_BOOT    0x0403u  //!< \brief SAFEOP to BOOT state transition,
#define SAFEOP_2_INIT    0x0401u  //!< \brief SAFEOP to INIT state transition,
#define SAFEOP_2_PREOP   0x0402u  //!< \brief SAFEOP to PREOP state transition,
#define SAFEOP_2_SAFEOP  0x0404u  //!< \brief SAFEOP to SAFEOP state transition,
#define SAFEOP_2_OP      0x0408u  //!< \brief SAFEOP to OP state transition,
#define OP_2_BOOT        0x0803u  //!< \brief OP to BOOT state transition,
#define OP_2_INIT        0x0801u  //!< \brief OP to INIT state transition,
#define OP_2_PREOP       0x0802u  //!< \brief OP to PREOP state transition,
#define OP_2_SAFEOP      0x0804u  //!< \brief OP to SAFEOP state transition,
#define OP_2_OP          0x0808u  //!< \brief OP to OP state transition,

typedef osal_uint16_t ec_state_transition_t;

//! slave sync manager settings
typedef struct PACKED ec_slave_sm {
    osal_uint16_t adr;              //!< sync manager address
                                    /*!<
                                     * This field specifies the physical address
                                     * where the sync manager starts.
                                     */

    osal_uint16_t len;              //!< sync manager length
                                    /*!<
                                     * This field specifies the length of the sync 
                                     * manager
                                     */
    osal_uint8_t control_register;  //!< control register
                                    /*!<
                                     * Defines Mode of Operation
                                     */
    osal_uint8_t status_regsiter;   //!< status register
                                    /*!<
                                     * don’t care
                                     */
    osal_uint8_t enable_sm;         //!< enable sync manager
                                    /*!<
                                     * Bit 0: enable
                                     * Bit 1: fixed content
                                     * Bit 2: virtual SyncManager
                                     * Bit 3: opOnly
                                     * Bit 7:4: reserved
                                     */
    osal_uint8_t sm_type;           //!< sync manager type
                                    /*!<
                                     * 0x00 = not used or unknown
                                     * 0x01 = used for mailbox out
                                     * 0x02 = used for mailbox in
                                     * 0x03 = used for process data outputs
                                     * 0x04 = used for process data inputs
                                     * 0x05 = used for dynamic process data outputs
                                     * 0x06 = used for dynamic process data inputs
                                     */


    // osal_uint32_t flags;        //!< sync manager flags
    //                             /*!<
    //                              * Sync manager flags according to EtherCAT 
    //                              * specifications
    //                              */
} PACKED ec_slave_sm_t;

//! slave fielbus memory management unit (fmmu) settings
typedef struct PACKED ec_slave_fmmu {
    osal_uint32_t log;          //!< logical bus address
                                /*!< This specifys to logical 32-bit bus 
                                 * address to listen to. If any EtherCAT 
                                 * datagram with logical addressing is passing 
                                 * with the correct logical address, the fmmu 
                                 * is copying data from and to the EtherCAT 
                                 * datagram.
                                 */

    osal_uint16_t log_len;      //!< length of logical address area
                                /*!< 
                                 * length of bytes starting from logical 
                                 * address, which should be copyied from/to 
                                 * EtherCAT datagram
                                 */

    osal_uint8_t  log_bit_start;//!< start bit at logical bus address
                                /*!<
                                 * start bit at logical start address
                                 */

    osal_uint8_t  log_bit_stop; //!< stop bit at logical address plus length
                                /*!<
                                 * end bit at logical end address
                                 */

    osal_uint16_t phys;         //!< physical (local) address in slave
                                /*!<
                                 * This defines the physical (local) address 
                                 * in the EtherCAT slave from where to start 
                                 * copying data from/to.
                                 */

    osal_uint8_t  phys_bit_start;//!< physical start bit at physical address
                                /*!<
                                 * This defines the first bit at physical start 
                                 * address to beging the copying.
                                 */

    osal_uint8_t  type;         //!< type, read or write
                                /*!<
                                 */

    osal_uint8_t  active;       //!< activation flag
                                /*!<
                                 */

    osal_uint8_t reserverd[3];  //!< reserved for future use
} PACKED ec_slave_fmmu_t;

//! EtherCAT sub device
typedef struct ec_slave_subdev {
    ec_pd_t pdin;               //!< process data inputs
    ec_pd_t pdout;              //!< process data outputs
} ec_slave_subdev_t;

//! slave mailbox init commands
typedef struct ec_init_cmd {
    int type;                   //!< Mailbox type
                                /*!< 
                                 * The type defines which kind of Mailbox 
                                 * protocol to use for the init command. This 
                                 * can be one of \link EC_MBX_COE \endlink, 
                                 * \link EC_MBX_SOE \endlink, ... 
                                 */
    
    int transition;             //!< ECat transition
                                /*!< 
                                 * This defines at which EtherCAT state machine 
                                 * transition the init command will be sent to 
                                 * the EtherCAT slave. The upper 4 bits specify 
                                 * the actual state and the lower 4 bits the 
                                 * target state. (e.g. 0x24 -> PRE to SAFE, ..)
                                 */

    LIST_ENTRY(ec_init_cmd) le; //!< List entry handle.

    int id;                     //!< index 
                                /*!< 
                                 * This depends of which Mailbox protocol is 
                                 * beeing used. For CoE it defines the 
                                 * dictionary identifier, for SoE the id 
                                 * number, ...
                                 */

    int si_el;                  //!< sub index
                                /*!< 
                                 * This depends of which Mailbox protocol is 
                                 * beeing used. For CoE it defines the sub 
                                 * identifier, for SoE  the id element, ...
                                 */

    int ca_atn;                 //!< flags 
                                /*!< 
                                 * The flags define some additional setting 
                                 * depending on the Mailbox protocol. (e.g. 
                                 * CoE complete access mode, SoE atn, ...)
                                 */

    osal_char_t data[LEC_MAX_INIT_CMD_DATA];    //!< new id data
    osal_size_t datalen;                        //!< new id data length
} ec_init_cmd_t;

#define INIT_CMD_SIZE       (sizeof(ec_init_cmd_t))
    
LIST_HEAD(ec_init_cmds, ec_init_cmd);

typedef struct worker_arg {
    struct ec *pec;             //!< \brief Pointer to EtherCAT master struct.
    int slave;                  //!< \brief Slave number this worker is doing things for.
    ec_state_t state;           //!< \brief State of EtherCAT slave.
} worker_arg_t;                 //!< \brief Worker thread argument structure.

typedef struct ec_slave {
    osal_uint32_t slave;            //!< \brief Slave index in EtherCAT master array.

    osal_int16_t auto_inc_address;   //!< physical bus address
    osal_uint16_t fixed_address;//!< virtual bus address, programmed on start

    osal_uint16_t type;         //!< value of EC_REG_TYPE
    osal_uint8_t sm_ch;         //!< number of sync manager channels
    osal_uint8_t fmmu_ch;       //!< number of fmmu channels
    osal_uint32_t ram_size;     //!< ram size in bytes
    osal_uint16_t features;     //!< fmmu operation, dc available
    osal_uint16_t pdi_ctrl;     //!< configuration of process data interface
    osal_uint8_t link_cnt;      //!< link count
    osal_uint8_t active_ports;  //!< active ports with link
    osal_uint16_t ptype;        //!< ptype
    osal_int32_t pdelay;             //!< propagation delay of the slave
    
    int entry_port;             //!< entry port from parent slave
    int parent;                 //!< parent slave number
    int port_on_parent;         //!< port attached on parent slave 

    int sm_set_by_user;         //!< sm set by user
                                /*!<
                                 * This defines if the sync manager settings
                                 * are set by the user and should not be 
                                 * figured out by the EtherCAT master state
                                 * machine. If not set, the master will try
                                 * to generate the sm settings either via a 
                                 * available mailbox protocol or the EEPROM.
                                 */

    ec_slave_sm_t sm[LEC_MAX_SLAVE_SM];          //!< array of sm settings
                                /*!<
                                 * These are the settings for the sync
                                 * managers of the EtherCAT slaves.
                                 * The size of the array is \link sm_ch
                                 * \endlink.
                                 */

    ec_slave_fmmu_t fmmu[LEC_MAX_SLAVE_FMMU];      //!< array of fmmu settings
                                /*!<
                                 * These are the settings for the fielbus 
                                 * management units of the EtherCAT slaves.
                                 * The size of the array is \link fmmu_ch
                                 * \endlink.
                                 */

    int assigned_pd_group;      //!< Process data group this slave is assigned to.
    ec_pd_t pdin;               //!< input process data 
                                /*!<
                                 * This is the complete input process data of 
                                 * the EtherCAT slave. Parts of it may also be 
                                 * accessed if we have multiple sub devices 
                                 * defined by the slave (\link subdevs 
                                 * \endlink)
                                 */

    ec_pd_t pdout;              //!< output process data
                                /*!<
                                 * This is the complete output process data of 
                                 * the EtherCAT slave. Parts of it may also be 
                                 * accessed if we have multiple sub devices 
                                 * defined by the slave (\link subdevs 
                                 * \endlink)
                                 */

    osal_size_t subdev_cnt;          //!< count of sub devices
                                /*!< 
                                 * An EtherCAT slave may have multiple sub
                                 * devices defines. These may be e.g. multiple
                                 * Sercos drives per slave, multiple CiA-DSP402
                                 * axes per slave, ...
                                 */

    ec_slave_subdev_t subdevs[LEC_MAX_DS402_SUBDEVS]; //!< array of sub devices
                                /*!< 
                                 * An EtherCAT slave may have multiple sub
                                 * devices defines. These may be e.g. multiple
                                 * Sercos drives per slave, multiple CiA-DSP402
                                 * axes per slave, ...
                                 */

    ec_mbx_t mbx;               //!< EtherCAT mailbox structure

    eeprom_info_t eeprom;       //!< EtherCAT slave EEPROM data
    ec_dc_info_slave_t dc;      //!< Distributed Clock settings

#if LIBETHERCAT_MBX_SUPPORT_EOE == 1
    ec_eoe_slave_config_t eoe;  //!< EoE config
#endif
    
    ec_state_t expected_state;  //!< Master expected slave state
    ec_state_t act_state;       //!< Actual/Last read slave state.

    osal_mutex_t transition_mutex;  //!< Lock for state transition pending.
    osal_bool_t transition_active;  //!< Flag is state transition is currently active.

    struct ec_init_cmds init_cmds;
                                //!< EtherCAT slave init commands
                                /*!<
                                 * This is a list of EtherCAT slave init 
                                 * commands. They should be addes to the list
                                 * by \link ec_slave_add_init_cmd \endlink.
                                 * An init command is usefull to make slave
                                 * specific settings while setting the state
                                 * machine from INIT to OP.
                                 */
                
    worker_arg_t worker_arg;    //!< Set state worker thread arguments.
                                /*!< 
                                 * These arguments are used for worker thread 
                                 * when threaded startup is used.
                                 */
    osal_task_t worker_tid;     //!< Set state worker thread handle.
                                /*!<
                                 * Handle to spawned worker thread if threaded
                                 * startup is used.
                                 */
} ec_slave_t;


#define ec_slave_ptr(ptr, pec, slave) \
    ec_slave_t *(ptr) = (ec_slave_t *)&pec->slaves[slave]; \
    assert((ptr) != NULL);

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

// init slave resources
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
void ec_slave_init(struct ec *pec, osal_uint16_t slave);

// free slave resources
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
void ec_slave_free(struct ec *pec, osal_uint16_t slave);

//! Set EtherCAT state on slave.
/*!
 * This call tries to set the EtherCAT slave to the requested state. If 
 * successfull a working counter of 1 will be returned. 
 *
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] state     New EtherCAT state which will be set on specified slave.
 *
 * \return EC_OK on success, otherwise EC_ERROR_* code.
 */
int ec_slave_set_state(struct ec *pec, osal_uint16_t slave, ec_state_t state);

//! Get EtherCAT state from slave.
/*!
 * This call tries to read the EtherCAT slave state. If 
 * successfull a working counter of 1 will be returned. 
 *
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[out] state        Returns current EtherCAT state.
 * \param[out] alstatcode   Return current AL StatusCode of specified
 *                          EtherCAT slave. (maybe NULL if you are not
 *                          interested in)
 *
 * \return EC_OK on success, otherwise EC_ERROR_* code.
 */
int ec_slave_get_state(struct ec *pec, osal_uint16_t slave, 
        ec_state_t *state, osal_uint16_t *alstatcode);

//! Generate process data mapping.
/*!
 * This tries to generate a mapping for the process data and figures out the 
 * settings for the sync managers. Therefor it either tries to use an 
 * available mailbox protocol or the information stored in the EEPROM.
 *  
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 *
 * \return EC_OK on success, otherwise EC_ERROR_* code.
 */
int ec_slave_generate_mapping(struct ec *pec, osal_uint16_t slave);

//! Prepare state transition on EtherCAT slave.
/*!
 * While prepare a state transition the master sends the init commands
 * to the slaves. These are usually settings for the process data mapping 
 * (e.g. PDOs, ...) or some slave specific settings.
 *
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] state         Prepare the EtherCAT slave for a switch to 
 *                          the specified EtherCAT state.
 *
 * \return Working counter of the used commands, should be 1 if it was successfull.
 */
int ec_slave_prepare_state_transition(struct ec *pec, osal_uint16_t slave, 
        ec_state_t state);

//! Execute state transition on EtherCAT slave
/*!
 * This actually performs the state transition.
 *
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] state         Switch the EtherCAT slave to the specified 
 *                          EtherCAT state.
 *
 * \return Working counter of the used commands, should be 1 if it was successfull.
 */
int ec_slave_state_transition(struct ec *pec, osal_uint16_t slave, 
        ec_state_t state);

//! Initialize CoE init command.
/*!
 * This adds an EtherCAT slave init command. 
 *
 * \param[in] cmd           Init command to add.
 * \param[in] transition    EtherCAT state transition in form 0xab, where 'a' is 
 *                          the state we are coming from and 'b' is the state 
 *                          we want to get.
 * \param[in] id            Either CoE Index number or the ServoDrive IDN.
 * \param[in] si_el         Either CoE SubIndex or ServoDrive element number.
 * \param[in] ca_atn        Either CoE complete access or ServoDrive ATN.
 * \param[in] data          Pointer to memory buffer with data which should 
 *                          be transfered.
 * \param[in] datalen       Length of \p data
 */
void ec_slave_mailbox_coe_init_cmd_init(ec_init_cmd_t *cmd,
        int transition, int id, int si_el, int ca_atn,
        osal_char_t *data, osal_size_t datalen);

#if LIBETHERCAT_MBX_SUPPORT_SOE == 1
//! Initialie SoE init command.
/*!
 * This adds an EtherCAT slave init command. 
 *
 * \param[in] cmd           Init command to add.
 * \param[in] transition    EtherCAT state transition in form 0xab, where 'a' is 
 *                          the state we are coming from and 'b' is the state 
 *                          we want to get.
 * \param[in] id            Either SoE Index number or the ServoDrive IDN.
 * \param[in] si_el         Either SoE SubIndex or ServoDrive element number.
 * \param[in] ca_atn        Either SoE complete access or ServoDrive ATN.
 * \param[in] data          Pointer to memory buffer with data which should 
 *                          be transfered.
 * \param[in] datalen       Length of \p data
 */
void ec_slave_mailbox_soe_init_cmd_init(ec_init_cmd_t *cmd,
        int transition, int id, int si_el, int ca_atn,
        osal_char_t *data, osal_size_t datalen);
#endif

//! Add master init command.
/*!
 * This adds an EtherCAT slave init command. 
 *
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] cmd           Init command to add.
 */
void ec_slave_add_init_cmd(ec_t *pec, osal_uint16_t slave, ec_init_cmd_t *cmd);

//! Set Distributed Clocks config to slave
/*! 
 * \param[in] pec            Pointer to ethercat master structure, 
 *                           which you got from \link ec_open \endlink.
 * \param[in] slave          Number of ethercat slave. this depends on 
 *                           the physical order of the ethercat slaves 
 *                           (usually the n'th slave attached).
 * \param[in] use_dc         Whether to en-/disable dc on slave.
 * \param[in] activation_reg DC Sync Activation Register (0x981). Sets
 *                           the type of DC sync:
 *                           * 0x3 = Sync0
 *                           * 0x5 = Sync1
 *                           * 0x7 = Sync01
 * \param[in] cycle_time_0   Cycle time of sync 0 [ns].
 * \param[in] cycle_time_1   Cycle time of sync 1 [ns].
 * \param[in] cycle_shift    Cycle shift time [ns].
 */
void ec_slave_set_dc_config(struct ec *pec, osal_uint16_t slave, 
        int use_dc, int type, osal_uint32_t cycle_time_0, 
        osal_uint32_t cycle_time_1, osal_int32_t cycle_shift);

#if LIBETHERCAT_MBX_SUPPORT_EOE == 1
//! Adds master EoE settings.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] mac           Pointer to 6 byte MAC address (mandatory).
 * \param[in] ip_address    Pointer to 4 byte IP address (optional maybe NULL).
 * \param[in] subnet        Pointer to 4 byte subnet address (optional maybe NULL).
 * \param[in] gateway       Pointer to 4 byte gateway address (optional maybe NULL).
 * \param[in] dns           Pointer to 4 byte DNS address (optional maybe NULL).
 * \param[in] dns_name      Null-terminated domain name server string.
 */
void ec_slave_set_eoe_settings(struct ec *pec, osal_uint16_t slave,
        osal_uint8_t *mac, osal_uint8_t *ip_address, osal_uint8_t *subnet, osal_uint8_t *gateway, 
        osal_uint8_t *dns, osal_char_t *dns_name);
#endif

const osal_char_t *al_status_code_2_string(int code);

#if 0 
{
#endif
#ifdef __cplusplus
}
#endif

/** @} */

#endif // LIBETHERCAT_SLAVE_H

