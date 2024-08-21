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
#include <libethercat/hw_pikeos.h>

#include <libosal/io.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if LIBETHERCAT_HAVE_NET_UTIL_INET_H == 1
#include <net/util/inet.h>
#endif

#include <p4ext_vmem.h>
#include <vm_part.h>
#include <drv/app_sbuf_svc.h>
#include <drv/sbuf_common_svc.h>

// forward declarations
int hw_device_pikeos_send(struct hw_common *phw, ec_frame_t *pframe);
int hw_device_pikeos_recv(struct hw_common *phw);
void hw_device_pikeos_send_finished(struct hw_common *phw);
int hw_device_pikeos_get_tx_buffer(struct hw_common *phw, ec_frame_t **ppframe);
int hw_device_pikeos_close(struct hw_common *phw);

void *hw_device_pikeos_rx_thread(void *arg);

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 * \param[in]   prio        Priority for receiver thread.
 * \param[in]   cpu_mask    CPU mask for receiver thread.
 *
 * \return 0 or negative error code
 */
int hw_device_pikeos_open(struct hw_pikeos *phw, const osal_char_t *devname, int prio, int cpumask) {
    assert(phw != NULL);
    assert(devname != NULL);

    int ret = EC_OK;
    P4_size_t vsize;
    vm_partition_stat_t pinfo;
    P4_e_t local_retval;
    P4_address_t vaddr;

    phw->common.send            = hw_device_pikeos_send;
    phw->common.recv            = hw_device_pikeos_recv;
    phw->common.send_finished   = hw_device_pikeos_send_finished;
    phw->common.get_tx_buffer   = hw_device_pikeos_get_tx_buffer;
    phw->common.close           = hw_device_pikeos_close;
    phw->common.mtu_size        = 1480;
    phw->use_sbuf               = OSAL_TRUE;

    char *tmp;
    if ((tmp = strchr(devname, '/')) != NULL) {
        *tmp = 0;
        tmp++;

        char *act = tmp;
        do {
            char *next = strchr(tmp, ':');
            if (next != NULL) { *next = '0'; next++; }

            char *value;
            if ((value = strchr(act, '=')) != NULL) {
                *value = 0;
                value++; 

                if (strcmp(act, "use_sbuf") == 0) {
                    phw->use_sbuf = atoi(value) == 0 ? OSAL_FALSE : OSAL_TRUE;
                }
            }

            if ((next != NULL) && (*next != '0')) { act = next; } else { break; }
        } while (1);
    }

    local_retval = vm_part_pstat(VM_RESPART_MYSELF, &pinfo);
    if (local_retval != P4_E_OK) {
        ec_log(1, "HW_OPEN", "vm_part_pstat failed!\n");
        ret = EC_ERROR_UNAVAILABLE;
    }

#if !(__GNUC__ == 5) // unsupported by pikeos-4.2
    if (ret == EC_OK) {
        if ((pinfo.abilities & VM_AB_ULOCK_SHARED) == 0) {
            ec_log(1, "HW_OPEN", "sbuf mode needs VM_AB_ULOCK_SHARED!\n");
            ret = EC_ERROR_UNAVAILABLE;
        }
    }
#endif

    if (ret == EC_OK) {
        vm_file_access_mode_t flags = VM_O_RD_WR;
        if (phw->use_sbuf) { flags |= VM_O_MAP; }
        local_retval = vm_open(devname, flags, &phw->fd);
        if (local_retval != P4_E_OK) {
            ec_log(1, "HW_OPEN", "vm_open on %s failed: %d\n", devname, local_retval);
            ret = EC_ERROR_UNAVAILABLE;
        } else {
            ec_log(10, "HW_OPEN", "opened %s\n", devname);
        }
    }

    if (phw->use_sbuf) {
        ec_log(10, "HW_OPEN", "using sbuf\n");

        if (ret == EC_OK) {
            local_retval = vm_io_sbuf_init(&phw->fd, &vsize);
            if (local_retval != P4_E_OK) {
                ec_log(1, "HW_OPEN", "vm_io_sbuf_init failed\n");
                ret = EC_ERROR_UNAVAILABLE;
            }
        }

        if (ret == EC_OK) {
            vaddr = p4ext_vmem_alloc(vsize);
            if (vaddr == 0) {
                ec_log(1, "HW_OPEN", "p4ext_vmem_alloc failed\n");
                ret = EC_ERROR_UNAVAILABLE;
            }
        }

        if (ret == EC_OK) {
            local_retval = vm_io_sbuf_map(&phw->fd, vaddr, &phw->sbuf);
            if (local_retval != P4_E_OK) {
                ec_log(1, "HW_OPEN", "vm_io_sbuf_map failed\n");
                ret = EC_ERROR_UNAVAILABLE;
            }
        }
    } else {
        ec_log(10, "HW_OPEN", "using vmread/vmwrite\n");
    }

    if (ret == EC_OK) {
        phw->rxthreadrunning = 1;
        osal_task_attr_t attr;
        attr.policy = OSAL_SCHED_POLICY_FIFO;
        attr.priority = prio;
        attr.affinity = cpumask;
        (void)strcpy(&attr.task_name[0], "ecat.rx");
        osal_task_create(&phw->rxthread, &attr, hw_device_pikeos_rx_thread, phw);
    }

    return ret;
}

//! Close hardware layer
/*!
 * \param[in]   phw         Pointer to hw handle.
 *
 * \return 0 or negative error code
 */
int hw_device_pikeos_close(struct hw_common *phw) {
    int ret = 0;

    struct hw_pikeos *phw_pikeos = container_of(phw, struct hw_pikeos, common);
    vm_close(&phw_pikeos->fd);

    // TODO some more close of sbuf things???

    return ret;
}

//! receiver thread
void *hw_device_pikeos_rx_thread(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    struct hw_pikeos *phw_pikeos = (struct hw_pikeos *) arg;

    assert(phw_pikeos != NULL);
    
    osal_task_sched_priority_t rx_prio;
    if (osal_task_get_priority(&phw_pikeos->rxthread, &rx_prio) != OSAL_OK) {
        rx_prio = 0;
    }

    ec_log(10, "HW_PIKEOS_RX", "receive thread running (prio %d)\n", rx_prio);

    while (phw_pikeos->rxthreadrunning != 0) {
        hw_device_pikeos_recv(&phw_pikeos->common);
    }
    
    ec_log(10, "HW_PIKEOS_RX", "receive thread stopped\n");
    
    return NULL;
}


//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 *
 * \return 0 or negative error code
 */
int hw_device_pikeos_recv(struct hw_common *phw) {
    assert(phw != NULL);
    
    struct hw_pikeos *phw_pikeos = container_of(phw, struct hw_pikeos, common);

    int ret = EC_OK;
    ec_frame_t *pframe = NULL;

    if (phw_pikeos->use_sbuf == OSAL_TRUE) {
        // get a full rxbuffer from RX ring (dontwait=0 -> WAITING)
        vm_io_buf_id_t rxbuf = vm_io_sbuf_rx_get(&phw_pikeos->sbuf, 0);
        if (rxbuf == VM_IO_BUF_ID_INVALID) {
            p4_sleep(P4_NSEC(1000000));
            ret = EC_ERROR_UNAVAILABLE;
        }

        if (ret == EC_OK) {
            pframe = (ec_frame_t *)vm_io_sbuf_rx_buf_addr(&phw_pikeos->sbuf, rxbuf);

            if (pframe == NULL) {
                ret = EC_ERROR_UNAVAILABLE;
            }
        }

        if (ret == EC_OK) {
            int32_t bytesrx = vm_io_sbuf_rx_buf_size(&phw_pikeos->sbuf, rxbuf);
            if (bytesrx > 0) {
                hw_process_rx_frame(phw, pframe);
            }

            vm_io_sbuf_rx_free(&phw_pikeos->sbuf, rxbuf);
        }
    } else { /* use_sbuf == OSAL_FALSE */
        pframe = (ec_frame_t *)&phw_pikeos->recv_frame[0];
        P4_size_t read_size;
        P4_e_t localret = vm_read(&phw_pikeos->fd, pframe, ETH_FRAME_LEN, &read_size);

        if (localret != P4_E_OK) {
            ret = EC_ERROR_UNAVAILABLE;
        } else {
            if (read_size > 0) {
                hw_process_rx_frame(phw, pframe);
            }
        }
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
int hw_device_pikeos_get_tx_buffer(struct hw_common *phw, ec_frame_t **ppframe) {
    assert(phw != NULL);
    assert(ppframe != NULL);

    struct hw_pikeos *phw_pikeos = container_of(phw, struct hw_pikeos, common);

    int ret = EC_ERROR_UNAVAILABLE;
    ec_frame_t *pframe = NULL;
    
    static const osal_uint8_t mac_dest[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const osal_uint8_t mac_src[] = {0x00, 0x30, 0x64, 0x0f, 0x83, 0x35};

    // cppcheck-suppress misra-c2012-11.3
    pframe = (ec_frame_t *)phw_pikeos->send_frame;

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
int hw_device_pikeos_send(struct hw_common *phw, ec_frame_t *pframe) {
    assert(phw != NULL);
    assert(pframe != NULL);

    struct hw_pikeos *phw_pikeos = container_of(phw, struct hw_pikeos, common);

    int ret = EC_OK;
    void* txbuf = NULL;

    if (phw_pikeos->use_sbuf == OSAL_TRUE) {
        // get an empty tx buffer from TX ring (dontwait)
        vm_io_buf_id_t sbuftx = vm_io_sbuf_tx_alloc(&phw_pikeos->sbuf, 1);
        if (sbuftx == VM_IO_BUF_ID_INVALID) {
            ret = EC_ERROR_UNAVAILABLE;
        }

        if (ret == EC_OK) {
            txbuf = (void*)vm_io_sbuf_tx_buf_addr(&phw_pikeos->sbuf, sbuftx);

            if (txbuf == NULL) {
                ret = EC_ERROR_UNAVAILABLE;
            }
        }

        if (ret == EC_OK) {
            (void)memcpy(txbuf, pframe, pframe->len);
            vm_io_sbuf_tx_ready(&phw_pikeos->sbuf, sbuftx, pframe->len);
        }
    } else { /* use_sbuf == OSAL_FALSE */
        P4_size_t written_size;
        P4_e_t localret = vm_write(&phw_pikeos->fd, pframe, pframe->len, &written_size);

        if (localret != P4_E_OK) {
            ret = EC_ERROR_UNAVAILABLE;
        }

    }

    return ret;
}

//! Doing internal stuff when finished sending frames
/*!
 * \param[in]   phw         Pointer to hw handle.
 */
void hw_device_pikeos_send_finished(struct hw_common *phw) {
    (void)phw;
}

