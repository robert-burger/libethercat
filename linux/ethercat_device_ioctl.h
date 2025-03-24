/**
 * \file ethercat_device_ioctl.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 2023-03-23
 *
 * \brief Character device for EtherCAT network device.
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
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ETHERCAT_DEVICE_IOCTL__H
#define ETHERCAT_DEVICE_IOCTL__H

#include <linux/ioctl.h>

/* ioctls */
#define ETHERCAT_DEVICE_MAGIC                   'e'

#define ETHERCAT_DEVICE_MONITOR_ENABLE          _IOW (ETHERCAT_DEVICE_MAGIC, 1, unsigned int)
#define ETHERCAT_DEVICE_SET_POLLING_RX_TIMEOUT  _IOW (ETHERCAT_DEVICE_MAGIC, 2, uint64_t)
#define ETHERCAT_DEVICE_SET_POLLING             _IOR (ETHERCAT_DEVICE_MAGIC, 3, uint32_t)

#define ETHERCAT_DEVICE_GET_POLLING             _IOR (ETHERCAT_DEVICE_MAGIC, 1, uint32_t)
#define ETHERCAT_DEVICE_GET_LINK_STATE          _IOR (ETHERCAT_DEVICE_MAGIC, 2, uint8_t)

#define ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC      (0x88A40000)
#define ETHERCAT_DEVICE_NET_DEVICE_GET_POLLING      (ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC | 0x0001)
#define ETHERCAT_DEVICE_NET_DEVICE_SET_POLLING      (ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC | 0x0002)
#define ETHERCAT_DEVICE_NET_DEVICE_RESET_POLLING    (ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC | 0x0003)
#define ETHERCAT_DEVICE_NET_DEVICE_DO_POLL_RX       (ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC | 0x0000)
#define ETHERCAT_DEVICE_NET_DEVICE_DO_POLL_TX       (ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC | 0x0004)

#endif 

