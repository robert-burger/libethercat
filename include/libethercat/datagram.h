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

#ifndef LIBETHERCAT_DATAGRAM_H
#define LIBETHERCAT_DATAGRAM_H

#include <libosal/mutex.h>
#include <libosal/types.h>

#include "libethercat/common.h"
#include "libethercat/idx.h"
#include "libethercat/pool.h"

/** \defgroup datagram_group Datagram
 *
 * This modules contains functions on how EtherCAT datagrams are beeing build.
 *
 * @{
 */

#define EC_WKC_SIZE (2u)  //!< \brief Working counter byte length.

typedef struct __attribute__((__packed__)) ec_frame {
    osal_uint8_t mac_dest[6];  //!< \brief destination mac address
    osal_uint8_t mac_src[6];   //!< \brief source mac addres
    osal_uint16_t ethertype;   //!< \brief ethertype, should be 0x88A4

    osal_uint16_t len : 11;      //!< \brief frame length
    osal_uint16_t reserved : 1;  //!< \brief not used
    osal_uint16_t type : 4;      //!< \brief protocol type, 4 - EtherCAT command
} ec_frame_t;                    //!< \brief EtherCAT frame type.

#define ec_frame_hdr_length (sizeof(ec_frame_t))  //!< \brief EtherCAT frame header length.
#define ec_frame_length(f) ((f)->len)             //!< \brief EtherCAT frame length.
#define ec_frame_end(pframe) \
    (&((osal_uint8_t*)(pframe))[(pframe)->len])  //!< \brief Pointer to EtherCAT frame end.

typedef struct __attribute__((__packed__)) ec_datagram {
    osal_uint8_t cmd;            //!< \brief ethercat command
    osal_uint8_t idx;            //!< \brief datagram index
    osal_uint32_t adr;           //!< \brief logical address
                                 //   \brief auto inc address + phys mem
                                 //   \brief configured address + phys mem
    osal_uint16_t len : 11;      //!< \brief datagram length
    osal_uint16_t reserved : 4;  //!< \brief not used
    osal_uint16_t next : 1;      //!< \brief 0 - last datagram, 1 - more follow
    osal_uint16_t irq;           //!< \brief reserved for future use
} ec_datagram_t;                 //!< \brief EtherCAT datagram type.

#define ec_datagram_hdr_length (sizeof(ec_datagram_t))  //!< \brief EtherCAT datagram header length.
#define ec_datagram_length(pdg) \
    (ec_datagram_hdr_length + (pdg)->len + EC_WKC_SIZE)  //!< \brief EtherCAT datagram length.

typedef struct ec_cyclic_datagram {
    osal_mutex_t lock;      //!< \brief Lock for cyclic datagram structure.
    pool_entry_t* p_entry;  //!< \brief EtherCAT datagram from pool
    idx_entry_t* p_idx;     //!< \brief EtherCAT datagram index from pool

    osal_uint64_t recv_timeout_ns;  //!< \brief Datagram receive timeout in [ns]
    osal_timer_t timeout;           //!< \brief Timer holding actual timeout.

    void (*user_cb)(void* arg, int num);  //!< \brief User callback.
    void* user_cb_arg;                    //!< \brief User argument for user_cb.
} ec_cyclic_datagram_t;                   //!< \brief EtherCAT cyclic datagram type.

#ifdef __cplusplus
extern "C" {
#endif

//! Initialize cyclic datagram structure
/*!
 * This function initializes a default datagram.
 *
 * \param[in]   cdg                 Pointer to cyclic datagram structure.
 * \param[in]   recv_timeout        Receive timeout in [ns].
 *
 * \retval EC_OK        On success.
 */
int ec_cyclic_datagram_init(ec_cyclic_datagram_t* cdg, osal_uint64_t recv_timeout);

//! Destroy cyclic datagram structure
/*!
 * This functions destroys a datagram.
 *
 * \param[in]   cdg     Pointer to cyclic datagram structure.
 *
 * \retval EC_OK        On success.
 */
int ec_cyclic_datagram_destroy(ec_cyclic_datagram_t* cdg);

//! Initialize empty frame.
/*!
 * Initialize a frame with empty content.
 *
 * \param[in,out]   frame   Pointer to frame.
 *
 * \retval EC_OK        On success.
 */
int ec_frame_init(ec_frame_t* frame);

//! Add datagram at the end of frame.
/*!
 * This function add a non-logical datagram at the end of \p frame.
 *
 * \param[in,out]   frame         Pointer to frame.
 * \param[in]       cmd           Ethercat command.
 * \param[in]       idx           Ethercat frame index.
 * \param[in]       adp           Auto inc/configured address.
 * \param[in]       ado           Physical mem address.
 * \param[in]       payload       Frame payload.
 * \param[in]       payload_len   Length of payload.
 */
void ec_frame_add_datagram_phys(ec_frame_t* frame, osal_uint8_t cmd, osal_uint8_t idx,
                                osal_uint16_t adp, osal_uint16_t ado, osal_uint8_t* payload,
                                osal_size_t payload_len);

//! Add datagram at the end of frame.
/*!
 * This function add a logical datagram at the end of \p frame.
 *
 * \param[in,out]   frame         Pointer to frame.
 * \param[in]       cmd           Ethercat command.
 * \param[in]       idx           Ethercat frame index.
 * \param[in]       adr           Logical address.
 * \param[in]       payload       Frame payload.
 * \param[in]       payload_len   Length of payload.
 */
void ec_frame_add_datagram_log(ec_frame_t* frame, osal_uint8_t cmd, osal_uint8_t idx,
                               osal_uint32_t adr, osal_uint8_t* payload, osal_size_t payload_len);

//! Cast pointer to ec_datagram_t type
/*!
 * \param[in]       p             Pointer which contains a ec_datagram_t.
 *
 * \return Pointer to ec_datagram_t.
 */
static inline ec_datagram_t* ec_datagram_cast(osal_uint8_t* p) {
    // cppcheck-suppress misra-c2012-11.3
    return ((ec_datagram_t*)(&((osal_uint8_t*)(p))[0]));
}

//! Marking next field in datagram.
/*!
 * \param[in,out]   pdg           Pointer which contains a datagram.
 */
static inline void ec_datagram_mark_next(ec_datagram_t* pdg) {
    // cppcheck-suppress misra-c2012-11.3
    ((ec_datagram_t*)(pdg))->next = 1;
}

//! Get pointer to first datagram in frame.
/*!
 * \param[in]       pf            Pointer which contains a EtherCAT frame.
 *
 * \return Pointer to first datagram in frame.
 */
static inline ec_datagram_t* ec_datagram_first(ec_frame_t* pf) {
    // cppcheck-suppress misra-c2012-11.3
    return (ec_datagram_t*)(&(((osal_uint8_t*)(pf))[sizeof(ec_frame_t)]));
}

//! Get pointer to next datagram in frame.
/*!
 * \param[in]       pdg             Pointer which contains a EtherCAT datagram.
 *
 * \return Pointer to next datagram in frame.
 */
static inline ec_datagram_t* ec_datagram_next(ec_datagram_t* pdg) {
    // cppcheck-suppress misra-c2012-11.3
    return (ec_datagram_t*)(&(((osal_uint8_t*)(pdg))[ec_datagram_length((pdg))]));
}

//! Get pointer to datagram payload.
/*!
 * \param[in]       pdg             Pointer which contains a EtherCAT datagram.
 *
 * \return Pointer to payload of datagram.
 */
static inline osal_uint8_t* ec_datagram_payload(ec_datagram_t* pdg) {
    // cppcheck-suppress misra-c2012-11.3
    return (&(((osal_uint8_t*)(pdg))[sizeof(ec_datagram_t)]));
}

//! Get working counter of datagram.
/*!
 * \param[in]       pdg             Pointer which contains a EtherCAT datagram.
 *
 * \return working counter of datagram.
 */
static inline osal_uint16_t ec_datagram_wkc(ec_datagram_t* pdg) {
    // cppcheck-suppress misra-c2012-11.3
    return (*(osal_uint16_t*)(&(((osal_uint8_t*)pdg)[ec_datagram_length(pdg) - 2u])));
}

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* LIBETHERCAT_DATAGRAM_H */
