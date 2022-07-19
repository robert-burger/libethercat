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

#include <stdint.h>
#include <stdlib.h>
#include "libethercat/common.h"

#define EC_WKC_SIZE     2u

typedef struct __attribute__((__packed__)) ec_frame {
    uint8_t mac_dest[6];        //!< destination mac address 
    uint8_t mac_src[6];         //!< source mac addres
    uint16_t ethertype;         //!< ethertype, should be 0x88A4
    
    uint16_t len        : 11;   //!< frame length
    uint16_t reserved   : 1;    //!< not used
    uint16_t type       : 4;    //!< protocol type, 4 - EtherCAT command
} ec_frame_t;

#define ec_frame_hdr_length     (sizeof(ec_frame_t))
#define ec_frame_length(f)      ((f)->len)
#define ec_frame_end(pframe)    ((uint8_t *)(pframe) + (pframe)->len)

typedef struct __attribute__((__packed__)) ec_datagram {
    uint8_t cmd;                //!< ethercat command
    uint8_t idx;                //!< datagram index
    //union {
    //    struct {
    //        uint16_t adp;       //!< auto inc/configured address
    //        uint16_t ado;       //!< physical mem address
    //    };
        uint32_t adr;           //!< logical address
    //};
    uint16_t len        : 11;   //!< datagram length
    uint16_t reserved   : 4;    //!< not used
    uint16_t next       : 1;    //!< 0 - last datagram, 1 - more follow
    uint16_t irq;               //!< reserved for future use
} ec_datagram_t;

#define ec_datagram_hdr_length  (sizeof(ec_datagram_t))
#define ec_datagram_length(pdg) \
    (ec_datagram_hdr_length + (pdg)->len + EC_WKC_SIZE)

#ifdef __cplusplus
extern "C" {
#endif

//! initialize empty frame
/*/
 * \param frame pointer to frame
 */
int ec_frame_init(ec_frame_t *frame);

//! add datagram at the end of frame
/*/
 * \param frame pointer to frame
 * \param cmd ethercat command
 * \param idx ethercat frame index
 * \param adp auto inc/configured address
 * \param ado physical mem address
 * \param payload frame payload
 * \param payload_len length of payload
 */
void ec_frame_add_datagram_phys(ec_frame_t *frame, uint8_t cmd, uint8_t idx, 
        uint16_t adp, uint16_t ado, uint8_t *payload, size_t payload_len);

//! add datagram at the end of frame
/*/
 * \param frame pointer to frame
 * \param cmd ethercat command
 * \param idx ethercat frame index
 * \param adr logical
 * \param payload frame payload
 * \param payload_len length of payload
 */
void ec_frame_add_datagram_log(ec_frame_t *frame, uint8_t cmd, uint8_t idx, 
        uint32_t adr, uint8_t *payload, size_t payload_len);

#define ec_datagram_mark_next(p)  ((ec_datagram_t *)(p))->next = 1
#define ec_datagram_first(pframe) \
    (ec_datagram_t *)(((uint8_t *)(pframe)) + sizeof(ec_frame_t))
#define ec_datagram_next(p)       \
    (ec_datagram_t *)((uint8_t *)(p) + ec_datagram_length(p))
#define ec_datagram_payload(p)    \
    ((uint8_t *)(p) + sizeof(ec_datagram_t))
#define ec_datagram_wkc(p)        \
    (*(uint16_t *)((uint8_t *)(p) + ec_datagram_length(p) - 2))

#ifdef __cplusplus
}
#endif

#endif /* LIBETHERCAT_DATAGRAM_H */

