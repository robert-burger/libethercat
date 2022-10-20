/**
 * \file hw_pikeos.c
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

#include <libethercat/config.h>
#include <libethercat/hw.h>
#include <libethercat/ec.h>
#include <libethercat/idx.h>
#include <libethercat/error_codes.h>

#include <assert.h>

//#include <net/ethernet.h>
//#include <sys/socket.h>
//#include <sys/ioctl.h>
//#include <linux/if_packet.h>
//#include <sys/mman.h>
//#include <poll.h>
//#include <fcntl.h>
//#include <unistd.h>
#include <string.h>
#include <errno.h>

#if LIBETHERCAT_HAVE_NET_UTIL_INET_H == 1
#include <net/util/inet.h>
#endif

#include <p4ext_vmem.h>
#include <vm_part.h>
#include <drv/app_sbuf_svc.h>
#include <drv/sbuf_common_svc.h>

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 *
 * \return 0 or negative error code
 */
int hw_device_open(hw_t *phw, const osal_char_t *devname) {
    assert(phw != NULL);
    assert(devname != NULL);

    int ret = EC_OK;
    P4_size_t vsize;
    vm_partition_stat_t pinfo;
    P4_e_t local_retval;
    P4_address_t vaddr;

    local_retval = vm_part_pstat(VM_RESPART_MYSELF, &pinfo);
    if (local_retval != P4_E_OK) {
        ec_log(1, __func__, "vm_part_pstat failed!\n");
        ret = EC_ERROR_UNAVAILABLE;
    }

    if (ret == EC_OK) {
        if ((pinfo.abilities & VM_AB_ULOCK_SHARED) == 0) {
            ec_log(1, __func__, "sbuf mode needs VM_AB_ULOCK_SHARED!\n");
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    if (ret == EC_OK) {
        local_retval = vm_open(devname, VM_O_RD_WR | VM_O_MAP, &phw->fd);
        if (local_retval != P4_E_OK) {
            ec_log(1, __func__, "vm_open on %s failed\n", devname);
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    if (ret == EC_OK) {
        local_retval = vm_io_sbuf_init(&phw->fd, &vsize);
        if (local_retval != P4_E_OK) {
            ec_log(1, __func__, "vm_io_sbuf_init failed\n");
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    if (ret == EC_OK) {
        vaddr = p4ext_vmem_alloc(vsize);
        if (vaddr == 0) {
            ec_log(1, __func__, "p4ext_vmem_alloc failed\n");
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    if (ret == EC_OK) {
        local_retval = vm_io_sbuf_map(&phw->fd, vaddr, &phw->sbuf);
        if (local_retval != P4_E_OK) {
            ec_log(1, __func__, "vm_io_sbuf_map failed\n");
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    return ret;
}


//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 *
 * \return 0 or negative error code
 */
int hw_device_recv(hw_t *phw) {
    assert(phw != NULL);

    int ret = EC_OK;
    ec_frame_t *pframe = NULL;

    // get a full rxbuffer from RX ring (dontwait=0 -> WAITING)
    vm_io_buf_id_t rxbuf = vm_io_sbuf_rx_get(&phw->sbuf, 0);
    if (rxbuf == VM_IO_BUF_ID_INVALID) {
        ret = EC_ERROR_UNAVAILABLE;
    }

    if (ret == EC_OK) {
        pframe = (ec_frame_t *)vm_io_sbuf_rx_buf_addr(&phw->sbuf, rxbuf);

        if (pframe == NULL) {
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    if (ret == EC_OK) {
        int32_t bytesrx = vm_io_sbuf_rx_buf_size(&phw->sbuf, rxbuf);

        if (bytesrx > 0) {
            hw_process_rx_frame(phw, pframe);
        }
    
        vm_io_sbuf_rx_free(&phw->sbuf, rxbuf);
    }

    return ret;
}

//! Get a free tx buffer from underlying hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   ppframe     Pointer to return frame buffer pointer.
 *
 * \return 0 or negative error code
 */
int hw_device_get_tx_buffer(hw_t *phw, ec_frame_t **ppframe) {
    assert(phw != NULL);
    assert(ppframe != NULL);

    int ret = EC_ERROR_UNAVAILABLE;
    ec_frame_t *pframe = NULL;
    
    static const osal_uint8_t mac_dest[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const osal_uint8_t mac_src[] = {0x00, 0x30, 0x64, 0x0f, 0x83, 0x35};

    // cppcheck-suppress misra-c2012-11.3
    pframe = (ec_frame_t *)phw->send_frame;

    // reset length to send new frame
    (void)memcpy(pframe->mac_dest, mac_dest, 6);
    (void)memcpy(pframe->mac_src, mac_src, 6);
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
int hw_device_send(hw_t *phw, ec_frame_t *pframe) {
    assert(phw != NULL);
    assert(pframe != NULL);

    int ret = EC_OK;
    void* txbuf = NULL;

    // get an empty tx buffer from TX ring (dontwait)
    vm_io_buf_id_t sbuftx = vm_io_sbuf_tx_alloc(&phw->sbuf, 1);
    if (sbuftx == VM_IO_BUF_ID_INVALID) {
        ret = EC_ERROR_UNAVAILABLE;
    }
    
    if (ret == EC_OK) {
        txbuf = (void*)vm_io_sbuf_tx_buf_addr(&phw->sbuf, sbuftx);
        
        if (txbuf == NULL) {
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    if (ret == EC_OK) {
        memcpy(txbuf, pframe, pframe->len);
        vm_io_sbuf_tx_ready(&phw->sbuf, sbuftx, pframe->len);
    }

    return ret;
}


