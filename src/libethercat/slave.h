/**
 * \file slave.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 21 Nov 2016
 *
 * \brief ethercat slave functions.
 *
 * These are EtherCAT slave specific configuration functions.
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

#ifndef __LIBETHERCAT_SLAVE_H__
#define __LIBETHERCAT_SLAVE_H__

#include <stdint.h>

#include "libethercat/common.h"
#include "libethercat/eeprom.h"
#include "libethercat/dc.h"

//! EtherCAT slave state transitions
typedef enum ec_state_transition {
    BOOT_2_INIT      = 0x0301,  //!< BOOT to INIT state transition
    INIT_2_BOOT      = 0x0103,  //!< INIT to BOOT state transition
    INIT_2_INIT      = 0x0101,  //!< INIT to INIT state transitio,
    INIT_2_PREOP     = 0x0102,  //!< INIT to PREOP state transitio,
    INIT_2_SAFEOP    = 0x0104,  //!< INIT to SAFEOP state transitio,
    INIT_2_OP        = 0x0108,  //!< INIT to OP state transitio,
    PREOP_2_INIT     = 0x0201,  //!< PREOP to INIT state transitio,
    PREOP_2_PREOP    = 0x0202,  //!< PREOP to PREOP state transitio,
    PREOP_2_SAFEOP   = 0x0204,  //!< PREOP to SAFEOP state transitio,
    PREOP_2_OP       = 0x0208,  //!< PREOP to OP state transitio,
    SAFEOP_2_INIT    = 0x0401,  //!< SAFEOP to INIT state transitio,
    SAFEOP_2_PREOP   = 0x0402,  //!< SAFEOP to PREOP state transitio,
    SAFEOP_2_SAFEOP  = 0x0404,  //!< SAFEOP to SAFEOP state transitio,
    SAFEOP_2_OP      = 0x0408,  //!< SAFEOP to OP state transitio,
    OP_2_INIT        = 0x0801,  //!< OP to INIT state transitio,
    OP_2_PREOP       = 0x0802,  //!< OP to PREOP state transitio,
    OP_2_SAFEOP      = 0x0804,  //!< OP to SAFEOP state transitio,
    OP_2_OP          = 0x0808,  //!< OP to OP state transitio,
} ec_state_transition_t;

//! ethercat slave mailbox settings
typedef struct ec_slave_mbx {
    uint8_t  sm_nr;             //!< mailbox sync manager numer
                                /*!<
                                 * This specifies the assign sync manager
                                 * for the mailbox access.
                                 */

    uint8_t *sm_state;          //!< sync manager state
                                /*!<
                                 * The field is used to receive the mailbox 
                                 * sync manager state. This is useful to 
                                 * determine if the mailbox is full or empty
                                 * without the need to poll the state manually.
                                 */

    uint8_t *buf;               //!< mailbox buffer
                                /*!<
                                 * Receive or Transmit buffer for mailbox 
                                 * messages.
                                 */

    uint8_t  skip_next;         //!< set if next message should be skipped
} ec_slave_mbx_t;

//! slave sync manager settings
typedef struct PACKED ec_slave_sm {
    uint16_t adr;               //!< sync manager address
                                /*!<
                                 * This field specifies the physical address
                                 * where the sync manager starts.
                                 */

    uint16_t len;               //!< sync manager length
                                /*!>
                                 * This field specifies the length of the sync 
                                 * manager
                                 */

    uint32_t flags;             //!< sync manager flags
                                /*!<
                                 * Sync manager flags according to EtherCAT 
                                 * specifications
                                 */
} PACKED ec_slave_sm_t;

//! slave fielbus memory management unit (fmmu) settings
typedef struct PACKED ec_slave_fmmu {
    uint32_t log;               //!< logical bus address
                                /*!< This specifys to logical 32-bit bus 
                                 * address to listen to. If any EtherCAT 
                                 * datagram with logical addressing is passing 
                                 * with the correct logical address, the fmmu 
                                 * is copying data from and to the EtherCAT 
                                 * datagram.
                                 */

    uint16_t log_len;           //!< length of logical address area
                                /*!< 
                                 * length of bytes starting from logical 
                                 * address, which should be copyied from/to 
                                 * EtherCAT datagram
                                 */

    uint8_t  log_bit_start;     //!< start bit at logical bus address
                                /*!<
                                 * start bit at logical start address
                                 */

    uint8_t  log_bit_stop;      //!< stop bit at logical address plus length
                                /*!<
                                 * end bit at logical end address
                                 */

    uint16_t phys;              //!< physical (local) address in slave
                                /*!<
                                 * This defines the physical (local) address 
                                 * in the EtherCAT slave from where to start 
                                 * copying data from/to.
                                 */

    uint8_t  phys_bit_start;    //!< physical start bit at physical address
                                /*!<
                                 * This defines the first bit at physical start 
                                 * address to beging the copying.
                                 */

    uint8_t  type;              //!< type, read or write
                                /*!<
                                 */

    uint8_t  active;            //!< activation flag
                                /*!<
                                 */

    uint8_t reserverd[3];       //!< reserved for future use
} PACKED ec_slave_fmmu_t;

//! ethercat sub device
typedef struct ec_slave_subdev {
    ec_pd_t pdin;               //!< process data inputs
    ec_pd_t pdout;              //!< process data outputs
} ec_slave_subdev_t;

//! slave mailbox init commands
typedef struct ec_slave_mailbox_init_cmd {
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

    char *data;                 //!< new id data
    size_t datalen;             //!< new id data length

    LIST_ENTRY(ec_slave_mailbox_init_cmd) le;
} ec_slave_mailbox_init_cmd_t;
    
LIST_HEAD(ec_slave_mailbox_init_cmds, ec_slave_mailbox_init_cmd);

typedef struct worker_arg {
    struct ec *pec;
    int slave;
    ec_state_t state;
} worker_arg_t;

typedef struct ec_slave {
    int16_t auto_inc_address;   //!< physical bus address
    uint16_t fixed_address;     //!< virtual bus address, programmed on start

    uint8_t sm_ch;              //!< number of sync manager channels
    uint8_t fmmu_ch;            //!< number of fmmu channels
    int ram_size;               //!< ram size in bytes
    uint16_t features;          //!< fmmu operation, dc available
    uint16_t pdi_ctrl;          //!< configuration of process data interface
    uint8_t link_cnt;           //!< link count
    uint8_t active_ports;       //!< active ports with link
    uint16_t ptype;             //!< ptype
    int32_t pdelay;             //!< propagation delay of the slave
    
    int entry_port;             //!< entry port from parent slave
    int parent;                 //!< parent slave number
    int parentport;             //!< port attached on parent slave 

    int sm_set_by_user;         //!< sm set by user
                                /*!<
                                 * This defines if the sync manager settings
                                 * are set by the user and should not be 
                                 * figured out by the EtherCAT master state
                                 * machine. If not set, the master will try
                                 * to generate the sm settings either via a 
                                 * available mailbox protocol or the EEPROM.
                                 */

    ec_slave_sm_t *sm;          //!< array of sm settings
                                /*!<
                                 * These are the settings for the sync
                                 * managers of the EtherCAT slaves.
                                 * The size of the array is \link sm_ch
                                 * \endlink.
                                 */

    ec_slave_fmmu_t *fmmu;      //!< array of fmmu settings
                                /*!<
                                 * These are the settings for the fielbus 
                                 * management units of the EtherCAT slaves.
                                 * The size of the array is \link fmmu_ch
                                 * \endlink.
                                 */

    pthread_mutex_t mbx_lock;   //!< mailbox lock
                                /*!<
                                 * Only one simoultaneous access to the 
                                 * EtherCAT slave mailbox is possible at the 
                                 * moment.
                                 */

    ec_slave_mbx_t mbx_read;    //!< read mailbox 
    ec_slave_mbx_t mbx_write;   //!< write mailbox

    int assigned_pd_group;
    ec_pd_t pdin;               //!< input process data 
                                /*!<
                                 * This is the complete input process data of 
                                 * the EtherCAT slave. Parts of it may also be 
                                 * accessed if we have multiple sub devices 
                                 * defined by the slave (\link subdevs 
                                 * \endlink)
                                 */
    size_t pdin_len;

    ec_pd_t pdout;              //!< output process data
                                /*!<
                                 * This is the complete output process data of 
                                 * the EtherCAT slave. Parts of it may also be 
                                 * accessed if we have multiple sub devices 
                                 * defined by the slave (\link subdevs 
                                 * \endlink)
                                 */
    size_t pdout_len;

    size_t subdev_cnt;          //!< count of sub devices
                                /*!< 
                                 * An EtherCAT slave may have multiple sub
                                 * devices defines. These may be e.g. multiple
                                 * Sercos drives per slave, multiple CiA-DSP402
                                 * axes per slave, ...
                                 */

    ec_slave_subdev_t *subdevs; //!< array of sub devices
                                /*!< 
                                 * An EtherCAT slave may have multiple sub
                                 * devices defines. These may be e.g. multiple
                                 * Sercos drives per slave, multiple CiA-DSP402
                                 * axes per slave, ...
                                 */

    eeprom_info_t eeprom;       //!< EtherCAT slave EEPROM data
    ec_dc_info_slave_t dc;      //!< Distributed Clock settings
    
    ec_state_t expected_state;  //!< Master expected slave state

    struct ec_slave_mailbox_init_cmds init_cmds;
                                //!< EtherCAT slave init commands
                                /*!<
                                 * This is a list of EtherCAT slave init 
                                 * commands. They should be addes to the list
                                 * by \link ec_slave_add_init_cmd \endlink.
                                 * An init command is usefull to make slave
                                 * specific settings while setting the state
                                 * machine from INIT to OP.
                                 */
                
    worker_arg_t worker_arg;
    pthread_t worker_tid;
} ec_slave_t;

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! set ethercat state on slave 
/*!
 * This call tries to set the EtherCAT slave to the requested state. If 
 * successfull a working counter of 1 will be returned. 
 *
 * \param pec ethercat master pointer
 * \param slave number
 * \param state new ethercat state
 * \return wkc
 */
int ec_slave_set_state(struct ec *pec, uint16_t slave, ec_state_t state);

//! get ethercat state from slave 
/*!
 * This call tries to read the EtherCAT slave state. If 
 * successfull a working counter of 1 will be returned. 
 *
 * \param pec ethercat master pointer
 * \param slave number
 * \param state return ethercat state
 * \param alstatcode return alstatcode (maybe NULL)
 * \return wkc
 */
int ec_slave_get_state(struct ec *pec, uint16_t slave, 
        ec_state_t *state, uint16_t *alstatcode);

//! generate pd mapping
/*!
 * This tries to generate a mapping for the process data and figures out the 
 * settings for the sync managers. Therefor it either tries to use an 
 * available mailbox protocol or the information stored in the EEPROM.
 *  
 * \param pec ethercat master pointer
 * \param slave slave number
 * \return wkc
 */
int ec_slave_generate_mapping(struct ec *pec, uint16_t slave);

//! prepare state transition on ethercat slave
/*!
 * While prepare a state transition the master sends the init commands
 * to the slaves. These are usually settings for the process data mapping 
 * (e.g. PDOs, ...) or some slave specific settings.
 *
 * \param pec ethercat master pointer
 * \param slave slave number
 * \param state switch to state
 * \return wkc
 */
int ec_slave_prepare_state_transition(struct ec *pec, uint16_t slave, 
        ec_state_t state);

//! state transition on ethercat slave
/*!
 * This actually performs the state transition.
 *
 * \param pec ethercat master pointer
 * \param slave slave number
 * \param state switch to state
 * \return wkc
 */
int ec_slave_state_transition(struct ec *pec, uint16_t slave, 
        ec_state_t state);

//! add master init command
/*!
 * This adds an EtherCAT slave init command. 
 *
 * \param ring master ring structure
 * \param atn drive atn
 * \param direction service direction
 * \param element service element
 * \param idn id number
 * \param value pointer to values
 * \param vallen legnth of values
 * \param desc null-terminated description (maybe NULL)
 */
void ec_slave_add_init_cmd(struct ec *pec, uint16_t slave,
        int type, int transition, int id, int si_el, int ca_atn,
        char *data, size_t datalen);

//! freeing init command strucuture
/*!
 * \param cmd init command to free
 */
void ec_slave_mailbox_init_cmd_free(ec_slave_mailbox_init_cmd_t *cmd);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_SLAVE_H__

