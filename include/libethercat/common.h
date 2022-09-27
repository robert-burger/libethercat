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

#ifndef LIBETHERCAT_COMMON_H
#define LIBETHERCAT_COMMON_H

#include <libethercat/config.h>
#include <libethercat/memory.h>

#include <libosal/types.h>

#include <stdint.h>

#ifndef LEC_MAX_SLAVES
#define LEC_MAX_SLAVES          (      256u)
#endif

#ifndef LEC_MAX_GROUPS
#define LEC_MAX_GROUPS          (        8u)
#endif

#ifndef LEC_MAX_PDLEN
#define LEC_MAX_PDLEN           (2u * 1518u)
#endif

#ifndef LEC_MAX_MBX_ENTRIES
#define LEC_MAX_MBX_ENTRIES     (       16u)
#endif

#ifndef LEC_MAX_INIT_CMD_DATA
#define LEC_MAX_INIT_CMD_DATA   (     2048u)
#endif

#ifndef LEC_MAX_SLAVE_FMMU
#define LEC_MAX_SLAVE_FMMU      (        8u)
#endif

#ifndef LEC_MAX_SLAVE_SM
#define LEC_MAX_SLAVE_SM        (        8u)
#endif

#ifndef LEC_MAX_DATAGRAMS       
#define LEC_MAX_DATAGRAMS       (      100u)
#endif 

#ifndef LEC_MAX_EEPROM_CAT_SM
#define LEC_MAX_EEPROM_CAT_SM   LEC_MAX_SLAVE_SM
#endif

#ifndef LEC_MAX_EEPROM_CAT_FMMU
#define LEC_MAX_EEPROM_CAT_FMMU LEC_MAX_SLAVE_FMMU
#endif

#ifndef LEC_MAX_EEPROM_CAT_PDO
#define LEC_MAX_EEPROM_CAT_PDO          128
#endif

#ifndef LEC_MAX_EEPROM_CAT_PDO_ENTRIES
#define LEC_MAX_EEPROM_CAT_PDO_ENTRIES   32
#endif

#define PACKED __attribute__((__packed__))

#ifndef min
#define min(a, b)  ((a) < (b) ? (a) : (b))
#endif

#define ec_min(a, b)  ((a) < (b) ? (a) : (b))

#define free_resource(a) {  \
    if ((a) != NULL) {      \
        (void)ec_free((a));          \
        (a) = NULL;         \
    } }

#define alloc_resource(a, type, len) {      \
    if ((len) > 0u) {                       \
        (a) = (type *)ec_malloc((len));        \
        (void)memset((a), 0u, (len)); } }

#define EC_MAX_DATA 4096u

//typedef union ec_data {
//    osal_uint8_t  bdata[EC_MAX_DATA]; /* variants for easy data access */
//    osal_uint16_t wdata[EC_MAX_DATA>>1u];
//    osal_uint32_t ldata[EC_MAX_DATA>>2u];
//} ec_data_t;

typedef osal_uint8_t ec_data_t[EC_MAX_DATA]; /* variants for easy data access */

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

// cppcheck-suppress misra-c2012-20.9
#if HAVE_MALLOC == 0
void *rpl_malloc(osal_size_t n);
#endif

#ifdef __VXWORKS__ 
osal_char_t *strndup(const osal_char_t *s, osal_size_t n);
#endif

//#define check_ret(cmd) { if ((cmd) != 0) { ec_log(1, __func__, #cmd " returned error\n"); } }

#endif // LIBETHERCAT_COMMON_H

