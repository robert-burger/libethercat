/**
 * \file slave.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief ethercat slave functions
 *
 */

/*
 * This file is part of robotkernel.
 *
 * robotkernel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * robotkernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with robotkernel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libethercat/slave.h"
#include "libethercat/ec.h"
#include "libethercat/coe.h"
#include "libethercat/soe.h"
#include "libethercat/mbx.h"
#include "libethercat/dc.h"

#include <string.h>
#include <errno.h>

static const char transition_string_boot_to_init[]     = "BOOT_2_INIT";
static const char transition_string_init_to_boot[]     = "INIT_2_BOOT";
static const char transition_string_init_to_init[]     = "INIT_2_INIT";
static const char transition_string_init_to_preop[]    = "INIT_2_PREOP";
static const char transition_string_init_to_safeop[]   = "INIT_2_SAFEOP";
static const char transition_string_init_to_op[]       = "INIT_2_OP";
static const char transition_string_preop_to_init[]    = "PREOP_2_INIT";
static const char transition_string_preop_to_preop[]   = "PREOP_2_PREOP";
static const char transition_string_preop_to_safeop[]  = "PREOP_2_SAFEOP";
static const char transition_string_preop_to_op[]      = "PREOP_2_OP";
static const char transition_string_safeop_to_init[]   = "SAFEOP_2_INIT";
static const char transition_string_safeop_to_preop[]  = "SAFEOP_2_PREOP";
static const char transition_string_safeop_to_safeop[] = "SAFEOP_2_SAFEOP";
static const char transition_string_safeop_to_op[]     = "SAFEOP_2_OP";
static const char transition_string_op_to_init[]       = "OP_2_INIT";
static const char transition_string_op_to_preop[]      = "OP_2_PREOP";
static const char transition_string_op_to_safeop[]     = "OP_2_SAFEOP";
static const char transition_string_op_to_op[]         = "OP_2_OP";
static const char transition_string_unknown[]          = "UNKNOWN";

static const char *get_transition_string(ec_state_transition_t transition) {
    switch (transition) {
        default:
            return transition_string_unknown;
        case BOOT_2_INIT:
            return transition_string_boot_to_init;
        case INIT_2_BOOT:
            return transition_string_init_to_boot;
        case INIT_2_INIT:
            return transition_string_init_to_init;
        case INIT_2_PREOP:
            return transition_string_init_to_preop;
        case INIT_2_SAFEOP:
            return transition_string_init_to_safeop;
        case INIT_2_OP:
            return transition_string_init_to_op;
        case PREOP_2_INIT:
            return transition_string_preop_to_init;
        case PREOP_2_PREOP:
            return transition_string_preop_to_preop;
        case PREOP_2_SAFEOP:
            return transition_string_preop_to_safeop;
        case PREOP_2_OP:
            return transition_string_preop_to_op;
        case SAFEOP_2_INIT:
            return transition_string_safeop_to_init;
        case SAFEOP_2_PREOP:
            return transition_string_safeop_to_preop;
        case SAFEOP_2_SAFEOP:
            return transition_string_safeop_to_safeop;
        case SAFEOP_2_OP:
            return transition_string_safeop_to_op;
        case OP_2_INIT:
            return transition_string_op_to_init;
        case OP_2_PREOP:
            return transition_string_op_to_preop;
        case OP_2_SAFEOP:
            return transition_string_op_to_safeop;
        case OP_2_OP:
            return transition_string_op_to_op;
    }

    return transition_string_unknown;
}

// allocate init command structure
static ec_slave_mailbox_init_cmd_t *ec_slave_mailbox_init_cmd_alloc(
        int type, int transition, int id, int si_el, int ca_atn,
        char *data, size_t datalen) {
    ec_slave_mailbox_init_cmd_t *cmd = (ec_slave_mailbox_init_cmd_t *)malloc(
            sizeof(ec_slave_mailbox_init_cmd_t));

    if (cmd == NULL) {
        ec_log(10, "ec_slave_mailbox_init_cmd_alloc", "malloc error %s\n", 
                strerror(errno));
        return NULL;
    }

    cmd->type       = type;
    cmd->transition = transition;
    cmd->id         = id; 
    cmd->si_el      = si_el;
    cmd->ca_atn     = ca_atn;
    cmd->data       = (char *)malloc(datalen);
    cmd->datalen    = datalen;
    
    if (cmd->data == NULL) {
        ec_log(10, "ec_slave_mailbox_init_cmd_alloc", "malloc error %s\n", 
                strerror(errno));
        free(cmd);
        return NULL;
    }

    memcpy(cmd->data, data, datalen);

    return cmd;
}

// freeing init command strucuture
void ec_slave_mailbox_init_cmd_free(ec_slave_mailbox_init_cmd_t *cmd) {
    if (cmd) {
        if (cmd->data)
            free(cmd->data);

        free (cmd);
    }
}

// add master init command
void ec_slave_add_init_cmd(ec_t *pec, uint16_t slave,
        int type, int transition, int id, int si_el, int ca_atn,
        char *data, size_t datalen) {

    ec_slave_mailbox_init_cmd_t *cmd = ec_slave_mailbox_init_cmd_alloc(
            type, transition, id, si_el, ca_atn, data, datalen);

    if (!cmd)
        return;

    ec_slave_mailbox_init_cmd_t *last_cmd = 
        LIST_FIRST(&pec->slaves[slave].init_cmds);

    while (
            (last_cmd != NULL) && 
            (LIST_NEXT(last_cmd, le) != NULL)) {
        last_cmd = LIST_NEXT(last_cmd, le);
    }

    if (last_cmd)
        LIST_INSERT_AFTER(last_cmd, cmd, le);
    else 
        LIST_INSERT_HEAD(&pec->slaves[slave].init_cmds, cmd, le);
}

// Set EtherCAT state on slave 
int ec_slave_set_state(ec_t *pec, uint16_t slave, ec_state_t state) {
    uint16_t wkc = 0, act_state, value;
    ec_fpwr(pec, pec->slaves[slave].fixed_address, 
            EC_REG_ALCTL, &state, sizeof(state), &wkc); 

    if (state & EC_STATE_RESET)
        return wkc; // just return here, we did an error reset

    pec->slaves[slave].expected_state = state;

    ec_timer_t timeout;
    ec_timer_init(&timeout, 10000000000); // 10 second timeout

    do {
        ec_fpwr(pec, pec->slaves[slave].fixed_address, 
                EC_REG_ALCTL, &state, sizeof(state), &wkc); 

        act_state = 0;
        wkc = ec_slave_get_state(pec, slave, &act_state, NULL);

        if (act_state & EC_STATE_ERROR) {
            ec_fprd(pec, pec->slaves[slave].fixed_address, 
                    EC_REG_ALSTATCODE, &value, sizeof(value), &wkc);
            ec_log(10, "EC_STATE_SET", "slave %2d: state switch to %d failed, "
                    "alstatcode 0x%04X\n", slave, state, value);

            ec_slave_set_state(pec, slave, (act_state & EC_STATE_MASK) | 
                    EC_STATE_RESET);
            break;
        }
    
        if (ec_timer_expired(&timeout)) {
            ec_log(10, "EC_STATE_SET", "slave %2d: did not respond on state "
                    "switch to %d\n", slave, state);
            wkc = 0;
            break;
        }

        ec_sleep(1000000);
    } while (act_state != state);

    ec_log(100, "EC_STATE_SET", "slave %2d: state %X, act_state %X, wkc %d\n", 
            slave, state, act_state, wkc);

    return wkc;
}

// get ethercat state from slave 
int ec_slave_get_state(ec_t *pec, uint16_t slave, ec_state_t *state, 
        uint16_t *alstatcode) {
    uint16_t wkc = 0, value = 0;
    ec_fprd(pec, pec->slaves[slave].fixed_address, 
            EC_REG_ALSTAT, &value, sizeof(value), &wkc);

    if (wkc)
        *state = (ec_state_t)value;

    if (alstatcode && (*state & 0x10)) {
        ec_fprd(pec, pec->slaves[slave].fixed_address, 
                EC_REG_ALSTATCODE, &value, sizeof(value), &wkc);

        if (wkc)
            *alstatcode = value;
    }

    return wkc;
}

// generate pd mapping
int ec_slave_generate_mapping(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (slv->sm_set_by_user)
        return 0; // we're already done

    // check sm settings
    if (slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE)
        ec_coe_generate_mapping(pec, slave);
    else if (slv->eeprom.mbx_supported & EC_EEPROM_MBX_SOE)
        ec_soe_generate_mapping(pec, slave);
    else {
        // try eeprom
        for (int sm_idx = 0; sm_idx < slv->sm_ch; ++sm_idx) {
            int txpdos_cnt = 0, rxpdos_cnt = 0;
            size_t bit_len = 0;
            ec_eeprom_cat_pdo_t *pdo;

            // inputs and outputs
            TAILQ_FOREACH(pdo, &slv->eeprom.txpdos, qh) {
                if (sm_idx == pdo->sm_nr) {
                    for (int entry_idx = 0; entry_idx < pdo->n_entry; 
                            ++entry_idx) { 
                        ec_log(100, "GENERATE_MAPPING EEP", "slave %2d: got "
                                "txpdo bit_len %d, sm %d\n", slave, 
                                pdo->entries[entry_idx].bit_len, pdo->sm_nr);
                        bit_len += pdo->entries[entry_idx].bit_len;
                    }

                    txpdos_cnt++;
                }
            }

            // outputs
            TAILQ_FOREACH(pdo, &slv->eeprom.rxpdos, qh) {
                if (sm_idx == pdo->sm_nr) {
                    for (int entry_idx = 0; entry_idx < pdo->n_entry; 
                            ++entry_idx) { 
                        ec_log(100, "GENERATE_MAPPING EEP", "slave %2d: got "
                                "rxpdo bit_len %d, sm %d\n", slave, 
                                pdo->entries[entry_idx].bit_len, pdo->sm_nr);
                        bit_len += pdo->entries[entry_idx].bit_len;
                    }

                    rxpdos_cnt++;
                }
            }

            ec_log(100, "GENERATE_MAPPING EEP", "slave %2d: txpdos %d, "
                    "rxpdos %d, bitlen%d %d\n", 
                    slave, txpdos_cnt, rxpdos_cnt, sm_idx, bit_len);

            if (bit_len > 0) {
                ec_log(10, "GENERATE_MAPPING EEP", "slave %2d: sm%d length "
                        "bits %d, bytes %d\n", 
                        slave, sm_idx, bit_len, (bit_len + 7) / 8);

                slv->sm[sm_idx].len = (bit_len + 7) / 8;
            }
        }
    }

    return 1;
}

// prepare state transition on ethercat slave
int ec_slave_prepare_state_transition(ec_t *pec, uint16_t slave, 
        ec_state_t state) {
    uint16_t wkc;
    ec_state_t act_state = 0;
    ec_slave_t *slv = &pec->slaves[slave];

    // check error state
    wkc = ec_slave_get_state(pec, slave, &act_state, NULL);
    if (!wkc) {
        ec_log(10, __func__, "slave %2d: error getting state\n", slave);
        return -1;
    }
    
    // generate transition
    ec_state_transition_t transition = ((act_state & EC_STATE_MASK) << 8) | 
        (state & EC_STATE_MASK); 

    switch (transition) {
        default:
            break;
        case INIT_2_SAFEOP:
        case PREOP_2_SAFEOP:
            ec_log(10, get_transition_string(transition), "slave %2d: "
                    "sending init cmds\n", slave);

            ec_slave_mailbox_init_cmd_t *cmd;
            LIST_FOREACH(cmd, &slv->init_cmds, le) {
                if (cmd->transition != 0x24) 
                    continue;

                if (cmd->type == EC_MBX_COE) {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d: sending CoE init cmd 0x%04X:%d, "
                            "ca %d, datalen %d, datap %p\n", slave, cmd->id, 
                            cmd->si_el, cmd->ca_atn, cmd->datalen, cmd->data);

                    uint8_t *buf = (uint8_t *)cmd->data;
                    size_t buf_len = cmd->datalen;
                    uint32_t abort_code = 0;

                    int ret = ec_coe_sdo_write(pec, slave, cmd->id, cmd->si_el, 
                            cmd->ca_atn, buf, buf_len, &abort_code);
                    if (ret != 0) {
                        ec_log(10, get_transition_string(transition), 
                                "slave %2d: writing sdo failed: error code 0x%X!\n", 
                                slave, ret);
                    } 
                }
            }

            break;
    }

    return 0;
}

// state transition on ethercat slave
int ec_slave_state_transition(ec_t *pec, uint16_t slave, ec_state_t state) {
    uint16_t wkc;
    ec_state_t act_state = 0;
    ec_slave_t *slv = &pec->slaves[slave];

#define ec_reg_read(reg, buf, buflen) {                                 \
    uint16_t wkc;                                                       \
    ec_fprd(pec, pec->slaves[slave].fixed_address, (reg),               \
            (buf), (buflen), &wkc);                                     \
    if (!wkc) ec_log(10, __func__,                                      \
            "reading reg 0x%X : no answer from slave %2d\n", slave); }

    // check error state
    wkc = ec_slave_get_state(pec, slave, &act_state, NULL);
    if (!wkc) {
        ec_log(10, "ERROR", "could not get state of slave %d\n", slave);
        return 0;
    }

    if (act_state & EC_STATE_ERROR) // reset error state first
        ec_slave_set_state(pec, slave, 
                (act_state & EC_STATE_MASK) | EC_STATE_RESET);

    // generate transition
    ec_state_transition_t transition = ((act_state & EC_STATE_MASK) << 8) | 
        (state & EC_STATE_MASK); 
            
    switch (transition) {
        case BOOT_2_PREOP:
        case BOOT_2_SAFEOP:
        case BOOT_2_OP: 
            break;
        case INIT_2_BOOT:
        case INIT_2_PREOP:
        case INIT_2_SAFEOP:
        case INIT_2_OP: {
            // init to preop stuff
            ec_log(10, get_transition_string(transition), 
                    "slave %2d, vendor 0x%08X, product 0x%08X, mbx 0x%04X\n",
                    slave, 
                    slv->eeprom.vendor_id, 
                    slv->eeprom.product_code, 
                    slv->eeprom.mbx_supported);

            // configure mailboxes if any supported
            if (slv->eeprom.mbx_supported) {
                // read mailbox
                if ((transition == INIT_2_BOOT) && 
                        slv->eeprom.boot_mbx_send_offset) {
                    slv->sm[1].adr = slv->eeprom.boot_mbx_send_offset;
                    slv->sm[1].len = slv->eeprom.boot_mbx_send_size;
                } else {
                    slv->sm[1].adr = slv->eeprom.mbx_send_offset;
                    slv->sm[1].len = slv->eeprom.mbx_send_size;
                }
                slv->sm[1].flags = 0x00010022;
                slv->mbx_read.sm_nr = 1;
                free_resource(slv->mbx_read.buf);
                alloc_resource(slv->mbx_read.buf, uint8_t, slv->sm[1].len);
                slv->mbx_read.sm_state = NULL;
                slv->mbx_read.skip_next = 0;

                // write mailbox
                if ((transition == INIT_2_BOOT) && 
                        slv->eeprom.boot_mbx_receive_offset) {
                    slv->sm[0].adr = slv->eeprom.boot_mbx_receive_offset;
                    slv->sm[0].len = slv->eeprom.boot_mbx_receive_size;
                } else {
                    slv->sm[0].adr = slv->eeprom.mbx_receive_offset;
                    slv->sm[0].len = slv->eeprom.mbx_receive_size;
                }

                slv->sm[0].flags = 0x00010026;
                slv->mbx_write.sm_nr = 0;
                free_resource(slv->mbx_write.buf);
                alloc_resource(slv->mbx_write.buf, uint8_t, slv->sm[0].len);
                slv->mbx_write.sm_state = NULL;
                slv->mbx_read.skip_next = 0;

                for (int sm_idx = 0; sm_idx < 2; ++sm_idx) {
                    ec_log(10, get_transition_string(transition), "slave %2d: "
                            "sm%d, adr 0x%04X, len %3d, flags 0x%08X\n",
                            slave, sm_idx, slv->sm[sm_idx].adr, 
                            slv->sm[sm_idx].len, slv->sm[sm_idx].flags);

                    ec_fpwr(pec, slv->fixed_address, 0x800 + (sm_idx * 8),
                            &slv->sm[sm_idx], sizeof(ec_slave_sm_t), &wkc);
                }

            }

            ec_eeprom_to_pdi(pec, slave);

            // write state to slave
            if (transition == INIT_2_BOOT) {
                wkc = ec_slave_set_state(pec, slave, EC_STATE_BOOT);
                break;
            } else
                wkc = ec_slave_set_state(pec, slave, EC_STATE_PREOP);
                
            if (transition == INIT_2_PREOP || transition == INIT_2_BOOT)
                break;
        }
        case PREOP_2_SAFEOP: 
        case PREOP_2_OP: {
            // configure distributed clocks if needed 
            if (pec->dc.have_dc && slv->dc.use_dc) {
                if (slv->dc.cycle_time_0 == 0)
                    slv->dc.cycle_time_0 = pec->dc.timer_override; 

                if (slv->dc.type == 1) {
                    if (slv->dc.cycle_time_1 == 0)
                        slv->dc.cycle_time_1 = pec->dc.timer_override; 

                    ec_log(10, get_transition_string(transition), 
                            "slave %2d: configuring dc sync 01, "
                            "cycle_times %d/%d, cycle_shift %d\n", 
                            slave, slv->dc.cycle_time_0, 
                            slv->dc.cycle_time_1, slv->dc.cycle_shift);

                    ec_dc_sync01(pec, slave, 1, slv->dc.cycle_time_0, 
                            slv->dc.cycle_time_1, slv->dc.cycle_shift);
                } else {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d: configuring dc sync 0, "
                            "cycle_time %d, cycle_shift %d\n", slave,
                            slv->dc.cycle_time_0, slv->dc.cycle_shift);

                    ec_dc_sync0(pec, slave, 1, slv->dc.cycle_time_0, 
                            slv->dc.cycle_shift);
                }
            } else
                ec_dc_sync0(pec, slave, 0, 0, 0);

            int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

            for (int sm_idx = start_sm; sm_idx < slv->sm_ch; ++sm_idx) {
                if (!slv->sm[sm_idx].adr)
                    continue;

                ec_log(10, get_transition_string(transition), "slave %2d: "
                        "sm%d, adr 0x%04X, len %3d, flags 0x%08X\n",
                        slave, sm_idx, slv->sm[sm_idx].adr, 
                        slv->sm[sm_idx].len, slv->sm[sm_idx].flags);

                ec_fpwr(pec, slv->fixed_address, 0x800 + (sm_idx * 8),
                        &slv->sm[sm_idx], sizeof(ec_slave_sm_t), &wkc);

                if (!wkc)
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d: no answer on "
                            "writing sm%d settings\n", slave, sm_idx);
            }

            for (int fmmu_idx = 0; fmmu_idx < slv->fmmu_ch; ++fmmu_idx) { 
                if (!slv->fmmu[fmmu_idx].active) 
                    continue;

                // safeop to op stuff 
                ec_log(10, get_transition_string(transition), 
                        "slave %2d: log%d 0x%08X/%d/%d, len %3d, pyhs "
                        "0x%04X/%d, type %d, active %d\n", slave, fmmu_idx,
                        slv->fmmu[fmmu_idx].log, 
                        slv->fmmu[fmmu_idx].log_bit_start,
                        slv->fmmu[fmmu_idx].log_bit_stop, 
                        slv->fmmu[fmmu_idx].log_len,
                        slv->fmmu[fmmu_idx].phys, 
                        slv->fmmu[fmmu_idx].phys_bit_start,
                        slv->fmmu[fmmu_idx].type, 
                        slv->fmmu[fmmu_idx].active);

                ec_fpwr(pec, slv->fixed_address, 0x600 + (16 * fmmu_idx),
                        (uint8_t *)&slv->fmmu[fmmu_idx], 
                        sizeof(ec_slave_fmmu_t), &wkc);

            }

            // write state to slave
            wkc = ec_slave_set_state(pec, slave, EC_STATE_SAFEOP);

            if (transition == INIT_2_SAFEOP || transition == PREOP_2_SAFEOP)
                break;
        }
        case SAFEOP_2_OP: {
            ec_log(10, get_transition_string(transition), 
                    "slave %2d setting to operational\n", slave);

            // write state to slave
            wkc = ec_slave_set_state(pec, slave, EC_STATE_OP);            
            break;
        }
        case OP_2_PREOP:
        case OP_2_SAFEOP:
        case OP_2_INIT:
        case SAFEOP_2_PREOP:
        case SAFEOP_2_INIT:
        case PREOP_2_INIT: {
            uint8_t dc_active = 0;
            ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_DCSYNCACT, 
                    &dc_active, sizeof(dc_active), &wkc);

            // write state to slave
            wkc = ec_slave_set_state(pec, slave, state);
        }
        case BOOT_2_INIT:
        case INIT_2_INIT: {
            printf("-----------------> init 2 inti\n");
            // rewrite fixed address
            ec_apwr(pec, slv->auto_inc_address, EC_REG_STADR, 
                    (uint8_t *)&slv->fixed_address, 
                    sizeof(slv->fixed_address), &wkc); 
        
            // disable ditributed clocks
            uint8_t dc_active = 0;
            ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_DCSYNCACT, 
                    &dc_active, sizeof(dc_active), &wkc);

            // free resources
            free_resource(slv->mbx_read.buf);
            free_resource(slv->mbx_write.buf);
            free_resource(slv->sm);
            free_resource(slv->fmmu);
            free_resource(slv->subdevs);

            // get number of sync managers
            ec_reg_read(EC_REG_SM_CH, &slv->sm_ch, 1);
            if (slv->sm_ch) {
                alloc_resource(slv->sm, ec_slave_sm_t, 
                        slv->sm_ch * sizeof(ec_slave_sm_t));

                for (int i = 0; i < slv->sm_ch; ++i) 
                    ec_transmit_no_reply(pec, EC_CMD_FPWR, 
                            ec_to_adr(slv->fixed_address, 0x800 + (8 * i)),
                            (uint8_t *)&slv->sm[i], sizeof(ec_slave_sm_t));
            }

            // get number of fmmus
            ec_reg_read(EC_REG_FMMU_CH, &slv->fmmu_ch, 1);
            if (slv->fmmu_ch) {
                alloc_resource(slv->fmmu, ec_slave_fmmu_t, 
                        slv->fmmu_ch * sizeof(ec_slave_fmmu_t));

                for (int i = 0; i < slv->fmmu_ch; ++i) 
                    ec_transmit_no_reply(pec, EC_CMD_FPWR, 
                            ec_to_adr(slv->fixed_address, 0x600 + (16 * i)),
                            (uint8_t *)&slv->fmmu[i], sizeof(ec_slave_fmmu_t));
            }

            // get ram size
            uint8_t ram_size = 0;
            ec_reg_read(EC_REG_RAM_SIZE, &ram_size, sizeof(ram_size));
            slv->ram_size = ram_size << 10;

            // get pdi control 
            ec_reg_read(EC_REG_PDICTL, &slv->pdi_ctrl, sizeof(slv->pdi_ctrl));
            // get features
            ec_reg_read(EC_REG_ESCSUP, &slv->features, sizeof(slv->features));

            ec_log(10, get_transition_string(transition), 
                    "slave %2d: pdi ctrl 0x%04X, fmmus %d, syncm %d\n", 
                    slave, slv->pdi_ctrl, slv->fmmu_ch, slv->sm_ch);
            
            // init to preop stuff
            slv->eeprom.read_eeprom = 0;
            ec_eeprom_dump(pec, slave);
                
            // allocate sub device structures
            if (slv->eeprom.general.ds402_channels > 0)
                slv->subdev_cnt = slv->eeprom.general.ds402_channels;
            else if (slv->eeprom.general.soe_channels > 0)
                slv->subdev_cnt = slv->eeprom.general.soe_channels;
            else 
                slv->subdev_cnt = 0;

            if (slv->subdev_cnt) {
                alloc_resource(slv->subdevs, ec_slave_subdev_t, 
                        slv->subdev_cnt * sizeof(ec_slave_subdev_t));

                int q;
                for (q = 0; q < slv->subdev_cnt; q++) {
                    slv->subdevs[q].pdin.pd = slv->subdevs[q].pdout.pd = NULL;
                    slv->subdevs[q].pdin.len = slv->subdevs[q].pdout.len = 0;
                }
            }

            ec_log(10, get_transition_string(transition), 
                    "slave %2d: vendor 0x%08X, product 0x%08X, mbx 0x%04X\n",
                    slave, slv->eeprom.vendor_id, slv->eeprom.product_code, 
                    slv->eeprom.mbx_supported);
        }
        case PREOP_2_PREOP:
        case SAFEOP_2_SAFEOP:
        case OP_2_OP:
            // write state to slave
            wkc = ec_slave_set_state(pec, slave, state);
            break;
        default:
            wkc = ec_slave_set_state(pec, slave, EC_STATE_INIT);
            ec_log(10, __func__, "unknown state transition for slave "
                    "%2d -> %04X\n", slave, transition);
            break;
    };

    return wkc;
}

