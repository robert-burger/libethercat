/**
 * \file hw_snarf_ll.c
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
#include <vxWorks.h>
#include <taskLib.h>
#include <sys/ioctl.h>

#define RECEIVE(fd, frame, len)     read((fd), (frame), (len))
#define SEND(fd, frame, len)        write((fd), (frame), (len))
    
//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 *
 * \return 0 or negative error code
 */
int hw_device_open(hw_t *phw, const osal_char_t *devname) {
    int ret = EC_OK;

    /* we use snarf link layer device driver */
    // cppcheck-suppress misra-c2012-7.1
    phw->sockfd = open(devname, O_RDWR, 0644);
    if (phw->sockfd <= 0) {
        ec_log(1, __func__, "error opening %s: %s\n", devname, strerror(errno));
    } else {
        phw->mtu_size = 1480;
    }

    return ret;
}

//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   frame       Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
int hw_device_recv(hw_t *phw, ec_frame_t *pframe) {
}
