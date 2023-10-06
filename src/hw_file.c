/**
 * \file hw_file_ll.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
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

#include <libethercat/config.h>
#include <libethercat/hw.h>
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

/* ioctls from ethercat_device */
#define ETHERCAT_DEVICE_MAGIC             'e'
#define ETHERCAT_DEVICE_MONITOR_ENABLE    _IOW (ETHERCAT_DEVICE_MAGIC, 1, unsigned int)
#define ETHERCAT_DEVICE_GET_POLLING       _IOR (ETHERCAT_DEVICE_MAGIC, 2, unsigned int)

// forward declarations
int hw_device_file_send(hw_t *phw, ec_frame_t *pframe);
int hw_device_file_recv(hw_t *phw);
void hw_device_file_send_finished(hw_t *phw);
int hw_device_file_get_tx_buffer(hw_t *phw, ec_frame_t **ppframe);

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 *
 * \return 0 or negative error code
 */
int hw_device_file_open(hw_t *phw, const osal_char_t *devname) {
    int ret = EC_OK;
    ec_frame_t *pframe = NULL;
    static const osal_uint8_t mac_dest[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const osal_uint8_t mac_src[] = {0x00, 0x30, 0x64, 0x0f, 0x83, 0x35};

    phw->send = hw_device_file_send;
    phw->recv = hw_device_file_recv;
    phw->send_finished = hw_device_file_send_finished;
    phw->get_tx_buffer = hw_device_file_get_tx_buffer;

    /* we use file link layer device driver */
    // cppcheck-suppress misra-c2012-7.1
    phw->sockfd = open(devname, O_RDWR, 0644);
    if (phw->sockfd <= 0) {
        ec_log(1, "HW_OPEN", "error opening %s: %s\n", devname, strerror(errno));
        ret = EC_ERROR_HW_NO_INTERFACE;
    } else {
        phw->mtu_size = 1480;
    
        // cppcheck-suppress misra-c2012-11.3
        pframe = (ec_frame_t *)phw->send_frame;
        (void)memcpy(pframe->mac_dest, mac_dest, 6);
        (void)memcpy(pframe->mac_src, mac_src, 6);

        // check polling mode
        phw->polling_mode = OSAL_FALSE;
#if LIBETHERCAT_HAVE_SYS_IOCTL_H == 1
        unsigned int pollval = 0;
        if (ioctl(phw->sockfd, ETHERCAT_DEVICE_GET_POLLING, &pollval) >= 0) {
            phw->polling_mode = pollval == 0 ? OSAL_FALSE : OSAL_TRUE;
        }

        unsigned int monitor = 0;
        (void)ioctl(phw->sockfd, ETHERCAT_DEVICE_MONITOR_ENABLE, &monitor);
#endif

        ec_log(10, "HW_OPEN", "%s polling mode\n", phw->polling_mode == OSAL_FALSE ? "not using" : "using");
    }

    return ret;
}

static void hw_device_file_recv_internal(hw_t *phw) {
    // cppcheck-suppress misra-c2012-11.3
    ec_frame_t *pframe = (ec_frame_t *) &phw->recv_frame;

    // using tradional recv function
    osal_ssize_t bytesrx = read(phw->sockfd, pframe, ETH_FRAME_LEN);

    if (bytesrx > 0) {
        hw_process_rx_frame(phw, pframe);
    }
}

//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   frame       Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
int hw_device_file_recv(hw_t *phw) {
    if (phw->polling_mode == OSAL_TRUE) {
        return EC_ERROR_HW_NOT_SUPPORTED;
    } 

    hw_device_file_recv_internal(phw);

    return EC_OK;
}

//! Get a free tx buffer from underlying hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   ppframe     Pointer to return frame buffer pointer.
 *
 * \return 0 or negative error code
 */
int hw_device_file_get_tx_buffer(hw_t *phw, ec_frame_t **ppframe) {
    assert(phw != NULL);
    assert(ppframe != NULL);

    int ret = EC_OK;
    ec_frame_t *pframe = NULL;
    
    // cppcheck-suppress misra-c2012-11.3
    pframe = (ec_frame_t *)phw->send_frame;

    // reset length to send new frame
    pframe->ethertype = htons(ETH_P_ECAT);
    pframe->type = 0x01;
    pframe->len = sizeof(ec_frame_t);

    *ppframe = pframe;

    return ret;
}

//! Send a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   pframe      Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
int hw_device_file_send(hw_t *phw, ec_frame_t *pframe) {
    assert(phw != NULL);
    assert(pframe != NULL);

    int ret = EC_OK;

    // no more datagrams need to be sent or no more space in frame
    osal_ssize_t bytestx = write(phw->sockfd, pframe, pframe->len);

    if ((osal_ssize_t)pframe->len != bytestx) {
        ec_log(1, "HW_TX", "got only %" PRId64 " bytes out of %d bytes "
                "through.\n", bytestx, pframe->len);

        if (bytestx == -1) {
            ec_log(1, "HW_TX", "error: %s\n", strerror(errno));
        }

        ret = EC_ERROR_HW_SEND;
    }
    
    phw->bytes_sent += bytestx;

    return ret;
}

//! Doing internal stuff when finished sending frames
/*!
 * \param[in]   phw         Pointer to hw handle.
 */
void hw_device_file_send_finished(hw_t *phw) {
    // in case of polling do receive now
    if (phw->polling_mode == OSAL_TRUE) {
        // sleep a little bit (at least packet-on-wire-duration)
        // 10 [ns] per bit on 100 Mbit/s Ethernet.
        //uint64_t packet_time = 10 * 8 * phw->bytes_sent; 
        //osal_sleep(packet_time * 0.4);
        phw->bytes_last_sent = phw->bytes_sent;
        phw->bytes_sent = 0;

        hw_device_file_recv_internal(phw);
    }
}

//! Get file hw device.
/*!
 * \param[in,out]   dev     Pointer to hw device struct.
 *
 * \return N/A
 */
void hw_device_file_get(hw_device_t *dev) {
    assert(dev != NULL);

    dev->open = hw_device_file_open;
    dev->send = hw_device_file_send;
    dev->recv = hw_device_file_recv;
    dev->send_finished = hw_device_file_send_finished;
    dev->get_tx_buffer = hw_device_file_get_tx_buffer;
}

