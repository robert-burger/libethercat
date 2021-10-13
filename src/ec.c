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
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

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
#include "libethercat/eeprom.h"

#define DC_DCSOFF_SAMPLES 1000

#ifndef max
#define max(a, b) \
    ((a) > (b) ? (a) : (b))
#endif

void default_log_func(int lvl, void* user, const char *format, ...){
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

void *ec_log_func_user = NULL;
void (*ec_log_func)(int lvl, void *user, const char *format, ...) = default_log_func;



void ec_log(int lvl, const char *pre, const char *format, ...) {
    if (ec_log_func != NULL) {
        char buf[512];
        char *tmp = &buf[0];

        // format argument list
        va_list args;
        va_start(args, format);
        int ret = snprintf(tmp, 512, "%-20.20s: ", pre);
        vsnprintf(tmp+ret, 512-ret, format, args);

        ec_log_func(lvl, ec_log_func_user, buf);
    }
}

//! create process data groups
/*!
 * \param pec ethercat master pointer
 * \param pd_group_cnt number of groups to create
 * \return 0 on success
 */
int ec_create_pd_groups(ec_t *pec, int pd_group_cnt) {
    assert(pec != NULL);

    int i;
    ec_destroy_pd_groups(pec);

    pec->pd_group_cnt = pd_group_cnt;
    alloc_resource(pec->pd_groups, ec_pd_group_t, sizeof(ec_pd_group_t) * 
            pd_group_cnt);
    for (i = 0; i < pec->pd_group_cnt; ++i) {
        pec->pd_groups[i].log       = 0x10000 * (i+1);
        pec->pd_groups[i].log_len   = 0;
        pec->pd_groups[i].pd        = NULL;
        pec->pd_groups[i].pdout_len = 0;
        pec->pd_groups[i].pdin_len  = 0;
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

    int i;

    if (pec->pd_groups) {
        for (i = 0; i < pec->pd_group_cnt; ++i)
            free_resource(pec->pd_groups[i].pd);
        free(pec->pd_groups);
    }

    pec->pd_group_cnt = 0;
    pec->pd_groups = NULL;

    return 0;
}

const char state_string_boot[]    = "EC_STATE_BOOT";
const char state_string_init[]    = "EC_STATE_INIT";
const char state_string_preop[]   = "EC_STATE_PREOP";
const char state_string_safeop[]  = "EC_STATE_SAFEOP";
const char state_string_op[]      = "EC_STATE_OP";
const char state_string_unknown[] = "EC_STATE_UNKNOWN";

const char *get_state_string(ec_state_t state) {
    if (state == EC_STATE_BOOT)
        return state_string_boot;
    if (state == EC_STATE_INIT)
        return state_string_init;
    if (state == EC_STATE_PREOP)
        return state_string_preop;
    if (state == EC_STATE_SAFEOP)
        return state_string_safeop;
    if (state == EC_STATE_OP)
        return state_string_op;

    return state_string_unknown;
}

void ec_create_logical_mapping_lrw(ec_t *pec, int group) {
    assert(pec != NULL);
    assert(group < pec->pd_group_cnt);

    int i, k;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    pd->pdout_len = pd->pdin_len = 0;
    pd->pd_lrw_len = 0;
    pd->wkc_expected = 0;

    for (i = 0; i < pec->slave_cnt; ++i) {
        ec_slave_t *slv = &pec->slaves[i];
        int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

        if (slv->assigned_pd_group != group)
            continue;

        size_t slv_pdin_len = 0, slv_pdout_len = 0;

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if (slv->sm[k].flags & 0x00000004) {
                slv_pdout_len += slv->sm[k].len; // outputs
            } else  {
                slv_pdin_len += slv->sm[k].len;  // inputs
            }
        }

        if (slv->eeprom.mbx_supported) {
            // add state of sync manager read mailbox
            slv_pdin_len += 1;
        }
        
        size_t max_len = max(slv_pdout_len, slv_pdin_len);

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
    pd->pd = (uint8_t *)malloc(pd->log_len);
    memset(pd->pd, 0, pd->log_len);

    uint8_t *pdout = pd->pd,
            *pdin = pd->pd + pd->pdout_len;

    uint32_t log_base = pd->log;

    for (i = 0; i < pec->slave_cnt; ++i) {
        ec_slave_t *slv = &pec->slaves[i];
        int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

        if (slv->assigned_pd_group != group)
            continue;

        int fmmu_next = 0;
        int wkc_expected = 0;

        uint8_t *tmp_pdout = pdout, *tmp_pdin = pdin;
        uint32_t log_base_out = log_base, log_base_in = log_base;

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if ((!slv->sm[k].len))
                continue; // empty 

            if (slv->sm[k].flags & 0x00000004) {
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
                    } else 
                        slv->pdout.len += slv->sm[k].len;

                    wkc_expected |= 2;

                    int z;
                    int pdoff = 0;
                    for (z = 0; z < slv->subdev_cnt; ++z) {
                        slv->subdevs[z].pdout.pd = pdout + pdoff;
                        pdoff += slv->subdevs[z].pdout.len;
                    }
                }

                tmp_pdout += slv->sm[k].len;
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
                    } else 
                        slv->pdin.len += slv->sm[k].len;

                    wkc_expected |= 1;

                    int z;
                    int pdoff = 0;
                    for (z = 0; z < slv->subdev_cnt; ++z) {
                        slv->subdevs[z].pdin.pd = pdin + pdoff;
                        pdoff += slv->subdevs[z].pdin.len;
                    }
                }

                tmp_pdin += slv->sm[k].len;
                log_base_in += slv->sm[k].len;
            }

            fmmu_next++;
        }

        if (slv->eeprom.mbx_supported) {
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
                    slv->pdin.len = 1;
                } else 
                    slv->pdin.len += 1;

                slv->mbx.sm_state = tmp_pdin;

                wkc_expected |= 1;
            }

            tmp_pdin += 1;
            log_base_in += 1;
        }

        pdin += max(slv->pdin.len, slv->pdout.len);
        pdout += max(slv->pdin.len, slv->pdout.len);
        log_base = max(log_base_in, log_base_out);
        pd->wkc_expected += wkc_expected;
    }
}

void ec_create_logical_mapping(ec_t *pec, int group) {
    assert(pec != NULL);
    assert(group < pec->pd_group_cnt);

    int i, k;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    pd->pdout_len = pd->pdin_len = 0;
    pd->wkc_expected = 0;

    for (i = 0; i < pec->slave_cnt; ++i) {
        ec_slave_t *slv = &pec->slaves[i];
        int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

        if (slv->assigned_pd_group != group)
            continue;

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if (slv->sm[k].flags & 0x00000004) {
                pd->pdout_len += slv->sm[k].len; // outputs
            } else  {
                pd->pdin_len += slv->sm[k].len;  // inputs
            }
        }

        if (slv->eeprom.mbx_supported)
            // add state of sync manager read mailbox
            pd->pdin_len += 1; 
    }

    ec_log(10, "CREATE_LOGICAL_MAPPING", "group %2d: pd out 0x%08X "
            "%3d bytes, in 0x%08X %3d bytes\n", group, pd->log, 
            pd->pdout_len, pd->log + pd->pdout_len, pd->pdin_len);

    pd->log_len = pd->pdout_len + pd->pdin_len;
    pd->pd = (uint8_t *)malloc(pd->log_len);
    memset(pd->pd, 0, pd->log_len);

    uint8_t *pdout = pd->pd,
            *pdin = pd->pd + pd->pdout_len;

    uint32_t log_base_out = pd->log,
             log_base_in = pd->log + pd->pdout_len;

    for (i = 0; i < pec->slave_cnt; ++i) {
        ec_slave_t *slv = &pec->slaves[i];
        int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

        if (slv->assigned_pd_group != group)
            continue;

        int fmmu_next = 0;
        int wkc_expected = 0;

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if ((!slv->sm[k].len))
                continue; // empty 

            if (slv->sm[k].flags & 0x00000004) {
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
                    } else 
                        slv->pdout.len += slv->sm[k].len;

                    wkc_expected |= 2;

                    int z;
                    int pdoff = 0;
                    for (z = 0; z < slv->subdev_cnt; ++z) {
                        slv->subdevs[z].pdout.pd = pdout + pdoff;
                        pdoff += slv->subdevs[z].pdout.len;
                    }
                }

                pdout += slv->sm[k].len;
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
                    } else 
                        slv->pdin.len += slv->sm[k].len;

                    wkc_expected |= 1;

                    int z;
                    int pdoff = 0;
                    for (z = 0; z < slv->subdev_cnt; ++z) {
                        slv->subdevs[z].pdin.pd = pdin + pdoff;
                        pdoff += slv->subdevs[z].pdin.len;
                    }
                }

                pdin += slv->sm[k].len;
                log_base_in += slv->sm[k].len;
            }

            fmmu_next++;
        }

        if (slv->eeprom.mbx_supported) {
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
                } else 
                    slv->pdin.len += 1;

                slv->mbx.sm_state = pdin;

                wkc_expected |= 1;
            }

            pdin += 1;
            log_base_in += 1;
        }

        pd->wkc_expected += wkc_expected;
    }
}

void *prepare_state_transition_wrapper(void *arg) {
    worker_arg_t *tmp = (worker_arg_t *)arg;
    
    ec_log(100, get_state_string(tmp->state), 
            "prepare state transition for slave %d\n", tmp->slave);
    ec_slave_prepare_state_transition(tmp->pec, tmp->slave, tmp->state);

    ec_log(100, get_state_string(tmp->state), 
            "generate mapping for slave %d\n", tmp->slave);
    ec_slave_generate_mapping(tmp->pec, tmp->slave);
    return NULL;
}

void *set_state_wrapper(void *arg) {
    worker_arg_t *tmp = (worker_arg_t *)arg;

    ec_log(100, get_state_string(tmp->state), 
            "setting state for slave %d\n", tmp->slave);
    ec_slave_state_transition(tmp->pec, tmp->slave, tmp->state); 

    return NULL;
}

//! loop over all slaves and prepare state transition
/*! 
 * \param pec ethercat master pointer
 * \param state new state to set
 */
void ec_prepare_state_transition_loop(ec_t *pec, ec_state_t state) {
    assert(pec != NULL);

    if (pec->threaded_startup) {
        for (int slave = 0; slave < pec->slave_cnt; ++slave) {
            if (pec->slaves[slave].assigned_pd_group == -1)
                continue;

            pec->slaves[slave].worker_arg.pec   = pec;
            pec->slaves[slave].worker_arg.slave = slave;
            pec->slaves[slave].worker_arg.state = state;

            pthread_create(&(pec->slaves[slave].worker_tid), NULL, 
                    prepare_state_transition_wrapper, 
                    &(pec->slaves[slave].worker_arg));
        }

        for (int slave = 0; slave < pec->slave_cnt; ++slave) {
            if (pec->slaves[slave].assigned_pd_group == -1)
                continue;

            pthread_join(pec->slaves[slave].worker_tid, NULL);
        }

        return;        
    } 
        
    for (int slave = 0; slave < pec->slave_cnt; ++slave) {                  
        if (pec->slaves[slave].assigned_pd_group == -1)
            continue;

        ec_log(100, get_state_string(state), "prepare state transition for slave %d\n", slave);
        ec_slave_prepare_state_transition(pec, slave, state);

        ec_log(100, get_state_string(state), "generate mapping for slave %d\n", slave);
        ec_slave_generate_mapping(pec, slave);
    }
}

//! loop over all slaves and set state
/*! 
 * \param pec ethercat master pointer
 * \param state new state to set
 * \param with_group if set, only slaves with assigned group are processed
 */
void ec_state_transition_loop(ec_t *pec, ec_state_t state, uint8_t with_group) {
    assert(pec != NULL);

    if (pec->threaded_startup) {
        for (int slave = 0; slave < pec->slave_cnt; ++slave) {
            if (with_group && (pec->slaves[slave].assigned_pd_group == -1))
                continue;

            pec->slaves[slave].worker_arg.pec   = pec;
            pec->slaves[slave].worker_arg.slave = slave;
            pec->slaves[slave].worker_arg.state = state;

            pthread_create(&(pec->slaves[slave].worker_tid), NULL, 
                    set_state_wrapper, 
                    &(pec->slaves[slave].worker_arg));
        }

        for (int slave = 0; slave < pec->slave_cnt; ++slave) {
            if (with_group && (pec->slaves[slave].assigned_pd_group == -1))
                continue;

            pthread_join(pec->slaves[slave].worker_tid, NULL);
        }

        return;
    } 
    
    for (int slave = 0; slave < pec->slave_cnt; ++slave) {
        ec_log(100, get_state_string(state), "slave %d, with_group %d, assigned %d\n", 
            slave, with_group, pec->slaves[slave].assigned_pd_group);
        if (with_group && (pec->slaves[slave].assigned_pd_group == -1))
            continue;

        ec_log(100, get_state_string(state), "setting state for slave %d\n", slave);
        ec_slave_state_transition(pec, slave, state); 
    }
}
//! scan ethercat bus for slaves and create strucutres
/*! 
 * \param pec ethercat master pointer
 */
void ec_scan(ec_t *pec) {
    assert(pec != NULL);

    uint16_t fixed = 1000, wkc = 0, val = 0, i;

    ec_state_t init_state = EC_STATE_INIT | EC_STATE_RESET;
    ec_bwr(pec, EC_REG_ALCTL, &init_state, sizeof(init_state), &wkc); 

    if (pec->slaves) {
        int cnt = pec->slave_cnt;
        pec->slave_cnt = 0;

        // free resources
        for (i = 0; i < cnt; ++i) {
            ec_slave_free(pec, i);
        }

        free_resource(pec->slaves);
    }
    
    // allocating slave structures
    ec_brd(pec, EC_REG_TYPE, (uint8_t *)&val, sizeof(val), &wkc); 
    pec->slave_cnt = wkc;
    
    alloc_resource(pec->slaves, ec_slave_t, pec->slave_cnt * 
            sizeof(ec_slave_t));

    for (i = 0; i < pec->slave_cnt; ++i) {
        int auto_inc = -1 * i;

        ec_aprd(pec, auto_inc, EC_REG_TYPE, (uint8_t *)&val, 
                sizeof(val), &wkc);

        if (wkc == 0)
            break;  // break here, cause there seems to be no more slave

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

        ec_apwr(pec, auto_inc, EC_REG_STADR, (uint8_t *)&fixed, 
                sizeof(fixed), &wkc); 
        if (wkc == 0)
            ec_log(1, __func__, "slave %2d: error writing fixed "
                    "address %d\n", i, fixed);

        // set eeprom to pdi, some slaves need this
        ec_eeprom_to_pdi(pec, i);
        init_state = EC_STATE_INIT | EC_STATE_RESET;
        ec_fpwr(pec, fixed, EC_REG_ALCTL, &init_state, 
                sizeof(init_state), &wkc); 

        fixed++;
    }

    ec_log(10, __func__, "found %d ethercat slaves\n", i);

    for (int slave = 0; slave < pec->slave_cnt; ++slave) {
        ec_slave_ptr(slv, pec, slave); 
        slv->link_cnt = 0;
        slv->active_ports = 0;

        uint16_t topology = 0;
        ec_fprd(pec, slv->fixed_address, EC_REG_DLSTAT, &topology,
                sizeof(topology), &wkc);

        // check if port is open and communication established
#define active_port(port) \
        if ( (topology & (3 << (8 + (2 * (port)))) ) == \
                (2 << (8 + (2 * (port)))) ) { \
            slv->link_cnt++; slv->active_ports |= 1<<(port); }

        for (int port = 0; port < 4; ++port)
            active_port(port);

        // read out physical type
        ec_fprd(pec, slv->fixed_address, EC_REG_PORTDES, &slv->ptype, 
                sizeof(slv->ptype), &wkc);

        // 0 = no links, not possible 
        // 1 =  1 link , end of line 
        // 2 =  2 links, one before and one after 
        // 3 =  3 links, split point 
        // 4 =  4 links, cross point 

        // search for parent
        slv->parent = -1; // parent is master at beginning
        if (slave >= 1) {
            int topoc = 0, tmp_slave = slave - 1;
            do {
                topology = pec->slaves[tmp_slave].link_cnt;
                if (topology == 1)
                    topoc--;    // endpoint found
                if (topology == 3)
                    topoc++;    // split found
                if (topology == 4)
                    topoc += 2; // cross found
                if (((topoc >= 0) && (topology > 1)) || (tmp_slave == 0)) { 
                    slv->parent = tmp_slave; // parent found
                    tmp_slave = 0;
                }
                tmp_slave--;
            }
            while (tmp_slave >= 0);
        }

        ec_log(100, __func__, "slave %2d has parent %d\n", 
                slave, slv->parent);
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

    pthread_mutex_lock(&pec->ec_lock);

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

            if (state == EC_STATE_INIT)
                break;

        case INIT_2_OP: 
        case INIT_2_SAFEOP:
        case INIT_2_PREOP:
        case PREOP_2_PREOP:
            // ====> switch to PREOP stuff
            ec_state_transition_loop(pec, EC_STATE_PREOP, 0);

            if (state == EC_STATE_PREOP)
                break;

        case PREOP_2_SAFEOP:
        case PREOP_2_OP: 
        case SAFEOP_2_SAFEOP:
            // ====> switch to SAFEOP stuff
            ec_dc_config(pec);

            // sending first time dc
            if (ec_send_distributed_clocks_sync(pec) == 0) {
                ec_timer_t dc_timeout;
                ec_timer_init(&dc_timeout, 10000000);
                ec_receive_distributed_clocks_sync(pec, &dc_timeout);
            } else {
                ec_log(1, __func__, "was not able to send first dc frame\n");
            }

            ec_prepare_state_transition_loop(pec, EC_STATE_SAFEOP);

            // ====> create logical mapping for cyclic operation
            for (int group = 0; group < pec->pd_group_cnt; ++group) {
                if (pec->pd_groups[group].use_lrw)
                    ec_create_logical_mapping_lrw(pec, group);
                else
                    ec_create_logical_mapping(pec, group);
            }
            
            ec_state_transition_loop(pec, EC_STATE_SAFEOP, 1);
    
            if (state == EC_STATE_SAFEOP)
                break;
        
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

            if (state == EC_STATE_SAFEOP)
                break;
        case SAFEOP_2_BOOT:
        case SAFEOP_2_INIT:
        case SAFEOP_2_PREOP:
            ec_log(10, __func__, "switching to PREOP\n");
            ec_state_transition_loop(pec, EC_STATE_PREOP, 0);

            // reset dc
            pec->dc.act_diff        = 0;
            pec->dc.timer_prev      = 0;
            pec->dc.dc_time         = 0;
            pec->dc.dc_cycle_sum    = 0;
            pec->dc.dc_cycle_cnt    = 0;
            pec->dc.rtc_time        = 0;
            pec->dc.rtc_cycle_sum   = 0;
            pec->dc.rtc_cycle       = 0;
            pec->dc.rtc_count       = 0;
            pec->dc.act_diff        = 0;

            pec->master_state = EC_STATE_PREOP;

            if (state == EC_STATE_PREOP)
                break;
        case PREOP_2_BOOT:
        case PREOP_2_INIT:
            ec_log(10, __func__, "switching to INIT\n");
            ec_state_transition_loop(pec, EC_STATE_INIT, 0);
            pec->master_state = EC_STATE_INIT;
            ec_log(10, __func__, "doing rescan\n");
            ec_scan(pec);
            ec_state_transition_loop(pec, EC_STATE_INIT, 0);

            if (state == EC_STATE_INIT)
                break;
        case INIT_2_BOOT:
        case BOOT_2_BOOT:
            ec_state_transition_loop(pec, EC_STATE_BOOT, 0);
            break;
        default:
            break;
    };
        
    pec->master_state = state;
    pec->state_transition_pending = 0;

    pthread_mutex_unlock(&pec->ec_lock);

    return pec->master_state;
}

pthread_t ec_tx;
void *ec_tx_thread(void *arg) {
    ec_t *pec = (ec_t *)arg;

    while (1) {
        hw_tx(pec->phw);
        ec_sleep(1000000);
    }

    return 0;
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
int ec_open(ec_t **ppec, const char *ifname, int prio, int cpumask,
        int eeprom_log) {
    assert(ppec != NULL);
    assert(ifname != NULL);

    //int i;
    ec_t *pec = malloc(sizeof(ec_t)); 
    (*ppec) = pec;
    if (!pec)
        return ENOMEM;

    ec_index_init(&pec->idx_q, 256);
    pec->master_state       = EC_STATE_UNKNOWN;

    // slaves'n groups
    pec->phw                = NULL;
    pec->slave_cnt          = 0;
    pec->pd_group_cnt       = 0;
    pec->slaves             = NULL;
    pec->pd_groups          = NULL;
    pec->tx_sync            = 1;
    pec->threaded_startup   = 0;
    pec->consecutive_max_miss   = 10;
    pec->state_transition_pending = 0;

    // init values for distributed clocks
    pec->dc.have_dc         = 0;
    pec->dc.dc_time         = 0;
    pec->dc.dc_cycle_sum    = 0;
    pec->dc.dc_cycle_cnt    = 0;
    pec->dc.rtc_time        = 0;
    pec->dc.rtc_cycle_sum   = 0;
    pec->dc.rtc_cycle       = 0;
    pec->dc.rtc_count       = 0;
    pec->dc.act_diff        = 0;
    
    pec->tun_fd             = 0;
    pec->tun_ip             = 0;
    pec->tun_running        = 0;
    
    pec->dc.p_de_dc         = NULL;
    pec->dc.p_idx_dc        = NULL;

    // eeprom logging level
    pec->eeprom_log         = eeprom_log;

    pool_open(&pec->pool, 1000, 1518);
        
    if (hw_open(&pec->phw, ifname, prio, cpumask, 0) == -1) {
        pool_close(pec->pool);

        ec_index_deinit(&pec->idx_q);
        free(pec);
        *ppec = NULL;

        return -1;
    }

    ec_async_message_loop_create(&pec->async_loop, pec);

    return 0;
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

    ec_log(10, __func__, "destroying async message pool\n");
    if (pec->async_loop) { ec_async_message_pool_destroy(pec->async_loop);}
    ec_log(10, __func__, "closing hardware handle\n");
    if (pec->phw) { hw_close(pec->phw);}
    ec_log(10, __func__, "freeing frame pool\n");
    if (pec->pool) { pool_close(pec->pool); }

    ec_log(10, __func__, "destroying pd_groups\n");
    ec_index_deinit(&pec->idx_q);
    ec_destroy_pd_groups(pec);

    ec_log(10, __func__, "destroying slaves\n");
    if (pec->slaves) {
        int slave;
        int cnt = pec->slave_cnt;
        pec->slave_cnt = 0;

        for (slave = 0; slave < cnt; ++slave) {
            ec_slave_free(pec, slave);
        }

        free(pec->slaves);
    }

    ec_log(10, __func__, "freeing master instance\n");
    free(pec);

    ec_log(10, __func__, "all done!\n");
    return 0;
}

//! local callack for syncronous read/write
static void cb_block(void *user_arg, struct pool_entry *p) {
    idx_entry_t *entry = (idx_entry_t *)user_arg;
    sem_post(&entry->waiter);
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
int ec_transceive(ec_t *pec, uint8_t cmd, uint32_t adr, 
        uint8_t *data, size_t datalen, uint16_t *wkc) {
    assert(pec != NULL);
    assert(data != NULL);

    pool_entry_t *p_entry;
    ec_datagram_t *p_dg;
    idx_entry_t *p_idx;

    if (ec_index_get(&pec->idx_q, &p_idx) != 0) {
        ec_log(1, "EC_TRANSCEIVE", "error getting ethercat index\n");
        return -1;
    }

    if (pool_get(pec->pool, &p_entry, NULL) != 0) {
        ec_index_put(&pec->idx_q, p_idx);
        ec_log(1, "EC_TRANSCEIVE", "error getting datagram from pool\n");
        return -1;
    }

    p_dg = (ec_datagram_t *)p_entry->data;

    memset(p_dg, 0, sizeof(ec_datagram_t) + datalen + 2);
    p_dg->cmd = cmd;
    p_dg->idx = p_idx->idx;
    p_dg->adr = adr;
    p_dg->len = datalen;
    p_dg->irq = 0;
    memcpy(ec_datagram_payload(p_dg), data, datalen);

    p_entry->user_cb = cb_block;
    p_entry->user_arg = p_idx;

    // queue frame and trigger tx
    pool_put(pec->phw->tx_low, p_entry);

    // send frame immediately if in sync mode
    if (pec->tx_sync) {
        hw_tx(pec->phw);
    }

    // wait for completion
    ec_timer_t timeout;
    ec_timer_init(&timeout, 100000000);   // roundtrip on bus should be shorter than 100ms
    struct timespec ts = { timeout.sec, timeout.nsec };
    int ret = sem_timedwait(&p_idx->waiter, &ts);
    if (ret == -1) {
        ec_log(1, "ec_transceive", "sem_wait returned: %s, cmd 0x%X, adr 0x%X\n", 
                strerror(errno), cmd, adr);
        wkc = 0;
    } else {
        *wkc = ec_datagram_wkc(p_dg);
        if (*wkc)
            memcpy(data, ec_datagram_payload(p_dg), datalen);
    }

    pool_put(pec->pool, p_entry);
    ec_index_put(&pec->idx_q, p_idx);

    return 0;
}

//! local callack for syncronous read/write
static void cb_no_reply(void *user_arg, struct pool_entry *p) {
    idx_entry_t *entry = (idx_entry_t *)user_arg;
    pool_put(entry->pec->pool, p);
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
int ec_transmit_no_reply(ec_t *pec, uint8_t cmd, uint32_t adr, 
        uint8_t *data, size_t datalen) {
    assert(pec != NULL);
    assert(data != NULL);

    pool_entry_t *p_entry;
    ec_datagram_t *p_dg;
    idx_entry_t *p_idx;

    if (ec_index_get(&pec->idx_q, &p_idx) != 0) {
        ec_log(1, "EC_TRANSMIT_NO_REPLY", 
                "error getting ethercat index\n");
        return -1;
    }

    if (pool_get(pec->pool, &p_entry, NULL) != 0) {
        ec_index_put(&pec->idx_q, p_idx);
        ec_log(1, "EC_TRANSMIT_NO_REPLY", 
                "error getting datagram from pool\n");
        return -1;
    }

    p_dg = (ec_datagram_t *)p_entry->data;

    memset(p_dg, 0, sizeof(ec_datagram_t) + datalen + 2);
    p_dg->cmd = cmd;
    p_dg->idx = p_idx->idx;
    p_dg->adr = adr;
    p_dg->len = datalen;
    p_dg->irq = 0;
    memcpy(ec_datagram_payload(p_dg), data, datalen);

    // don't care about answer
    p_idx->pec = pec;
    p_entry->user_cb = cb_no_reply;
    p_entry->user_arg = p_idx;

    // queue frame and return, we don't care about an answer
    pool_put(pec->phw->tx_low, p_entry);

    // send frame immediately if in sync mode
    if (pec->tx_sync) {
        hw_tx(pec->phw);
    }

    return 0;
}

//! send process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \return 0 on success
 */
int ec_send_process_data_group(ec_t *pec, int group) {
    assert(pec != NULL);

    ec_pd_group_t *pd = &pec->pd_groups[group];
    ec_datagram_t *p_dg;
    unsigned pd_len = 0;

    if ((pd->p_entry != NULL) || (pd->p_idx != NULL)) {
        ec_log(1, __func__, "already sent group frame, will not send until it "
                "has returned...\n");
        return -1;
    }

    if (ec_index_get(&pec->idx_q, &pd->p_idx) != 0) {
        ec_log(1, "EC_SEND_PROCESS_DATA_GROUP", 
                "error getting ethercat index\n");
        return -1;
    }

    if (pool_get(pec->pool, &pd->p_entry, NULL) != 0) {
        ec_index_put(&pec->idx_q, pd->p_idx);
        ec_log(1, "EC_SEND_PROCESS_DATA_GROUP", 
                "error getting datagram from pool\n");
        return -1;
    }

    if (pd->use_lrw) {
        pd_len = max(pd->pdout_len, pd->pdin_len);
    } else {
        pd_len = pd->log_len;
    }
    
    p_dg = (ec_datagram_t *)pd->p_entry->data;

    memset(p_dg, 0, sizeof(ec_datagram_t) + pd_len + 2);
    p_dg->cmd = EC_CMD_LRW;
    p_dg->idx = pd->p_idx->idx;
    p_dg->adr = pd->log;
    p_dg->len = pd_len;

    p_dg->irq = 0;
    if (pd->pd) {
        memcpy(ec_datagram_payload(p_dg), pd->pd, pd->pdout_len);
    }

    pd->p_entry->user_cb = cb_block;
    pd->p_entry->user_arg = pd->p_idx;

    // queue frame and trigger tx
    pool_put(pec->phw->tx_high, pd->p_entry);

    return 0;
}

//! receive process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \param timeout for waiting for packet
 * \return 0 on success
 */
int ec_receive_process_data_group(ec_t *pec, int group, ec_timer_t *timeout) {
    assert(pec != NULL);
    assert(timeout != NULL);

    static int wkc_mismatch_cnt = 0;
    int ret = 0;
    ec_datagram_t *p_dg;

    uint16_t wkc = 0;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    if (!pd->p_idx)
        return ret;
    
    if ((pd->p_entry == NULL) || (pd->p_idx == NULL)) {
        ec_log(1, __func__, "did not sent group frame\n");
        return -1;
    }
    
    p_dg = (ec_datagram_t *)pd->p_entry->data;

    // wait for completion
    struct timespec ts = { timeout->sec, timeout->nsec };
    ret = sem_timedwait(&pd->p_idx->waiter, &ts);
    if (ret == -1) {
        if (++pd->recv_missed < pec->consecutive_max_miss) {
            ec_log(1, "EC_RECEIVE_PROCESS_DATA_GROUP", 
                "sem_timedwait group id %d: %s, consecutive act %d limit %d\n", group, strerror(errno),
                pd->recv_missed, pec->consecutive_max_miss);
        } else {
            ec_log(1, "EC_RECEIVE_PROCESS_DATA_GROUP", 
                    "too much missed receive frames, falling back to INIT!\n");
            ec_set_state(pec, EC_STATE_INIT);
        }
    
        goto local_exit;
    }
    
    // reset consecutive missed counter
    pd->recv_missed = 0;

    wkc = ec_datagram_wkc(p_dg);
    if (pd->pd) {
        if (pd->use_lrw) {
            memcpy(pd->pd + pd->pdout_len, ec_datagram_payload(p_dg), pd->pdin_len);
        } else {
            memcpy(pd->pd + pd->pdout_len, ec_datagram_payload(p_dg) + pd->pdout_len, pd->pdin_len);
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
        ret = -1;
    } else {
        wkc_mismatch_cnt = 0;
    }

    for (int slave = 0; slave < pec->slave_cnt; ++slave) {
        ec_slave_ptr(slv, pec, slave);
        if (slv->assigned_pd_group != group) { continue; }

        if (slv->eeprom.mbx_supported && slv->mbx.sm_state) {
            if (*slv->mbx.sm_state & 0x08) {
                //ec_log(10, __func__, "slave %2d: sm_state %X\n", slave, *slv->mbx.sm_state);
                ec_mbx_sched_read(pec, slave);
            }
        }
    }

local_exit:
    pool_put(pec->pool, pd->p_entry);
    ec_index_put(&pec->idx_q, pd->p_idx);

    pd->p_entry = NULL;
    pd->p_idx = NULL;

    return ret;
}

pthread_mutex_t send_dc_lock = PTHREAD_MUTEX_INITIALIZER;

//! send distributed clock sync datagram
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_send_distributed_clocks_sync(ec_t *pec) {
    assert(pec != NULL);

    ec_datagram_t *p_dg = NULL;

    if (!pec->dc.have_dc || !pec->dc.rtc_sto) {
        return -1;
    }

    pthread_mutex_lock(&send_dc_lock);

    if ((pec->dc.p_de_dc != NULL) || (pec->dc.p_idx_dc != NULL)) {
//        ec_log(1, __func__, "already sent dc frame, will not send until it "
//                "has returned...\n");
//
        pthread_mutex_unlock(&send_dc_lock);
        return -1;
    }

    uint64_t act_rtc_time = ec_timer_gettime_nsec();

    if (pec->dc.rtc_time != 0) {
        if (act_rtc_time > pec->dc.rtc_time)
            pec->dc.rtc_cycle_sum += act_rtc_time - pec->dc.rtc_time;
        else
            pec->dc.rtc_cycle_sum -= pec->dc.rtc_time - act_rtc_time;

        pec->dc.rtc_count++;

        if (pec->dc.rtc_count == DC_DCSOFF_SAMPLES) {
            pec->dc.rtc_cycle = pec->dc.rtc_cycle_sum / DC_DCSOFF_SAMPLES;
            pec->dc.rtc_count = 0;
            pec->dc.rtc_cycle_sum = 0;
        }
    }

    if (pec->dc.timer_override > 0) {
        if (pec->dc.timer_prev == 0) {
            if (pec->dc.mode == dc_mode_master_as_ref_clock)
                pec->dc.timer_prev = act_rtc_time - pec->dc.rtc_sto;
            else
                pec->dc.timer_prev = act_rtc_time;
        } else
            pec->dc.timer_prev += (pec->dc.timer_override);
    }

    pec->dc.rtc_time = act_rtc_time;

    if (ec_index_get(&pec->idx_q, &pec->dc.p_idx_dc) != 0) {
        ec_log(1, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", 
                "error getting ethercat index\n");
        pthread_mutex_unlock(&send_dc_lock);
        return -1;
    }

    if (pool_get(pec->pool, &pec->dc.p_de_dc, NULL) != 0) {
        ec_index_put(&pec->idx_q, pec->dc.p_idx_dc);
        ec_log(1, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", 
                "error getting datagram from pool\n");
        pthread_mutex_unlock(&send_dc_lock);
        return -1;
    }

    p_dg = (ec_datagram_t *)pec->dc.p_de_dc->data;
    memset(p_dg, 0, sizeof(ec_datagram_t) + 8 + 2);

    if (pec->dc.mode == dc_mode_master_as_ref_clock) {
        p_dg->cmd = EC_CMD_BWR;
        p_dg->idx = pec->dc.p_idx_dc->idx;
        p_dg->adr = (EC_REG_DCSYSTIME << 16);
        p_dg->len = 8;
        p_dg->irq = 0;
            
        memcpy(ec_datagram_payload(p_dg), &pec->dc.timer_prev, sizeof(pec->dc.timer_prev));
    } else {
        p_dg->cmd = EC_CMD_FRMW;
        p_dg->idx = pec->dc.p_idx_dc->idx;
        p_dg->adr = (EC_REG_DCSYSTIME << 16) | pec->dc.master_address;
        p_dg->len = 8;
        p_dg->irq = 0;
    }

    pec->dc.p_de_dc->user_cb = cb_block;
    pec->dc.p_de_dc->user_arg = pec->dc.p_idx_dc;

    // queue frame and trigger tx
    pool_put(pec->phw->tx_high, pec->dc.p_de_dc);
      
    pthread_mutex_unlock(&send_dc_lock);

    return 0;
}

//! receive distributed clocks sync datagram
/*!
 * \param pec ethercat master pointer
 * \param timeout absolute timeout
 * \return 0 on success
 */
int ec_receive_distributed_clocks_sync(ec_t *pec, ec_timer_t *timeout) {
    assert(pec != NULL);
    assert(timeout != NULL);

    uint16_t wkc; 
    ec_datagram_t *p_dg = NULL;

    if (!pec->dc.have_dc || !pec->dc.p_de_dc) {
        return 0;
    }
    
    if ((pec->dc.p_de_dc == NULL) || (pec->dc.p_idx_dc == NULL)) {
        ec_log(1, __func__, "no dc frame was sent!\n");
        return -1;
    }
            
    // wait for completion
    struct timespec ts = { timeout->sec, timeout->nsec };
    int ret = sem_timedwait(&pec->dc.p_idx_dc->waiter, &ts);
    if (ret == -1) {
        ec_log(1, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC",
                "sem_timedwait distributed clocks (sent: %lld): %s\n", 
                pec->dc.rtc_time, strerror(errno));
        goto dc_exit;
    }

    uint64_t rtc = ec_timer_gettime_nsec();
    
    p_dg = (ec_datagram_t *)pec->dc.p_de_dc->data;
    wkc = ec_datagram_wkc(p_dg);

    if (pec->dc.mode == dc_mode_master_as_ref_clock)
        goto dc_exit;

    if (wkc) {
        uint64_t act_dc_time; 
        memcpy(&act_dc_time, ec_datagram_payload(p_dg), 8);
        
        if (((++pec->dc.offset_compensation_cnt) 
                    % pec->dc.offset_compensation_cycles) == 0) {
            pec->dc.offset_compensation_cnt = 0;

            // doing offset compensation in dc master clock
            // getting current system time first relative to ecat start
            uint64_t act_rtc_time = ((pec->dc.timer_override > 0) ?
                                     pec->dc.timer_prev : rtc) - pec->dc.rtc_sto;

            // clamp every value to 32-bit, so we do not need to care 
            // about wheter dc is 32-bit or 64-bit.
            int64_t rtc_temp = act_rtc_time % UINT_MAX;
            int64_t dc_temp  = act_dc_time  % UINT_MAX;

            pec->dc.act_diff = rtc_temp - dc_temp;
            if ((pec->dc.prev_rtc < rtc_temp) && (pec->dc.prev_dc > dc_temp))
                pec->dc.act_diff = rtc_temp - (UINT_MAX + dc_temp);
            else if ((pec->dc.prev_rtc > rtc_temp) && 
                    (pec->dc.prev_dc < dc_temp))
                pec->dc.act_diff = UINT_MAX + rtc_temp - dc_temp;

            // only compensate within one cycle, add rest to system time offset
            if (pec->dc.timer_override > 0) {
                // for example with a cycle of 1 ms we want to control between
                // -0.5 ms to +0.5 ms.
                int ticks_off = pec->dc.act_diff / (pec->dc.timer_override / 2);
                pec->dc.rtc_sto  += ticks_off * (pec->dc.timer_override / 2);             
                pec->dc.act_diff  = pec->dc.act_diff % (pec->dc.timer_override / 2);
                
                if (ticks_off > 0) {
                    ec_log(10, __func__, "compensating %d cycles, timer_prev %lld, rtc_sto %lld\n", 
                            ticks_off, pec->dc.timer_prev, pec->dc.rtc_sto);
                }
            }

            pec->dc.prev_rtc = rtc_temp;
            pec->dc.prev_dc  = dc_temp;

            // dc_mode 1 is sync ref_clock to master_clock
            if (pec->dc.mode == dc_mode_master_clock) {
                // sending offset compensation value to dc master clock
                pool_entry_t *p_entry_dc_sto;
                idx_entry_t *p_idx_dc_sto;

                // dc system time offset frame
                if (ec_index_get(&pec->idx_q, &p_idx_dc_sto) != 0) {
                    ec_log(1, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC", 
                            "error getting ethercat index\n");
                    goto sto_exit;
                }

                if (pool_get(pec->pool, &p_entry_dc_sto, NULL) != 0) {
                    ec_index_put(&pec->idx_q, p_idx_dc_sto);
                    ec_log(1, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC", 
                            "error getting datagram from pool\n");
                    goto sto_exit;
                }

                // correct system time offset, sync ref_clock to master_clock
                pec->dc.dc_sto += pec->dc.act_diff;
                p_dg = (ec_datagram_t *)p_entry_dc_sto->data;
                memset(p_dg, 0, sizeof(ec_datagram_t) + 10);
                p_dg->cmd = EC_CMD_FPWR;
                p_dg->idx = p_idx_dc_sto->idx;
                p_dg->adr = (EC_REG_DCSYSOFFSET << 16) | pec->dc.master_address;
                p_dg->len = sizeof(pec->dc.dc_sto);
                p_dg->irq = 0;
                memcpy(ec_datagram_payload(p_dg), &pec->dc.dc_sto, sizeof(pec->dc.dc_sto));
                // we don't care about the answer, cb_no_reply frees datagram 
                // and index
                p_idx_dc_sto->pec = pec;
                p_entry_dc_sto->user_cb = cb_no_reply;
                p_entry_dc_sto->user_arg = p_idx_dc_sto;

                // queue frame and trigger tx
                pool_put(pec->phw->tx_low, p_entry_dc_sto);
            }
        }

sto_exit:
        if (pec->dc.dc_time > 0) {
            if (act_dc_time < pec->dc.dc_time) {
                if (pec->dc.dc_time < (uint64_t)UINT_MAX) // 32-bit dc clock
                    pec->dc.dc_cycle_sum += ((uint64_t)UINT_MAX 
                            - pec->dc.dc_time) + act_dc_time;
                else
                    pec->dc.dc_cycle_sum += (ULONG_MAX - pec->dc.dc_time) 
                        + act_dc_time;
            } else 
                pec->dc.dc_cycle_sum += act_dc_time - pec->dc.dc_time;
            pec->dc.dc_cycle_cnt++;                

            if (pec->dc.dc_cycle_cnt == DC_DCSOFF_SAMPLES) {                    
                pec->dc.dc_cycle = pec->dc.dc_cycle_sum / DC_DCSOFF_SAMPLES;
                pec->dc.dc_cycle_cnt = 0;
                pec->dc.dc_cycle_sum = 0;
            }
        }

        pec->dc.dc_time = act_dc_time;
    }

dc_exit:
    pool_put(pec->pool, pec->dc.p_de_dc);
    ec_index_put(&pec->idx_q, pec->dc.p_idx_dc);

    pec->dc.p_de_dc = NULL;
    pec->dc.p_idx_dc = NULL;

    return 0;
}

//! send broadcast read to ec state
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_send_brd_ec_state(ec_t *pec) {
    assert(pec != NULL);

    ec_datagram_t *p_dg = NULL;

    if (ec_index_get(&pec->idx_q, &pec->p_idx_state) != 0) {
        ec_log(1, __func__, "error getting ethercat index\n");
        return -1;
    }

    if (pool_get(pec->pool, &pec->p_de_state, NULL) != 0) {
        ec_index_put(&pec->idx_q, pec->p_idx_state);
        ec_log(1, __func__, "error getting datagram from pool\n");
        return -1;
    }
    
    p_dg = (ec_datagram_t *)pec->p_de_state->data;

    memset(p_dg, 0, sizeof(ec_datagram_t) + 4 + 2);
    p_dg->cmd = EC_CMD_BRD;
    p_dg->idx = pec->p_idx_state->idx;
    p_dg->adr = EC_REG_ALSTAT << 16;
    p_dg->len = 2;
    p_dg->irq = 0;

    pec->p_de_state->user_cb = cb_block;
    pec->p_de_state->user_arg = pec->p_idx_state;

    // queue frame and trigger tx
    pool_put(pec->phw->tx_high, pec->p_de_state);

    return 0;
}

//! receive broadcast read to ec_state
/*!
 * \param pec ethercat master pointer
 * \param timeout for waiting for packet
 * \return 0 on success
 */
int ec_receive_brd_ec_state(ec_t *pec, ec_timer_t *timeout) {
    assert(pec != NULL);
    assert(timeout != NULL);

    static int wkc_mismatch_cnt_ec_state = 0;
    static int ec_state_mismatch_cnt = 0;

    ec_datagram_t *p_dg = NULL;
    int ret = 0;
    uint16_t al_status;

    uint16_t wkc = 0;
    if (!pec->p_idx_state)
        return ret;
    
    // wait for completion
    struct timespec ts = { timeout->sec, timeout->nsec };
    ret = sem_timedwait(&pec->p_idx_state->waiter, &ts);
    if (ret == -1) {
        ec_log(1, __func__, "sem_timedwait ec_state: %s\n", strerror(errno));
        goto local_exit;
    }
        
    p_dg = (ec_datagram_t *)pec->p_de_state->data;

    wkc = ec_datagram_wkc(p_dg);
    memcpy(&al_status, ec_datagram_payload(p_dg), 2);

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
        ret = -1;
    } else {
        wkc_mismatch_cnt_ec_state = 0;
    }

    if (!pec->state_transition_pending && (al_status != pec->master_state)) {
        if ((ec_state_mismatch_cnt++%1000) == 0)
            ec_log(1, __func__, "al status mismatch, got 0x%X, master state is 0x%X\n", 
                    al_status, pec->master_state);
    }

local_exit:
    pool_put(pec->pool, pec->p_de_state);
    ec_index_put(&pec->idx_q, pec->p_idx_state);

    return ret;
}

//! configures tun device of EtherCAT master, used for EoE slaves.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] ip_address    IP address to be set for tun device.
 */
void ec_configure_tun(ec_t *pec, uint8_t ip_address[4]) {
    assert(pec != NULL);

    memcpy(&pec->tun_ip, &ip_address[0], 4);
    ec_eoe_setup_tun(pec);
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

