/**
 * \file common.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 23 Nov 2016
 *
 * \brief ethercat master common stuff
 *
 * 
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

#ifndef LIBETHERCAT_COMMON_H
#define LIBETHERCAT_COMMON_H

#include <libosal/types.h>

#include <stdint.h>

#ifdef LIBETHERCAT_MAX_SLAVES
//! Maximum number of EtherCAT slaves supported.
#define LEC_MAX_SLAVES                      ( (osal_size_t)LIBETHERCAT_MAX_SLAVES )
#else
//! Maximum number of EtherCAT slaves supported.
#define LEC_MAX_SLAVES                      ( (osal_size_t)     256u)
#endif

#ifdef LIBETHERCAT_MAX_GROUPS
//! Maximum number of EtherCAT groups supported.
#define LEC_MAX_GROUPS                      ( (osal_size_t)LIBETHERCAT_MAX_GROUPS )
#else
//! Maximum number of EtherCAT groups supported.
#define LEC_MAX_GROUPS                      ( (osal_size_t)       8u)
#endif

#ifdef LIBETHERCAT_MAX_PDLEN
//! Maximum process data length.
#define LEC_MAX_PDLEN                       ( (osal_size_t)LIBETHERCAT_MAX_PDLEN )
#else
//! Maximum process data length.
#define LEC_MAX_PDLEN                       ( (osal_size_t)(2u * 1518u))
#endif

#ifdef LIBETHERCAT_MAX_MBX_ENTRIES
//! Maximum number of mailbox entries.
#define LEC_MAX_MBX_ENTRIES                 ( (osal_size_t)LIBETHERCAT_MAX_MBX_ENTRIES )
#else
//! Maximum number of mailbox entries.
#define LEC_MAX_MBX_ENTRIES                 ( (osal_size_t)      16u)
#endif

#ifdef LIBETHERCAT_MAX_INIT_CMD_DATA
//! Maximum size of init command data.
#define LEC_MAX_INIT_CMD_DATA               ( (osal_size_t)LIBETHERCAT_MAX_INIT_CMD_DATA )
#else
//! Maximum size of init command data.
#define LEC_MAX_INIT_CMD_DATA               ( (osal_size_t)    2048u)
#endif

#ifdef LIBETHERCAT_MAX_SLAVE_FMMU
//! Maximum number of slave FMMUs.
#define LEC_MAX_SLAVE_FMMU                  ( (osal_size_t)LIBETHERCAT_MAX_SLAVE_FMMU )
#else
//! Maximum number of slave FMMUs.
#define LEC_MAX_SLAVE_FMMU                  ( (osal_size_t)       8u)
#endif

#ifdef LIBETHERCAT_MAX_SLAVE_SM
//! Maximum number of slave sync managers.
#define LEC_MAX_SLAVE_SM                    ( (osal_size_t)LIBETHERCAT_MAX_SLAVE_SM )
#else
//! Maximum number of slave sync managers.
#define LEC_MAX_SLAVE_SM                    ( (osal_size_t)       8u)
#endif

#ifdef LIBETHERCAT_MAX_DATAGRAMS                   
//! Maximum number of datagrams.
#define LEC_MAX_DATAGRAMS                   ( (osal_size_t)LIBETHERCAT_MAX_DATAGRAMS )
#else
//! Maximum number of datagrams.
#define LEC_MAX_DATAGRAMS                   ( (osal_size_t)     100u)
#endif 

#ifdef LIBETHERCAT_MAX_EEPROM_CAT_SM
//! Maximum number of EEPROM catergory sync manager entries.
#define LEC_MAX_EEPROM_CAT_SM               ( (osal_size_t)LIBETHERCAT_MAX_EEPROM_CAT_SM )
#else
//! Maximum number of EEPROM catergory sync manager entries.
#define LEC_MAX_EEPROM_CAT_SM               ( (osal_size_t)LEC_MAX_SLAVE_SM)
#endif

#ifdef LIBETHERCAT_MAX_EEPROM_CAT_FMMU
//! Maximum number of EEPROM catergory FMMU entries.
#define LEC_MAX_EEPROM_CAT_FMMU             ( (osal_size_t)LIBETHERCAT_MAX_EEPROM_CAT_FMMU )
#else
//! Maximum number of EEPROM catergory FMMU entries.
#define LEC_MAX_EEPROM_CAT_FMMU             ( (osal_size_t)LEC_MAX_SLAVE_FMMU)
#endif

#ifdef LIBETHERCAT_MAX_EEPROM_CAT_PDO
//! Maximum number of EEPROM catergory PDO entries.
#define LEC_MAX_EEPROM_CAT_PDO              ( (osal_size_t)LIBETHERCAT_MAX_EEPROM_CAT_PDO )
#else
//! Maximum number of EEPROM catergory PDO entries.
#define LEC_MAX_EEPROM_CAT_PDO              ( (osal_size_t)     128u)
#endif

#ifdef LIBETHERCAT_MAX_EEPROM_CAT_PDO_ENTRIES
//! Maximum number of EEPROM catergory PDO entries.
#define LEC_MAX_EEPROM_CAT_PDO_ENTRIES      ( (osal_size_t)LIBETHERCAT_MAX_EEPROM_CAT_PDO_ENTRIES )
#else
//! Maximum number of EEPROM catergory PDO entries.
#define LEC_MAX_EEPROM_CAT_PDO_ENTRIES      ( (osal_size_t)      32u)
#endif

#ifdef LIBETHERCAT_MAX_EEPROM_CAT_STRINGS      
//! Maximum number of EEPROM catergory string entries.
#define LEC_MAX_EEPROM_CAT_STRINGS          ( (osal_size_t)LIBETHERCAT_MAX_EEPROM_CAT_STRINGS )
#else
//! Maximum number of EEPROM catergory string entries.
#define LEC_MAX_EEPROM_CAT_STRINGS          ( (osal_size_t)     128u)
#endif

#ifdef LIBETHERCAT_MAX_EEPROM_CAT_DC      
//! Maximum number of EEPROM catergory distributed clocks entries.
#define LEC_MAX_EEPROM_CAT_DC               ( (osal_size_t)LIBETHERCAT_MAX_EEPROM_CAT_DC )
#else
//! Maximum number of EEPROM catergory distributed clocks entries.
#define LEC_MAX_EEPROM_CAT_DC               ( (osal_size_t)       8u)
#endif

#ifdef LIBETHERCAT_MAX_STRING_LEN
//! Maximum string length.
#define LEC_MAX_STRING_LEN                  ( (osal_size_t)LIBETHERCAT_MAX_STRING_LEN )
#else
//! Maximum string length.
#define LEC_MAX_STRING_LEN                  ( (osal_size_t)     128u)
#endif

#ifdef LIBETHERCAT_MAX_DATA
//! Maximum data length.
#define LEC_MAX_DATA                        ( (osal_size_t)LIBETHERCAT_MAX_DATA )
#else
//! Maximum data length.
#define LEC_MAX_DATA                        ( (osal_size_t)    4096u)
#endif

#ifdef LIBETHERCAT_MAX_DS402_SUBDEVS
//! Maximum DS420 sub devices.
#define LEC_MAX_DS402_SUBDEVS               ( (osal_size_t)LIBETHERCAT_MAX_DS402_SUBDEVS )
#else
//! Maximum DS420 sub devices.
#define LEC_MAX_DS402_SUBDEVS               ( (osal_size_t)       4u)
#endif

#ifdef LIBETHERCAT_MAX_COE_EMERGENCIES
//! Maximum number of CoE emergency messages.
#define LEC_MAX_COE_EMERGENCIES             ( (osal_size_t)LIBETHERCAT_MAX_COE_EMERGENCIES )
#else
//! Maximum number of CoE emergency messages.
#define LEC_MAX_COE_EMERGENCIES             ( (osal_size_t)      10u)
#endif

#ifdef LIBETHERCAT_MAX_COE_EMERGENCY_MSG_LEN
//! Maximum message length of CoE emergency messages.
#define LEC_MAX_COE_EMERGENCY_MSG_LEN       ( (osal_size_t)LIBETHERCAT_MAX_COE_EMERGENCY_MSG_LEN )
#else
//! Maximum message length of CoE emergency messages.
#define LEC_MAX_COE_EMERGENCY_MSG_LEN       ( (osal_size_t)      32u)
#endif

#define PACKED __attribute__((__packed__))

#ifndef LEC_MIN
#define LEC_MIN(a, b)  ((a) < (b) ? (a) : (b))
#endif

typedef osal_uint8_t ec_data_t[LEC_MAX_DATA]; /* variants for easy data access */

//! process data structure
typedef struct ec_pd {
    osal_uint8_t *pd;        //!< pointer to process data
    osal_size_t len;         //!< process data length
} ec_pd_t;

typedef osal_uint16_t ec_state_t;
#define EC_STATE_UNKNOWN     ((osal_uint16_t)(0x0000u))       //!< \brief unknown state
#define EC_STATE_INIT        ((osal_uint16_t)(0x0001u))       //!< \brief EtherCAT INIT state
#define EC_STATE_PREOP       ((osal_uint16_t)(0x0002u))       //!< \brief EtherCAT PREOP state
#define EC_STATE_BOOT        ((osal_uint16_t)(0x0003u))       //!< \brief EtherCAT BOOT state
#define EC_STATE_SAFEOP      ((osal_uint16_t)(0x0004u))       //!< \brief EtherCAT SAFEOP state
#define EC_STATE_OP          ((osal_uint16_t)(0x0008u))       //!< \brief EtherCAT OP state
#define EC_STATE_MASK        ((osal_uint16_t)(0x000Fu))       //!< \brief EtherCAT state mask
#define EC_STATE_ERROR       ((osal_uint16_t)(0x0010u))       //!< \brief EtherCAT ERROR
#define EC_STATE_RESET       ((osal_uint16_t)(0x0010u))       //!< \brief EtherCAT ERROR reset

#define EC_TIMEOUT_FRAME     ((osal_uint64_t) 2000000u)        //!< \brief EtherCAT frame timeout in [ns].
#define EC_TIMEOUT_LOW_PRIO  ((osal_uint64_t)50000000u)        //!< \brief Timeout for low-priority tranceive frames, will be re-send on loss.

#endif // LIBETHERCAT_COMMON_H

