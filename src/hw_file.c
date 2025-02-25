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

#if LIBETHERCAT_BUILD_DEVICE_FILE == 1

#include <libethercat/hw_file.h>
#include <libethercat/ec.h>
#include <libethercat/idx.h>
#include <libethercat/error_codes.h>

#include <linux/ethercat_device_ioctl.h>

#include <assert.h>
#include <sys/stat.h>

#if LIBETHERCAT_HAVE_SYS_IOCTL_H == 1
#include <sys/ioctl.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#if LIBETHERCAT_HAVE_UNISTD_H == 1
#include <unistd.h>
#endif

#if LIBETHERCAT_HAVE_NETINET_IN_H == 1
#include <netinet/in.h>
#endif

#if LIBETHERCAT_HAVE_WINSOCK_H == 1
#include <winsock.h>
#endif

// forward declarations
int hw_device_file_send(struct hw_common *phw, ec_frame_t *pframe, pooltype_t pool_type);
int hw_device_file_recv(struct hw_common *phw);
void hw_device_file_send_finished(struct hw_common *phw);
int hw_device_file_get_tx_buffer(struct hw_common *phw, ec_frame_t **ppframe);
int hw_device_file_close(struct hw_common *phw);

static osal_bool_t hw_device_file_recv_internal(struct hw_file *phw_file);
static void *hw_device_file_rx_thread(void *arg);

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 * \param[in]   prio        Priority for receiver thread.
 * \param[in]   cpu_mask    CPU mask for receiver thread.
 *
 * \return 0 or negative error code
 */
int hw_device_file_open(struct hw_file *phw_file, struct ec *pec, const osal_char_t *devname, int prio, int cpumask) {
    int ret = EC_OK;
    ec_frame_t *pframe = NULL;
    static const osal_uint8_t mac_dest[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const osal_uint8_t mac_src[] = {0x00, 0x30, 0x64, 0x0f, 0x83, 0x35};

    hw_open(&phw_file->common, pec);

    phw_file->common.send = hw_device_file_send;
    phw_file->common.recv = hw_device_file_recv;
    phw_file->common.send_finished = hw_device_file_send_finished;
    phw_file->common.get_tx_buffer = hw_device_file_get_tx_buffer;
    phw_file->common.close = hw_device_file_close;
    
    int flags = O_RDWR;
    uint64_t rx_timeout_ns = 1000000;
    uint64_t link_timeout_sec = 5;

    char *tmp;
    if ((tmp = strchr(devname, ':')) != NULL) {
        *tmp = 0;
        tmp++;

        char *act = tmp;
        do {
            char *next = strchr(act, ':');
            if (next != NULL) { *next = 0; next++; }

            char *value;
            if ((value = strchr(act, '=')) != NULL) {
                *value = 0;
                value++; 

                if (strcmp(act, "rx_timeout_nsec") == 0) {
                    rx_timeout_ns = strtoull(value, NULL, 10);
                } else if (strcmp(act, "link_timeout_sec") == 0) {
                    link_timeout_sec = strtoull(value, NULL, 10);
                }
            } else {
                if (strcmp(act, "polling") == 0) {
                    ec_log(10, "HW_OPEN", "switching to polling mode\n");
                    phw_file->polling_mode = OSAL_TRUE;
                }
            }

            if ((next != NULL) && (*next != '0')) { act = next; } else { break; }
        } while (1);
    }

    /* we use file link layer device driver */
    // cppcheck-suppress misra-c2012-7.1
    phw_file->fd = open(devname, flags, 0644);
    if (phw_file->fd <= 0) {
        ec_log(1, "HW_OPEN", "error opening %s: %s\n", devname, strerror(errno));
        ret = EC_ERROR_HW_NO_INTERFACE;
    } else {
        ec_log(10, "HW_OPEN", "device opened successfully\n");
        
#if LIBETHERCAT_HAVE_SYS_IOCTL_H == 1
        int set_polling = phw_file->polling_mode == OSAL_TRUE ? 1 : 0;
        (void)ioctl(phw_file->fd, ETHERCAT_DEVICE_SET_POLLING, &set_polling);
#endif

        ec_log(10, "HW_OPEN", "waiting max %" PRId64 " seconds for link to come up...\n", link_timeout_sec);

        osal_timer_t timeout;
        osal_timer_init(&timeout, link_timeout_sec * 1000000000);
        while (1) {
            if (osal_timer_expired(&timeout) == OSAL_ERR_TIMEOUT) {
                ec_log(1, "HW_OPEN", "timeout waiting for link!\n");
                ret = EC_ERROR_HW_NO_LINK;
                break;
            }

            uint8_t link_state = 0;
            if (ioctl(phw_file->fd, ETHERCAT_DEVICE_GET_LINK_STATE, &link_state) >= 0) {
                if (link_state != 0) {
                    ec_log(10, "HW_OPEN", "success, got link!\n");
                    break;
                }
            }

            osal_microsleep(100000);
        }
    }

    if (ret == EC_OK) {
        phw_file->common.mtu_size = 1480;
    
        // cppcheck-suppress misra-c2012-11.3
        pframe = (ec_frame_t *)phw_file->send_frame;
        (void)memcpy(pframe->mac_dest, mac_dest, 6);
        (void)memcpy(pframe->mac_src, mac_src, 6);

        // check polling mode
#if LIBETHERCAT_HAVE_SYS_IOCTL_H == 1
        unsigned int pollval = 0;
        if (ioctl(phw_file->fd, ETHERCAT_DEVICE_GET_POLLING, &pollval) >= 0) {
            phw_file->polling_mode = pollval == 0 ? OSAL_FALSE : OSAL_TRUE;
        }
        
        if (phw_file->polling_mode == OSAL_TRUE) {
            ec_log(10, "HW_OPEN", "set rx polling timeout to %" PRIu64 " ns.\n", rx_timeout_ns);
            (void)ioctl(phw_file->fd, ETHERCAT_DEVICE_SET_POLLING_RX_TIMEOUT, &rx_timeout_ns);
        }

        unsigned int monitor = 0;
        (void)ioctl(phw_file->fd, ETHERCAT_DEVICE_MONITOR_ENABLE, &monitor);
#endif

        ec_log(10, "HW_OPEN", "%s polling mode\n", phw_file->polling_mode == OSAL_FALSE ? "not using" : "using");
    }

    if (ret == EC_OK) {
        if (phw_file->polling_mode == OSAL_FALSE) {
            phw_file->rxthreadrunning = 1;
            osal_task_attr_t attr;
            attr.policy = OSAL_SCHED_POLICY_FIFO;
            attr.priority = prio;
            attr.affinity = cpumask;
            (void)strcpy(&attr.task_name[0], "ecat.rx");
            osal_task_create(&phw_file->rxthread, &attr, hw_device_file_rx_thread, phw_file);
        }
    }

    if (ret != EC_OK) {
        if (phw_file->fd > 0) {
            close(phw_file->fd);
        }
    }

    phw_file->frames_send = 0;

    return ret;
}

//! Close hardware layer
/*!
 * \param[in]   phw         Pointer to hw handle.
 *
 * \return 0 or negative error code
 */
int hw_device_file_close(struct hw_common *phw) {
    int ret = 0;

    struct hw_file *phw_file = container_of(phw, struct hw_file, common);

    if (phw_file->polling_mode == OSAL_FALSE) {
        phw_file->rxthreadrunning = 0;
        osal_task_join(&phw_file->rxthread, NULL);
    }

    (void)close(phw_file->fd);

    return ret;
}

osal_bool_t hw_device_file_recv_internal(struct hw_file *phw_file) {
    osal_bool_t ret = OSAL_FALSE;

    // cppcheck-suppress misra-c2012-11.3
    ec_frame_t *pframe = (ec_frame_t *) &phw_file->recv_frame;

    // using tradional recv function
    osal_ssize_t bytesrx = read(phw_file->fd, pframe, ETH_FRAME_LEN);

    if (bytesrx > 0) {
        hw_process_rx_frame(&phw_file->common, pframe);
        ret = OSAL_TRUE;
    }

    return ret;
}

//! receiver thread
void *hw_device_file_rx_thread(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    struct hw_file *phw_file = (struct hw_file *) arg;
    ec_t *pec = phw_file->common.pec;

    assert(phw_file != NULL);
    
    osal_task_sched_priority_t rx_prio;
    if (osal_task_get_priority(&phw_file->rxthread, &rx_prio) != OSAL_OK) {
        rx_prio = 0;
    }

    ec_log(10, "HW_FILE_RX", "receive thread running (prio %d)\n", rx_prio);

    while (phw_file->rxthreadrunning != 0) {
        (void)hw_device_file_recv_internal(phw_file);
    }
    
    ec_log(10, "HW_FILE_RX", "receive thread stopped\n");
    
    return NULL;
}

//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   frame       Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
int hw_device_file_recv(struct hw_common *phw) {
    assert(phw != NULL);

    struct hw_file *phw_file = container_of(phw, struct hw_file, common);

    if (phw_file->polling_mode == OSAL_TRUE) {
        return EC_ERROR_HW_NOT_SUPPORTED;
    } 

    hw_device_file_recv_internal(phw_file);

    return EC_OK;
}

//! Get a free tx buffer from underlying hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   ppframe     Pointer to return frame buffer pointer.
 *
 * \return 0 or negative error code
 */
int hw_device_file_get_tx_buffer(struct hw_common *phw, ec_frame_t **ppframe) {
    assert(phw != NULL);
    assert(ppframe != NULL);

    int ret = EC_OK;
    ec_frame_t *pframe = NULL;
    struct hw_file *phw_file = container_of(phw, struct hw_file, common);
    
    // cppcheck-suppress misra-c2012-11.3
    pframe = (ec_frame_t *)phw_file->send_frame;

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
 * \param[in]   pool_type   Pool type to distinguish between high and low prio frames.
 *
 * \return 0 or negative error code
 */
int hw_device_file_send(struct hw_common *phw, ec_frame_t *pframe, pooltype_t pool_type) {
    assert(phw != NULL);
    assert(pframe != NULL);

    (void)pool_type;

    int ret = EC_OK;
    ec_t *pec = phw->pec;
    struct hw_file *phw_file = container_of(phw, struct hw_file, common);

    // no more datagrams need to be sent or no more space in frame
    osal_ssize_t bytestx = write(phw_file->fd, pframe, pframe->len);

    if ((osal_ssize_t)pframe->len != bytestx) {
        ec_log(1, "HW_TX", "got only %" PRId64 " bytes out of %d bytes "
                "through.\n", bytestx, pframe->len);

        if (bytestx == -1) {
            ec_log(1, "HW_TX", "error: %s\n", strerror(errno));
        }

        ret = EC_ERROR_HW_SEND;
    }
    
    if (ret == EC_OK) {
        phw_file->common.bytes_sent += bytestx;
        phw_file->frames_send++;
    }

    return ret;
}

//! Doing internal stuff when finished sending frames
/*!
 * \param[in]   phw         Pointer to hw handle.
 */
void hw_device_file_send_finished(struct hw_common *phw) {
    assert(phw != NULL);

    struct hw_file *phw_file = container_of(phw, struct hw_file, common);
    
    // in case of polling do receive now
    if (phw_file->polling_mode == OSAL_TRUE) {
        while (phw_file->frames_send > 0) {
            // sleep a little bit (at least packet-on-wire-duration)
            // 10 [ns] per bit on 100 Mbit/s Ethernet.
            //uint64_t packet_time = 10 * 8 * phw->bytes_sent; 
            //osal_sleep(packet_time * 0.4);
            phw_file->common.bytes_last_sent = phw_file->common.bytes_sent;
            phw_file->common.bytes_sent = 0;

            if (hw_device_file_recv_internal(phw_file) == OSAL_FALSE) {
                break;
            }

            phw_file->frames_send--;
        }
    }
}

#endif /* LIBETHERCAT_BUILD_DEVICE_FILE == 1 */
