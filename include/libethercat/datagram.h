/**
 * \file datagram.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief ethercat datagram
 *
 * These are EtherCAT datagram specific configuration functions.
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

#ifndef LIBETHERCAT_DATAGRAM_H
#define LIBETHERCAT_DATAGRAM_H

#include <libosal/types.h>
#include <libosal/mutex.h>

#include "libethercat/common.h"
#include "libethercat/pool.h"
#include "libethercat/idx.h"

#define EC_WKC_SIZE     2u

typedef struct __attribute__((__packed__)) ec_frame {
    osal_uint8_t mac_dest[6];        //!< destination mac address 
    osal_uint8_t mac_src[6];         //!< source mac addres
    osal_uint16_t ethertype;         //!< ethertype, should be 0x88A4
    
    osal_uint16_t len        : 11;   //!< frame length
    osal_uint16_t reserved   : 1;    //!< not used
    osal_uint16_t type       : 4;    //!< protocol type, 4 - EtherCAT command
} ec_frame_t;

#define ec_frame_hdr_length     (sizeof(ec_frame_t))
#define ec_frame_length(f)      ((f)->len)
#define ec_frame_end(pframe)    (&((osal_uint8_t *)(pframe))[(pframe)->len])

typedef struct __attribute__((__packed__)) ec_datagram {
    osal_uint8_t cmd;                //!< ethercat command
    osal_uint8_t idx;                //!< datagram index
    osal_uint32_t adr;               //!< logical address 
                                     //   auto inc address + phys mem
                                     //   configured address + phys mem
    osal_uint16_t len        : 11;   //!< datagram length
    osal_uint16_t reserved   : 4;    //!< not used
    osal_uint16_t next       : 1;    //!< 0 - last datagram, 1 - more follow
    osal_uint16_t irq;               //!< reserved for future use
} ec_datagram_t;

#define ec_datagram_hdr_length  (sizeof(ec_datagram_t))
#define ec_datagram_length(pdg) \
    (ec_datagram_hdr_length + (pdg)->len + EC_WKC_SIZE)

typedef struct ec_cyclic_datagram {
    osal_mutex_t lock;                      //!< \brief Lock for cyclic datagram structure.
    pool_entry_t *p_entry;                  //!< \brief EtherCAT datagram from pool
    idx_entry_t *p_idx;                     //!< \brief EtherCAT datagram index from pool

    osal_uint64_t recv_timeout_ns;          //!< \brief Datagram receive timeout in [ns]
    osal_timer_t timeout;                   //!< \brief Timer holding actual timeout.
    int had_timeout;                        //!<¸\brief Had timeout last time.

    void (*user_cb)(void *arg, int num);    //!< \brief User callback.
    void *user_cb_arg;                      //!< \brief User argument for user_cb.
} ec_cyclic_datagram_t;

#ifdef __cplusplus
extern "C" {
#endif

//! Initialize cyclic datagram structure
/*!
 * \param[in]   cdg                 Pointer to cyclic datagram structure.
 * \param[in]   recv_timeout_ns     Receive timeout in [ns].
 * \return EC_OK on success, otherwise error code.
 */
int ec_cyclic_datagram_init(ec_cyclic_datagram_t *cdg, osal_uint64_t recv_timeout);

//! Destroy cyclic datagram structure
/*!
 * \param[in]   cdg     Pointer to cyclic datagram structure.
 * \return EC_OK on success, otherwise error code.
 */
int ec_cyclic_datagram_destroy(ec_cyclic_datagram_t *cdg);

//! Initialize empty frame.
/*!
 * \param[in,out]   frame   Pointer to frame.
 *
 * \return EC_OK
 */
int ec_frame_init(ec_frame_t *frame);

//! Add datagram at the end of frame.
/*!
 * \param[in,out]   frame         Pointer to frame.
 * \param[in]       cmd           Ethercat command.
 * \param[in]       idx           Ethercat frame index.
 * \param[in]       adp           Auto inc/configured address.
 * \param[in]       ado           Physical mem address.
 * \param[in]       payload       Frame payload.
 * \param[in]       payload_len   Length of payload.
 */
void ec_frame_add_datagram_phys(ec_frame_t *frame, osal_uint8_t cmd, osal_uint8_t idx, 
        osal_uint16_t adp, osal_uint16_t ado, osal_uint8_t *payload, osal_size_t payload_len);

//! Add datagram at the end of frame.
/*/
 * \param[in,out]   frame         Pointer to frame.
 * \param[in]       cmd           Ethercat command.
 * \param[in]       idx           Ethercat frame index.
 * \param[in]       adr           Logical address.
 * \param[in]       payload       Frame payload.
 * \param[in]       payload_len   Length of payload.
 */
void ec_frame_add_datagram_log(ec_frame_t *frame, osal_uint8_t cmd, osal_uint8_t idx, 
        osal_uint32_t adr, osal_uint8_t *payload, osal_size_t payload_len);

static inline ec_datagram_t *ec_datagram_cast(osal_uint8_t *p) {
    // cppcheck-suppress misra-c2012-11.3
    return ((ec_datagram_t *)(&((osal_uint8_t *)(p))[0]));
}

static inline void ec_datagram_mark_next(ec_datagram_t *pdg) {
    // cppcheck-suppress misra-c2012-11.3
    ((ec_datagram_t *)(pdg))->next = 1;
}

static inline ec_datagram_t *ec_datagram_first(ec_frame_t *pf) {
    // cppcheck-suppress misra-c2012-11.3
    return (ec_datagram_t *)(&(((osal_uint8_t *)(pf))[sizeof(ec_frame_t)]));
}

static inline ec_datagram_t *ec_datagram_next(ec_datagram_t *pdg) {
    // cppcheck-suppress misra-c2012-11.3
    return (ec_datagram_t *)(&(((osal_uint8_t *)(pdg))[ec_datagram_length((pdg))]));
}

static inline osal_uint8_t *ec_datagram_payload(ec_datagram_t *pdg) {
    // cppcheck-suppress misra-c2012-11.3
    return (&(((osal_uint8_t *)(pdg))[sizeof(ec_datagram_t)]));
}

static inline osal_uint16_t ec_datagram_wkc(ec_datagram_t *pdg) {
    // cppcheck-suppress misra-c2012-11.3
    return (*(osal_uint16_t *)(&(((osal_uint8_t *)pdg)[ec_datagram_length(pdg) - 2u])));
}

#ifdef __cplusplus
}
#endif

#endif /* LIBETHERCAT_DATAGRAM_H */

