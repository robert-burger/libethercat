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

#include <libosal/types.h>
#include <libosal/semaphore.h>

#include "libethercat/common.h"

#define LEC_EOE_MAC_LEN         (6u)
#define LEC_EOE_IP_ADDRESS_LEN  (4u)
#define LEC_EOE_SUBNET_LEN      (4u)
#define LEC_EOE_GATEWAY_LEN     (4u)
#define LEC_EOE_DNS_LEN         (4u)
#define LEC_EOE_DNS_NAME_LEN    (128u)

typedef struct ec_eoe_slave_config {
    int use_eoe;                                    //!< \brief Using EoE on actual slave.

    osal_uint8_t mac[LEC_EOE_MAC_LEN];              //!< \brief MAC address to configure (mandatory)
    osal_uint8_t ip_address[LEC_EOE_IP_ADDRESS_LEN];//!< \brief IP address to configure (optional, maybe NULL).
    osal_uint8_t subnet[LEC_EOE_SUBNET_LEN];        //!< \brief Subnet to configure (optional, maybe NULL).
    osal_uint8_t gateway[LEC_EOE_GATEWAY_LEN];      //!< \brief Gateway to configure (optional, maybe NULL).
    osal_uint8_t dns[LEC_EOE_DNS_LEN];              //!< \brief DNS to configure (optional, maybe NULL).
    osal_char_t dns_name[LEC_EOE_DNS_NAME_LEN];     //!< \brief DNS name to configure (optional, maybe NULL).
} ec_eoe_slave_config_t;

typedef struct ec_eoe {
    pool_t recv_pool;                               //!< \brief Mailbox message with EoE fragments received.
    pool_t response_pool;

    pool_entry_t free_frames[128];                  //!< \brief Static Ethernet frames for Pool, do not use directly.
    pool_t eth_frames_free_pool;                    //!< \brief Pool with Ethernet frames currently unused.
    pool_t eth_frames_recv_pool;                    //!< \brief Pool where to store Ethernet frames nobody cared so far.

    osal_mutex_t lock;
    osal_semaphore_t send_sync;
} ec_eoe_t;

#ifdef __cplusplus
extern "C" {
#endif

//! initialize EoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_eoe_init(ec_t *pec, osal_uint16_t slave);

//! deinitialize EoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_eoe_deinit(ec_t *pec, osal_uint16_t slave);

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
void ec_eoe_enqueue(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry);

int ec_eoe_set_ip_parameter(ec_t *pec, osal_uint16_t slave, osal_uint8_t *mac,
        osal_uint8_t *ip_address, osal_uint8_t *subnet, osal_uint8_t *gateway, 
        osal_uint8_t *dns, osal_char_t *dns_name);

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
int ec_eoe_send_frame(ec_t *pec, osal_uint16_t slave, osal_uint8_t *frame, 
        osal_size_t frame_len);

// setup tun interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
int ec_eoe_setup_tun(ec_t *pec);

// Destroy tun interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
void ec_eoe_destroy_tun(ec_t *pec);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_EOE_H

