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

#include "config.h"

#include "libethercat/slave.h"
#include "libethercat/ec.h"
#include "libethercat/coe.h"
#include "libethercat/soe.h"
#include "libethercat/foe.h"
#include "libethercat/mbx.h"
#include "libethercat/dc.h"

#include <string.h>
#include <errno.h>
#include <assert.h>

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
static ec_slave_mailbox_init_cmd_t *ec_slave_mailbox_coe_init_cmd_alloc(
        int transition, int id, int si_el, int ca_atn,
        char *data, size_t datalen) 
{
    ec_slave_mailbox_init_cmd_t *cmd = (void *)malloc(COE_INIT_CMD_SIZE);

    if (cmd == NULL) {
        ec_log(10, __func__, "malloc error %s\n", strerror(errno));
        return NULL;
    }

    cmd->type       = EC_MBX_COE;
    cmd->transition = transition;
    
    ec_coe_init_cmd_t *coe = (void *)cmd->cmd;
    coe->id         = id; 
    coe->si_el      = si_el;
    coe->ca_atn     = ca_atn;
    coe->data       = (char *)malloc(datalen);
    coe->datalen    = datalen;
    
    if (coe->data == NULL) {
        ec_log(10, __func__, "malloc error %s\n", strerror(errno));
        free(cmd);
        return NULL;
    }

    memcpy(coe->data, data, datalen);

    return cmd;
}

// freeing init command strucuture
void ec_slave_mailbox_coe_init_cmd_free(ec_slave_mailbox_init_cmd_t *cmd) {
    assert(cmd != NULL);
        
    if (cmd->type == EC_MBX_COE) {
        ec_coe_init_cmd_t *coe = (void *)cmd->cmd;

        if (coe->data) { free(coe->data); }
    }

    free (cmd);
}

// add master init command
void ec_slave_add_coe_init_cmd(ec_t *pec, uint16_t slave,
        int transition, int id, int si_el, int ca_atn,
        char *data, size_t datalen) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_mailbox_init_cmd_t *cmd = ec_slave_mailbox_coe_init_cmd_alloc(
            transition, id, si_el, ca_atn, data, datalen);

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

// allocate init command structure
static ec_slave_mailbox_init_cmd_t *ec_slave_mailbox_soe_init_cmd_alloc(
        int transition, int id, int si_el, int ca_atn,
        char *data, size_t datalen) 
{
    ec_slave_mailbox_init_cmd_t *cmd = (void *)malloc(SOE_INIT_CMD_SIZE);
    
    if (cmd == NULL) {
        ec_log(10, __func__, "malloc error %s\n", strerror(errno));
        return NULL;
    }

    cmd->type       = EC_MBX_SOE;
    cmd->transition = transition;
    
    ec_soe_init_cmd_t *soe = (void *)cmd->cmd;
    soe->id         = id; 
    soe->si_el      = si_el;
    soe->ca_atn     = ca_atn;
    soe->data       = (char *)malloc(datalen);
    soe->datalen    = datalen;
    
    if (soe->data == NULL) {
        ec_log(10, __func__, "malloc error %s\n", strerror(errno));
        free(cmd);
        return NULL;
    }

    memcpy(soe->data, data, datalen);

    return cmd;
}

// freeing init command strucuture
void ec_slave_mailbox_soe_init_cmd_free(ec_slave_mailbox_init_cmd_t *cmd) {
    assert(cmd != NULL);
    
    if (cmd->type == EC_MBX_SOE) {
        ec_soe_init_cmd_t *soe = (void *)cmd->cmd;

        if (soe->data) { free(soe->data); }
    }

    free (cmd);
}

// add master init command
void ec_slave_add_soe_init_cmd(ec_t *pec, uint16_t slave,
        int transition, int id, int si_el, int ca_atn,
        char *data, size_t datalen) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_mailbox_init_cmd_t *cmd = ec_slave_mailbox_soe_init_cmd_alloc(
            transition, id, si_el, ca_atn, data, datalen);

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

// freeing init command strucuture
void ec_slave_mailbox_init_cmd_free(ec_slave_mailbox_init_cmd_t *cmd) {
    assert(cmd != NULL);

    switch(cmd->type) {
        case EC_MBX_COE:
            ec_slave_mailbox_coe_init_cmd_free(cmd);
            break;
        case EC_MBX_SOE:
            ec_slave_mailbox_soe_init_cmd_free(cmd);
            break;
        default: 
            free(cmd);
            break;
    }
}

// Set Distributed Clocks config to slave
void ec_slave_set_dc_config(struct ec *pec, uint16_t slave, 
        int use_dc, int type, uint32_t cycle_time_0, 
        uint32_t cycle_time_1, uint32_t cycle_shift) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    pec->slaves[slave].dc.use_dc        = use_dc;
    pec->slaves[slave].dc.type          = type;
    pec->slaves[slave].dc.cycle_time_0  = cycle_time_0;
    pec->slaves[slave].dc.cycle_time_1  = cycle_time_1;
    pec->slaves[slave].dc.cycle_shift   = cycle_shift;
}

#define AL_STATUS_CODE__NOERROR                        0x0000 
#define AL_STATUS_CODE__UNSPECIFIEDERROR               0x0001 
#define AL_STATUS_CODE__NOMEMORY                       0x0002 
#define AL_STATUS_CODE__FW_SII_NOT_MATCH               0x0006 
#define AL_STATUS_CODE__FW_UPDATE_FAILED               0x0007 
#define AL_STATUS_CODE__INVALIDALCONTROL               0x0011 
#define AL_STATUS_CODE__UNKNOWNALCONTROL               0x0012 
#define AL_STATUS_CODE__BOOTNOTSUPP                    0x0013 
#define AL_STATUS_CODE__NOVALIDFIRMWARE                0x0014 
#define AL_STATUS_CODE__INVALIDMBXCFGINBOOT            0x0015 
#define AL_STATUS_CODE__INVALIDMBXCFGINPREOP           0x0016 
#define AL_STATUS_CODE__INVALIDSMCFG                   0x0017 
#define AL_STATUS_CODE__NOVALIDINPUTS                  0x0018 
#define AL_STATUS_CODE__NOVALIDOUTPUTS                 0x0019 
#define AL_STATUS_CODE__SYNCERROR                      0x001A 
#define AL_STATUS_CODE__SMWATCHDOG                     0x001B 
#define AL_STATUS_CODE__SYNCTYPESNOTCOMPATIBLE         0x001C 
#define AL_STATUS_CODE__INVALIDSMOUTCFG                0x001D 
#define AL_STATUS_CODE__INVALIDSMINCFG                 0x001E 
#define AL_STATUS_CODE__INVALIDWDCFG                   0x001F 
#define AL_STATUS_CODE__WAITFORCOLDSTART               0x0020 
#define AL_STATUS_CODE__WAITFORINIT                    0x0021 
#define AL_STATUS_CODE__WAITFORPREOP                   0x0022 
#define AL_STATUS_CODE__WAITFORSAFEOP                  0x0023 
#define AL_STATUS_CODE__INVALIDINPUTMAPPING            0x0024 
#define AL_STATUS_CODE__INVALIDOUTPUTMAPPING           0x0025 
#define AL_STATUS_CODE__INCONSISTENTSETTINGS           0x0026 
#define AL_STATUS_CODE__FREERUNNOTSUPPORTED            0x0027 
#define AL_STATUS_CODE__SYNCHRONNOTSUPPORTED           0x0028 
#define AL_STATUS_CODE__FREERUNNEEDS3BUFFERMODE        0x0029 
#define AL_STATUS_CODE__BACKGROUNDWATCHDOG             0x002A 
#define AL_STATUS_CODE__NOVALIDINPUTSANDOUTPUTS        0x002B 
#define AL_STATUS_CODE__FATALSYNCERROR                 0x002C 
#define AL_STATUS_CODE__NOSYNCERROR                    0x002D 
#define AL_STATUS_CODE__CYCLETIMETOOSMALL              0x002E 
#define AL_STATUS_CODE__DCINVALIDSYNCCFG               0x0030 
#define AL_STATUS_CODE__DCINVALIDLATCHCFG              0x0031 
#define AL_STATUS_CODE__DCPLLSYNCERROR                 0x0032 
#define AL_STATUS_CODE__DCSYNCIOERROR                  0x0033 
#define AL_STATUS_CODE__DCSYNCMISSEDERROR              0x0034 
#define AL_STATUS_CODE__DCINVALIDSYNCCYCLETIME         0x0035 
#define AL_STATUS_CODE__DCSYNC0CYCLETIME               0x0036 
#define AL_STATUS_CODE__DCSYNC1CYCLETIME               0x0037 
#define AL_STATUS_CODE__MBX_AOE                        0x0041 
#define AL_STATUS_CODE__MBX_EOE                        0x0042 
#define AL_STATUS_CODE__MBX_COE                        0x0043 
#define AL_STATUS_CODE__MBX_FOE                        0x0044 
#define AL_STATUS_CODE__MBX_SOE                        0x0045 
#define AL_STATUS_CODE__MBX_VOE                        0x004F 
#define AL_STATUS_CODE__EE_NOACCESS                    0x0050 
#define AL_STATUS_CODE__EE_ERROR                       0x0051 
#define AL_STATUS_CODE__EXT_HARDWARE_NOT_READY         0x0052 

const static char *_AL_STATUS_CODE__NOERROR                       = "no error"; 
const static char *_AL_STATUS_CODE__UNSPECIFIEDERROR              = "unspecified error"; 
const static char *_AL_STATUS_CODE__NOMEMORY                      = "no memory"; 
const static char *_AL_STATUS_CODE__FW_SII_NOT_MATCH              = "firmware sii not match"; 
const static char *_AL_STATUS_CODE__FW_UPDATE_FAILED              = "firmware update failed"; 
const static char *_AL_STATUS_CODE__INVALIDALCONTROL              = "invalid al control"; 
const static char *_AL_STATUS_CODE__UNKNOWNALCONTROL              = "unknown al control"; 
const static char *_AL_STATUS_CODE__BOOTNOTSUPP                   = "boot not supported"; 
const static char *_AL_STATUS_CODE__NOVALIDFIRMWARE               = "no valid firmware"; 
const static char *_AL_STATUS_CODE__INVALIDMBXCFGINBOOT           = "invalid mailbox config in boot"; 
const static char *_AL_STATUS_CODE__INVALIDMBXCFGINPREOP          = "invalid mailbox config in prepop"; 
const static char *_AL_STATUS_CODE__INVALIDSMCFG                  = "invalid sync manager config"; 
const static char *_AL_STATUS_CODE__NOVALIDINPUTS                 = "invalid inputs"; 
const static char *_AL_STATUS_CODE__NOVALIDOUTPUTS                = "invalid outputs"; 
const static char *_AL_STATUS_CODE__SYNCERROR                     = "sync error"; 
const static char *_AL_STATUS_CODE__SMWATCHDOG                    = "sync manager watchdog"; 
const static char *_AL_STATUS_CODE__SYNCTYPESNOTCOMPATIBLE        = "sync types not compatible"; 
const static char *_AL_STATUS_CODE__INVALIDSMOUTCFG               = "invalid sync manager out config"; 
const static char *_AL_STATUS_CODE__INVALIDSMINCFG                = "invalid sync manager in config"; 
const static char *_AL_STATUS_CODE__INVALIDWDCFG                  = "invalid watchdog config"; 
const static char *_AL_STATUS_CODE__WAITFORCOLDSTART              = "wait for cold start"; 
const static char *_AL_STATUS_CODE__WAITFORINIT                   = "wait for init"; 
const static char *_AL_STATUS_CODE__WAITFORPREOP                  = "wait for preop"; 
const static char *_AL_STATUS_CODE__WAITFORSAFEOP                 = "wait for safeop"; 
const static char *_AL_STATUS_CODE__INVALIDINPUTMAPPING           = "invalid input mapping"; 
const static char *_AL_STATUS_CODE__INVALIDOUTPUTMAPPING          = "invalid output mapping"; 
const static char *_AL_STATUS_CODE__INCONSISTENTSETTINGS          = "inconsistent settings"; 
const static char *_AL_STATUS_CODE__FREERUNNOTSUPPORTED           = "freerun not supported"; 
const static char *_AL_STATUS_CODE__SYNCHRONNOTSUPPORTED          = "synchron not supported"; 
const static char *_AL_STATUS_CODE__FREERUNNEEDS3BUFFERMODE       = "freerun needs 3 buffer mode"; 
const static char *_AL_STATUS_CODE__BACKGROUNDWATCHDOG            = "background watchdog"; 
const static char *_AL_STATUS_CODE__NOVALIDINPUTSANDOUTPUTS       = "no valid inputs and outputs"; 
const static char *_AL_STATUS_CODE__FATALSYNCERROR                = "fatal sync error"; 
const static char *_AL_STATUS_CODE__NOSYNCERROR                   = "no sync error"; 
const static char *_AL_STATUS_CODE__CYCLETIMETOOSMALL             = "cycletime too small"; 
const static char *_AL_STATUS_CODE__DCINVALIDSYNCCFG              = "dc invalid sync config"; 
const static char *_AL_STATUS_CODE__DCINVALIDLATCHCFG             = "dc invalid latch config"; 
const static char *_AL_STATUS_CODE__DCPLLSYNCERROR                = "dc pll sync error"; 
const static char *_AL_STATUS_CODE__DCSYNCIOERROR                 = "dc sync io error"; 
const static char *_AL_STATUS_CODE__DCSYNCMISSEDERROR             = "dc sync missed error"; 
const static char *_AL_STATUS_CODE__DCINVALIDSYNCCYCLETIME        = "dc invalid sync cycletime"; 
const static char *_AL_STATUS_CODE__DCSYNC0CYCLETIME              = "dc sync0 cycletime"; 
const static char *_AL_STATUS_CODE__DCSYNC1CYCLETIME              = "dc sync1 cycletime"; 
const static char *_AL_STATUS_CODE__MBX_AOE                       = "mailbox aoe"; 
const static char *_AL_STATUS_CODE__MBX_EOE                       = "mailbox eoe"; 
const static char *_AL_STATUS_CODE__MBX_COE                       = "mailbox coe"; 
const static char *_AL_STATUS_CODE__MBX_FOE                       = "mailbox foe"; 
const static char *_AL_STATUS_CODE__MBX_SOE                       = "mailbox soe"; 
const static char *_AL_STATUS_CODE__MBX_VOE                       = "mailbox voe"; 
const static char *_AL_STATUS_CODE__EE_NOACCESS                   = "ee no access"; 
const static char *_AL_STATUS_CODE__EE_ERROR                      = "ee error"; 
const static char *_AL_STATUS_CODE__EXT_HARDWARE_NOT_READY        = "ext hardware not ready"; 

const char *al_status_code_2_string(int code) {
    switch (code) {
        case AL_STATUS_CODE__NOERROR:
            return _AL_STATUS_CODE__NOERROR;
        case AL_STATUS_CODE__UNSPECIFIEDERROR:
            return _AL_STATUS_CODE__UNSPECIFIEDERROR;
        case AL_STATUS_CODE__NOMEMORY:
            return _AL_STATUS_CODE__NOMEMORY;
        case AL_STATUS_CODE__FW_SII_NOT_MATCH:
            return _AL_STATUS_CODE__FW_SII_NOT_MATCH;
        case AL_STATUS_CODE__FW_UPDATE_FAILED:
            return _AL_STATUS_CODE__FW_UPDATE_FAILED;
        case AL_STATUS_CODE__INVALIDALCONTROL:
            return _AL_STATUS_CODE__INVALIDALCONTROL;
        case AL_STATUS_CODE__UNKNOWNALCONTROL:
            return _AL_STATUS_CODE__UNKNOWNALCONTROL;
        case AL_STATUS_CODE__BOOTNOTSUPP:
            return _AL_STATUS_CODE__BOOTNOTSUPP;
        case AL_STATUS_CODE__NOVALIDFIRMWARE:
            return _AL_STATUS_CODE__NOVALIDFIRMWARE;
        case AL_STATUS_CODE__INVALIDMBXCFGINBOOT:
            return _AL_STATUS_CODE__INVALIDMBXCFGINBOOT;
        case AL_STATUS_CODE__INVALIDMBXCFGINPREOP:
            return _AL_STATUS_CODE__INVALIDMBXCFGINPREOP;
        case AL_STATUS_CODE__INVALIDSMCFG:
            return _AL_STATUS_CODE__INVALIDSMCFG;
        case AL_STATUS_CODE__NOVALIDINPUTS:
            return _AL_STATUS_CODE__NOVALIDINPUTS;
        case AL_STATUS_CODE__NOVALIDOUTPUTS:
            return _AL_STATUS_CODE__NOVALIDOUTPUTS;
        case AL_STATUS_CODE__SYNCERROR:
            return _AL_STATUS_CODE__SYNCERROR;
        case AL_STATUS_CODE__SMWATCHDOG:
            return _AL_STATUS_CODE__SMWATCHDOG;
        case AL_STATUS_CODE__SYNCTYPESNOTCOMPATIBLE:
            return _AL_STATUS_CODE__SYNCTYPESNOTCOMPATIBLE;
        case AL_STATUS_CODE__INVALIDSMOUTCFG:
            return _AL_STATUS_CODE__INVALIDSMOUTCFG;
        case AL_STATUS_CODE__INVALIDSMINCFG:
            return _AL_STATUS_CODE__INVALIDSMINCFG;
        case AL_STATUS_CODE__INVALIDWDCFG:
            return _AL_STATUS_CODE__INVALIDWDCFG;
        case AL_STATUS_CODE__WAITFORCOLDSTART:
            return _AL_STATUS_CODE__WAITFORCOLDSTART;
        case AL_STATUS_CODE__WAITFORINIT:
            return _AL_STATUS_CODE__WAITFORINIT;
        case AL_STATUS_CODE__WAITFORPREOP:
            return _AL_STATUS_CODE__WAITFORPREOP;
        case AL_STATUS_CODE__WAITFORSAFEOP:
            return _AL_STATUS_CODE__WAITFORSAFEOP;
        case AL_STATUS_CODE__INVALIDINPUTMAPPING:
            return _AL_STATUS_CODE__INVALIDINPUTMAPPING;
        case AL_STATUS_CODE__INVALIDOUTPUTMAPPING:
            return _AL_STATUS_CODE__INVALIDOUTPUTMAPPING;
        case AL_STATUS_CODE__INCONSISTENTSETTINGS:
            return _AL_STATUS_CODE__INCONSISTENTSETTINGS;
        case AL_STATUS_CODE__FREERUNNOTSUPPORTED:
            return _AL_STATUS_CODE__FREERUNNOTSUPPORTED;
        case AL_STATUS_CODE__SYNCHRONNOTSUPPORTED:
            return _AL_STATUS_CODE__SYNCHRONNOTSUPPORTED;
        case AL_STATUS_CODE__FREERUNNEEDS3BUFFERMODE:
            return _AL_STATUS_CODE__FREERUNNEEDS3BUFFERMODE;
        case AL_STATUS_CODE__BACKGROUNDWATCHDOG:
            return _AL_STATUS_CODE__BACKGROUNDWATCHDOG;
        case AL_STATUS_CODE__NOVALIDINPUTSANDOUTPUTS:
            return _AL_STATUS_CODE__NOVALIDINPUTSANDOUTPUTS;
        case AL_STATUS_CODE__FATALSYNCERROR:
            return _AL_STATUS_CODE__FATALSYNCERROR;
        case AL_STATUS_CODE__NOSYNCERROR:
            return _AL_STATUS_CODE__NOSYNCERROR;
        case AL_STATUS_CODE__CYCLETIMETOOSMALL:
            return _AL_STATUS_CODE__CYCLETIMETOOSMALL;
        case AL_STATUS_CODE__DCINVALIDSYNCCFG:
            return _AL_STATUS_CODE__DCINVALIDSYNCCFG;
        case AL_STATUS_CODE__DCINVALIDLATCHCFG:
            return _AL_STATUS_CODE__DCINVALIDLATCHCFG;
        case AL_STATUS_CODE__DCPLLSYNCERROR:
            return _AL_STATUS_CODE__DCPLLSYNCERROR;
        case AL_STATUS_CODE__DCSYNCIOERROR:
            return _AL_STATUS_CODE__DCSYNCIOERROR;
        case AL_STATUS_CODE__DCSYNCMISSEDERROR:
            return _AL_STATUS_CODE__DCSYNCMISSEDERROR;
        case AL_STATUS_CODE__DCINVALIDSYNCCYCLETIME:
            return _AL_STATUS_CODE__DCINVALIDSYNCCYCLETIME;
        case AL_STATUS_CODE__DCSYNC0CYCLETIME:
            return _AL_STATUS_CODE__DCSYNC0CYCLETIME;
        case AL_STATUS_CODE__DCSYNC1CYCLETIME:
            return _AL_STATUS_CODE__DCSYNC1CYCLETIME;
        case AL_STATUS_CODE__MBX_AOE:
            return _AL_STATUS_CODE__MBX_AOE;
        case AL_STATUS_CODE__MBX_EOE:
            return _AL_STATUS_CODE__MBX_EOE;
        case AL_STATUS_CODE__MBX_COE:
            return _AL_STATUS_CODE__MBX_COE;
        case AL_STATUS_CODE__MBX_FOE:
            return _AL_STATUS_CODE__MBX_FOE;
        case AL_STATUS_CODE__MBX_SOE:
            return _AL_STATUS_CODE__MBX_SOE;
        case AL_STATUS_CODE__MBX_VOE:
            return _AL_STATUS_CODE__MBX_VOE;
        case AL_STATUS_CODE__EE_NOACCESS:
            return _AL_STATUS_CODE__EE_NOACCESS;
        case AL_STATUS_CODE__EE_ERROR:
            return _AL_STATUS_CODE__EE_ERROR;
        case AL_STATUS_CODE__EXT_HARDWARE_NOT_READY:
            return _AL_STATUS_CODE__EXT_HARDWARE_NOT_READY;
    }

    return NULL;
}

const static char *_ECAT_STATE_INIT     = "INIT";
const static char *_ECAT_STATE_PREOP    = "PRE-OPERATIONAL";
const static char *_ECAT_STATE_SAFEOP   = "SAFE-OPERATIONAL";
const static char *_ECAT_STATE_OP       = "OPERATIONAL";
const static char *_ECAT_STATE_BOOT     = "BOOT";

const char *ecat_state_2_string(int state) {
    switch (state) {
        case EC_STATE_INIT:
            return _ECAT_STATE_INIT;
        case EC_STATE_PREOP:
            return _ECAT_STATE_PREOP;
        case EC_STATE_SAFEOP:
            return _ECAT_STATE_SAFEOP;
        case EC_STATE_OP:
            return _ECAT_STATE_OP;
        case EC_STATE_BOOT:
            return _ECAT_STATE_BOOT;
    }

    return NULL;
}

// Set EtherCAT state on slave 
int ec_slave_set_state(ec_t *pec, uint16_t slave, ec_state_t state) {
    uint16_t wkc = 0, act_state, value;
    
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_fpwr(pec, pec->slaves[slave].fixed_address, 
            EC_REG_ALCTL, &state, sizeof(state), &wkc); 

    if (state & EC_STATE_RESET)
        return wkc; // just return here, we did an error reset

    ec_log(10, "EC_STATE_SET", "slave %2d: %s state requested\n", slave, ecat_state_2_string(state));

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
                    "alstatcode 0x%04X : %s\n", slave, state, value, al_status_code_2_string(value));

            ec_slave_set_state(pec, slave, (act_state & EC_STATE_MASK) | 
                    EC_STATE_RESET);
            break;
        }
    
        if (ec_timer_expired(&timeout)) {
            ec_log(1, "EC_STATE_SET", "slave %2d: did not respond on state "
                    "switch to %d\n", slave, state);
            wkc = 0;
            break;
        }

        ec_sleep(1000000);
    } while (act_state != state);

    ec_log(100, "EC_STATE_SET", "slave %2d: state %X, act_state %X, wkc %d\n", 
            slave, state, act_state, wkc);

    if (act_state == state) {    
        ec_log(10, "EC_STATE_SET", "slave %2d: %s state reached\n", slave, ecat_state_2_string(act_state));
    } else {
        ec_log(1, "EC_STATE_SET", "slave %2d: %s state switch FAILED!\n", slave, ecat_state_2_string(act_state));
    }
            
    pec->slaves[slave].act_state = act_state;
    return wkc;
}

// get ethercat state from slave 
int ec_slave_get_state(ec_t *pec, uint16_t slave, ec_state_t *state, 
        uint16_t *alstatcode) 
{
    uint16_t wkc = 0, value = 0;
    
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    ec_fprd(pec, pec->slaves[slave].fixed_address, 
            EC_REG_ALSTAT, &value, sizeof(value), &wkc);

    if (wkc) {
        slv->act_state = *state = (ec_state_t)value;
    }

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
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

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
        ec_state_t state) 
{
    uint16_t wkc;
    ec_state_t act_state = 0;
    
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

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

                switch (cmd->type) {
                    case EC_MBX_COE: {
                        ec_coe_init_cmd_t *coe = (void *)cmd->cmd;

                        ec_log(10, get_transition_string(transition), 
                                "slave %2d: sending CoE init cmd 0x%04X:%d, "
                                "ca %d, datalen %d, datap %p\n", slave, coe->id, 
                                coe->si_el, coe->ca_atn, coe->datalen, coe->data);

                        uint8_t *buf = (uint8_t *)coe->data;
                        size_t buf_len = coe->datalen;
                        uint32_t abort_code = 0;

                        int ret = ec_coe_sdo_write(pec, slave, coe->id, coe->si_el, 
                                coe->ca_atn, buf, buf_len, &abort_code);
                        if (ret != 0) {
                            ec_log(10, get_transition_string(transition), 
                                    "slave %2d: writing sdo failed: error code 0x%X!\n", 
                                    slave, ret);
                        } 
                    }
                }
            }

            break;
    }

    return 0;
}

// free slave resources
void ec_slave_free(ec_t *pec, uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    ec_mbx_deinit(pec, slave);

    // free resources
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
}

// state transition on ethercat slave
int ec_slave_state_transition(ec_t *pec, uint16_t slave, ec_state_t state) {
    uint16_t wkc;
    ec_state_t act_state = 0;
    
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

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
                    slv->sm[MAILBOX_READ].adr = slv->eeprom.boot_mbx_send_offset;
                    slv->sm[MAILBOX_READ].len = slv->eeprom.boot_mbx_send_size;
                } else {
                    slv->sm[MAILBOX_READ].adr = slv->eeprom.mbx_send_offset;
                    slv->sm[MAILBOX_READ].len = slv->eeprom.mbx_send_size;
                }
                slv->sm[MAILBOX_READ].flags = 0x00010022;
                slv->mbx.sm_state = NULL;

                // write mailbox
                if ((transition == INIT_2_BOOT) && 
                        slv->eeprom.boot_mbx_receive_offset) {
                    slv->sm[MAILBOX_WRITE].adr = slv->eeprom.boot_mbx_receive_offset;
                    slv->sm[MAILBOX_WRITE].len = slv->eeprom.boot_mbx_receive_size;
                } else {
                    slv->sm[MAILBOX_WRITE].adr = slv->eeprom.mbx_receive_offset;
                    slv->sm[MAILBOX_WRITE].len = slv->eeprom.mbx_receive_size;
                }

                slv->sm[MAILBOX_WRITE].flags = 0x00010026;

                ec_mbx_init(pec, slave);

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
//                break;
            } else
                wkc = ec_slave_set_state(pec, slave, EC_STATE_PREOP);
                
            // apply eoe settings if any
            if (slv->eoe.use_eoe) {
                ec_log(10, get_transition_string(transition), 
                        "slave %2d: applying EoE settings\n", slave);
                if (slv->eoe.mac) {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d:         MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                            slave, slv->eoe.mac[0], slv->eoe.mac[1], slv->eoe.mac[2],
                            slv->eoe.mac[3], slv->eoe.mac[4], slv->eoe.mac[5]);
                }
                if (slv->eoe.ip_address) {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d:         IP     %d.%d.%d.%d\n",
                            slave, slv->eoe.ip_address[3], slv->eoe.ip_address[2], 
                            slv->eoe.ip_address[1], slv->eoe.ip_address[0]);
                }
                if (slv->eoe.subnet) {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d:         Subnet  %d.%d.%d.%d\n",
                            slave, slv->eoe.subnet[3], slv->eoe.subnet[2], 
                            slv->eoe.subnet[1], slv->eoe.subnet[0]);
                }
                if (slv->eoe.gateway) {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d:         Gateway %d.%d.%d.%d\n",
                            slave, slv->eoe.gateway[3], slv->eoe.gateway[2], 
                            slv->eoe.gateway[1], slv->eoe.gateway[0]);
                }                        
                if (slv->eoe.dns) {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d:         DNS     %d.%d.%d.%d\n",
                            slave, slv->eoe.dns[3], slv->eoe.dns[2], 
                            slv->eoe.dns[1], slv->eoe.dns[0]);
                }
                if (slv->eoe.dns_name) {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d:         DNSname %s\n",
                            slave, slv->eoe.dns_name);
                }

                ec_eoe_set_ip_parameter(pec, slave, slv->eoe.mac, slv->eoe.ip_address, 
                                slv->eoe.subnet, slv->eoe.gateway, slv->eoe.dns, slv->eoe.dns_name);
            }

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

                    ec_dc_sync(pec, slave, 7, slv->dc.cycle_time_0, 
                            slv->dc.cycle_time_1, slv->dc.cycle_shift);
                } else {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d: configuring dc sync 0, "
                            "cycle_time %d, cycle_shift %d\n", slave,
                            slv->dc.cycle_time_0, slv->dc.cycle_shift);

                    ec_dc_sync(pec, slave, 3, slv->dc.cycle_time_0, 0, 
                            slv->dc.cycle_shift);
                }
            } else
                ec_dc_sync(pec, slave, 0, 0, 0, 0);

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
            // write state to slave
            wkc = ec_slave_set_state(pec, slave, EC_STATE_OP);            
            break;
        }
        case OP_2_SAFEOP:
        case OP_2_PREOP:
            // write state to slave
            wkc = ec_slave_set_state(pec, slave, state);

            if (transition == OP_2_SAFEOP)
                break;
        case OP_2_INIT:
        case SAFEOP_2_PREOP:
        case SAFEOP_2_INIT:
        case PREOP_2_INIT: {
            uint8_t dc_active = 0;
            ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_DCSYNCACT, 
                    &dc_active, sizeof(dc_active), &wkc);

            // write state to slave
            wkc = ec_slave_set_state(pec, slave, state);

            if (    transition == OP_2_PREOP || 
                    transition == SAFEOP_2_PREOP)
                break;
        }
        case BOOT_2_INIT:
        case INIT_2_INIT: {
            // rewrite fixed address
            ec_apwr(pec, slv->auto_inc_address, EC_REG_STADR, 
                    (uint8_t *)&slv->fixed_address, 
                    sizeof(slv->fixed_address), &wkc); 
        
            // disable ditributed clocks
            uint8_t dc_active = 0;
            ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_DCSYNCACT, 
                    &dc_active, sizeof(dc_active), &wkc);

            // free resources
            ec_slave_free(pec, slave);

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

//! Adds master EoE settings.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] mac           Pointer to 6 byte MAC address (mandatory).
 * \param[in] ip_address    Pointer to 4 byte IP address (optional maybe NULL).
 * \param[in] subnet        Pointer to 4 byte subnet address (optional maybe NULL).
 * \param[in] gateway       Pointer to 4 byte gateway address (optional maybe NULL).
 * \param[in] dns           Pointer to 4 byte DNS address (optional maybe NULL).
 * \param[in] dns_name      Null-terminated domain name server string.
 */
void ec_slave_set_eoe_settings(struct ec *pec, uint16_t slave,
        uint8_t *mac, uint8_t *ip_address, uint8_t *subnet, uint8_t *gateway, 
        uint8_t *dns, char *dns_name) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    slv->eoe.use_eoe = 1;

#define EOE_ALLOC(field, sz) {          \
    if (field) {                        \
        slv->eoe.field = malloc(sz);        \
        memcpy(slv->eoe.field, field, sz);  \
    } else { slv->eoe.field = NULL; } }

    EOE_ALLOC(mac, 6);
    EOE_ALLOC(ip_address, 4);
    EOE_ALLOC(subnet, 4);
    EOE_ALLOC(gateway, 4);
    EOE_ALLOC(dns, 4);
    if (dns_name) { slv->eoe.dns_name = strdup(dns_name); } else { slv->eoe.dns_name = NULL; }

#undef EOE_ALLOC

}

