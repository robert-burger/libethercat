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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <limits.h>


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

void *ec_log_func_user = NULL;
void (*ec_log_func)(int lvl, void *user, const char *format, ...) = NULL;

void ec_log(int lvl, const char *pre, const char *format, ...) {
    if (ec_log_func == NULL) {
        va_list ap;
        va_start(ap, format);
        printf("[%-20.20s] ", pre);
        vprintf(format, ap);
        va_end(ap);
    } else {
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

int ec_master_state_set(ec_t *pec, ec_state_t state) {
    uint16_t wkc = 0;
    uint16_t value = (uint16_t)state;
    ec_bwr(pec, EC_REG_ALCTL, &value, sizeof(value), &wkc); 
    return wkc;
}


int ec_coe_calc_pd_len(ec_t *pec, uint16_t slave, uint16_t pdo_reg) {
//    ec_coe_sdo_read(pec, slave, pdo_reg, 0, buf, 
    return 0;
}


//! create process data groups
/*!
 * \param pec ethercat master pointer
 * \param pd_group_cnt number of groups to create
 * \return 0 on success
 */
int ec_create_pd_groups(ec_t *pec, int pd_group_cnt) {
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
    }

    return 0;
}

//! destroy process data groups
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_destroy_pd_groups(ec_t *pec) {
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

        slv->pdin_len = 0;
        slv->pdout_len = 0;

        for (k = start_sm; k < slv->sm_ch; ++k) {
            if (slv->sm[k].flags & 0x00000004) {
                slv->pdout_len += slv->sm[k].len; // outputs
            } else  {
                slv->pdin_len += slv->sm[k].len;  // inputs
            }
        }

        if (slv->eeprom.mbx_supported)
            // add state of sync manager read mailbox
            slv->pdin_len += 1;
        
        size_t max_len = max(slv->pdout_len, slv->pdin_len);

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

                slv->mbx_read.sm_state = tmp_pdin;

                wkc_expected |= 1;
            }

            tmp_pdin += 1;
            log_base_in += 1;
        }

        pdin += max(slv->pdin_len, slv->pdout_len);
        pdout += max(slv->pdin_len, slv->pdout_len);
        log_base = max(log_base_in, log_base_out);
        pd->wkc_expected += wkc_expected;
    }
}

void ec_create_logical_mapping(ec_t *pec, int group) {
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

                slv->mbx_read.sm_state = pdin;

                wkc_expected |= 1;
            }

            pdin += 1;
            log_base_in += 1;
        }

        pd->wkc_expected += wkc_expected;
    }
}

void *prepare_state_transition_wrapper(void *arg) {
    worker_arg_t *tmp = 
        (worker_arg_t *)arg;
    
    ec_log(100, get_state_string(tmp->state), 
            "prepare state transition for slave %d\n", tmp->slave);
    ec_slave_prepare_state_transition(
            tmp->pec, tmp->slave, tmp->state);

    ec_log(100, get_state_string(tmp->state), 
            "generate mapping for slave %d\n", tmp->slave);
    ec_slave_generate_mapping(tmp->pec, tmp->slave);
    return NULL;
}

void *set_state_wrapper(void *arg) {
    worker_arg_t *tmp = 
        (worker_arg_t *)arg;

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
    uint16_t fixed = 1000, wkc = 0, val = 0, i;

    ec_state_t init_state = EC_STATE_INIT | EC_STATE_RESET;
    ec_bwr(pec, EC_REG_ALCTL, &init_state, sizeof(init_state), &wkc); 

    // allocating slave structures
    ec_brd(pec, EC_REG_TYPE, (uint8_t *)&val, sizeof(val), &wkc); 
    pec->slave_cnt = wkc;
    alloc_resource(pec->slaves, ec_slave_t, pec->slave_cnt * 
            sizeof(ec_slave_t));

    for (i = 0; i < 65536; ++i) {
        int auto_inc = -1 * i;

        ec_aprd(pec, auto_inc, EC_REG_TYPE, (uint8_t *)&val, 
                sizeof(val), &wkc);

        if (wkc == 0)
            break;  // break here, cause there seems to be no more slave

        ec_log(100, "EC_OPEN", "slave %2d: auto inc %3d, fixed %d\n", 
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
        pthread_mutex_init(&pec->slaves[i].mbx_lock, NULL);

        ec_apwr(pec, auto_inc, EC_REG_STADR, (uint8_t *)&fixed, 
                sizeof(fixed), &wkc); 
        if (wkc == 0)
            ec_log(10, "EC_OPEN", "slave %2d: error writing fixed "
                    "address %d\n", i, fixed);

        // set eeprom to pdi, some slaves need this
        ec_eeprom_to_pdi(pec, i);
        init_state = EC_STATE_INIT | EC_STATE_RESET;
        ec_fpwr(pec, fixed, EC_REG_ALCTL, &init_state, 
                sizeof(init_state), &wkc); 

        fixed++;
    }

    ec_log(10, "EC_OPEN", "found %d ethercat slaves\n", i);

    for (int slave = 0; slave < pec->slave_cnt; ++slave) {
        ec_slave_t *slv = &pec->slaves[slave]; 
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

        ec_log(100, "EC_OPEN", "slave %2d has parent %d\n", 
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
    ec_log(10, "SET MASTER STATE", "switch to from %s to %s\n", 
            get_state_string(pec->master_state), get_state_string(state));

    // generate transition
    ec_state_transition_t transition = ((pec->master_state & EC_STATE_MASK) << 8) | 
        (state & EC_STATE_MASK); 
            
    switch (transition) {
        case BOOT_2_INIT:
        case BOOT_2_PREOP:
        case BOOT_2_SAFEOP:
        case BOOT_2_OP: 
        case INIT_2_INIT:
            // ====> switch to INIT stuff
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
            ec_dc_config(pec);

            if (state == EC_STATE_PREOP)
                break;

        case PREOP_2_SAFEOP:
        case PREOP_2_OP: 
        case SAFEOP_2_SAFEOP:
            // ====> switch to SAFEOP stuff
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
        case OP_2_OP: 
            // ====> switch to OP stuff
            ec_state_transition_loop(pec, EC_STATE_OP, 1);
            break;
            
        case OP_2_BOOT:
        case OP_2_INIT:
        case OP_2_PREOP:
        case OP_2_SAFEOP:
            ec_state_transition_loop(pec, EC_STATE_SAFEOP, 0);

            if (state == EC_STATE_SAFEOP)
                break;
        case SAFEOP_2_BOOT:
        case SAFEOP_2_INIT:
        case SAFEOP_2_PREOP:
            ec_state_transition_loop(pec, EC_STATE_PREOP, 0);

            if (state == EC_STATE_PREOP)
                break;
        case PREOP_2_BOOT:
        case PREOP_2_INIT:
            ec_state_transition_loop(pec, EC_STATE_INIT, 0);
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
        
    return (pec->master_state = state);
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
    //int i;
    ec_t *pec = malloc(sizeof(ec_t)); 
    (*ppec) = pec;
    if (!pec)
        return ENOMEM;

    ec_index_init(&pec->idx_q);
    pec->master_state       = EC_STATE_INIT;

    // slaves'n groups
    pec->phw                = NULL;
    pec->slave_cnt          = 0;
    pec->pd_group_cnt       = 0;
    pec->slaves             = NULL;
    pec->pd_groups          = NULL;
    pec->tx_sync            = 1;
    pec->threaded_startup   = 1;

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

    // eeprom logging level
    pec->eeprom_log         = eeprom_log;

    datagram_pool_open(&pec->pool, 1000);
        
    if (hw_open(&pec->phw, ifname, prio, cpumask) == -1) {
        datagram_pool_close(pec->pool);

        ec_index_deinit(&pec->idx_q);
        free(pec);
        *ppec = NULL;

        return -1;
    }

    ec_async_message_loop_create(&pec->async_loop, pec);
    ec_set_state(pec, EC_STATE_INIT);

    return 0;
}

//! closes ethercat master
/*!
 * \param pec pointer to ethercat master
 * \return 0 on success 
 */
int ec_close(ec_t *pec) {
    ec_async_message_pool_destroy(pec->async_loop);
    hw_close(pec->phw);
    datagram_pool_close(pec->pool);

    ec_index_deinit(&pec->idx_q);
    ec_destroy_pd_groups(pec);

    if (pec->slaves) {
        int slave;
        for (slave = 0; slave < pec->slave_cnt; ++slave) {
            ec_slave_t *slv = &pec->slaves[slave];

            if (slv->eeprom.strings) {
                int string;
                for (string = 0; string < slv->eeprom.strings_cnt; ++string)
                    free(slv->eeprom.strings[string]);

                free(slv->eeprom.strings);
            }
            
            ec_eeprom_cat_pdo_t *pdo;
            while ((pdo = TAILQ_FIRST(&slv->eeprom.txpdos)) != NULL) {
                TAILQ_REMOVE(&slv->eeprom.txpdos, pdo, qh);
                if (pdo->entries)
                    free(pdo->entries);
                free(pdo);
            }
           
            while ((pdo = TAILQ_FIRST(&slv->eeprom.rxpdos)) != NULL) {
                TAILQ_REMOVE(&slv->eeprom.rxpdos, pdo, qh);
                if (pdo->entries)
                    free(pdo->entries);
                free(pdo);
            }

            ec_slave_mailbox_init_cmd_t *cmd;
            while ((cmd = LIST_FIRST(&slv->init_cmds)) != NULL) {
                LIST_REMOVE(cmd, le);
                ec_slave_mailbox_init_cmd_free(cmd);                
            }

            free_resource(slv->eeprom.sms);
            free_resource(slv->eeprom.fmmus);
            free_resource(slv->sm);
            free_resource(slv->fmmu);
            free_resource(slv->mbx_read.buf);
            free_resource(slv->mbx_write.buf);

            pthread_mutex_destroy(&slv->mbx_lock);
        }

        free(pec->slaves);
    }

    free(pec);

    return 0;
}

//! local callack for syncronous read/write
static void cb_block(void *user_arg, struct datagram_entry *p) {
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
    datagram_entry_t *p_de;
    idx_entry_t *p_idx;

    if (ec_index_get(&pec->idx_q, &p_idx) != 0) {
        ec_log(5, "EC_TRANSCEIVE", "error getting ethercat index\n");
        return -1;
    }

    if (datagram_pool_get(pec->pool, &p_de, NULL) != 0) {
        ec_index_put(&pec->idx_q, p_idx);
        ec_log(5, "EC_TRANSCEIVE", "error getting datagram from pool\n");
        return -1;
    }

    memset(&p_de->datagram, 0, sizeof(ec_datagram_t) + datalen + 2);
    p_de->datagram.cmd = cmd;
    p_de->datagram.idx = p_idx->idx;
    p_de->datagram.adr = adr;
    p_de->datagram.len = datalen;
    p_de->datagram.irq = 0;
    memcpy(ec_datagram_payload(&p_de->datagram), data, datalen);

    p_de->user_cb = cb_block;
    p_de->user_arg = p_idx;

    // queue frame and trigger tx
    datagram_pool_put(pec->phw->tx_low, p_de);

    // send frame immediately if in sync mode
    if (pec->tx_sync)
        hw_tx(pec->phw);

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
        *wkc = ec_datagram_wkc(&p_de->datagram);
        if (*wkc)
            memcpy(data, ec_datagram_payload(&p_de->datagram), datalen);
    }

    datagram_pool_put(pec->pool, p_de);
    ec_index_put(&pec->idx_q, p_idx);

    return 0;
}

//! local callack for syncronous read/write
static void cb_no_reply(void *user_arg, struct datagram_entry *p) {
    idx_entry_t *entry = (idx_entry_t *)user_arg;
    datagram_pool_put(entry->pec->pool, p);
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
    datagram_entry_t *p_de;
    idx_entry_t *p_idx;

    if (ec_index_get(&pec->idx_q, &p_idx) != 0) {
        ec_log(5, "EC_TRANSMIT_NO_REPLY", 
                "error getting ethercat index\n");
        return -1;
    }

    if (datagram_pool_get(pec->pool, &p_de, NULL) != 0) {
        ec_index_put(&pec->idx_q, p_idx);
        ec_log(5, "EC_TRANSMIT_NO_REPLY", 
                "error getting datagram from pool\n");
        return -1;
    }

    memset(&p_de->datagram, 0, sizeof(ec_datagram_t) + datalen + 2);
    p_de->datagram.cmd = cmd;
    p_de->datagram.idx = p_idx->idx;
    p_de->datagram.adr = adr;
    p_de->datagram.len = datalen;
    p_de->datagram.irq = 0;
    memcpy(ec_datagram_payload(&p_de->datagram), data, datalen);

    // don't care about answer
    p_idx->pec = pec;
    p_de->user_cb = cb_no_reply;
    p_de->user_arg = p_idx;

    // queue frame and return, we don't care about an answer
    datagram_pool_put(pec->phw->tx_low, p_de);

    // send frame immediately if in sync mode
    if (pec->tx_sync)
        hw_tx(pec->phw);

    return 0;
}

//! send process data for specific group with logical commands
/*!
 * \param pec ethercat master pointer
 * \param group group number
 * \return 0 on success
 */
int ec_send_process_data_group(ec_t *pec, int group) {
    ec_pd_group_t *pd = &pec->pd_groups[group];

    if (ec_index_get(&pec->idx_q, &pd->p_idx) != 0) {
        ec_log(5, "EC_SEND_PROCESS_DATA_GROUP", 
                "error getting ethercat index\n");
        return -1;
    }

    if (datagram_pool_get(pec->pool, &pd->p_de, NULL) != 0) {
        ec_index_put(&pec->idx_q, pd->p_idx);
        ec_log(5, "EC_SEND_PROCESS_DATA_GROUP", 
                "error getting datagram from pool\n");
        return -1;
    }

    memset(&pd->p_de->datagram, 0, sizeof(ec_datagram_t) + pd->log_len + 2);
    pd->p_de->datagram.cmd = EC_CMD_LRW;
    pd->p_de->datagram.idx = pd->p_idx->idx;
    pd->p_de->datagram.adr = pd->log;
    
    if (pd->use_lrw) {
        pd->p_de->datagram.len = pd->pdout_len;
    } else {
        pd->p_de->datagram.len = pd->log_len;
    }

    pd->p_de->datagram.irq = 0;
    if (pd->pd) {
        memcpy(ec_datagram_payload(&pd->p_de->datagram), 
                pd->pd, pd->pdout_len);
    }

    pd->p_de->user_cb = cb_block;
    pd->p_de->user_arg = pd->p_idx;

    // queue frame and trigger tx
    datagram_pool_put(pec->phw->tx_high, pd->p_de);

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
    static int wkc_mismatch_cnt = 0;
    int ret = 0;

    uint16_t wkc = 0;
    ec_pd_group_t *pd = &pec->pd_groups[group];
    if (!pd->p_idx)
        return ret;
    
    // wait for completion
    struct timespec ts = { timeout->sec, timeout->nsec };
    ret = sem_timedwait(&pd->p_idx->waiter, &ts);
    if (ret == -1) {
        ec_log(5, "EC_RECEIVE_PROCESS_DATA_GROUP", 
                "sem_timedwait group id %d: %s\n", group, strerror(errno));
        goto local_exit;
    }
        
    wkc = ec_datagram_wkc(&pd->p_de->datagram);
    if (pd->pd) {
        if (pd->use_lrw)
            memcpy(pd->pd + pd->pdout_len, ec_datagram_payload(&pd->p_de->datagram),
                    pd->pdin_len);
        else
            memcpy(pd->pd + pd->pdout_len, 
                    ec_datagram_payload(&pd->p_de->datagram) + 
                    pd->pdout_len, pd->pdin_len);
    }

    if (    (   (pec->master_state == EC_STATE_SAFEOP) || 
                (pec->master_state == EC_STATE_OP)  ) && 
            (wkc != pd->wkc_expected)) {
        if ((wkc_mismatch_cnt++%1000) == 0) {
            ec_log(10, "EC_RECEIVE_PROCESS_DATA_GROUP", 
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

local_exit:
    datagram_pool_put(pec->pool, pd->p_de);
    ec_index_put(&pec->idx_q, pd->p_idx);

    return ret;
}

//! send distributed clock sync datagram
/*!
 * \param pec ethercat master pointer
 * \return 0 on success
 */
int ec_send_distributed_clocks_sync(ec_t *pec) {
    if (!pec->dc.have_dc)
        return 0;

    uint64_t act_rtc_time = ec_timer_gettime_nsec();

    if (pec->dc.rtc_time != 0) {
        pec->dc.rtc_cycle_sum += abs(act_rtc_time - pec->dc.rtc_time);
        pec->dc.rtc_count++;

        if (pec->dc.rtc_count == DC_DCSOFF_SAMPLES) {
            pec->dc.rtc_cycle = pec->dc.rtc_cycle_sum / DC_DCSOFF_SAMPLES;
            pec->dc.rtc_count = 0;
            pec->dc.rtc_cycle_sum = 0;
        }
    }

    if (pec->dc.timer_override > 0) {
        if (pec->dc.timer_prev == 0) {
            if (pec->dc.mode == dc_mode_master_as_ref_clock) {
                pec->dc.timer_prev = act_rtc_time - pec->dc.rtc_sto;
                ec_log(10, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", "sending first dc time %llu\n", pec->dc.timer_prev);
            } else
                pec->dc.timer_prev = act_rtc_time;
        } else
            pec->dc.timer_prev += 
                (pec->dc.timer_override);// * pec->dc.offset_compensation);
    }

    pec->dc.rtc_time = act_rtc_time;

    if (ec_index_get(&pec->idx_q, &pec->dc.p_idx_dc) != 0) {
        ec_log(5, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", 
                "error getting ethercat index\n");
        return -1;
    }

    if (datagram_pool_get(pec->pool, &pec->dc.p_de_dc, NULL) != 0) {
        ec_index_put(&pec->idx_q, pec->dc.p_idx_dc);
        ec_log(5, "EC_SEND_DISTRIBUTED_CLOCKS_SYNC", 
                "error getting datagram from pool\n");
        return -1;
    }

    memset(&pec->dc.p_de_dc->datagram, 0, sizeof(ec_datagram_t) + 8 + 2);

    if (pec->dc.mode == dc_mode_master_as_ref_clock) {
        pec->dc.p_de_dc->datagram.cmd = EC_CMD_BWR;
        pec->dc.p_de_dc->datagram.idx = pec->dc.p_idx_dc->idx;
        pec->dc.p_de_dc->datagram.adr = (EC_REG_DCSYSTIME << 16);
        pec->dc.p_de_dc->datagram.len = 8;
        pec->dc.p_de_dc->datagram.irq = 0;
            
        memcpy(ec_datagram_payload(&pec->dc.p_de_dc->datagram),
                &pec->dc.timer_prev, sizeof(pec->dc.timer_prev));
    } else {
        pec->dc.p_de_dc->datagram.cmd = EC_CMD_FRMW;
        pec->dc.p_de_dc->datagram.idx = pec->dc.p_idx_dc->idx;
        pec->dc.p_de_dc->datagram.adr = (EC_REG_DCSYSTIME << 16) | 
            pec->dc.master_address;
        pec->dc.p_de_dc->datagram.len = 8;
        pec->dc.p_de_dc->datagram.irq = 0;
    }

    pec->dc.p_de_dc->user_cb = cb_block;
    pec->dc.p_de_dc->user_arg = pec->dc.p_idx_dc;

    // queue frame and trigger tx
    datagram_pool_put(pec->phw->tx_high, pec->dc.p_de_dc);
    return 0;
}

//! receive distributed clocks sync datagram
/*!
 * \param pec ethercat master pointer
 * \param timeout absolute timeout
 * \return 0 on success
 */
int ec_receive_distributed_clocks_sync(ec_t *pec, ec_timer_t *timeout) {
    uint16_t wkc; 

    if (!pec->dc.have_dc)
        return 0;
            
    // wait for completion
    struct timespec ts = { timeout->sec, timeout->nsec };
    int ret = sem_timedwait(&pec->dc.p_idx_dc->waiter, &ts);
    if (ret == -1) {
        ec_log(5, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC",
                "sem_timedwait distributed clocks (sent: %lld): %s\n", 
                pec->dc.rtc_time, strerror(errno));
        goto dc_exit;
    }

    wkc = ec_datagram_wkc(&pec->dc.p_de_dc->datagram);

    if (pec->dc.mode == dc_mode_master_as_ref_clock)
        goto dc_exit;

    if (wkc) {
        uint64_t act_dc_time; 
        memcpy(&act_dc_time, 
                ec_datagram_payload(&pec->dc.p_de_dc->datagram), 8);

        if (((++pec->dc.offset_compensation_cnt) 
                    % pec->dc.offset_compensation) == 0) {
            pec->dc.offset_compensation_cnt = 0;

            // doing offset compensation in dc master clock
            // getting current system time first relative to ecat start
            uint64_t act_rtc_time = ((pec->dc.timer_override > 0) ?
                pec->dc.timer_prev : ec_timer_gettime_nsec()) - pec->dc.rtc_sto;

            int64_t rtc_temp = act_rtc_time%UINT_MAX;
            int64_t dc_temp  = act_dc_time %UINT_MAX;

            // fix datatype wrap around
            pec->dc.act_diff = rtc_temp - dc_temp;
            if ((pec->dc.prev_rtc < rtc_temp) && (pec->dc.prev_dc > dc_temp))
                pec->dc.act_diff = rtc_temp - (UINT_MAX + dc_temp);
            else if ((pec->dc.prev_rtc > rtc_temp) && (pec->dc.prev_dc < dc_temp))
                pec->dc.act_diff = UINT_MAX + rtc_temp - dc_temp;

            // clamp to maximum compensation value per tick
            if (pec->dc.act_diff > pec->dc.offset_compensation_max)
                pec->dc.act_diff = pec->dc.offset_compensation_max;
            else if (pec->dc.act_diff < (-1 * pec->dc.offset_compensation_max))
                pec->dc.act_diff = -1 * pec->dc.offset_compensation_max;

            pec->dc.prev_rtc = rtc_temp;
            pec->dc.prev_dc  = dc_temp;

            // dc_mode 1 is sync ref_clock to master_clock
            if (pec->dc.mode == dc_mode_master_clock) {
                // sending offset compensation value to dc master clock
                datagram_entry_t *p_de_dc_sto;
                idx_entry_t *p_idx_dc_sto;

                // dc system time offset frame
                if (ec_index_get(&pec->idx_q, &p_idx_dc_sto) != 0) {
                    ec_log(5, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC", 
                            "error getting ethercat index\n");
                    goto sto_exit;
                }

                if (datagram_pool_get(pec->pool, &p_de_dc_sto, NULL) != 0) {
                    ec_index_put(&pec->idx_q, p_idx_dc_sto);
                    ec_log(5, "EC_RECEIVE_DISTRIBUTED_CLOCKS_SYNC", 
                            "error getting datagram from pool\n");
                    goto sto_exit;
                }

                // correct system time offset, sync ref_clock to master_clock
                pec->dc.dc_sto += pec->dc.act_diff;
                memset(&p_de_dc_sto->datagram, 0, sizeof(ec_datagram_t) + 10);
                p_de_dc_sto->datagram.cmd = EC_CMD_FPWR;
                p_de_dc_sto->datagram.idx = p_idx_dc_sto->idx;
                p_de_dc_sto->datagram.adr = (EC_REG_DCSYSOFFSET << 16) | 
                    pec->dc.master_address;
                p_de_dc_sto->datagram.len = sizeof(pec->dc.dc_sto);
                p_de_dc_sto->datagram.irq = 0;
                memcpy(ec_datagram_payload(&p_de_dc_sto->datagram), 
                        &pec->dc.dc_sto, sizeof(pec->dc.dc_sto));

                // we don't care about the answer, cb_no_reply frees datagram 
                // and index
                p_idx_dc_sto->pec = pec;
                p_de_dc_sto->user_cb = cb_no_reply;
                p_de_dc_sto->user_arg = p_idx_dc_sto;

                // queue frame and trigger tx
                datagram_pool_put(pec->phw->tx_low, p_de_dc_sto);
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
    datagram_pool_put(pec->pool, pec->dc.p_de_dc);
    ec_index_put(&pec->idx_q, pec->dc.p_idx_dc);

    return 0;
}


