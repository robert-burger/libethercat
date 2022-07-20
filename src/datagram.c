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
 * If not, see <www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <assert.h>

#include "libethercat/regs.h"
#include "libethercat/datagram.h"
#include "libethercat/error_codes.h"

//! Initialize empty frame.
/*/
 * \param[in,out]   frame   Pointer to frame.
 *
 * \return EC_OK
 */
int ec_frame_init(ec_frame_t *frame) {
    int i;
    int ret = EC_OK;

    assert(frame != NULL);

    (void)memset(frame, 0, 1518);
    for (i = 0; i < 6; ++i) {
        frame->mac_dest[i] = 0xFFu;
        frame->mac_src[i] = i;
    }

    frame->ethertype = 0x88A4u;
    frame->len = sizeof(ec_frame_t);
    frame->type = 4;

    return ret;
}

//! Add datagram at the end of frame.
/*/
 * \param[in,out]   frame         Pointer to frame.
 * \param[in]       cmd           Ethercat command.
 * \param[in]       idx           Ethercat frame index.
 * \param[in]       adp           Auto inc/configured address.
 * \param[in]       ado           Physical mem address.
 * \param[in]       payload       Frame payload.
 * \param[in]       payload_len   Length of payload.
 */
void ec_frame_add_datagram_phys(ec_frame_t *frame, uint8_t cmd, uint8_t idx, 
        uint16_t adp, uint16_t ado, uint8_t *payload, size_t payload_len) {
    assert(frame != NULL);
    assert(payload != NULL);

    ec_frame_add_datagram_log(frame, cmd, idx, 
            (((uint32_t)ado << 16u) || (uint32_t)adp), payload, payload_len);
}

//! Add datagram at the end of frame.
/*/
 * \param[in,out]   frame         Pointer to frame.
 * \param[in]       cmd           Ethercat command.
 * \param[in]       idx           Ethercat frame index.
 * \param[in]       adr           Logical address.
 * \param[in]       payload       Frame payload.
 * \param[in]       payload_len   Length of payload.
 */
void ec_frame_add_datagram_log(ec_frame_t *frame, uint8_t cmd, uint8_t idx, 
        uint32_t adr, uint8_t *payload, size_t payload_len) {
    assert(frame != NULL);
    assert(payload != NULL);

    // get address to first datagram
    // cppcheck-suppress misra-c2012-11.3
    ec_datagram_t *d = (ec_datagram_t *)&((uint8_t *)frame)[sizeof(ec_frame_t)];

    while (((uint8_t *)d < (&((uint8_t *)frame)[frame->len])) && d->next) {
        // cppcheck-suppress misra-c2012-11.3
        d = (ec_datagram_t *)&((uint8_t *)d)[ec_datagram_length(d)];
    }

    // get next 
    d->next = 1;
    // cppcheck-suppress misra-c2012-11.3
    d = (ec_datagram_t *)&((uint8_t *)d)[ec_datagram_length(d)];

    d->cmd = cmd; 
    d->idx = idx;
    d->adr = adr;
    (void)memcpy(&((uint8_t *)d)[sizeof(ec_datagram_t)], payload, payload_len);
    d->len = payload_len;

    frame->len += ec_datagram_length(d);
}

