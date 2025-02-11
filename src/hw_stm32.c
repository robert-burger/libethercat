/**
 * \file hw_stm32_ll.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 * \author Marcel Beausencourt <marcel.beausencourt@dlr.de>
 *
 * \date 26 Nov 2024
 *
 * \brief hardware access functions
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
#ifdef HAVE_CONFIG_H
#include <libethercat/config.h>
#endif

#include <libethercat/hw_stm32.h>
#include <libethercat/ec.h>
#include <libethercat/idx.h>
#include <libethercat/error_codes.h>

#include <assert.h>
#include <sys/stat.h>

#if LIBETHERCAT_HAVE_SYS_IOCTL_H == 1
#include <sys/ioctl.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#if LIBETHERCAT_HAVE_UNISTD_H == 1
#include <unistd.h>
#endif

#if LIBETHERCAT_HAVE_NETINET_IN_H == 1
#include <netinet/in.h>
#endif

#if LIBETHERCAT_HAVE_WINSOCK_H == 1
#include <winsock.h>
#endif

// MB includes/defines
#include "eth.h"
#define ETH_TX_TIMEOUT  2000u // taken from eth.c

/* ioctls from ethercat_device */
#define ETHERCAT_DEVICE_MAGIC             'e'
#define ETHERCAT_DEVICE_MONITOR_ENABLE    _IOW (ETHERCAT_DEVICE_MAGIC, 1, unsigned int)
#define ETHERCAT_DEVICE_GET_POLLING       _IOR (ETHERCAT_DEVICE_MAGIC, 2, unsigned int)

// forward declarations
int hw_device_stm32_send(struct hw_common *phw, ec_frame_t *pframe, pooltype_t pool_type);
int hw_device_stm32_recv(struct hw_common *phw);
void hw_device_stm32_send_finished(struct hw_common *phw);
int hw_device_stm32_get_tx_buffer(struct hw_common *phw, ec_frame_t **ppframe);
int hw_device_stm32_close(struct hw_common *phw);

// Opens EtherCAT hw device.
int hw_device_stm32_open(struct hw_stm32 *phw_stm32, struct ec *pec) {
    int ret = EC_OK;
    ec_frame_t *pframe = NULL;
    static const osal_uint8_t mac_dest[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const osal_uint8_t mac_src[] = {0x00, 0x80, 0xc1, 0xc0, 0xff, 0xee};

    hw_open(&phw_stm32->common, pec);

    phw_stm32->common.send = hw_device_stm32_send;
    phw_stm32->common.recv = hw_device_stm32_recv;
    phw_stm32->common.send_finished = hw_device_stm32_send_finished;
    phw_stm32->common.get_tx_buffer = hw_device_stm32_get_tx_buffer;
    phw_stm32->common.close = hw_device_stm32_close;
    phw_stm32->common.mtu_size = 1480;

    phw_stm32->frames_sent = 0;
    memset(&phw_stm32->TxConfig, 0 , sizeof(ETH_TxPacketConfig));
    phw_stm32->TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    phw_stm32->TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    phw_stm32->TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    // cppcheck-suppress misra-c2012-11.3
    pframe = (ec_frame_t *)phw_stm32->send_frame;
    (void)memcpy(pframe->mac_dest, mac_dest, 6);
    (void)memcpy(pframe->mac_src, mac_src, 6);

    return ret;
}

//! Close hardware layer
/*!
 * \param[in]   phw         Pointer to hw handle.
 *
 * \return 0 or negative error code
 */
int hw_device_stm32_close(struct hw_common *phw) {
    int ret = EC_OK;
    return ret;
}

//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   frame       Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
int hw_device_stm32_recv(struct hw_common *phw) {
    assert(phw != NULL);

    // new code MB
    HAL_StatusTypeDef status;
    void *app_buff;
    osal_timer_t to;
    osal_timer_init(&to, 100000);

    DECLARE_CRITICAL_SECTION();
    do {
        ENTER_CRITICAL_SECTION();
        status = HAL_ETH_ReadData(&heth, &app_buff);
        LEAVE_CRITICAL_SECTION();

        if ((status == HAL_OK) && (app_buff != NULL)) {
            // Invalidate if cache is enabled
            if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
                SCB_InvalidateDCache_by_Addr((uint32_t *)app_buff, ((ec_frame_t *)app_buff)->len);
            }

            hw_process_rx_frame(phw, app_buff);
            return EC_OK;
        }
    } while (osal_timer_expired(&to) != OSAL_ERR_TIMEOUT);

    return EC_ERROR_UNAVAILABLE; // maybe write some other ERROR code in the error_code.h!?
}

//! Get a free tx buffer from underlying hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   ppframe     Pointer to return frame buffer pointer.
 *
 * \return 0 or negative error code
 */
int hw_device_stm32_get_tx_buffer(struct hw_common *phw, ec_frame_t **ppframe) {
    assert(phw != NULL);
    assert(ppframe != NULL);

    int ret = EC_OK;
    ec_frame_t *pframe = NULL;
    struct hw_stm32 *phw_stm32 = container_of(phw, struct hw_stm32, common);

    // cppcheck-suppress misra-c2012-11.3
    pframe = (ec_frame_t *)phw_stm32->send_frame;

    // reset length to send new frame
    pframe->ethertype = htons(ETH_P_ECAT);
    pframe->type = 0x01;
    pframe->len = sizeof(ec_frame_t);

    *ppframe = pframe;

    return ret;
}

extern ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];

//! Send a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   pframe      Pointer to frame buffer.
 * \param[in]   pool_type   Pool type to distinguish between high and low prio frames.
 *
 * \return 0 or negative error code
 */
int hw_device_stm32_send(struct hw_common *phw, ec_frame_t *pframe, pooltype_t pool_type) {
    assert(phw != NULL);
    assert(pframe != NULL);

    (void)pool_type;

    int ret = EC_OK;
    struct hw_stm32 *phw_stm32 = container_of(phw, struct hw_stm32, common);

    int errval = ETH_OK;

    size_t frame_len = ec_frame_length(pframe);

    // Invalidate if cache is enabled
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
        SCB_CleanDCache_by_Addr((void*)pframe, pframe->len);
    }

    DECLARE_CRITICAL_SECTION();
    ENTER_CRITICAL_SECTION();

    Txbuffer[0].buffer = (uint8_t *)(pframe);
    Txbuffer[0].len = frame_len;
    Txbuffer[0].next = NULL;

    phw_stm32->TxConfig.Length = frame_len;
    phw_stm32->TxConfig.TxBuffer = Txbuffer;
    phw_stm32->TxConfig.pData = NULL;

    do {
        if (HAL_ETH_Transmit(&heth, &(phw_stm32->TxConfig), ETH_TX_TIMEOUT) == HAL_OK) {
            errval = ETH_OK;
        } else {
            if (HAL_ETH_GetError(&heth) & HAL_ETH_ERROR_BUSY) {
                /* Wait for descriptors to become available */
                errval = ETH_ERR_NO_BUFFER;
            } else {
                /* Other error */
                errval = ETH_ERR_OTHER;
                ret = EC_ERROR_HW_SEND;

                break;
            }
        }
    } while (errval == ETH_ERR_NO_BUFFER);

    phw_stm32->common.bytes_sent += frame_len;
    phw_stm32->frames_sent++;

    LEAVE_CRITICAL_SECTION();

    return ret;
}

//! Doing internal stuff when finished sending frames
/*!
 * \param[in]   phw         Pointer to hw handle.
 */
void hw_device_stm32_send_finished(struct hw_common *phw) {
    assert(phw != NULL);

    struct hw_stm32 *phw_stm32 = container_of(phw, struct hw_stm32, common);

    // in case of polling do receive now
    while (phw_stm32->frames_sent > 0) {
        phw_stm32->common.bytes_last_sent = phw_stm32->common.bytes_sent;
        phw_stm32->common.bytes_sent = 0;

        if (hw_device_stm32_recv(phw) == EC_ERROR_UNAVAILABLE) {
            break;
        }

        phw_stm32->frames_sent--;
    }
}

