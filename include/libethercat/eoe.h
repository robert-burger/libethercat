/**
 * \file eoe.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 22 Nov 2016
 *
 * \brief EtherCAT eoe functions.
 *
 * Implementaion of the Ethernet over EtherCAT mailbox protocol
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

#ifndef LIBETHERCAT_EOE_H
#define LIBETHERCAT_EOE_H

#include "libethercat/common.h"

typedef struct ec_eoe {
    pool_t *recv_pool;
} ec_eoe_t;

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! initialize EoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_eoe_init(ec_t *pec, uint16_t slave);

//! \brief Wait for EoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] pp_entry  Returns pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_eoe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry);

//! \brief Enqueue EoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_eoe_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry);

int ec_eoe_set_ip_parameter(ec_t *pec, uint16_t slave, uint8_t *mac,
        uint8_t *ip_address, uint8_t *subnet, uint8_t *gateway, 
        uint8_t *dns, char *dns_name);

// send ethernet frame, fragmented if needed
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] frame         Ethernet frame buffer to be sent.
 * \param[in] frame_len     Length of Ethernet frame buffer.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_eoe_send_frame(ec_t *pec, uint16_t slave, uint8_t *frame, 
        size_t frame_len);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_EOE_H

