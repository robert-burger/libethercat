/**
 * \file datagram.c
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

#include "config.h"

#include <string.h>
#include <assert.h>

#include "libethercat/regs.h"
#include "libethercat/datagram.h"

//! initialize empty frame
/*/
 * \param frame pointer to frame
 */
int ec_frame_init(ec_frame_t *frame) {
    int i;

    assert(frame != NULL);

    memset(frame, 0, 1518);
    for (i = 0; i < 6; ++i) {
        frame->mac_dest[i] = 0xFF;      
        frame->mac_src[i] = i;
    }

    frame->ethertype = 0x88A4;
    frame->len = sizeof(ec_frame_t);
    frame->type = 4;

    return 0;
}

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
int ec_frame_add_datagram_phys(ec_frame_t *frame, uint8_t cmd, uint8_t idx, 
        uint16_t adp, uint16_t ado, uint8_t *payload, size_t payload_len) {
    assert(frame != NULL);
    assert(payload != NULL);

    // get address to first datagram
    ec_datagram_t *d = (ec_datagram_t *)((uint8_t *)frame + 
            sizeof(ec_frame_t));

    while (((uint8_t *)d < ((uint8_t *)frame + frame->len)) && d->next)
        d = (ec_datagram_t *)((uint8_t *)d + ec_datagram_length(d));

    // get next 
    d->next = 1;
    d = (ec_datagram_t *)((uint8_t *)d + ec_datagram_length(d));

    d->cmd = cmd; 
    d->idx = idx;
    d->adp = adp;
    d->ado = ado;
    memcpy((uint8_t *)d + sizeof(ec_datagram_t), payload, payload_len);
    d->len = payload_len;

    frame->len += ec_datagram_length(d);
    return 0;
}

//! add datagram at the end of frame
/*/
 * \param frame pointer to frame
 * \param cmd ethercat command
 * \param idx ethercat frame index
 * \param adr logical
 * \param payload frame payload
 * \param payload_len length of payload
 */
int ec_frame_add_datagram_log(ec_frame_t *frame, uint8_t cmd, uint8_t idx, 
        uint32_t adr, uint8_t *payload, size_t payload_len) {
    assert(frame != NULL);
    assert(payload != NULL);

    return ec_frame_add_datagram_phys(frame, cmd, idx, adr & 0x0000FFFF, 
            (adr & 0xFFFF0000) >> 16, payload, payload_len);
}

