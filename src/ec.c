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

// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <limits.h>
#include <assert.h>

#if LIBETHERCAT_HAVE_INTTYPES_H == 1
#include <inttypes.h>
#endif


#include "libethercat/ec.h"
#include "libethercat/slave.h"
#include "libethercat/mbx.h"
#include "libethercat/coe.h"
#include "libethercat/dc.h"
#include "libethercat/eeprom.h"
#include "libethercat/error_codes.h"

#define DC_DCSOFF_SAMPLES 1000u

#ifndef max
#define max(a, b) \
    ((a) > (b) ? (a) : (b))
#endif

// forward declaration
static void default_log_func(int lvl, void* user, const osal_char_t *format, ...) __attribute__ ((format (printf, 3, 4)));

//! calculate signed difference of 64-bit unsigned int's
/*!
 * \param[in]   a       Minuend.
 * \param[in]   b       Subtrahend.
 *
 * \return Difference 
 */
static int64_t signed64_diff(osal_uint64_t a, osal_uint64_t b) {
    osal_uint64_t tmp_a = a;
    osal_uint64_t tmp_b = b;
    osal_uint64_t abs_diff = (a - b);
    if (a < b) { abs_diff = (b - a); }

    if (abs_diff > (osal_uint64_t)INT64_MAX) {
        if (a > (osal_uint64_t)INT64_MAX) {
            tmp_a = UINT64_MAX - a;
            tmp_b = b;
        } else if (b > (osal_uint64_t)INT64_MAX) {
            tmp_a = a;
            tmp_b = UINT64_MAX - b;
        } else {
        }
            
        abs_diff = (tmp_a - tmp_b);
        if (tmp_a < tmp_b) { abs_diff = (tmp_b - tmp_a); }
    }
    return (tmp_a > tmp_b) ? (int64_t)abs_diff : -(int64_t)abs_diff;
}

void default_log_func(int lvl, void* user, const osal_char_t *format, ...) {
    (void)lvl;
    (void)user;

    va_list args;                   // cppcheck-suppress misra-c2012-17.1
    va_start(args, format);         // cppcheck-suppress misra-c2012-17.1
    (void)vfprintf(stderr, format, args);
    va_end(args);                   // cppcheck-suppress misra-c2012-17.1
}

void *ec_log_func_user = NULL;
void (*ec_log_func)(int lvl, void *user, const osal_char_t *format, ...) __attribute__ ((format (printf, 3, 4))) = default_log_func;

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
        (void)ec_cyclic_datagram_init(&pec->pd_groups[i].cdg, 10000000);
        pec->pd_groups[i].group     = i;
        pec->pd_groups[i].log       = 0x10000u * ((osal_uint32_t)i+1u);
        pec->pd_groups[i].log_len   = 0u;
        pec->pd_groups[i].pdout_len = 0u;
        pec->pd_groups[i].pdin_len  = 0u;
        pec->pd_groups[i].use_lrw   = 1;
        pec->pd_groups[i].recv_missed = 0;
        pec->pd_groups[i].divisor   = 1;
        pec->pd_groups[i].divisor_cnt = 0;
    }

    return 0;
}

//! \brief Configure process data group settings.
/*!
 * \param[in] pec           Pointer to EtherCAT master structure.
 * \param[in] group         Number of group to configure.
 * \param[in] clock_divisor Send group datagram every 'clock_divisor' ticks.
 * \param[in] user_cb       Callback when group datagram returned, maybe NULL.
 * \param[in] user_cb_arg   Argument passed to 'user_cb', maybe NULL.
 */
void ec_configure_pd_group(ec_t *pec, osal_uint16_t group, int clock_divisor,
    void (*user_cb)(void *arg, int num), void *user_cb_arg) 
{
    assert(pec != NULL);
    assert(group < pec->pd_group_cnt);

    if (clock_divisor > 0) {
        pec->pd_groups[group].divisor = clock_divisor;
    }

    pec->pd_groups[group].cdg.user_cb = user_cb;
    pec->pd_groups[group].cdg.user_cb_arg = user_cb_arg;
}

//! destroy process data groups
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_destroy_pd_groups(ec_t *pec) {
    assert(pec != NULL);

    for (osal_uint16_t i = 0; i < pec->pd_group_cnt; ++i) {
        (void)ec_cyclic_datagram_destroy(&pec->pd_groups[i].cdg);
    }

    pec->pd_group_cnt = 0;

    return 0;
}

static const osal_char_t *get_state_string(ec_state_t state) {
    static const osal_char_t state_string_boot[]    = (char[]){ "EC_STATE_BOOT" };
    static const osal_char_t state_string_init[]    = (char[]){ "EC_STATE_INIT" };
    static const osal_char_t state_string_preop[]   = (char[]){ "EC_STATE_PREOP" };
    static const osal_char_t state_string_safeop[]  = (char[]){ "EC_STATE_SAFEOP" };
    static const osal_char_t state_string_op[]      = (char[]){ "EC_STATE_OP" };
    static const osal_char_t state_string_unknown[] = (char[]){ "EC_STATE_UNKNOWN" };

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

    osal_mutex_lock(&pd->cdg.lock);

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
            "%3" PRIu64 " bytes, in 0x%08" PRIx64 " %3" PRIu64 " bytes, lrw windows %3" PRIu64 " bytes\n", 
            group, pd->log, pd->pdout_len, pd->log + pd->pdout_len, 
            pd->pdin_len, pd->pd_lrw_len);

    pd->log_len = pd->pdout_len + pd->pdin_len;

    osal_uint8_t *pdout = &pd->pd[0];
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
                    osal_off_t pdoff = 0;
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
                    osal_off_t pdoff = 0;
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

        if (    (slv->eeprom.mbx_supported != 0u) &&
                (slv->mbx.map_mbx_state == OSAL_TRUE)) {
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
    
    osal_mutex_unlock(&pd->cdg.lock);
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

    osal_mutex_lock(&pd->cdg.lock);

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
            "%3" PRIu64 " bytes, in 0x%08" PRIx64 " %3" PRIu64 " bytes\n", group, pd->log, 
            pd->pdout_len, pd->log + pd->pdout_len, pd->pdin_len);

    pd->log_len = pd->pdout_len + pd->pdin_len;

    osal_uint8_t *pdout = &pd->pd[0];
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

        if (    (slv->eeprom.mbx_supported != 0u) &&
                (slv->mbx.map_mbx_state == OSAL_TRUE)) {
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
    
    osal_mutex_unlock(&pd->cdg.lock);
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
                (void)snprintf(&attr.task_name[0], TASK_NAME_LEN, "ecat.worker%u", slave);
                (void)osal_task_create(&(pec->slaves[slave].worker_tid), &attr, 
                        prepare_state_transition_wrapper, 
                        &(pec->slaves[slave].worker_arg));
            }
        }

        for (osal_uint32_t slave = 0u; slave < pec->slave_cnt; ++slave) {
            if (pec->slaves[slave].assigned_pd_group != -1) {
                (void)osal_task_join(&pec->slaves[slave].worker_tid, NULL);
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
                (void)snprintf(&attr.task_name[0], TASK_NAME_LEN, "ecat.worker%u", slave);
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

                pec->slaves[i].slave = i;
                pec->slaves[i].assigned_pd_group = -1;
                pec->slaves[i].auto_inc_address = auto_inc;
                pec->slaves[i].fixed_address = fixed;
                pec->slaves[i].dc.use_dc = 1;
                pec->slaves[i].sm_set_by_user = 0;
                pec->slaves[i].subdev_cnt = 0;
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
            slv->mbx.handler_running = 0;

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
                slv->ptype = 0;
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

    ec_log(10, __func__, "switching from %s to %s\n", 
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
            if (pec->slave_cnt == 0) {
                ret = EC_ERROR_UNAVAILABLE;
            } else {
                ec_state_transition_loop(pec, EC_STATE_INIT, 0);
            }

            if ((state == EC_STATE_INIT) || (ret != EC_OK)) {
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

            pec->master_state = EC_STATE_PREOP;

            // cleaning up datagram/index pointers
            // return group datagrams
            for (int i = 0; i < pec->pd_group_cnt; ++i) {
                ec_pd_group_t *pd = &pec->pd_groups[i];

                if (pd->cdg.p_idx != NULL) {
                    pec->hw.tx_send[pd->cdg.p_idx->idx] = NULL;
                    ec_index_put(&pec->idx_q, pd->cdg.p_idx);
                    pd->cdg.p_idx = NULL;
                }

                if (pd->cdg.p_entry != NULL) {
                    pool_put(&pec->pool, pd->cdg.p_entry);
                    pd->cdg.p_entry = NULL;
                }
            }

            // return distributed clocks datagram
            if (pec->dc.cdg.p_idx != NULL) {
                pec->hw.tx_send[pec->dc.cdg.p_idx->idx] = NULL;
                ec_index_put(&pec->idx_q, pec->dc.cdg.p_idx);
                pec->dc.cdg.p_idx = NULL;
            }

            if (pec->dc.cdg.p_entry != NULL) {
                pool_put(&pec->pool, pec->dc.cdg.p_entry);
                pec->dc.cdg.p_entry = NULL;
            }

            // return broadcast state datagram
            if (pec->cdg_state.p_idx != NULL) {
                pec->hw.tx_send[pec->cdg_state.p_idx->idx] = NULL;
                ec_index_put(&pec->idx_q, pec->cdg_state.p_idx);
                pec->cdg_state.p_idx = NULL;
            }

            if (pec->cdg_state.p_entry != NULL) {
                pool_put(&pec->pool, pec->cdg_state.p_entry);
                pec->cdg_state.p_entry = NULL;
            }

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
        
    if (ret == EC_OK) {
        pec->master_state = state;
    }

    pec->state_transition_pending = 0;

    return pec->master_state;
}

//! open ethercat master
/*!
 * \param ppec return value for ethercat master pointer
 * \param ifname ethercat master interface name
 * \param prio receive thread priority
 * \param cpumask receive thread cpumask
 * \param eeprom_log log eeprom to stdout
 * \return 0 on succes, otherwise error code
 */
int ec_open(ec_t *pec, const osal_char_t *ifname, int prio, int cpumask, int eeprom_log) {
    assert(pec != NULL);
    assert(ifname != NULL);

    // reset data structure
    (void)memset(pec, 0, sizeof(ec_t));

    int ret = EC_OK;
    
    ret = ec_index_init(&pec->idx_q);
    
    if (ret == EC_OK) {
        pec->master_state       = EC_STATE_UNKNOWN;

        // slaves'n groups
        pec->slave_cnt          = 0;
        pec->pd_group_cnt       = 0;
        pec->threaded_startup   = 0;
        pec->consecutive_max_miss   = 10;
        pec->state_transition_pending = 0;

        // init values for distributed clocks
        pec->dc.have_dc         = 0;
        pec->dc.dc_time         = 0;
        pec->dc.rtc_time        = 0;
        pec->dc.act_diff        = 0;
        pec->dc.control.diffsum = 0;
        pec->dc.control.diffsum_limit = 1000000;
        pec->dc.control.kp      = 1;
        pec->dc.control.ki      = 0.1;
        pec->dc.control.v_part_old = 0.0;

        pec->tun_fd             = 0;
        pec->tun_ip             = 0;
        pec->tun_running        = 0;

        (void)ec_cyclic_datagram_init(&pec->cdg_state, 1000000);
        (void)ec_cyclic_datagram_init(&pec->dc.cdg, 1000000);

        // eeprom logging level
        pec->eeprom_log         = eeprom_log;

        (void)memset(&pec->dg_entries[0], 0, sizeof(pool_entry_t) * LEC_MAX_DATAGRAMS);
        ret = pool_open(&pec->pool, 100, &pec->dg_entries[0]);
    }

    (void)pool_open(&pec->mbx_message_pool_recv_free, LEC_MAX_MBX_ENTRIES, &pec->mbx_mp_recv_free_entries[0]);
    (void)pool_open(&pec->mbx_message_pool_send_free, LEC_MAX_MBX_ENTRIES, &pec->mbx_mp_send_free_entries[0]);

    if (ret == EC_OK) {
        ret = hw_open(&pec->hw, pec, ifname, prio, cpumask);
    }

    if (ret == EC_OK) {
        ret = ec_async_loop_create(&pec->async_loop, pec);
    }

    ec_log(10, __func__, "libethercat version          : %s\n", LIBETHERCAT_VERSION);
    ec_log(10, __func__, "  MAX_SLAVES                 : %d\n", LEC_MAX_SLAVES);
    ec_log(10, __func__, "  MAX_GROUPS                 : %d\n", LEC_MAX_GROUPS);
    ec_log(10, __func__, "  MAX_PDLEN                  : %d\n", LEC_MAX_PDLEN);
    ec_log(10, __func__, "  MAX_MBX_ENTRIES            : %d\n", LEC_MAX_MBX_ENTRIES);
    ec_log(10, __func__, "  MAX_INIT_CMD_DATA          : %d\n", LEC_MAX_INIT_CMD_DATA);
    ec_log(10, __func__, "  MAX_SLAVE_FMMU             : %d\n", LEC_MAX_SLAVE_FMMU);
    ec_log(10, __func__, "  MAX_SLAVE_SM               : %d\n", LEC_MAX_SLAVE_SM);
    ec_log(10, __func__, "  MAX_DATAGRAMS              : %d\n", LEC_MAX_DATAGRAMS);
    ec_log(10, __func__, "  MAX_EEPROM_CAT_SM          : %d\n", LEC_MAX_EEPROM_CAT_SM); 
    ec_log(10, __func__, "  MAX_EEPROM_CAT_FMMU        : %d\n", LEC_MAX_EEPROM_CAT_FMMU);
    ec_log(10, __func__, "  MAX_EEPROM_CAT_PDO         : %d\n", LEC_MAX_EEPROM_CAT_PDO);
    ec_log(10, __func__, "  MAX_EEPROM_CAT_PDO_ENTRIES : %d\n", LEC_MAX_EEPROM_CAT_PDO_ENTRIES);
    ec_log(10, __func__, "  MAX_EEPROM_CAT_STRINGS     : %d\n", LEC_MAX_EEPROM_CAT_STRINGS);
    ec_log(10, __func__, "  MAX_EEPROM_CAT_DC          : %d\n", LEC_MAX_EEPROM_CAT_DC);
    ec_log(10, __func__, "  MAX_STRING_LEN             : %d\n", LEC_MAX_STRING_LEN);
    ec_log(10, __func__, "  MAX_DATA                   : %d\n", LEC_MAX_DATA);
    ec_log(10, __func__, "  MAX_DS402_SUBDEVS          : %d\n", LEC_MAX_DS402_SUBDEVS);
    ec_log(10, __func__, "  MAX_COE_EMERGENCIES        : %d\n", LEC_MAX_COE_EMERGENCIES);
    ec_log(10, __func__, "  MAX_COE_EMERGENCY_MSG_LEN  : %d\n", LEC_MAX_COE_EMERGENCY_MSG_LEN);

    if (ret != EC_OK) {
        if (pec != NULL) {
            int local_ret = hw_close(&pec->hw);
            if (local_ret != EC_OK) {
                ec_log(1, __func__, "hw_close failed with %d\n", local_ret);
            }
            
            local_ret = pool_close(&pec->pool);
            if (local_ret != EC_OK) {
                ec_log(1, __func__, "pool_close failed with %d\n", local_ret);
            }

            ec_index_deinit(&pec->idx_q);
        }
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
    (void)ec_async_loop_destroy(&pec->async_loop);
    ec_log(10, __func__, "closing hardware handle\n");
    (void)hw_close(&pec->hw);
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
    }
        
    (void)pool_close(&pec->mbx_message_pool_recv_free);
    (void)pool_close(&pec->mbx_message_pool_send_free);
        
    (void)ec_cyclic_datagram_destroy(&pec->dc.cdg);
    (void)ec_cyclic_datagram_destroy(&pec->cdg_state);

    ec_log(10, __func__, "all done!\n");
    return 0;
}

//! local callack for syncronous read/write
static void cb_block(struct ec *pec, void *user_arg, struct pool_entry *p) {
    (void)pec;
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
        pool_put(&pec->hw.tx_low, p_entry);

        // send frame immediately if in sync mode
        if (    (pec->master_state != EC_STATE_SAFEOP) &&
                (pec->master_state != EC_STATE_OP)  ) {
            if (hw_tx(&pec->hw) != EC_OK) {
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
static void cb_no_reply(struct ec *pec, void *user_arg, struct pool_entry *p) {
    // cppcheck-suppress misra-c2012-11.5
    idx_entry_t *entry = (idx_entry_t *)user_arg;
    pool_put(&pec->pool, p);
    ec_index_put(&pec->idx_q, entry);
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
        p_entry->user_cb = cb_no_reply;
        p_entry->user_arg = p_idx;

        // queue frame and return, we don't care about an answer
        pool_put(&pec->hw.tx_low, p_entry);

        // send frame immediately if in sync mode
        if (    (pec->master_state != EC_STATE_SAFEOP) &&
                (pec->master_state != EC_STATE_OP)  ) {
            if (hw_tx(&pec->hw) != EC_OK) {
                ec_log(1, __func__, "hw_tx failed!\n");
            }
        }
    }

    return ret;
}

//! receive process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \return 0 on success
 */
static int ec_receive_process_data_group(ec_t *pec, int group) {
    assert(pec != NULL);

    int ret = EC_OK;

    ec_pd_group_t *pd = &pec->pd_groups[group];
    ec_datagram_t *p_dg;

    if (pd->cdg.p_entry != NULL) {
        p_dg = ec_datagram_cast(pd->cdg.p_entry->data);

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

            ec_async_check_group(&pec->async_loop, group);
            ret = EC_ERROR_WKC_MISMATCH;
        } else {
            wkc_mismatch_cnt = 0;
        }

        for (osal_uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
            ec_slave_ptr(slv, pec, slave);
            if (slv->assigned_pd_group != group) { continue; }

            if ((slv->eeprom.mbx_supported != 0u) && (slv->mbx.sm_state != NULL)) {
                if ((*slv->mbx.sm_state & 0x08u) != 0u) {
#ifdef LIBETHERCAT_DEBUG
                    ec_log(100, __func__, "slave %2d: sm_state %X\n", slave, *slv->mbx.sm_state);
#endif
                    ec_mbx_sched_read(pec, slave);
                }
            }
        }
    }

    return ret;
}


//! datagram callack for receiving process data group answer
static void cb_process_data_group(struct ec *pec, void *user_arg, struct pool_entry *p) {
    int group = *(osal_uint32_t *)user_arg;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    (void)p;

#ifdef LIBETHERCAT_DEBUG
    ec_log(100, __func__, "group %2d: received process data\n", group);
#endif

    osal_mutex_lock(&pd->cdg.lock);
    (void)ec_receive_process_data_group(pec, group);
    osal_mutex_unlock(&pd->cdg.lock);

    if (pd->cdg.user_cb != NULL) {
        (*pd->cdg.user_cb)(pd->cdg.user_cb_arg, pd->group);
    }
}

//! send process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \return 0 on success
 */
static int ec_send_process_data_group(ec_t *pec, int group) {
    assert(pec != NULL);

    int ret = EC_OK;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    ec_datagram_t *p_dg;

#ifdef LIBETHERCAT_DEBUG
    ec_log(100, __func__, "group %2d: sending process data\n", group);
#endif

    osal_mutex_lock(&pd->cdg.lock);

    if (pd->cdg.p_idx == NULL) {
        if (ec_index_get(&pec->idx_q, &pd->cdg.p_idx) != EC_OK) {
            ec_log(1, "EC_SEND_PROCESS_DATA_GROUP", "error getting ethercat index\n");
            ret = EC_ERROR_OUT_OF_INDICES;
        }
    }

    if ((ret == EC_OK) && (pd->cdg.p_entry == NULL)) {
        if (pool_get(&pec->pool, &pd->cdg.p_entry, NULL) != EC_OK) {
            ec_index_put(&pec->idx_q, pd->cdg.p_idx);
            pd->cdg.p_idx = NULL;
            ec_log(1, "EC_SEND_PROCESS_DATA_GROUP", "error getting datagram from pool\n");
            ret = EC_ERROR_OUT_OF_DATAGRAMS;
        }
    }

    if (ret == EC_OK) {
        osal_uint32_t pd_len;
        if (pd->use_lrw != 0) {
            pd_len = max(pd->pdout_len, pd->pdin_len);
        } else {
            pd_len = pd->log_len;
        }

        if (pd_len > 0u) {
            p_dg = ec_datagram_cast(pd->cdg.p_entry->data);

            (void)memset(p_dg, 0, sizeof(ec_datagram_t) + pd_len + 2u);
            p_dg->cmd = EC_CMD_LRW;
            p_dg->idx = pd->cdg.p_idx->idx;
            p_dg->adr = pd->log;
            p_dg->len = pd_len;
            p_dg->irq = 0;

            pd->cdg.p_entry->user_cb = cb_process_data_group;
            pd->cdg.p_entry->user_arg = (void *)&pd->group;

            if (pd->pd != NULL) {
                (void)memcpy(ec_datagram_payload(p_dg), pd->pd, pd->pdout_len);
            }

            // queue frame and trigger tx
            pool_put(&pec->hw.tx_high, pd->cdg.p_entry);

            // set timeout
            osal_timer_init(&pd->cdg.timeout, pd->cdg.recv_timeout_ns);
        }
    }

    osal_mutex_unlock(&pd->cdg.lock);

    return ret;
}

//! Receive answer of reading mailbox state from slaves without lrw mapped state.
/*!
 * \param[in]   pec     Pointer to EtherCAT master structure.
 * \param[in]   slave   Number of EtherCAT slave.
 * \return EC_OK on success or error code.
 */
static int ec_receive_mbx_state(ec_t *pec, int slave) {
    assert(pec != NULL);

    int ret = EC_OK;

    ec_slave_ptr(slv, pec, slave);
    ec_datagram_t *p_dg = ec_datagram_cast(slv->mbx.cdg.p_entry->data);

    osal_uint16_t wkc = 0u;
    wkc = ec_datagram_wkc(p_dg);
    if (wkc != 0u) {
        (void)memcpy(&slv->mbx.mbx_state, ec_datagram_payload(p_dg), sizeof(osal_uint8_t));
    }

    if ((slv->mbx.mbx_state & 0x08u) != 0u) {
        ec_log(100, __func__, "slave %2d: sm_state %X\n", slave, slv->mbx.mbx_state);
        ec_mbx_sched_read(pec, slave);
    }

    pool_put(&pec->pool, slv->mbx.cdg.p_entry);
    ec_index_put(&pec->idx_q, slv->mbx.cdg.p_idx);

    slv->mbx.cdg.p_entry = NULL;
    slv->mbx.cdg.p_idx = NULL;

    return ret;
}

//! datagram callack for receiving mbx_state answer
static void cb_mbx_state(struct ec *pec, void *user_arg, struct pool_entry *p) {
    int slave = *(osal_uint32_t *)user_arg;
    ec_slave_ptr(slv, pec, slave);
    (void)p;

#ifdef LIBETHERCAT_DEBUG
    ec_log(100, __func__, "slave %2d: received mbx state\n", slave);
#endif

    osal_mutex_lock(&slv->mbx.cdg.lock);
    (void)ec_receive_mbx_state(pec, slave);
    osal_mutex_unlock(&slv->mbx.cdg.lock);
}

//! send mailbox sync manager state for slaves when state is not lrw mapped.
/*!
 * \param[in] pec       Ethercat master pointer.
 * \param[in] slave     Slave number.
 * \return 0 on success
 */
static int ec_send_mbx_state(ec_t *pec, int slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    int ret = EC_OK;
    ec_datagram_t *p_dg;
    ec_slave_ptr(slv, pec, slave);

#ifdef LIBETHERCAT_DEBUG
    ec_log(100, __func__, "slave %2d: sending mbx_state\n", slave);
#endif

    osal_mutex_lock(&slv->mbx.cdg.lock);

    if ((slv->mbx.cdg.p_entry != NULL) || (slv->mbx.cdg.p_idx != NULL)) {
        ec_log(1, __func__, "slave %2d: already sent mbx_state frame, check for timeout...\n", slave);

        if (osal_timer_expired(&slv->mbx.cdg.timeout) == OSAL_ERR_TIMEOUT) {
            ec_log(1, __func__, "got timeout on last frame... sending new one!\n");
            pool_put(&pec->pool, slv->mbx.cdg.p_entry);
            ec_index_put(&pec->idx_q, slv->mbx.cdg.p_idx);

            slv->mbx.cdg.p_entry = NULL;
            slv->mbx.cdg.p_idx = NULL;
        } else {
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    if (ret == EC_OK) {
        if (ec_index_get(&pec->idx_q, &slv->mbx.cdg.p_idx) != EC_OK) {
            ec_log(1, __func__, "error getting ethercat index\n");
            ret = EC_ERROR_OUT_OF_INDICES;
        } else if (pool_get(&pec->pool, &slv->mbx.cdg.p_entry, NULL) != EC_OK) {
            ec_index_put(&pec->idx_q, slv->mbx.cdg.p_idx);
            ec_log(1, __func__, "error getting datagram from pool\n");
            ret = EC_ERROR_OUT_OF_DATAGRAMS;
        } else {
            p_dg = ec_datagram_cast(slv->mbx.cdg.p_entry->data);
            (void)memset(p_dg, 0, sizeof(ec_datagram_t) + sizeof(osal_uint8_t) + 2u);
            p_dg->cmd = EC_CMD_FPRD;
            p_dg->idx = slv->mbx.cdg.p_idx->idx;
            p_dg->adr = ((osal_uint32_t)(EC_REG_SM1STAT) << 16u) | (osal_uint32_t)((slv->fixed_address) & 0xFFFFu);
            p_dg->len = sizeof(osal_uint8_t);
            p_dg->irq = 0;

            slv->mbx.cdg.p_entry->user_cb = cb_mbx_state;
            slv->mbx.cdg.p_entry->user_arg = (void *)&slv->slave;

            // queue frame and trigger tx
            pool_put(&pec->hw.tx_high, slv->mbx.cdg.p_entry);

            // set timeout
            osal_timer_init(&slv->mbx.cdg.timeout, 10000000);
        }
    }

    osal_mutex_unlock(&slv->mbx.cdg.lock);

    return ret;
}

//! send mailbox sync manager state for slaves in group when state is not lrw mapped.
/*!
 * \param[in] pec       Ethercat master pointer.
 * \param[in] group     Group number.
 * \return 0 on success
 */
static int ec_send_mbx_state_group(ec_t *pec, int group) {
    assert(pec != NULL);

    int ret = EC_OK;

    for (osal_uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
        ec_slave_ptr(slv, pec, slave);

        if (    (slv->assigned_pd_group != group) ||
                (slv->eeprom.mbx_supported == 0u) ||
                (slv->mbx.map_mbx_state == OSAL_TRUE)) { continue; }

        (void)ec_send_mbx_state(pec, slave);
    }

    return ret;
}

//! send process data with logical commands
/*!
 * \param[in]   pec     Pointer to EtherCAT master struct.
 * \return EC_OK on success
 */
int ec_send_process_data(ec_t *pec) {
    int ret = EC_OK;
    int i;

    for (i = 0; i < pec->pd_group_cnt; ++i) {
        ec_pd_group_t *pd = &pec->pd_groups[i];

        if ((++pd->divisor_cnt % pd->divisor) == 0) {
            // reset divisor cnt and queue datagram
            pd->divisor_cnt = 0;
            ret = ec_send_process_data_group(pec, i);
            (void)ec_send_mbx_state_group(pec, i);
        }

        if (ret != EC_OK) {
            break;
        }
    }

    return ret;
}

#define sign(x)     (((x) < 0 ? -1 : (x) > 0 ? 1 : 0))

//! receive distributed clocks sync datagram
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
static int ec_receive_distributed_clocks_sync(ec_t *pec) {
    assert(pec != NULL);

    int ret = EC_OK;
    osal_uint16_t wkc; 
    ec_datagram_t *p_dg = NULL;

    p_dg = ec_datagram_cast(pec->dc.cdg.p_entry->data);
    wkc = ec_datagram_wkc(p_dg);

    pec->dc.packet_duration = osal_timer_gettime_nsec() - pec->dc.sent_time_nsec;

    if (wkc != 0u) {
        (void)memcpy((osal_uint8_t *)&pec->dc.dc_time, (osal_uint8_t *)ec_datagram_payload(p_dg), 8);

        // get clock difference
        pec->dc.act_diff = signed64_diff(pec->dc.rtc_time, pec->dc.dc_time) % pec->dc.timer_override; 
        if (pec->dc.act_diff > (pec->dc.timer_override/2)) { pec->dc.act_diff -= pec->dc.timer_override; }
        else if (pec->dc.act_diff < -1.*(pec->dc.timer_override/2)) { pec->dc.act_diff += pec->dc.timer_override; }

        if (pec->dc.mode == dc_mode_ref_clock) {
            // calc proportional part
            double p_part = pec->dc.control.kp * pec->dc.act_diff;
            
            // sum it up for integral part
            pec->dc.control.diffsum += pec->dc.control.ki * pec->dc.act_diff;

            // limit diffsum
            if (pec->dc.control.diffsum > pec->dc.control.diffsum_limit) { 
                pec->dc.control.diffsum = pec->dc.control.diffsum_limit; 
            } else if (pec->dc.control.diffsum < (-1 * pec->dc.control.diffsum_limit)) { 
                pec->dc.control.diffsum = -1 * pec->dc.control.diffsum_limit; 
            }

            pec->dc.timer_correction = p_part + pec->dc.control.diffsum;
        } else if (pec->dc.mode == dc_mode_master_clock) {
            // sending offset compensation value to dc master clock
            pool_entry_t *p_entry_dc_sto;
            idx_entry_t *p_idx_sto;

            // dc system time offset frame
            if (ec_index_get(&pec->idx_q, &p_idx_sto) != EC_OK) {
                ec_log(1, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC", "error getting ethercat index\n");
                ret = EC_ERROR_OUT_OF_INDICES;
            } else if (pool_get(&pec->pool, &p_entry_dc_sto, NULL) != EC_OK) {
                ec_index_put(&pec->idx_q, p_idx_sto);
                ec_log(1, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC", "error getting datagram from pool\n");
                ret = EC_ERROR_OUT_OF_DATAGRAMS;
            } else {
                // correct system time offset, sync ref_clock to master_clock
                // only correct half diff to avoid overshoot in slave.
                pec->dc.dc_sto += pec->dc.act_diff / 2.;
                p_dg = ec_datagram_cast(p_entry_dc_sto->data);
                (void)memset(p_dg, 0, sizeof(ec_datagram_t) + 10u);
                p_dg->cmd = EC_CMD_FPWR;
                p_dg->idx = p_idx_sto->idx;
                p_dg->adr = ((osal_uint32_t)EC_REG_DCSYSOFFSET << 16u) | pec->dc.master_address;
                p_dg->len = sizeof(pec->dc.dc_sto);
                p_dg->irq = 0;
                (void)memcpy(ec_datagram_payload(p_dg), (osal_uint8_t *)&pec->dc.dc_sto, sizeof(pec->dc.dc_sto));
                // we don't care about the answer, cb_no_reply frees datagram 
                // and index
                p_entry_dc_sto->user_cb = cb_no_reply;
                p_entry_dc_sto->user_arg = p_idx_sto;

                // queue frame and trigger tx
                pool_put(&pec->hw.tx_low, p_entry_dc_sto);
            }
        } else {}
    }

    return ret;
}


//! local callack for syncronous read/write
static void cb_distributed_clocks(struct ec *pec, void *user_arg, struct pool_entry *p) {
    (void)user_arg;
    (void)p;

#ifdef LIBETHERCAT_DEBUG
    ec_log(100, __func__, "received distributed clock\n");
#endif

    osal_mutex_lock(&pec->dc.cdg.lock);
    (void)ec_receive_distributed_clocks_sync(pec);
    osal_mutex_unlock(&pec->dc.cdg.lock);

    if (pec->dc.cdg.user_cb != NULL) {
        (*pec->dc.cdg.user_cb)(pec->dc.cdg.user_cb_arg, 0);
    }

    return;
}

//! send distributed clock sync datagram
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_send_distributed_clocks_sync(ec_t *pec) {
    assert(pec != NULL);

    int ret = EC_OK;
    ec_datagram_t *p_dg = NULL;

#ifdef LIBETHERCAT_DEBUG
    ec_log(100, __func__, "sending distributed clock\n");
#endif

    osal_mutex_lock(&pec->dc.cdg.lock);
    
    if (!pec->dc.have_dc || !pec->dc.rtc_sto) {
        ret = EC_ERROR_UNAVAILABLE;
    } else {
        if (pec->dc.cdg.p_idx == NULL) {
            if (ec_index_get(&pec->idx_q, &pec->dc.cdg.p_idx) != EC_OK) {
                ec_log(1, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", "error getting ethercat index\n");
                ret = EC_ERROR_OUT_OF_INDICES;
            } 
        }

        if ((ret == EC_OK) && (pec->dc.cdg.p_entry == NULL)) {
            if (pool_get(&pec->pool, &pec->dc.cdg.p_entry, NULL) != EC_OK) {
                ec_index_put(&pec->idx_q, pec->dc.cdg.p_idx);
                ec_log(1, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", "error getting datagram from pool\n");
                ret = EC_ERROR_OUT_OF_DATAGRAMS;
            }
        }

        if (ret == EC_OK) {
            p_dg = ec_datagram_cast(pec->dc.cdg.p_entry->data);
            (void)memset(p_dg, 0, sizeof(ec_datagram_t) + 8u + 2u);

            if (pec->dc.mode == dc_mode_master_as_ref_clock) {
                p_dg->cmd = EC_CMD_BWR;
                p_dg->idx = pec->dc.cdg.p_idx->idx;
                p_dg->adr = ((osal_uint32_t)EC_REG_DCSYSTIME << 16u);
                p_dg->len = 8;
                p_dg->irq = 0;
            } else {
                p_dg->cmd = EC_CMD_FRMW;
                p_dg->idx = pec->dc.cdg.p_idx->idx;
                p_dg->adr = ((osal_uint32_t)EC_REG_DCSYSTIME << 16u) | pec->dc.master_address;
                p_dg->len = 8;
                p_dg->irq = 0;
            }

            pec->dc.cdg.p_entry->user_cb = cb_distributed_clocks;
            pec->dc.cdg.p_entry->user_arg = pec;

            osal_uint64_t act_rtc_time = osal_timer_gettime_nsec();

            if (pec->dc.mode == dc_mode_ref_clock) {
                if (pec->dc.timer_override > 0) {
                    if (pec->dc.rtc_time == 0u) {
                        //int64_t tmp = (int64_t)act_rtc_time - pec->dc.rtc_sto;
                        //pec->dc.rtc_time = (osal_uint64_t)tmp;
                    } else {
                        pec->dc.rtc_time += (osal_uint64_t)(pec->dc.timer_override);
                    }
                }   
            } else if (pec->dc.mode == dc_mode_master_as_ref_clock) {
                pec->dc.rtc_time = (int64_t)act_rtc_time - pec->dc.rtc_sto;
                (void)memcpy((osal_uint8_t *)ec_datagram_payload(p_dg), (osal_uint8_t *)&pec->dc.rtc_time, sizeof(pec->dc.rtc_time));
            } else {
                pec->dc.rtc_time = (int64_t)act_rtc_time - pec->dc.rtc_sto;
            }

            // queue frame and trigger tx
            pool_put(&pec->hw.tx_high, pec->dc.cdg.p_entry);

            pec->dc.sent_time_nsec = act_rtc_time;
            osal_timer_init(&pec->dc.cdg.timeout, pec->dc.cdg.recv_timeout_ns);
        }
    }
    
    osal_mutex_unlock(&pec->dc.cdg.lock);
      
    return ret;
}

//! receive broadcast read to ec_state
/*!
 * \param pec ethercat master pointer
 * \param timeout for waiting for packet
 * \return 0 on success
 */
static int ec_receive_brd_ec_state(ec_t *pec) {
    assert(pec != NULL);

    ec_datagram_t *p_dg = NULL;
    int ret = EC_OK;
    osal_uint16_t al_status = 0u;

    static int wkc_mismatch_cnt_ec_state = 0;

    osal_uint16_t wkc;
    p_dg = ec_datagram_cast(pec->cdg_state.p_entry->data);

    wkc = ec_datagram_wkc(p_dg);
    (void)memcpy((osal_uint8_t *)&al_status, ec_datagram_payload(p_dg), 2u);

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
        static int ec_state_mismatch_cnt = 0;

        if ((ec_state_mismatch_cnt++%1000) == 0) {
            ec_log(1, __func__, "al status mismatch, got 0x%X, master state is 0x%X\n", 
                    al_status, pec->master_state);
            ec_async_check_all(&pec->async_loop);
        }
    }

    return ret;
}

//! local callack for syncronous read/write
static void cb_brd_ec_state(struct ec *pec, void *user_arg, struct pool_entry *p) {
    (void)user_arg;
    (void)p;

#ifdef LIBETHERCAT_DEBUG
    ec_log(100, __func__, "received broadcast ec state\n");
#endif

    osal_mutex_lock(&pec->cdg_state.lock);
    (void)ec_receive_brd_ec_state(pec);
    osal_mutex_unlock(&pec->cdg_state.lock);
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
    
#ifdef LIBETHERCAT_DEBUG
    ec_log(100, __func__, "sending broadcast ec state\n");
#endif

    osal_mutex_lock(&pec->cdg_state.lock);

    if (pec->cdg_state.p_idx == NULL) {
        if (ec_index_get(&pec->idx_q, &pec->cdg_state.p_idx) != EC_OK) {
            ec_log(1, __func__, "error getting ethercat index\n");
            ret = EC_ERROR_OUT_OF_INDICES;
        } 
    }

    if ((ret == EC_OK) && (pec->cdg_state.p_entry == NULL)) {
        if (pool_get(&pec->pool, &pec->cdg_state.p_entry, NULL) != EC_OK) {
            ec_index_put(&pec->idx_q, pec->cdg_state.p_idx);
            ec_log(1, __func__, "error getting datagram from pool\n");
            ret = EC_ERROR_OUT_OF_DATAGRAMS;
        } else {
            pec->cdg_state.p_entry->user_cb = cb_brd_ec_state;
            pec->cdg_state.p_entry->user_arg = NULL;
        }
    } 

    if (ret == EC_OK) {
        p_dg = ec_datagram_cast(pec->cdg_state.p_entry->data);

        (void)memset(p_dg, 0, sizeof(ec_datagram_t) + 4u + 2u);
        p_dg->cmd = EC_CMD_BRD;
        p_dg->idx = pec->cdg_state.p_idx->idx;
        p_dg->adr = (osal_uint32_t)EC_REG_ALSTAT << 16u;
        p_dg->len = 2;
        p_dg->irq = 0;

        // queue frame and trigger tx
        pool_put(&pec->hw.tx_high, pec->cdg_state.p_entry);
        osal_timer_init(&pec->cdg_state.timeout, 10000000);
    }
    
    osal_mutex_unlock(&pec->cdg_state.lock);

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

//! \brief Configures distributed clocks settings on EtherCAT master.
/*!
 * \param[in] pec           Pointer to EtherCAT master structure.
 * \param[in] timer         Fixed expected cyclic timer value.
 * \param[in] mode          Distributed clock operating mode.
 * \param[in] user_cb       Callback when DC datagram returned, maybe NULL.
 * \param[in] user_arg      Argument passed to 'user_cb', maybe NULL.
 */
void ec_configure_dc(ec_t *pec, osal_uint64_t timer, ec_dc_mode_t mode, 
    void (*user_cb)(void *arg, int num), void *user_cb_arg) 
{
    assert(pec != NULL);

    pec->dc.timer_override = timer;
    pec->dc.mode = mode;
    pec->dc.cdg.user_cb = user_cb;
    pec->dc.cdg.user_cb_arg = user_cb_arg;
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

