//! ethercat master
/*!
 * author: Robert Burger
 *
 * $Id$
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <limits.h>
#include <assert.h>

#include "libethercat/ec.h"
#include "libethercat/slave.h"
#include "libethercat/mbx.h"
#include "libethercat/coe.h"
#include "libethercat/dc.h"
#include "libethercat/memory.h"
#include "libethercat/eeprom.h"
#include "libethercat/error_codes.h"

#define DC_DCSOFF_SAMPLES 1000u

#ifndef max
#define max(a, b) \
    ((a) > (b) ? (a) : (b))
#endif

static void default_log_func(int lvl, void* user, const osal_char_t *format, ...){
    (void)lvl;
    (void)user;

    va_list args;                   // cppcheck-suppress misra-c2012-17.1
    va_start(args, format);         // cppcheck-suppress misra-c2012-17.1
    (void)vfprintf(stderr, format, args);
    va_end(args);                   // cppcheck-suppress misra-c2012-17.1
}

void *ec_log_func_user = NULL;
void (*ec_log_func)(int lvl, void *user, const osal_char_t *format, ...) = default_log_func;

void ec_log(int lvl, const osal_char_t *pre, const osal_char_t *format, ...) {
    if (ec_log_func != NULL) {
        osal_char_t buf[512];
        osal_char_t *tmp = &buf[0];

        // format argument list
        va_list args;                   // cppcheck-suppress misra-c2012-17.1
        va_start(args, format);         // cppcheck-suppress misra-c2012-17.1
        int ret = snprintf(tmp, 512, "%-20.20s: ", pre);
        (void)vsnprintf(&tmp[ret], 512-ret, format, args);
        va_end(args);                   // cppcheck-suppress misra-c2012-17.1

        ec_log_func(lvl, ec_log_func_user, buf);
    }
}

//! create process data groups
/*!
 * \param pec ethercat master pointer
 * \param pd_group_cnt number of groups to create
 * \return 0 on success
 */
int ec_create_pd_groups(ec_t *pec, osal_uint32_t pd_group_cnt) {
    assert(pec != NULL);
    assert(pd_group_cnt < LEC_MAX_GROUPS);

    (void)ec_destroy_pd_groups(pec);

    pec->pd_group_cnt = pd_group_cnt;
    // cppcheck-suppress misra-c2012-21.3
    for (osal_uint16_t i = 0; i < pec->pd_group_cnt; ++i) {
        pec->pd_groups[i].log       = 0x10000u * ((osal_uint32_t)i+1u);
        pec->pd_groups[i].log_len   = 0u;
        pec->pd_groups[i].pd        = NULL;
        pec->pd_groups[i].pdout_len = 0u;
        pec->pd_groups[i].pdin_len  = 0u;
        pec->pd_groups[i].use_lrw   = 1;
        pec->pd_groups[i].recv_missed = 0;
    }

    return 0;
}

//! destroy process data groups
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_destroy_pd_groups(ec_t *pec) {
    assert(pec != NULL);

    if (pec->pd_groups != NULL) {
        for (osal_uint16_t i = 0; i < pec->pd_group_cnt; ++i) {
            // cppcheck-suppress misra-c2012-21.3
            free_resource(pec->pd_groups[i].pd);
        }
    }

    pec->pd_group_cnt = 0;

    return 0;
}

static const osal_char_t *get_state_string(ec_state_t state) {
    static const osal_char_t state_string_boot[]    = "EC_STATE_BOOT";
    static const osal_char_t state_string_init[]    = "EC_STATE_INIT";
    static const osal_char_t state_string_preop[]   = "EC_STATE_PREOP";
    static const osal_char_t state_string_safeop[]  = "EC_STATE_SAFEOP";
    static const osal_char_t state_string_op[]      = "EC_STATE_OP";
    static const osal_char_t state_string_unknown[] = "EC_STATE_UNKNOWN";

    const osal_char_t *ret;

    if (state == EC_STATE_BOOT) {
        ret = state_string_boot;
    } else if (state == EC_STATE_INIT) {
        ret = state_string_init;
    } else if (state == EC_STATE_PREOP) {
        ret = state_string_preop;
    } else if (state == EC_STATE_SAFEOP) {
        ret = state_string_safeop;
    } else if (state == EC_STATE_OP) {
        ret = state_string_op;
    } else {
        ret = state_string_unknown;
    }

    return ret;
}

static void ec_create_logical_mapping_lrw(ec_t *pec, osal_uint32_t group) {
    assert(pec != NULL);
    assert(group < pec->pd_group_cnt);

    osal_uint32_t i;
    osal_uint32_t k;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    pd->pdout_len = 0;
    pd->pdin_len = 0;
    pd->pd_lrw_len = 0;
    pd->wkc_expected = 0;

    for (i = 0; i < pec->slave_cnt; ++i) {
        ec_slave_t *slv = &pec->slaves[i];
        int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

        if (slv->assigned_pd_group != (int)group) {
            continue;
        }

        osal_size_t slv_pdin_len = 0u;
        osal_size_t slv_pdout_len = 0u;

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if ((slv->sm[k].flags & 0x00000004u) != 0u) {
                slv_pdout_len += slv->sm[k].len; // outputs
            } else  {
                slv_pdin_len += slv->sm[k].len;  // inputs
            }
        }

        if (slv->eeprom.mbx_supported != 0u) {
            // add state of sync manager read mailbox
            slv_pdin_len += 1u;
        }
        
        osal_size_t max_len = max(slv_pdout_len, slv_pdin_len);

        // add to pd lengths
        pd->pdout_len += max_len;
        pd->pdin_len  += max_len;

        // outputs and inputs are lying in the same memory
        // so we only need the bigger size
        pd->pd_lrw_len += max_len;
    }

    ec_log(10, "CREATE_LOGICAL_MAPPING", "group %2d: pd out 0x%08X "
            "%3d bytes, in 0x%08X %3d bytes, lrw windows %3d bytes\n", 
            group, pd->log, pd->pdout_len, pd->log + pd->pdout_len, 
            pd->pdin_len, pd->pd_lrw_len);

    pd->log_len = pd->pdout_len + pd->pdin_len;
    // cppcheck-suppress misra-c2012-21.3
    pd->pd = (osal_uint8_t *)ec_malloc(pd->log_len);
    (void)memset(pd->pd, 0, pd->log_len);

    osal_uint8_t *pdout = pd->pd;
    osal_uint8_t *pdin = &pd->pd[pd->pdout_len];

    osal_uint32_t log_base = pd->log;

    for (i = 0; i < pec->slave_cnt; ++i) {
        ec_slave_t *slv = &pec->slaves[i];
        osal_uint32_t start_sm = 0u;
        if (slv->eeprom.mbx_supported != 0u) { start_sm = 2u; }

        if (slv->assigned_pd_group != (int)group) {
            continue;
        }

        osal_uint32_t fmmu_next = 0u;
        osal_uint16_t wkc_expected = 0u;

        osal_uint8_t *tmp_pdout = pdout;
        osal_uint8_t *tmp_pdin = pdin;
        osal_uint32_t log_base_out = log_base;
        osal_uint32_t log_base_in = log_base;

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if ((!slv->sm[k].len)) {
                continue; // empty 
            }

            if ((slv->sm[k].flags & 0x00000004u) != 0u) {
                if (fmmu_next < slv->fmmu_ch) {
                    slv->fmmu[fmmu_next].log = log_base_out;
                    slv->fmmu[fmmu_next].log_len = slv->sm[k].len;
                    slv->fmmu[fmmu_next].log_bit_stop = 7;
                    slv->fmmu[fmmu_next].phys = slv->sm[k].adr;
                    slv->fmmu[fmmu_next].type = 2;
                    slv->fmmu[fmmu_next].active = 1;

                    if (!slv->pdout.len) {
                        slv->pdout.pd = tmp_pdout; 
                        slv->pdout.len = slv->sm[k].len;
                    } else {
                        slv->pdout.len += slv->sm[k].len;
                    }

                    wkc_expected |= 2u;

                    osal_uint32_t z;
                    off_t pdoff = 0;
                    for (z = 0u; z < slv->subdev_cnt; ++z) {
                        slv->subdevs[z].pdout.pd = &pdout[pdoff];
                        pdoff += slv->subdevs[z].pdout.len;
                    }
                }

                tmp_pdout = &tmp_pdout[slv->sm[k].len];
                log_base_out += slv->sm[k].len;
            } else {
                if (fmmu_next < slv->fmmu_ch) {
                    slv->fmmu[fmmu_next].log = log_base_in;
                    slv->fmmu[fmmu_next].log_len = slv->sm[k].len;
                    slv->fmmu[fmmu_next].log_bit_stop = 7;
                    slv->fmmu[fmmu_next].phys = slv->sm[k].adr;
                    slv->fmmu[fmmu_next].type = 1;
                    slv->fmmu[fmmu_next].active = 1;

                    if (!slv->pdin.len) {
                        slv->pdin.pd = tmp_pdin; 
                        slv->pdin.len = slv->sm[k].len;
                    } else {
                        slv->pdin.len += slv->sm[k].len;
                    }

                    wkc_expected |= 1u;

                    osal_uint32_t z;
                    off_t pdoff = 0;
                    for (z = 0; z < slv->subdev_cnt; ++z) {
                        slv->subdevs[z].pdin.pd = &pdin[pdoff];
                        pdoff += slv->subdevs[z].pdin.len;
                    }
                }

                tmp_pdin = &tmp_pdin[slv->sm[k].len];
                log_base_in += slv->sm[k].len;
            }

            fmmu_next++;
        }

        if (slv->eeprom.mbx_supported != 0u) {
            if (fmmu_next < slv->fmmu_ch) {
                // add state of sync manager read mailbox
                slv->fmmu[fmmu_next].log = log_base_in;
                slv->fmmu[fmmu_next].log_len = 1;
                slv->fmmu[fmmu_next].log_bit_start = 0;
                slv->fmmu[fmmu_next].log_bit_stop = 7;
                slv->fmmu[fmmu_next].phys = EC_REG_SM1STAT;
                slv->fmmu[fmmu_next].type = 1;
                slv->fmmu[fmmu_next].active = 1;

                if (!slv->pdin.len) {
                    slv->pdin.pd = tmp_pdin; 
                    slv->pdin.len = 1u;
                } else {
                    slv->pdin.len += 1u;
                }

                slv->mbx.sm_state = tmp_pdin;

                wkc_expected |= 1u;
            }

            tmp_pdin = &tmp_pdin[1];
            log_base_in += 1u;
        }

        pdin = &pdin[max(slv->pdin.len, slv->pdout.len)];
        pdout = &pdout[max(slv->pdin.len, slv->pdout.len)];
        log_base = max(log_base_in, log_base_out);
        pd->wkc_expected += wkc_expected;
    }
}

static void ec_create_logical_mapping(ec_t *pec, osal_uint32_t group) {
    assert(pec != NULL);
    assert(group < pec->pd_group_cnt);

    osal_uint32_t i;
    osal_uint32_t k;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    pd->pdout_len = 0;
    pd->pdin_len = 0;
    pd->wkc_expected = 0u;

    for (i = 0; i < pec->slave_cnt; ++i) {
        ec_slave_t *slv = &pec->slaves[i];
        int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

        if (slv->assigned_pd_group != (int)group) {
            continue;
        }

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if ((slv->sm[k].flags & 0x00000004u) != 0u) {
                pd->pdout_len += slv->sm[k].len; // outputs
            } else  {
                pd->pdin_len += slv->sm[k].len;  // inputs
            }
        }

        if (slv->eeprom.mbx_supported != 0u) {
            // add state of sync manager read mailbox
            pd->pdin_len += 1u; 
        }
    }

    ec_log(10, "CREATE_LOGICAL_MAPPING", "group %2d: pd out 0x%08X "
            "%3d bytes, in 0x%08X %3d bytes\n", group, pd->log, 
            pd->pdout_len, pd->log + pd->pdout_len, pd->pdin_len);

    pd->log_len = pd->pdout_len + pd->pdin_len;
    // cppcheck-suppress misra-c2012-21.3
    pd->pd = (osal_uint8_t *)ec_malloc(pd->log_len);
    (void)memset(pd->pd, 0, pd->log_len);

    osal_uint8_t *pdout = pd->pd;
    osal_uint8_t *pdin = &pd->pd[pd->pdout_len];

    osal_uint32_t log_base_out = pd->log;
    osal_uint32_t log_base_in = pd->log + pd->pdout_len;

    for (i = 0; i < pec->slave_cnt; ++i) {
        ec_slave_t *slv = &pec->slaves[i];
        int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

        if (slv->assigned_pd_group != (int)group) {
            continue;
        }

        osal_uint32_t fmmu_next = 0u;
        osal_uint32_t wkc_expected = 0u;

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if ((!slv->sm[k].len)) {
                continue; // empty 
            }

            if ((slv->sm[k].flags & 0x00000004u) != 0u) {
                if (fmmu_next < slv->fmmu_ch) {
                    slv->fmmu[fmmu_next].log = log_base_out;
                    slv->fmmu[fmmu_next].log_len = slv->sm[k].len;
                    slv->fmmu[fmmu_next].log_bit_stop = 7;
                    slv->fmmu[fmmu_next].phys = slv->sm[k].adr;
                    slv->fmmu[fmmu_next].type = 2;
                    slv->fmmu[fmmu_next].active = 1;

                    if (!slv->pdout.len) {
                        slv->pdout.pd = pdout; 
                        slv->pdout.len = slv->sm[k].len;
                    } else {
                        slv->pdout.len += slv->sm[k].len;
                    }

                    wkc_expected |= 2u;

                    osal_uint32_t z;
                    osal_uint32_t pdoff = 0u;
                    for (z = 0u; z < slv->subdev_cnt; ++z) {
                        slv->subdevs[z].pdout.pd = &pdout[pdoff];
                        pdoff += slv->subdevs[z].pdout.len;
                    }
                }

                pdout = &pdout[slv->sm[k].len];
                log_base_out += slv->sm[k].len;
            } else {
                if (fmmu_next < slv->fmmu_ch) {
                    slv->fmmu[fmmu_next].log = log_base_in;
                    slv->fmmu[fmmu_next].log_len = slv->sm[k].len;
                    slv->fmmu[fmmu_next].log_bit_stop = 7;
                    slv->fmmu[fmmu_next].phys = slv->sm[k].adr;
                    slv->fmmu[fmmu_next].type = 1;
                    slv->fmmu[fmmu_next].active = 1;

                    if (!slv->pdin.len) {
                        slv->pdin.pd = pdin; 
                        slv->pdin.len = slv->sm[k].len;
                    } else {
                        slv->pdin.len += slv->sm[k].len;
                    }

                    wkc_expected |= 1u;

                    osal_uint32_t z;
                    osal_uint32_t pdoff = 0u;
                    for (z = 0u; z < slv->subdev_cnt; ++z) {
                        slv->subdevs[z].pdin.pd = &pdin[pdoff];
                        pdoff += slv->subdevs[z].pdin.len;
                    }
                }

                pdin = &pdin[slv->sm[k].len];
                log_base_in += slv->sm[k].len;
            }

            fmmu_next++;
        }

        if (slv->eeprom.mbx_supported != 0u) {
            if (fmmu_next < slv->fmmu_ch) {
                // add state of sync manager read mailbox
                slv->fmmu[fmmu_next].log = log_base_in;
                slv->fmmu[fmmu_next].log_len = 1;
                slv->fmmu[fmmu_next].log_bit_start = 0;
                slv->fmmu[fmmu_next].log_bit_stop = 7;
                slv->fmmu[fmmu_next].phys = EC_REG_SM1STAT;
                slv->fmmu[fmmu_next].type = 1;
                slv->fmmu[fmmu_next].active = 1;

                if (!slv->pdin.len) {
                    slv->pdin.pd = pdin; 
                    slv->pdin.len = 1;
                } else {
                    slv->pdin.len += 1u;
                }

                slv->mbx.sm_state = pdin;

                wkc_expected |= 1u;
            }

            pdin = &pdin[1];
            log_base_in += 1u;
        }

        pd->wkc_expected += wkc_expected;
    }
}

static void *prepare_state_transition_wrapper(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    worker_arg_t *tmp = (worker_arg_t *)arg;
    
    ec_log(100, get_state_string(tmp->state), "prepare state transition for slave %d\n", tmp->slave);
    if (ec_slave_prepare_state_transition(tmp->pec, tmp->slave, tmp->state) != EC_OK) {
        ec_log(1, __func__, "ec_slave_prepare_state_transition failed!\n");
    }

    ec_log(100, get_state_string(tmp->state), "generate mapping for slave %d\n", tmp->slave);
    if (ec_slave_generate_mapping(tmp->pec, tmp->slave) != EC_OK) {
        ec_log(1, __func__, "ec_slave_generate_mapping failed!\n");
    }

    return NULL;
}

static void *set_state_wrapper(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    worker_arg_t *tmp = (worker_arg_t *)arg;

    ec_log(100, get_state_string(tmp->state), "setting state for slave %d\n", tmp->slave);

    if (ec_slave_state_transition(tmp->pec, tmp->slave, tmp->state) != EC_OK) {
        ec_log(1, __func__, "ec_slave_state_transition failed!\n"); 
    }

    return NULL;
}

//! loop over all slaves and prepare state transition
/*! 
 * \param pec ethercat master pointer
 * \param state new state to set
 */
static void ec_prepare_state_transition_loop(ec_t *pec, ec_state_t state) {
    assert(pec != NULL);

    if (pec->threaded_startup != 0) {
        for (osal_uint32_t slave = 0u; slave < pec->slave_cnt; ++slave) {
            if (pec->slaves[slave].assigned_pd_group != -1) {
                pec->slaves[slave].worker_arg.pec   = pec;
                pec->slaves[slave].worker_arg.slave = slave;
                pec->slaves[slave].worker_arg.state = state;

                osal_task_attr_t attr;
                attr.priority = 0;
                attr.affinity = 0xFF;
                snprintf(&attr.task_name[0], TASK_NAME_LEN, "ecat.worker%d", slave);
                osal_task_create(&(pec->slaves[slave].worker_tid), &attr, 
                        prepare_state_transition_wrapper, 
                        &(pec->slaves[slave].worker_arg));
            }
        }

        for (osal_uint32_t slave = 0u; slave < pec->slave_cnt; ++slave) {
            if (pec->slaves[slave].assigned_pd_group != -1) {
                osal_task_join(&pec->slaves[slave].worker_tid, NULL);
            }
        }
    } else { 
        for (osal_uint32_t slave = 0u; slave < pec->slave_cnt; ++slave) {                  
            if (pec->slaves[slave].assigned_pd_group != -1) {
                ec_log(100, get_state_string(state), "prepare state transition for slave %d\n", slave);
                int ret = ec_slave_prepare_state_transition(pec, slave, state);
                if (ret != EC_OK) {
                    ec_log(1, __func__, "ec_slave_prepare_state_transition failed with %d\n", ret);
                }

                ec_log(100, get_state_string(state), "generate mapping for slave %d\n", slave);
                ret = ec_slave_generate_mapping(pec, slave);
                if (ret != EC_OK) {
                    ec_log(1, __func__, "ec_slave_generate_mapping failed with %d\n", ret);
                }
            }
        }
    }
}

//! loop over all slaves and set state
/*! 
 * \param pec ethercat master pointer
 * \param state new state to set
 * \param with_group if set, only slaves with assigned group are processed
 */
static void ec_state_transition_loop(ec_t *pec, ec_state_t state, osal_uint8_t with_group) {
    assert(pec != NULL);

    if (pec->threaded_startup != 0) {
        for (osal_uint32_t slave = 0u; slave < pec->slave_cnt; ++slave) {
            if ((with_group == 0u) || (pec->slaves[slave].assigned_pd_group != -1)) {
                pec->slaves[slave].worker_arg.pec   = pec;
                pec->slaves[slave].worker_arg.slave = slave;
                pec->slaves[slave].worker_arg.state = state;

                osal_task_attr_t attr;
                attr.priority = 0;
                attr.affinity = 0xFF;
                snprintf(&attr.task_name[0], TASK_NAME_LEN, "ecat.worker%d", slave);
                osal_task_create(&(pec->slaves[slave].worker_tid), &attr, 
                        set_state_wrapper, 
                        &(pec->slaves[slave].worker_arg));
            }
        }

        for (osal_uint32_t slave = 0u; slave < pec->slave_cnt; ++slave) {
            if ((with_group == 0u) || (pec->slaves[slave].assigned_pd_group != -1)) {
                osal_task_join(&pec->slaves[slave].worker_tid, NULL);
            }
        }
    } else {
        for (osal_uint32_t slave = 0u; slave < pec->slave_cnt; ++slave) {
            ec_log(100, get_state_string(state), "slave %d, with_group %d, assigned %d\n", 
                    slave, with_group, pec->slaves[slave].assigned_pd_group);

            if ((with_group == 0u) || (pec->slaves[slave].assigned_pd_group != -1)) {
                ec_log(100, get_state_string(state), "setting state for slave %d\n", slave);
                if (ec_slave_state_transition(pec, slave, state) != EC_OK) {
                    ec_log(1, __func__, "ec_slave_state_transition failed!\n");
                }
            }
        }
    }
}
//! scan ethercat bus for slaves and create strucutres
/*! 
 * \param pec ethercat master pointer
 */
static void ec_scan(ec_t *pec) {
    assert(pec != NULL);

    osal_uint16_t fixed = 1000u;
    osal_uint16_t wkc = 0u;
    osal_uint16_t val = 0u;
    osal_uint16_t i;

    ec_state_t init_state = EC_STATE_INIT | EC_STATE_RESET;
    (void)ec_bwr(pec, EC_REG_ALCTL, &init_state, sizeof(init_state), &wkc); 

    if (pec->slaves != NULL) {
        osal_uint16_t cnt = pec->slave_cnt;

        // free resources
        for (i = 0; i < cnt; ++i) {
            ec_slave_free(pec, i);
        }

        pec->slave_cnt = 0;
    }
    
    // allocating slave structures
    int ret = ec_brd(pec, EC_REG_TYPE, (osal_uint8_t *)&val, sizeof(val), &wkc); 
    if (ret != EC_OK) {
        ec_log(1, __func__, "broadcast read of slave types failed with %d\n", ret);
    } else {
        assert(wkc < LEC_MAX_SLAVES);

        pec->slave_cnt = wkc;

        for (i = 0; i < pec->slave_cnt; ++i) {
            int16_t auto_inc = (int16_t)-1 * (int16_t)i;

            int local_ret = ec_aprd(pec, auto_inc, EC_REG_TYPE, (osal_uint8_t *)&val, sizeof(val), &wkc);

            if (local_ret == EC_OK) {
                if (wkc == 0u) {
                    break;  // break here, cause there seems to be no more slave
                }

                ec_log(100, __func__, "slave %2d: auto inc %3d, fixed %d\n", 
                        i, auto_inc, fixed);

                pec->slaves[i].assigned_pd_group = -1;
                pec->slaves[i].auto_inc_address = auto_inc;
                pec->slaves[i].fixed_address = fixed;
                pec->slaves[i].dc.use_dc = 1;
                pec->slaves[i].sm_set_by_user = 0;
                pec->slaves[i].subdev_cnt = 0;
                pec->slaves[i].subdevs = NULL;
                pec->slaves[i].eeprom.read_eeprom = 0;
                TAILQ_INIT(&pec->slaves[i].eeprom.txpdos);
                TAILQ_INIT(&pec->slaves[i].eeprom.rxpdos);
                LIST_INIT(&pec->slaves[i].init_cmds);

                local_ret = ec_apwr(pec, auto_inc, EC_REG_STADR, (osal_uint8_t *)&fixed, sizeof(fixed), &wkc); 
                if ((local_ret != EC_OK) || (wkc == 0u)) {
                    ec_log(1, __func__, "slave %2d: error writing fixed address %d\n", i, fixed);
                }

                // set eeprom to pdi, some slaves need this
                (void)ec_eeprom_to_pdi(pec, i);
                init_state = EC_STATE_INIT | EC_STATE_RESET;
                local_ret = ec_fpwr(pec, fixed, EC_REG_ALCTL, &init_state, sizeof(init_state), &wkc); 
                if (local_ret != EC_OK) {
                    ec_log(1, __func__, "salve %2d: reading al control failed with %d\n", i, local_ret);
                }

                fixed++;
            } else {
                ec_log(1, __func__, "ec_aprd %d returned %d\n", auto_inc, local_ret);
            }
        }

        ec_log(10, __func__, "found %d ethercat slaves\n", i);

        for (osal_uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
            ec_slave_ptr(slv, pec, slave); 
            slv->link_cnt = 0;
            slv->active_ports = 0;

            osal_uint16_t topology = 0;
            int local_ret = ec_fprd(pec, slv->fixed_address, EC_REG_DLSTAT, &topology, sizeof(topology), &wkc);

            if (local_ret == EC_OK) {
                // check if port is open and communication established
#define active_port(port) \
                if ( (topology & ((osal_uint16_t)3u << (8u + (2u * (port)))) ) == ((osal_uint16_t)2u << (8u + (2u * (port)))) ) { \
                    slv->link_cnt++; slv->active_ports |= (osal_uint8_t)1u<<(port); }

                for (osal_uint16_t port = 0u; port < 4u; ++port) {
                    active_port(port);
                }

                // read out physical type
                local_ret = ec_fprd(pec, slv->fixed_address, EC_REG_PORTDES, &slv->ptype, sizeof(slv->ptype), &wkc);
            }

            if (local_ret == EC_OK) {
                // 0 = no links, not possible 
                // 1 =  1 link , end of line 
                // 2 =  2 links, one before and one after 
                // 3 =  3 links, split point 
                // 4 =  4 links, cross point 

                // search for parent
                slv->parent = -1; // parent is master at beginning
                if (slave >= 1u) {
                    int16_t topoc = 0u;
                    int tmp_slave = (int)slave - 1;

                    do {
                        topology = pec->slaves[tmp_slave].link_cnt;
                        if (topology == 1u) {
                            topoc--;    // endpoint found
                        } else if (topology == 3u) {
                            topoc++;    // split found
                        } else if (topology == 4u) {
                            topoc += 2; // cross found
                        } else if (((topoc >= 0) && (topology > 1u)) || (tmp_slave == 0)) { 
                            slv->parent = tmp_slave; // parent found
                            tmp_slave = 0;
                        } else {}
                        tmp_slave--;
                    }
                    while (tmp_slave >= 0);
                }

                ec_log(100, __func__, "slave %2d has parent %d\n", slave, slv->parent);
            }
        }
    }
}

//! set state on ethercat bus
/*! 
 * \param pec ethercat master pointer
 * \param state new ethercat state
 * \return 0 on success
 */
int ec_set_state(ec_t *pec, ec_state_t state) {
    assert(pec != NULL);
    int ret = EC_OK;

    ec_log(10, __func__, "switch to from %s to %s\n", 
            get_state_string(pec->master_state), get_state_string(state));

    pec->state_transition_pending = 1;

    // generate transition
    ec_state_transition_t transition = ((pec->master_state & EC_STATE_MASK) << 8) | 
        (state & EC_STATE_MASK); 
            
    switch (transition) {
        case BOOT_2_INIT:
        case BOOT_2_PREOP:
        case BOOT_2_SAFEOP:
        case BOOT_2_OP: 
        case INIT_2_INIT:
        case UNKNOWN_2_INIT:
        case UNKNOWN_2_PREOP:
        case UNKNOWN_2_SAFEOP:
        case UNKNOWN_2_OP:
            // ====> switch to INIT stuff
            ec_state_transition_loop(pec, EC_STATE_INIT, 0);
            ec_scan(pec);
            ec_state_transition_loop(pec, EC_STATE_INIT, 0);

            if (state == EC_STATE_INIT) {
                break;
            }
            // cppcheck-suppress misra-c2012-16.3
        case INIT_2_OP: 
        case INIT_2_SAFEOP:
        case INIT_2_PREOP:
        case PREOP_2_PREOP:
            // ====> switch to PREOP stuff
            ec_state_transition_loop(pec, EC_STATE_PREOP, 0);

            if (state == EC_STATE_PREOP) {
                break;
            }
            // cppcheck-suppress misra-c2012-16.3
        case PREOP_2_SAFEOP:
        case PREOP_2_OP: 
        case SAFEOP_2_SAFEOP:
            // ====> switch to SAFEOP stuff
            ret = ec_dc_config(pec);
            if (ret != EC_OK) {
                ec_log(1, __func__, "configuring distributed clocks failed with %d\n", ret);
            }

            // sending first time dc
            if (ec_send_distributed_clocks_sync(pec) == EC_OK) {
                osal_timer_t dc_timeout;
                osal_timer_init(&dc_timeout, 10000000);
                (void)ec_receive_distributed_clocks_sync(pec, &dc_timeout);
            } else {
                ec_log(1, __func__, "was not able to send first dc frame\n");
            }

            ec_prepare_state_transition_loop(pec, EC_STATE_SAFEOP);

            // ====> create logical mapping for cyclic operation
            for (osal_uint16_t group = 0u; group < pec->pd_group_cnt; ++group) {
                if (pec->pd_groups[group].use_lrw != 0) {
                    ec_create_logical_mapping_lrw(pec, group);
                } else {
                    ec_create_logical_mapping(pec, group);
                }
            }
            
            ec_state_transition_loop(pec, EC_STATE_SAFEOP, 1);
    
            if (state == EC_STATE_SAFEOP) {
                break;
            }
            // cppcheck-suppress misra-c2012-16.3
        case SAFEOP_2_OP: 
        case OP_2_OP: {
            // ====> switch to OP stuff
            ec_state_transition_loop(pec, EC_STATE_OP, 1);
            break;
        }
        case OP_2_BOOT:
        case OP_2_INIT:
        case OP_2_PREOP:
        case OP_2_SAFEOP:
            ec_log(10, __func__, "switching to SAFEOP\n");
            ec_state_transition_loop(pec, EC_STATE_SAFEOP, 0);
    
            pec->master_state = EC_STATE_SAFEOP;

            if (state == EC_STATE_SAFEOP) {
                break;
            }
            // cppcheck-suppress misra-c2012-16.3
        case SAFEOP_2_BOOT:
        case SAFEOP_2_INIT:
        case SAFEOP_2_PREOP:
            ec_log(10, __func__, "switching to PREOP\n");
            ec_state_transition_loop(pec, EC_STATE_PREOP, 0);

            // reset dc
            pec->dc.dc_time         = 0;
            pec->dc.rtc_time        = 0;
            pec->dc.act_diff        = 0;
            pec->tx_sync            = 1;

            pec->master_state = EC_STATE_PREOP;

            if (state == EC_STATE_PREOP) {
                break;
            }
            // cppcheck-suppress misra-c2012-16.3
        case PREOP_2_BOOT:
        case PREOP_2_INIT:
            ec_log(10, __func__, "switching to INIT\n");
            ec_state_transition_loop(pec, EC_STATE_INIT, 0);
            pec->master_state = EC_STATE_INIT;
            ec_log(10, __func__, "doing rescan\n");
            ec_scan(pec);
            ec_state_transition_loop(pec, EC_STATE_INIT, 0);

            if (state == EC_STATE_INIT) {
                break;
            }
            // cppcheck-suppress misra-c2012-16.3
        case INIT_2_BOOT:
        case BOOT_2_BOOT:
            ec_state_transition_loop(pec, EC_STATE_BOOT, 0);
            break;
        default:
            break;
    };
        
    pec->master_state = state;
    pec->state_transition_pending = 0;

    return pec->master_state;
}

//static pthread_t ec_tx;
//static void *ec_tx_thread(void *arg) {
//    ec_t *pec = (ec_t *)arg;
//
//    while (1) {
//        hw_tx(pec->phw);
//        ec_sleep(1000000);
//    }
//
//    return 0;
//}

//! open ethercat master
/*!
 * \param ppec return value for ethercat master pointer
 * \param ifname ethercat master interface name
 * \param prio receive thread priority
 * \param cpumask receive thread cpumask
 * \param eeprom_log log eeprom to stdout
 * \return 0 on succes, otherwise error code
 */
int ec_open(ec_t **ppec, const osal_char_t *ifname, int prio, int cpumask, int eeprom_log) {
    assert(ppec != NULL);
    assert(ifname != NULL);

    int ret = EC_OK;

    // cppcheck-suppress misra-c2012-21.3
    ec_t *pec = (ec_t *)ec_malloc(sizeof(ec_t)); 
    (*ppec) = pec;
    if (pec == NULL) {
        ret = EC_ERROR_OUT_OF_MEMORY;
    } 

    if (ret == EC_OK) {
        ret = ec_index_init(&pec->idx_q);
    }
    
    if (ret == EC_OK) {
        pec->master_state       = EC_STATE_UNKNOWN;

        // slaves'n groups
        pec->phw                = NULL;
        pec->slave_cnt          = 0;
        pec->pd_group_cnt       = 0;
        pec->tx_sync            = 1;
        pec->threaded_startup   = 0;
        pec->consecutive_max_miss   = 10;
        pec->state_transition_pending = 0;

        // init values for distributed clocks
        pec->dc.have_dc         = 0;
        pec->dc.dc_time         = 0;
        pec->dc.rtc_time        = 0;
        pec->dc.act_diff        = 0;

        pec->tun_fd             = 0;
        pec->tun_ip             = 0;
        pec->tun_running        = 0;

        pec->dc.p_de_dc         = NULL;
        pec->dc.p_idx_dc        = NULL;

        // eeprom logging level
        pec->eeprom_log         = eeprom_log;

        ret = pool_open(&pec->pool, 100, &pec->dg_entries[0]);
    }

    (void)pool_open(&pec->mbx_message_pool_recv_free, LEC_MBX_MAX_ENTRIES, &pec->mbx_mp_recv_free_entries[0]);
    (void)pool_open(&pec->mbx_message_pool_send_free, LEC_MBX_MAX_ENTRIES, &pec->mbx_mp_send_free_entries[0]);

    if (ret == EC_OK) {
        ret = hw_open(&pec->phw, ifname, prio, cpumask, 0);
    }

    if (ret == EC_OK) {
        ret = ec_async_loop_create(&pec->async_loop, pec);
    }

    ec_log(1, __func__, "libethercat misra-2012-libosal-no-alloc\n");

    // destruct everything if something failed
    if (ret != EC_OK) {
        if (pec != NULL) {
            int local_ret = hw_close(pec->phw);
            if (local_ret != EC_OK) {
                ec_log(1, __func__, "hw_close failed with %d\n", local_ret);
            }
            
            local_ret = pool_close(&pec->pool);
            if (local_ret != EC_OK) {
                ec_log(1, __func__, "pool_close failed with %d\n", local_ret);
            }

            ec_index_deinit(&pec->idx_q);

            // cppcheck-suppress misra-c2012-21.3
            ec_free(pec);
        }

        *ppec = NULL;
    }

    return ret;
}

//! closes ethercat master
/*!
 * \param pec pointer to ethercat master
 * \return 0 on success 
 */
int ec_close(ec_t *pec) {
    assert(pec != NULL);

    ec_log(10, __func__, "detroying tun device...\n");

    ec_eoe_destroy_tun(pec);

    ec_log(10, __func__, "destroying async loop\n");
    if (pec->async_loop != NULL) { (void)ec_async_loop_destroy(pec->async_loop);}
    ec_log(10, __func__, "closing hardware handle\n");
    if (pec->phw != NULL) { (void)hw_close(pec->phw);}
    ec_log(10, __func__, "freeing frame pool\n");
    (void)pool_close(&pec->pool);

    ec_log(10, __func__, "destroying pd_groups\n");
    ec_index_deinit(&pec->idx_q);
    (void)ec_destroy_pd_groups(pec);

    ec_log(10, __func__, "destroying slaves\n");
    if (pec->slaves != NULL) {
        int slave;
        int cnt = pec->slave_cnt;

        for (slave = 0; slave < cnt; ++slave) {
            ec_slave_free(pec, slave);
        }

        pec->slave_cnt = 0;
        // cppcheck-suppress misra-c2012-21.3
        ec_free(pec->slaves);
    }
        
    (void)pool_close(&pec->mbx_message_pool_recv_free);
    (void)pool_close(&pec->mbx_message_pool_send_free);

    ec_log(10, __func__, "freeing master instance\n");
    // cppcheck-suppress misra-c2012-21.3
    ec_free(pec);

    ec_log(10, __func__, "all done!\n");
    return 0;
}

//! local callack for syncronous read/write
static void cb_block(void *user_arg, struct pool_entry *p) {
    (void)p;

    // cppcheck-suppress misra-c2012-11.5
    idx_entry_t *entry = (idx_entry_t *)user_arg;
    osal_binary_semaphore_post(&entry->waiter);
}

//! syncronous ethercat read/write
/*!
 * \param pec pointer to ethercat master
 * \param cmd ethercat command
 * \param adr 32-bit address of slave
 * \param data data buffer to read/write 
 * \param datalen length of data
 * \param wkc return value for working counter
 * \return 0 on succes, otherwise error code
 */
int ec_transceive(ec_t *pec, osal_uint8_t cmd, osal_uint32_t adr, 
        osal_uint8_t *data, osal_size_t datalen, osal_uint16_t *wkc) {
    assert(pec != NULL);
    assert(data != NULL);

    int ret = EC_OK;

    pool_entry_t *p_entry;
    ec_datagram_t *p_dg;
    idx_entry_t *p_idx;

    if (ec_index_get(&pec->idx_q, &p_idx) != EC_OK) {
        ec_log(1, "EC_TRANSCEIVE", "error getting ethercat index\n");
        ret = EC_ERROR_OUT_OF_INDICES;
    } else if (pool_get(&pec->pool, &p_entry, NULL) != EC_OK) {
        ec_index_put(&pec->idx_q, p_idx);
        ec_log(1, "EC_TRANSCEIVE", "error getting datagram from pool\n");
        ret = EC_ERROR_OUT_OF_DATAGRAMS;
    } else {
        p_dg = ec_datagram_cast(p_entry->data);

        (void)memset(p_dg, 0, sizeof(ec_datagram_t) + datalen + 2u);
        p_dg->cmd = cmd;
        p_dg->idx = p_idx->idx;
        p_dg->adr = adr;
        p_dg->len = datalen;
        p_dg->irq = 0;
        (void)memcpy(ec_datagram_payload(p_dg), data, datalen);

        p_entry->user_cb = cb_block;
        p_entry->user_arg = p_idx;

        // queue frame and trigger tx
        pool_put(&pec->phw->tx_low, p_entry);

        // send frame immediately if in sync mode
        if (pec->tx_sync != 0) {
            if (hw_tx(pec->phw) != EC_OK) {
                ec_log(1, __func__, "hw_tx failed!\n");
            }
        }

        // wait for completion
        osal_timer_t to;
        osal_timer_init(&to, 100000000);
        int local_ret = osal_binary_semaphore_timedwait(&p_idx->waiter, &to);
        if (local_ret != OSAL_OK) {
            if (local_ret == OSAL_ERR_TIMEOUT) {
                ec_log(1, "ec_transceive", "timeout on cmd 0x%X, adr 0x%X\n", cmd, adr);
            } else {
                ec_log(1, "ec_transceive", "osal_binary_semaphore_wait returned: %d, cmd 0x%X, adr 0x%X\n", 
                        local_ret, cmd, adr);
            }

            *wkc = 0u;
            ret = EC_ERROR_TIMEOUT;
        } else {
            *wkc = ec_datagram_wkc(p_dg);
            if ((*wkc) != 0u) {
                (void)memcpy(data, ec_datagram_payload(p_dg), datalen);
            }
        }

        pool_put(&pec->pool, p_entry);
        ec_index_put(&pec->idx_q, p_idx);
    }

    return ret;
}

//! local callack for syncronous read/write
static void cb_no_reply(void *user_arg, struct pool_entry *p) {
    // cppcheck-suppress misra-c2012-11.5
    idx_entry_t *entry = (idx_entry_t *)user_arg;
    pool_put(&entry->pec->pool, p);
    ec_index_put(&entry->pec->idx_q, entry);
}

//! asyncronous ethercat read/write, answer don't care
/*!
 * \param pec pointer to ethercat master
 * \param cmd ethercat command
 * \param adr 32-bit address of slave
 * \param data data buffer to read/write 
 * \param datalen length of data
 * \return 0 on succes, otherwise error code
 */
int ec_transmit_no_reply(ec_t *pec, osal_uint8_t cmd, osal_uint32_t adr, 
        osal_uint8_t *data, osal_size_t datalen) {
    assert(pec != NULL);
    assert(data != NULL);

    int ret = EC_OK;
    pool_entry_t *p_entry;
    ec_datagram_t *p_dg;
    idx_entry_t *p_idx;

    if (ec_index_get(&pec->idx_q, &p_idx) != EC_OK) {
        ec_log(1, "EC_TRANSMIT_NO_REPLY", "error getting ethercat index\n");
        ret = EC_ERROR_OUT_OF_INDICES;
    } else if (pool_get(&pec->pool, &p_entry, NULL) != EC_OK) {
        ec_index_put(&pec->idx_q, p_idx);
        ec_log(1, "EC_TRANSMIT_NO_REPLY", "error getting datagram from pool\n");
        ret = EC_ERROR_OUT_OF_DATAGRAMS;
    } else {
        p_dg = ec_datagram_cast(p_entry->data);

        (void)memset(p_dg, 0, sizeof(ec_datagram_t) + datalen + 2u);
        p_dg->cmd = cmd;
        p_dg->idx = p_idx->idx;
        p_dg->adr = adr;
        p_dg->len = datalen;
        p_dg->irq = 0;
        (void)memcpy(ec_datagram_payload(p_dg), data, datalen);

        // don't care about answer
        p_idx->pec = pec;
        p_entry->user_cb = cb_no_reply;
        p_entry->user_arg = p_idx;

        // queue frame and return, we don't care about an answer
        pool_put(&pec->phw->tx_low, p_entry);

        // send frame immediately if in sync mode
        if (pec->tx_sync != 0) {
            if (hw_tx(pec->phw) != EC_OK) {
                ec_log(1, __func__, "hw_tx failed!\n");
            }
        }
    }

    return ret;
}

//! send process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \return 0 on success
 */
int ec_send_process_data_group(ec_t *pec, int group) {
    assert(pec != NULL);

    int ret = EC_OK;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    ec_datagram_t *p_dg;

    if ((pd->p_entry != NULL) || (pd->p_idx != NULL)) {
        ec_log(1, __func__, "already sent group frame, will not send until it has returned...\n");
        ret = EC_ERROR_UNAVAILABLE;
    } else if (ec_index_get(&pec->idx_q, &pd->p_idx) != EC_OK) {
        ec_log(1, "EC_SEND_PROCESS_DATA_GROUP", "error getting ethercat index\n");
        ret = EC_ERROR_OUT_OF_INDICES;
    } else if (pool_get(&pec->pool, &pd->p_entry, NULL) != EC_OK) {
        ec_index_put(&pec->idx_q, pd->p_idx);
        ec_log(1, "EC_SEND_PROCESS_DATA_GROUP", "error getting datagram from pool\n");
        ret = EC_ERROR_OUT_OF_DATAGRAMS;
    } else {
        osal_uint32_t pd_len;
        if (pd->use_lrw != 0) {
            pd_len = max(pd->pdout_len, pd->pdin_len);
        } else {
            pd_len = pd->log_len;
        }

        p_dg = ec_datagram_cast(pd->p_entry->data);

        (void)memset(p_dg, 0, sizeof(ec_datagram_t) + pd_len + 2u);
        p_dg->cmd = EC_CMD_LRW;
        p_dg->idx = pd->p_idx->idx;
        p_dg->adr = pd->log;
        p_dg->len = pd_len;

        p_dg->irq = 0;
        if (pd->pd != NULL) {
            (void)memcpy(ec_datagram_payload(p_dg), pd->pd, pd->pdout_len);
        }

        pd->p_entry->user_cb = cb_block;
        pd->p_entry->user_arg = pd->p_idx;

        // queue frame and trigger tx
        pool_put(&pec->phw->tx_high, pd->p_entry);
    }

    return ret;
}

//! receive process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \param timeout for waiting for packet
 * \return 0 on success
 */
int ec_receive_process_data_group(ec_t *pec, int group, osal_timer_t *timeout) {
    assert(pec != NULL);
    assert(timeout != NULL);

    int ret = EC_OK;

    ec_pd_group_t *pd = &pec->pd_groups[group];
    if ((pd->p_entry == NULL) || (pd->p_idx == NULL)) {
        ec_log(1, __func__, "did not sent group frame\n");
        ret = EC_ERROR_UNAVAILABLE;
    } else {
        ec_datagram_t *p_dg;

        p_dg = ec_datagram_cast(pd->p_entry->data);

        // wait for completion
        if (osal_binary_semaphore_timedwait(&pd->p_idx->waiter, timeout) != 0) {
            if (++pd->recv_missed < pec->consecutive_max_miss) {
                ec_log(1, "EC_RECEIVE_PROCESS_DATA_GROUP", 
                        "osal_semaphore_timedwait group id %d: %s, consecutive act %d limit %d\n", group, strerror(errno),
                        pd->recv_missed, pec->consecutive_max_miss);
            } else {
                ec_log(1, "EC_RECEIVE_PROCESS_DATA_GROUP", 
                        "too much missed receive frames, falling back to INIT!\n");
                ret = EC_ERROR_TIMEOUT;
            }
        } else {
            static int wkc_mismatch_cnt = 0;
            osal_uint16_t wkc = 0;

            // reset consecutive missed counter
            pd->recv_missed = 0;

            wkc = ec_datagram_wkc(p_dg);
            if (pd->pd != NULL) {
                if (pd->use_lrw != 0) {
                    (void)memcpy(&pd->pd[pd->pdout_len], ec_datagram_payload(p_dg), pd->pdin_len);
                } else {
                    (void)memcpy(&pd->pd[pd->pdout_len], &ec_datagram_payload(p_dg)[pd->pdout_len], pd->pdin_len);
                }
            }

            if (    (   (pec->master_state == EC_STATE_SAFEOP) || 
                        (pec->master_state == EC_STATE_OP)  ) && 
                    (wkc != pd->wkc_expected)) {
                if ((wkc_mismatch_cnt++%1000) == 0) {
                    ec_log(1, "EC_RECEIVE_PROCESS_DATA_GROUP", 
                            "group %2d: working counter mismatch got %u, "
                            "expected %u, slave_cnt %d, mismatch_cnt %d\n", 
                            group, wkc, pd->wkc_expected, 
                            pec->slave_cnt, wkc_mismatch_cnt);
                }

                ec_async_check_group(pec->async_loop, group);
                ret = EC_ERROR_WKC_MISMATCH;
            } else {
                wkc_mismatch_cnt = 0;
            }

            for (osal_uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
                ec_slave_ptr(slv, pec, slave);
                if (slv->assigned_pd_group != group) { continue; }

                if ((slv->eeprom.mbx_supported != 0u) && (slv->mbx.sm_state != NULL)) {
                    if ((*slv->mbx.sm_state & 0x08u) != 0u) {
                        ec_log(100, __func__, "slave %2d: sm_state %X\n", slave, *slv->mbx.sm_state);
                        ec_mbx_sched_read(pec, slave);
                    }
                }
            }
        }

        pool_put(&pec->pool, pd->p_entry);
        ec_index_put(&pec->idx_q, pd->p_idx);

        pd->p_entry = NULL;
        pd->p_idx = NULL;
    }

    return ret;
}

//! send distributed clock sync datagram
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_send_distributed_clocks_sync(ec_t *pec) {
    static osal_mutex_t send_dc_lock = PTHREAD_MUTEX_INITIALIZER;

    assert(pec != NULL);

    int ret = EC_OK;
    ec_datagram_t *p_dg = NULL;

    osal_mutex_lock(&send_dc_lock);

    if (!pec->dc.have_dc || !pec->dc.rtc_sto) {
        ret = EC_ERROR_UNAVAILABLE;
    } else if ((pec->dc.p_de_dc != NULL) || (pec->dc.p_idx_dc != NULL)) {
        ret = EC_ERROR_UNAVAILABLE;
    } else {
        osal_uint64_t act_rtc_time = osal_timer_gettime_nsec();

        if (pec->dc.mode == dc_mode_ref_clock) {
            if (pec->dc.timer_override > 0) {
                if (pec->dc.rtc_time == 0u) {
                    int64_t tmp = (int64_t)act_rtc_time - pec->dc.rtc_sto;
                    pec->dc.rtc_time = (osal_uint64_t)tmp;
                } else {
                    pec->dc.rtc_time += (osal_uint64_t)(pec->dc.timer_override);
                }
            }   
        } else {
            pec->dc.rtc_time = (int64_t)act_rtc_time - pec->dc.rtc_sto;
        }

        if (ec_index_get(&pec->idx_q, &pec->dc.p_idx_dc) != EC_OK) {
            ec_log(1, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", "error getting ethercat index\n");
            ret = EC_ERROR_OUT_OF_INDICES;
        } else if (pool_get(&pec->pool, &pec->dc.p_de_dc, NULL) != EC_OK) {
            ec_index_put(&pec->idx_q, pec->dc.p_idx_dc);
            ec_log(1, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", "error getting datagram from pool\n");
            ret = EC_ERROR_OUT_OF_DATAGRAMS;
        } else {
            p_dg = ec_datagram_cast(pec->dc.p_de_dc->data);
            (void)memset(p_dg, 0, sizeof(ec_datagram_t) + 8u + 2u);

            if (pec->dc.mode == dc_mode_master_as_ref_clock) {
                p_dg->cmd = EC_CMD_BWR;
                p_dg->idx = pec->dc.p_idx_dc->idx;
                p_dg->adr = ((osal_uint32_t)EC_REG_DCSYSTIME << 16u);
                p_dg->len = 8;
                p_dg->irq = 0;

                (void)memcpy((osal_uint8_t *)ec_datagram_payload(p_dg), (osal_uint8_t *)&pec->dc.rtc_time, sizeof(pec->dc.rtc_time));
            } else {
                p_dg->cmd = EC_CMD_FRMW;
                p_dg->idx = pec->dc.p_idx_dc->idx;
                p_dg->adr = ((osal_uint32_t)EC_REG_DCSYSTIME << 16u) | pec->dc.master_address;
                p_dg->len = 8;
                p_dg->irq = 0;
            }

            pec->dc.p_de_dc->user_cb = cb_block;
            pec->dc.p_de_dc->user_arg = pec->dc.p_idx_dc;

            // queue frame and trigger tx
            pool_put(&pec->phw->tx_high, pec->dc.p_de_dc);
        }
    }
      
    osal_mutex_unlock(&send_dc_lock);

    return ret;
}

//! calculate signed difference of 64-bit unsigned int's
/*!
 * \param[in]   a       Minuend.
 * \param[in]   b       Subtrahend.
 *
 * \return Difference 
 */
static int64_t signed64_diff(osal_uint64_t a, osal_uint64_t b) {
    osal_uint64_t abs_diff = (a > b) ? (a - b) : (b - a);
    if (abs_diff > INT64_MAX) {
        if (a > INT64_MAX) {
            a = UINT64_MAX - a;
        } else if (b > INT64_MAX) {
            b = UINT64_MAX - b;
        }
            
        abs_diff = (a > b) ? (a - b) : (b - a);
    }
    return (a > b) ? (int64_t)abs_diff : -(int64_t)abs_diff;
}

//! receive distributed clocks sync datagram
/*!
 * \param pec ethercat master pointer
 * \param timeout absolute timeout
 * \return 0 on success
 */
int ec_receive_distributed_clocks_sync(ec_t *pec, osal_timer_t *timeout) {
    assert(pec != NULL);
    assert(timeout != NULL);

    int ret = EC_OK;
    osal_uint16_t wkc; 
    ec_datagram_t *p_dg = NULL;

    if (pec->dc.have_dc == 0) {
        ret = EC_ERROR_UNAVAILABLE;
    } else if ((pec->dc.p_de_dc == NULL) || (pec->dc.p_idx_dc == NULL)) {
        ec_log(1, __func__, "no dc frame was sent!\n");
        ret = EC_ERROR_UNAVAILABLE;
    } else {
        // wait for completion
        if (osal_binary_semaphore_timedwait(&pec->dc.p_idx_dc->waiter, timeout) != 0) 
        {
            ec_log(1, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC",
                    "osal_semaphore_timedwait distributed clocks (sent: %lld): %s\n", 
                    pec->dc.rtc_time, strerror(errno));
            ret = EC_ERROR_TIMEOUT;
        } else {
            p_dg = ec_datagram_cast(pec->dc.p_de_dc->data);
            wkc = ec_datagram_wkc(p_dg);

            if (wkc != 0u) {
                (void)memcpy((osal_uint8_t *)&pec->dc.dc_time, (osal_uint8_t *)ec_datagram_payload(p_dg), 8);

                // get clock difference
                pec->dc.act_diff = signed64_diff(pec->dc.rtc_time, pec->dc.dc_time); 

                if (pec->dc.mode == dc_mode_ref_clock) {
                    // only compensate within one cycle, add rest to system time offset
                    int ticks_off = pec->dc.act_diff / (pec->dc.timer_override);
                    if (ticks_off != 0) {
                        ec_log(100, __func__, "compensating %d cycles, rtc_time %lld, dc_time %lld, act_diff %d\n", 
                                ticks_off, pec->dc.rtc_time, pec->dc.dc_time, pec->dc.act_diff);
                        pec->dc.rtc_time -= ticks_off * (pec->dc.timer_override);
                        pec->dc.act_diff  = signed64_diff(pec->dc.rtc_time, pec->dc.dc_time);
                    }

                    ec_log(100, __func__, "rtc %lu, dc %lu, act_diff %d\n", pec->dc.rtc_time, pec->dc.dc_time, pec->dc.act_diff);
                } else if (pec->dc.mode == dc_mode_master_clock) {
                    // sending offset compensation value to dc master clock
                    pool_entry_t *p_entry_dc_sto;
                    idx_entry_t *p_idx_dc_sto;

                    // dc system time offset frame
                    if (ec_index_get(&pec->idx_q, &p_idx_dc_sto) != EC_OK) {
                        ec_log(1, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC", "error getting ethercat index\n");
                        ret = EC_ERROR_OUT_OF_INDICES;
                    } else if (pool_get(&pec->pool, &p_entry_dc_sto, NULL) != EC_OK) {
                        ec_index_put(&pec->idx_q, p_idx_dc_sto);
                        ec_log(1, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC", "error getting datagram from pool\n");
                        ret = EC_ERROR_OUT_OF_DATAGRAMS;
                    } else {
                        // correct system time offset, sync ref_clock to master_clock
                        // only correct half diff to avoid overshoot in slave.
                        pec->dc.dc_sto += pec->dc.act_diff / 2.;
                        p_dg = ec_datagram_cast(p_entry_dc_sto->data);
                        (void)memset(p_dg, 0, sizeof(ec_datagram_t) + 10u);
                        p_dg->cmd = EC_CMD_FPWR;
                        p_dg->idx = p_idx_dc_sto->idx;
                        p_dg->adr = ((osal_uint32_t)EC_REG_DCSYSOFFSET << 16u) | pec->dc.master_address;
                        p_dg->len = sizeof(pec->dc.dc_sto);
                        p_dg->irq = 0;
                        (void)memcpy(ec_datagram_payload(p_dg), (osal_uint8_t *)&pec->dc.dc_sto, sizeof(pec->dc.dc_sto));
                        // we don't care about the answer, cb_no_reply frees datagram 
                        // and index
                        p_idx_dc_sto->pec = pec;
                        p_entry_dc_sto->user_cb = cb_no_reply;
                        p_entry_dc_sto->user_arg = p_idx_dc_sto;

                        // queue frame and trigger tx
                        pool_put(&pec->phw->tx_low, p_entry_dc_sto);
                    }
                }
            }
        }

        pool_put(&pec->pool, pec->dc.p_de_dc);
        ec_index_put(&pec->idx_q, pec->dc.p_idx_dc);

        pec->dc.p_de_dc = NULL;
        pec->dc.p_idx_dc = NULL;
    }

    return ret;
}

//! send broadcast read to ec state
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_send_brd_ec_state(ec_t *pec) {
    assert(pec != NULL);

    int ret = EC_OK;
    ec_datagram_t *p_dg = NULL;

    if (ec_index_get(&pec->idx_q, &pec->p_idx_state) != EC_OK) {
        ec_log(1, __func__, "error getting ethercat index\n");
        ret = EC_ERROR_OUT_OF_INDICES;
    } else if (pool_get(&pec->pool, &pec->p_de_state, NULL) != EC_OK) {
        ec_index_put(&pec->idx_q, pec->p_idx_state);
        ec_log(1, __func__, "error getting datagram from pool\n");
        ret = EC_ERROR_OUT_OF_DATAGRAMS;
    } else {
        p_dg = ec_datagram_cast(pec->p_de_state->data);

        (void)memset(p_dg, 0, sizeof(ec_datagram_t) + 4u + 2u);
        p_dg->cmd = EC_CMD_BRD;
        p_dg->idx = pec->p_idx_state->idx;
        p_dg->adr = (osal_uint32_t)EC_REG_ALSTAT << 16u;
        p_dg->len = 2;
        p_dg->irq = 0;

        pec->p_de_state->user_cb = cb_block;
        pec->p_de_state->user_arg = pec->p_idx_state;

        // queue frame and trigger tx
        pool_put(&pec->phw->tx_high, pec->p_de_state);
    }

    return ret;
}

//! receive broadcast read to ec_state
/*!
 * \param pec ethercat master pointer
 * \param timeout for waiting for packet
 * \return 0 on success
 */
int ec_receive_brd_ec_state(ec_t *pec, osal_timer_t *timeout) {
    assert(pec != NULL);
    assert(timeout != NULL);

    static int wkc_mismatch_cnt_ec_state = 0;
    static int ec_state_mismatch_cnt = 0;

    ec_datagram_t *p_dg = NULL;
    int ret = EC_OK;
    osal_uint16_t al_status = 0u;

    if (pec->p_idx_state == NULL) {
        ret = EC_ERROR_UNAVAILABLE;
    } else if (osal_binary_semaphore_timedwait(&pec->p_idx_state->waiter, timeout) != 0) 
    { // wait for completion
        ec_log(1, __func__, "osal_semaphore_timedwait ec_state: %s\n", strerror(errno));
        ret = EC_ERROR_TIMEOUT;
    } else {
        osal_uint16_t wkc;
        p_dg = ec_datagram_cast(pec->p_de_state->data);

        wkc = ec_datagram_wkc(p_dg);
        (void)memcpy((osal_uint8_t *)&al_status, ec_datagram_payload(p_dg), 2iu);

        if (    (   (pec->master_state == EC_STATE_SAFEOP) || 
                    (pec->master_state == EC_STATE_OP)  ) && 
                (wkc != pec->slave_cnt)) {
            if ((wkc_mismatch_cnt_ec_state++%1000) == 0) {
                ec_log(1, __func__, 
                        "brd ec_state: working counter mismatch got %u, "
                        "slave_cnt %d, mismatch_cnt %d\n", 
                        wkc, pec->slave_cnt, wkc_mismatch_cnt_ec_state);
            }

            //        ec_async_check_group(pec->async_loop, group);
            ret = EC_ERROR_WKC_MISMATCH;
        } else {
            wkc_mismatch_cnt_ec_state = 0u;
        }

        if (!pec->state_transition_pending && (al_status != pec->master_state)) {
            if ((ec_state_mismatch_cnt++%1000) == 0) {
                ec_log(1, __func__, "al status mismatch, got 0x%X, master state is 0x%X\n", 
                        al_status, pec->master_state);
            }
        }

        pool_put(&pec->pool, pec->p_de_state);
        ec_index_put(&pec->idx_q, pec->p_idx_state);
    }

    return ret;
}

//! configures tun device of EtherCAT master, used for EoE slaves.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] ip_address    IP address to be set for tun device.
 */
void ec_configure_tun(ec_t *pec, osal_uint8_t ip_address[4]) {
    assert(pec != NULL);

    (void)memcpy((osal_uint8_t *)&pec->tun_ip, (osal_uint8_t *)&ip_address[0], 4);
    if (ec_eoe_setup_tun(pec) != EC_OK) {
        ec_log(1, __func__, "ec_eoe_setup_tun failed!\n");
    }
}

//! \brief Return current slave count.
/*!
 * \param[in] pec           Pointer to ethercat master structure.
 * \return cnt current slave count.
 */
int ec_get_slave_count(ec_t *pec) {
    assert(pec != NULL);

    return pec->slave_cnt;
}

