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

#include "libethercat/slave.h"
#include "libethercat/ec.h"

#if LIBETHERCAT_MBX_SUPPORT_COE == 1
#include "libethercat/coe.h"
#endif

#if LIBETHERCAT_MBX_SUPPORT_SOE == 1
#include "libethercat/soe.h"
#endif

#if LIBETHERCAT_MBX_SUPPORT_FOE == 1
#include "libethercat/foe.h"
#endif

#include "libethercat/mbx.h"
#include "libethercat/dc.h"
#include "libethercat/error_codes.h"

#include <string.h>
#include <errno.h>
#include <assert.h>

#if LIBETHERCAT_HAVE_INTTYPES_H == 1
#include <inttypes.h>
#endif

static const osal_char_t *get_transition_string(ec_state_transition_t transition) {
    static const osal_char_t transition_string_boot_to_init[]     = (char[]){ "BOOT_2_INIT" };
    static const osal_char_t transition_string_init_to_boot[]     = (char[]){ "INIT_2_BOOT" };
    static const osal_char_t transition_string_init_to_init[]     = (char[]){ "INIT_2_INIT" };
    static const osal_char_t transition_string_init_to_preop[]    = (char[]){ "INIT_2_PREOP" };
    static const osal_char_t transition_string_init_to_safeop[]   = (char[]){ "INIT_2_SAFEOP" };
    static const osal_char_t transition_string_init_to_op[]       = (char[]){ "INIT_2_OP" };
    static const osal_char_t transition_string_preop_to_init[]    = (char[]){ "PREOP_2_INIT" };
    static const osal_char_t transition_string_preop_to_preop[]   = (char[]){ "PREOP_2_PREOP" };
    static const osal_char_t transition_string_preop_to_safeop[]  = (char[]){ "PREOP_2_SAFEOP" };
    static const osal_char_t transition_string_preop_to_op[]      = (char[]){ "PREOP_2_OP" };
    static const osal_char_t transition_string_safeop_to_init[]   = (char[]){ "SAFEOP_2_INIT" };
    static const osal_char_t transition_string_safeop_to_preop[]  = (char[]){ "SAFEOP_2_PREOP" };
    static const osal_char_t transition_string_safeop_to_safeop[] = (char[]){ "SAFEOP_2_SAFEOP" };
    static const osal_char_t transition_string_safeop_to_op[]     = (char[]){ "SAFEOP_2_OP" };
    static const osal_char_t transition_string_op_to_init[]       = (char[]){ "OP_2_INIT" };
    static const osal_char_t transition_string_op_to_preop[]      = (char[]){ "OP_2_PREOP" };
    static const osal_char_t transition_string_op_to_safeop[]     = (char[]){ "OP_2_SAFEOP" };
    static const osal_char_t transition_string_op_to_op[]         = (char[]){ "OP_2_OP" };
    static const osal_char_t transition_string_unknown[]          = (char[]){ "UNKNOWN" };

    const osal_char_t *ret = transition_string_unknown;

    switch (transition) {
        default:
            ret = transition_string_unknown;
            break;
        case BOOT_2_INIT:
            ret = transition_string_boot_to_init;
            break;
        case INIT_2_BOOT:
            ret = transition_string_init_to_boot;
            break;
        case INIT_2_INIT:
            ret = transition_string_init_to_init;
            break;
        case INIT_2_PREOP:
            ret = transition_string_init_to_preop;
            break;
        case INIT_2_SAFEOP:
            ret = transition_string_init_to_safeop;
            break;
        case INIT_2_OP:
            ret = transition_string_init_to_op;
            break;
        case PREOP_2_INIT:
            ret = transition_string_preop_to_init;
            break;
        case PREOP_2_PREOP:
            ret = transition_string_preop_to_preop;
            break;
        case PREOP_2_SAFEOP:
            ret = transition_string_preop_to_safeop;
            break;
        case PREOP_2_OP:
            ret = transition_string_preop_to_op;
            break;
        case SAFEOP_2_INIT:
            ret = transition_string_safeop_to_init;
            break;
        case SAFEOP_2_PREOP:
            ret = transition_string_safeop_to_preop;
            break;
        case SAFEOP_2_SAFEOP:
            ret = transition_string_safeop_to_safeop;
            break;
        case SAFEOP_2_OP:
            ret = transition_string_safeop_to_op;
            break;
        case OP_2_INIT:
            ret = transition_string_op_to_init;
            break;
        case OP_2_PREOP:
            ret = transition_string_op_to_preop;
            break;
        case OP_2_SAFEOP:
            ret = transition_string_op_to_safeop;
            break;
        case OP_2_OP:
            ret = transition_string_op_to_op;
            break;
    }

    return ret;
}

// initialize init command structure
void ec_slave_mailbox_coe_init_cmd_init(ec_init_cmd_t *cmd,
        int transition, int id, int si_el, int ca_atn,
        osal_char_t *data, osal_size_t datalen) 
{
    assert(cmd != NULL);
    assert(datalen < LEC_MAX_INIT_CMD_DATA);

    cmd->type       = EC_MBX_COE;
    cmd->transition = transition;
    cmd->id         = id; 
    cmd->si_el      = si_el;
    cmd->ca_atn     = ca_atn;
    cmd->datalen    = datalen;

    (void)memcpy(cmd->data, data, datalen);
}

#if LIBETHERCAT_MBX_SUPPORT_SOE == 1
// initialize init command structure
void ec_slave_mailbox_soe_init_cmd_init(ec_init_cmd_t *cmd,
        int transition, int id, int si_el, int ca_atn,
        osal_char_t *data, osal_size_t datalen) 
{
    assert(cmd != NULL);
    assert(datalen < LEC_MAX_INIT_CMD_DATA);

    cmd->type       = EC_MBX_SOE;
    cmd->transition = transition;
    cmd->id         = id; 
    cmd->si_el      = si_el;
    cmd->ca_atn     = ca_atn;
    cmd->datalen    = datalen;

    (void)memcpy(cmd->data, data, datalen);
}
#endif

// add master init command
void ec_slave_add_init_cmd(ec_t *pec, osal_uint16_t slave, ec_init_cmd_t *cmd)
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(cmd != NULL);

    ec_init_cmd_t *last_cmd = LIST_FIRST(&pec->slaves[slave].init_cmds);

    while ( (last_cmd != NULL) && (LIST_NEXT(last_cmd, le) != NULL)) {
        last_cmd = LIST_NEXT(last_cmd, le);
    }

    if (last_cmd != NULL) {
        LIST_INSERT_AFTER(last_cmd, cmd, le);
    } else {
        LIST_INSERT_HEAD(&pec->slaves[slave].init_cmds, cmd, le);
    }
}

// Set Distributed Clocks config to slave
void ec_slave_set_dc_config(struct ec *pec, osal_uint16_t slave, 
        int use_dc, int activation_reg, osal_uint32_t cycle_time_0, 
        osal_uint32_t cycle_time_1, osal_int32_t cycle_shift) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    ec_slave_ptr(slv, pec, slave);

    slv->dc.use_dc         = use_dc;
    slv->dc.activation_reg = activation_reg;
    slv->dc.cycle_time_0   = cycle_time_0;
    slv->dc.cycle_time_1   = cycle_time_1;
    slv->dc.cycle_shift    = cycle_shift;
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

const osal_char_t *al_status_code_2_string(int code) {
    static const osal_char_t *AL_STATUS_CODE_STRING__NOERROR                       = "no error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__UNSPECIFIEDERROR              = "unspecified error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__NOMEMORY                      = "no memory"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__FW_SII_NOT_MATCH              = "firmware sii not match"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__FW_UPDATE_FAILED              = "firmware update failed"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDALCONTROL              = "invalid al control"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__UNKNOWNALCONTROL              = "unknown al control"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__BOOTNOTSUPP                   = "boot not supported"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__NOVALIDFIRMWARE               = "no valid firmware"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDMBXCFGINBOOT           = "invalid mailbox config in boot"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDMBXCFGINPREOP          = "invalid mailbox config in prepop"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDSMCFG                  = "invalid sync manager config"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__NOVALIDINPUTS                 = "invalid inputs"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__NOVALIDOUTPUTS                = "invalid outputs"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__SYNCERROR                     = "sync error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__SMWATCHDOG                    = "sync manager watchdog"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__SYNCTYPESNOTCOMPATIBLE        = "sync types not compatible"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDSMOUTCFG               = "invalid sync manager out config"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDSMINCFG                = "invalid sync manager in config"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDWDCFG                  = "invalid watchdog config"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__WAITFORCOLDSTART              = "wait for cold start"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__WAITFORINIT                   = "wait for init"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__WAITFORPREOP                  = "wait for preop"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__WAITFORSAFEOP                 = "wait for safeop"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDINPUTMAPPING           = "invalid input mapping"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INVALIDOUTPUTMAPPING          = "invalid output mapping"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__INCONSISTENTSETTINGS          = "inconsistent settings"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__FREERUNNOTSUPPORTED           = "freerun not supported"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__SYNCHRONNOTSUPPORTED          = "synchron not supported"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__FREERUNNEEDS3BUFFERMODE       = "freerun needs 3 buffer mode"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__BACKGROUNDWATCHDOG            = "background watchdog"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__NOVALIDINPUTSANDOUTPUTS       = "no valid inputs and outputs"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__FATALSYNCERROR                = "fatal sync error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__NOSYNCERROR                   = "no sync error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__CYCLETIMETOOSMALL             = "cycletime too small"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__DCINVALIDSYNCCFG              = "dc invalid sync config"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__DCINVALIDLATCHCFG             = "dc invalid latch config"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__DCPLLSYNCERROR                = "dc pll sync error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__DCSYNCIOERROR                 = "dc sync io error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__DCSYNCMISSEDERROR             = "dc sync missed error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__DCINVALIDSYNCCYCLETIME        = "dc invalid sync cycletime"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__DCSYNC0CYCLETIME              = "dc sync0 cycletime"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__DCSYNC1CYCLETIME              = "dc sync1 cycletime"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__MBX_AOE                       = "mailbox aoe"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__MBX_EOE                       = "mailbox eoe"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__MBX_COE                       = "mailbox coe"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__MBX_FOE                       = "mailbox foe"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__MBX_SOE                       = "mailbox soe"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__MBX_VOE                       = "mailbox voe"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__EE_NOACCESS                   = "ee no access"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__EE_ERROR                      = "ee error"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__EXT_HARDWARE_NOT_READY        = "ext hardware not ready"; 
    static const osal_char_t *AL_STATUS_CODE_STRING__UNKNOWN                       = "unknown"; 

    const osal_char_t *ret = AL_STATUS_CODE_STRING__UNKNOWN;

    switch (code) {
        default:
            ret = AL_STATUS_CODE_STRING__UNKNOWN;
            break;
        case AL_STATUS_CODE__NOERROR:
            ret = AL_STATUS_CODE_STRING__NOERROR;
            break;
        case AL_STATUS_CODE__UNSPECIFIEDERROR:
            ret = AL_STATUS_CODE_STRING__UNSPECIFIEDERROR;
            break;
        case AL_STATUS_CODE__NOMEMORY:
            ret = AL_STATUS_CODE_STRING__NOMEMORY;
            break;
        case AL_STATUS_CODE__FW_SII_NOT_MATCH:
            ret = AL_STATUS_CODE_STRING__FW_SII_NOT_MATCH;
            break;
        case AL_STATUS_CODE__FW_UPDATE_FAILED:
            ret = AL_STATUS_CODE_STRING__FW_UPDATE_FAILED;
            break;
        case AL_STATUS_CODE__INVALIDALCONTROL:
            ret = AL_STATUS_CODE_STRING__INVALIDALCONTROL;
            break;
        case AL_STATUS_CODE__UNKNOWNALCONTROL:
            ret = AL_STATUS_CODE_STRING__UNKNOWNALCONTROL;
            break;
        case AL_STATUS_CODE__BOOTNOTSUPP:
            ret = AL_STATUS_CODE_STRING__BOOTNOTSUPP;
            break;
        case AL_STATUS_CODE__NOVALIDFIRMWARE:
            ret = AL_STATUS_CODE_STRING__NOVALIDFIRMWARE;
            break;
        case AL_STATUS_CODE__INVALIDMBXCFGINBOOT:
            ret = AL_STATUS_CODE_STRING__INVALIDMBXCFGINBOOT;
            break;
        case AL_STATUS_CODE__INVALIDMBXCFGINPREOP:
            ret = AL_STATUS_CODE_STRING__INVALIDMBXCFGINPREOP;
            break;
        case AL_STATUS_CODE__INVALIDSMCFG:
            ret = AL_STATUS_CODE_STRING__INVALIDSMCFG;
            break;
        case AL_STATUS_CODE__NOVALIDINPUTS:
            ret = AL_STATUS_CODE_STRING__NOVALIDINPUTS;
            break;
        case AL_STATUS_CODE__NOVALIDOUTPUTS:
            ret = AL_STATUS_CODE_STRING__NOVALIDOUTPUTS;
            break;
        case AL_STATUS_CODE__SYNCERROR:
            ret = AL_STATUS_CODE_STRING__SYNCERROR;
            break;
        case AL_STATUS_CODE__SMWATCHDOG:
            ret = AL_STATUS_CODE_STRING__SMWATCHDOG;
            break;
        case AL_STATUS_CODE__SYNCTYPESNOTCOMPATIBLE:
            ret = AL_STATUS_CODE_STRING__SYNCTYPESNOTCOMPATIBLE;
            break;
        case AL_STATUS_CODE__INVALIDSMOUTCFG:
            ret = AL_STATUS_CODE_STRING__INVALIDSMOUTCFG;
            break;
        case AL_STATUS_CODE__INVALIDSMINCFG:
            ret = AL_STATUS_CODE_STRING__INVALIDSMINCFG;
            break;
        case AL_STATUS_CODE__INVALIDWDCFG:
            ret = AL_STATUS_CODE_STRING__INVALIDWDCFG;
            break;
        case AL_STATUS_CODE__WAITFORCOLDSTART:
            ret = AL_STATUS_CODE_STRING__WAITFORCOLDSTART;
            break;
        case AL_STATUS_CODE__WAITFORINIT:
            ret = AL_STATUS_CODE_STRING__WAITFORINIT;
            break;
        case AL_STATUS_CODE__WAITFORPREOP:
            ret = AL_STATUS_CODE_STRING__WAITFORPREOP;
            break;
        case AL_STATUS_CODE__WAITFORSAFEOP:
            ret = AL_STATUS_CODE_STRING__WAITFORSAFEOP;
            break;
        case AL_STATUS_CODE__INVALIDINPUTMAPPING:
            ret = AL_STATUS_CODE_STRING__INVALIDINPUTMAPPING;
            break;
        case AL_STATUS_CODE__INVALIDOUTPUTMAPPING:
            ret = AL_STATUS_CODE_STRING__INVALIDOUTPUTMAPPING;
            break;
        case AL_STATUS_CODE__INCONSISTENTSETTINGS:
            ret = AL_STATUS_CODE_STRING__INCONSISTENTSETTINGS;
            break;
        case AL_STATUS_CODE__FREERUNNOTSUPPORTED:
            ret = AL_STATUS_CODE_STRING__FREERUNNOTSUPPORTED;
            break;
        case AL_STATUS_CODE__SYNCHRONNOTSUPPORTED:
            ret = AL_STATUS_CODE_STRING__SYNCHRONNOTSUPPORTED;
            break;
        case AL_STATUS_CODE__FREERUNNEEDS3BUFFERMODE:
            ret = AL_STATUS_CODE_STRING__FREERUNNEEDS3BUFFERMODE;
            break;
        case AL_STATUS_CODE__BACKGROUNDWATCHDOG:
            ret = AL_STATUS_CODE_STRING__BACKGROUNDWATCHDOG;
            break;
        case AL_STATUS_CODE__NOVALIDINPUTSANDOUTPUTS:
            ret = AL_STATUS_CODE_STRING__NOVALIDINPUTSANDOUTPUTS;
            break;
        case AL_STATUS_CODE__FATALSYNCERROR:
            ret = AL_STATUS_CODE_STRING__FATALSYNCERROR;
            break;
        case AL_STATUS_CODE__NOSYNCERROR:
            ret = AL_STATUS_CODE_STRING__NOSYNCERROR;
            break;
        case AL_STATUS_CODE__CYCLETIMETOOSMALL:
            ret = AL_STATUS_CODE_STRING__CYCLETIMETOOSMALL;
            break;
        case AL_STATUS_CODE__DCINVALIDSYNCCFG:
            ret = AL_STATUS_CODE_STRING__DCINVALIDSYNCCFG;
            break;
        case AL_STATUS_CODE__DCINVALIDLATCHCFG:
            ret = AL_STATUS_CODE_STRING__DCINVALIDLATCHCFG;
            break;
        case AL_STATUS_CODE__DCPLLSYNCERROR:
            ret = AL_STATUS_CODE_STRING__DCPLLSYNCERROR;
            break;
        case AL_STATUS_CODE__DCSYNCIOERROR:
            ret = AL_STATUS_CODE_STRING__DCSYNCIOERROR;
            break;
        case AL_STATUS_CODE__DCSYNCMISSEDERROR:
            ret = AL_STATUS_CODE_STRING__DCSYNCMISSEDERROR;
            break;
        case AL_STATUS_CODE__DCINVALIDSYNCCYCLETIME:
            ret = AL_STATUS_CODE_STRING__DCINVALIDSYNCCYCLETIME;
            break;
        case AL_STATUS_CODE__DCSYNC0CYCLETIME:
            ret = AL_STATUS_CODE_STRING__DCSYNC0CYCLETIME;
            break;
        case AL_STATUS_CODE__DCSYNC1CYCLETIME:
            ret = AL_STATUS_CODE_STRING__DCSYNC1CYCLETIME;
            break;
        case AL_STATUS_CODE__MBX_AOE:
            ret = AL_STATUS_CODE_STRING__MBX_AOE;
            break;
        case AL_STATUS_CODE__MBX_EOE:
            ret = AL_STATUS_CODE_STRING__MBX_EOE;
            break;
        case AL_STATUS_CODE__MBX_COE:
            ret = AL_STATUS_CODE_STRING__MBX_COE;
            break;
        case AL_STATUS_CODE__MBX_FOE:
            ret = AL_STATUS_CODE_STRING__MBX_FOE;
            break;
        case AL_STATUS_CODE__MBX_SOE:
            ret = AL_STATUS_CODE_STRING__MBX_SOE;
            break;
        case AL_STATUS_CODE__MBX_VOE:
            ret = AL_STATUS_CODE_STRING__MBX_VOE;
            break;
        case AL_STATUS_CODE__EE_NOACCESS:
            ret = AL_STATUS_CODE_STRING__EE_NOACCESS;
            break;
        case AL_STATUS_CODE__EE_ERROR:
            ret = AL_STATUS_CODE_STRING__EE_ERROR;
            break;
        case AL_STATUS_CODE__EXT_HARDWARE_NOT_READY:
            ret = AL_STATUS_CODE_STRING__EXT_HARDWARE_NOT_READY;
            break;
    }

    return ret;
}

static const osal_char_t *ecat_state_2_string(int state) {
    static const osal_char_t *ECAT_STATE_STRING_INIT     = "INIT";
    static const osal_char_t *ECAT_STATE_STRING_PREOP    = "PRE-OPERATIONAL";
    static const osal_char_t *ECAT_STATE_STRING_SAFEOP   = "SAFE-OPERATIONAL";
    static const osal_char_t *ECAT_STATE_STRING_OP       = "OPERATIONAL";
    static const osal_char_t *ECAT_STATE_STRING_BOOT     = "BOOT";
    static const osal_char_t *ECAT_STATE_STRING_UNKNOWN  = "UNKNOWN";

    const osal_char_t *ret = ECAT_STATE_STRING_UNKNOWN;

    switch (state) {
        default:
            break;
        case EC_STATE_INIT:
            ret = ECAT_STATE_STRING_INIT;
            break;
        case EC_STATE_PREOP:
            ret = ECAT_STATE_STRING_PREOP;
            break;
        case EC_STATE_SAFEOP:
            ret = ECAT_STATE_STRING_SAFEOP;
            break;
        case EC_STATE_OP:
            ret = ECAT_STATE_STRING_OP;
            break;
        case EC_STATE_BOOT:
            ret = ECAT_STATE_STRING_BOOT;
            break;
    }

    return ret;
}

// Set EtherCAT state on slave 
int ec_slave_set_state(ec_t *pec, osal_uint16_t slave, ec_state_t state) {
    int ret = EC_OK;
    osal_uint16_t wkc = 0u;
    osal_uint16_t act_state = 0u;
    osal_uint16_t value = 0u;
    
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
        
    // generate transition
    ec_state_transition_t transition = ((pec->slaves[slave].act_state & EC_STATE_MASK) << 8u) | (state & EC_STATE_MASK); 

    if (ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_ALCTL, &state, sizeof(state), &wkc) != EC_OK) { 
        // just return, we got an error from ec_transceive
        ret = EC_ERROR_SLAVE_NOT_RESPONDING;
    } else if ((state & EC_STATE_RESET) != 0u) {
        osal_timer_t timeout;
        osal_timer_init(&timeout, 1000000000);

        do {
            wkc = 0u;

            (void)ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_ALCTL, &state, sizeof(state), &wkc);
            (void)ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_ALSTAT, &act_state, sizeof(act_state), &wkc);

            if ((wkc != 0u) && !(act_state & EC_STATE_ERROR)) {
                ec_log(1, get_transition_string(transition), "slave %2d: resetting seems to have succeeded, wkc %d\n", slave, wkc);
                break;
            }
        } while (osal_timer_expired(&timeout) != OSAL_ERR_TIMEOUT);
    } else {
        ec_log(10, get_transition_string(transition), "slave %2d: %s state requested\n", slave, ecat_state_2_string(state));
    
        pec->slaves[slave].transition_active = OSAL_TRUE;
        pec->slaves[slave].expected_state = state;

        osal_timer_t timeout;
        osal_timer_init(&timeout, 10000000000); // 10 second timeout

        do {
            if (ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_ALCTL, &state, sizeof(state), &wkc) == EC_OK) { 
                act_state = 0;
                ret = ec_slave_get_state(pec, slave, &act_state, NULL);

                if ((act_state & EC_STATE_ERROR) != 0u) {
                    if (ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_ALSTATCODE, &value, sizeof(value), &wkc) == EC_OK) {
                        if (value != 0u) {
                            ec_log(10, get_transition_string(transition), "slave %2d: state switch to %d failed, "
                                    "alstatcode 0x%04X : %s\n", slave, state, value, al_status_code_2_string(value));

                            (void)ec_slave_set_state(pec, slave, (act_state & EC_STATE_MASK) | EC_STATE_RESET);
                        }
                    }
                }
            }

            osal_sleep(1000000);
        } while ((act_state != state) && (osal_timer_expired(&timeout) != OSAL_ERR_TIMEOUT));
 
        if (osal_timer_expired(&timeout) == OSAL_ERR_TIMEOUT) {
            ec_log(1, get_transition_string(transition), "slave %2d: did not respond on state switch to %d\n", slave, state);
            ret = EC_ERROR_SLAVE_NOT_RESPONDING;
        }

        if (ret == EC_OK) {
            ec_log(100, get_transition_string(transition), "slave %2d: state %X, act_state %X, wkc %d\n", slave, state, act_state, wkc);

            if (act_state == state) {    
                ec_log(10, get_transition_string(transition), "slave %2d: %s state reached\n", slave, ecat_state_2_string(act_state));
                ret = EC_OK;
            } else {
                ec_log(1, get_transition_string(transition), "slave %2d: %s state switch FAILED!\n", slave, ecat_state_2_string(act_state));
                ret = EC_ERROR_SLAVE_STATE_SWITCH;
            }

            pec->slaves[slave].act_state = act_state;
        }
        
        pec->slaves[slave].transition_active = OSAL_FALSE;
    }

    return ret;
}

// get ethercat state from slave 
int ec_slave_get_state(ec_t *pec, osal_uint16_t slave, ec_state_t *state, osal_uint16_t *alstatcode) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    int ret = EC_OK;
    osal_uint16_t wkc = 0u;
    osal_uint16_t value = 0u;
    
    ec_slave_ptr(slv, pec, slave);
    ret = ec_fprd(pec, pec->slaves[slave].fixed_address, 
            EC_REG_ALSTAT, &value, sizeof(value), &wkc);

    if ((ret != EC_OK) || (wkc == 0u)) {
        *state = EC_STATE_UNKNOWN;
        ret = EC_ERROR_SLAVE_NOT_RESPONDING;
    } else {
        slv->act_state = (ec_state_t)value;
        *state = slv->act_state;

        if (alstatcode && (*state & 0x10)) {
            value = 0u;
            wkc = 0u;
            ret = ec_fprd(pec, pec->slaves[slave].fixed_address, 
                    EC_REG_ALSTATCODE, &value, sizeof(value), &wkc);

            if ((ret == EC_OK) && (wkc != 0u)) {
                *alstatcode = value;
            }
        }
    }

    return ret;
}

// generate pd mapping
int ec_slave_generate_mapping(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    int ret = EC_OK;
    ec_slave_ptr(slv, pec, slave);

    if (slv->sm_set_by_user != 0) {
        // we're already done
    } else {
        // check sm settings
#if LIBETHERCAT_MBX_SUPPORT_COE == 1
        if ((slv->eeprom.mbx_supported & EC_EEPROM_MBX_COE) != 0u) {
            ret = ec_coe_generate_mapping(pec, slave);
        } else 
#endif
#if LIBETHERCAT_MBX_SUPPORT_SOE == 1
        if ((slv->eeprom.mbx_supported & EC_EEPROM_MBX_SOE) != 0u) {
            ret = ec_soe_generate_mapping(pec, slave);
        } else
#endif
        {
            // try eeprom
            for (osal_uint8_t sm_idx = 0; sm_idx < slv->sm_ch; ++sm_idx) {
                int txpdos_cnt = 0;
                int rxpdos_cnt = 0;
                osal_size_t bit_len = 0u;
                ec_eeprom_cat_pdo_t *pdo;

                // inputs and outputs
                TAILQ_FOREACH(pdo, &slv->eeprom.txpdos, qh) {
                    if (sm_idx == pdo->sm_nr) { // cppcheck-suppress uninitvar
                        for (osal_uint32_t entry_idx = 0; entry_idx < pdo->n_entry; ++entry_idx) { 
                            ec_log(100, "SLAVE_GENERATE_MAPPING_EEPROM", "slave %2d: got "
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
                        for (osal_uint32_t entry_idx = 0; entry_idx < pdo->n_entry; ++entry_idx) { 
                            ec_log(100, "SLAVE_GENERATE_MAPPING_EEPROM", "slave %2d: got "
                                    "rxpdo bit_len %d, sm %d\n", slave, 
                                    pdo->entries[entry_idx].bit_len, pdo->sm_nr);
                            bit_len += pdo->entries[entry_idx].bit_len;
                        }

                        rxpdos_cnt++;
                    }
                }

                ec_log(100, "SLAVE_GENERATE_MAPPING_EEPROM", "slave %2d: txpdos %d, rxpdos %d, bitlen%d %" PRIu64 "\n", 
                        slave, txpdos_cnt, rxpdos_cnt, sm_idx, bit_len);

                if (bit_len > 0u) {
                    ec_log(10, "SLAVE_GENERATE_MAPPING_EEPROM", "slave %2d: sm%d length bits %" PRIu64 ", bytes %" PRIu64 "\n", 
                            slave, sm_idx, bit_len, (bit_len + 7u) / 8u);

                    slv->sm[sm_idx].len = (bit_len + 7u) / 8u;
                }
            }
        }
    }

    return ret;
}

// prepare state transition on ethercat slave
int ec_slave_prepare_state_transition(ec_t *pec, osal_uint16_t slave, 
        ec_state_t state) 
{
    ec_state_t act_state = 0;
    int ret = EC_OK;
    
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    // check error state
    if (ec_slave_get_state(pec, slave, &act_state, NULL) != EC_OK) {
        ec_log(10, "SLAVE_PREPARE_TRANSITION", "slave %2d: error getting state\n", slave);
        ret = EC_ERROR_SLAVE_NOT_RESPONDING;
    } else {
        // generate transition
        ec_state_transition_t transition = ((act_state & EC_STATE_MASK) << 8u) | (state & EC_STATE_MASK); 

        switch (transition) {
            default:
                break;
            case INIT_2_SAFEOP:
            case PREOP_2_SAFEOP:
                ec_log(10, get_transition_string(transition), "slave %2d: sending init cmds\n", slave);

                ec_init_cmd_t *cmd;
                LIST_FOREACH(cmd, &slv->init_cmds, le) {
                    if (cmd->transition != 0x24) { // cppcheck-suppress uninitvar
                        continue;
                    }

                    switch (cmd->type) {
                        default:
                            break;
#if LIBETHERCAT_MBX_SUPPORT_COE == 1
                        case EC_MBX_COE: {
                            ec_log(10, get_transition_string(transition), 
                                    "slave %2d: sending CoE init cmd 0x%04X:%d, "
                                    "ca %d, datalen %" PRIu64 ", datap %p\n", slave, cmd->id, 
                                    cmd->si_el, cmd->ca_atn, cmd->datalen, cmd->data);

                            osal_uint8_t *buf = (osal_uint8_t *)cmd->data;
                            osal_size_t buf_len = cmd->datalen;
                            osal_uint32_t abort_code = 0;

                            int local_ret = ec_coe_sdo_write(pec, slave, cmd->id, cmd->si_el, cmd->ca_atn, buf, buf_len, &abort_code);
                            if (local_ret != EC_OK) {
                                ec_log(10, get_transition_string(transition), 
                                        "slave %2d: writing sdo failed: error code 0x%X!\n", 
                                        slave, local_ret);
                            } 
                            break;
                        }
#endif
#if LIBETHERCAT_MBX_SUPPORT_SOE == 1
                        case EC_MBX_SOE: {
                            ec_log(10, get_transition_string(transition), 
                                    "slave %2d: sending SoE init cmd 0x%04X:%d, "
                                    "atn %d, datalen %" PRIu64 ", datap %p\n", slave, cmd->id, 
                                    cmd->si_el, cmd->ca_atn, cmd->datalen, cmd->data);

                            osal_uint8_t *buf = (osal_uint8_t *)cmd->data;
                            osal_size_t buf_len = cmd->datalen;

                            int local_ret = ec_soe_write(pec, slave, cmd->ca_atn, cmd->id, cmd->si_el, buf, buf_len);

                            if (local_ret != EC_OK) {
                                ec_log(10, get_transition_string(transition), 
                                        "slave %2d: writing SoE failed: error code 0x%X!\n", 
                                        slave, local_ret);
                            } 
                            break;
                        }
#endif
                    }
                }

                break;
        }
    }

    return ret;
}

// init slave resources
void ec_slave_init(struct ec *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    
    ec_slave_ptr(slv, pec, slave);

    slv->transition_active = OSAL_FALSE;
    osal_mutex_init(&slv->transition_mutex, NULL);
}

// free slave resources
void ec_slave_free(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    ec_mbx_deinit(pec, slave);
    
    osal_mutex_destroy(&slv->transition_mutex);
}

#if LIBETHERCAT_MBX_SUPPORT_EOE == 1
static osal_bool_t check_null(osal_uint8_t *ptr, osal_size_t len) {
    osal_bool_t ret = OSAL_TRUE;

    for (unsigned i = 0; i < len; ++i) {
        if (ptr[i] != 0) {
            ret = OSAL_FALSE;
            break;
        }
    }

    return ret;
}
#endif

// state transition on ethercat slave
int ec_slave_state_transition(ec_t *pec, osal_uint16_t slave, ec_state_t state) {
    osal_uint16_t wkc;
    osal_uint16_t al_status_code = 0u;
    ec_state_t act_state = 0;
    int ret = EC_OK;
    
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

#define ec_reg_read(reg, buf, buflen) {                                 \
    (void)ec_fprd(pec, pec->slaves[slave].fixed_address, (reg),         \
            (buf), (buflen), &wkc);                                     \
    if (!wkc) { ec_log(10, "SLAVE_REG_READ",                            \
            "reading reg 0x%X : no answer from slave %2d\n", (reg), slave); } }
    
    osal_mutex_lock(&slv->transition_mutex);

    // check error state
    ret = ec_slave_get_state(pec, slave, &act_state, &al_status_code);
    if (ret != EC_OK) {
        ec_log(10, "ERROR", "could not get state of slave %d\n", slave);
        
        // rewrite fixed address
        (void)ec_apwr(pec, slv->auto_inc_address, EC_REG_STADR, 
                (osal_uint8_t *)&slv->fixed_address, 
                sizeof(slv->fixed_address), &wkc); 

        ret = EC_ERROR_SLAVE_NOT_RESPONDING;
    } else {
        if (    ((act_state & EC_STATE_ERROR) != 0u) &&
                ((al_status_code != 0u))) { // reset error state first
            osal_timer_t error_reset_timeout;
            osal_timer_init(&error_reset_timeout, 1000000000);

            do {
                ec_log(10, "SLAVE_TRANSITION", "slave %2d is in ERROR (AL status 0x%04X, AL status code 0x%04X), resetting first.\n", slave, act_state, al_status_code);
                (void)ec_slave_set_state(pec, slave, (act_state & EC_STATE_MASK) | EC_STATE_RESET);
                act_state = 0;
                al_status_code = 0;
    
                ret = ec_slave_get_state(pec, slave, &act_state, &al_status_code);
                if (ret == EC_OK) {
                    if ((act_state & EC_STATE_ERROR) == 0u) {
                        break;
                    }
                } 
            } while (osal_timer_expired(&error_reset_timeout) != OSAL_ERR_TIMEOUT);

            if (osal_timer_expired(&error_reset_timeout) == OSAL_ERR_TIMEOUT) {
                ec_log(10, "SLAVE_TRANSITION", "slave %2d: try to reset PDI\n", slave);
                osal_uint8_t reset_vals[] = { (osal_uint8_t)'R', (osal_uint8_t)'E', (osal_uint8_t)'S' };
                for (int i = 0; i < 3; ++i) {
                    (void)ec_fpwr(pec, slv->fixed_address, 0x41, &reset_vals[i], 1, &wkc);
                }
                ec_log(10, "SLAVE_TRANSITION", "slave %2d: try to reset ESC\n", slave);
                for (int i = 0; i < 3; ++i) {
                    (void)ec_fpwr(pec, slv->fixed_address, 0x40, &reset_vals[i], 1, &wkc);
                }

                ret = EC_ERROR_SLAVE_NOT_RESPONDING;
            } else {
                ret = EC_OK;
            }
        }
    }

    if (ret == EC_OK) {
        // generate transition
        ec_state_transition_t transition = ((act_state & EC_STATE_MASK) << 8u) | 
            (state & EC_STATE_MASK); 

        ec_log(10, get_transition_string(transition), "slave %2d executing transition %X\n", slave, transition);

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
                if (slv->eeprom.mbx_supported != 0u) {
                    // read mailbox
                    if ((transition == INIT_2_BOOT) && 
                            slv->eeprom.boot_mbx_send_offset) {
                        slv->sm[MAILBOX_READ].adr = slv->eeprom.boot_mbx_send_offset;
                        slv->sm[MAILBOX_READ].len = slv->eeprom.boot_mbx_send_size;
                    } else {
                        slv->sm[MAILBOX_READ].adr = slv->eeprom.mbx_send_offset;
                        slv->sm[MAILBOX_READ].len = slv->eeprom.mbx_send_size;
                    }
                    slv->sm[MAILBOX_READ].enable_sm = 0x01;
                    slv->sm[MAILBOX_READ].control_register = 0x22;

                    // write mailbox
                    if ((transition == INIT_2_BOOT) && 
                            slv->eeprom.boot_mbx_receive_offset) {
                        slv->sm[MAILBOX_WRITE].adr = slv->eeprom.boot_mbx_receive_offset;
                        slv->sm[MAILBOX_WRITE].len = slv->eeprom.boot_mbx_receive_size;
                    } else {
                        slv->sm[MAILBOX_WRITE].adr = slv->eeprom.mbx_receive_offset;
                        slv->sm[MAILBOX_WRITE].len = slv->eeprom.mbx_receive_size;
                    }

                    slv->sm[MAILBOX_WRITE].enable_sm = 0x01;
                    slv->sm[MAILBOX_WRITE].control_register = 0x26;

                    ec_mbx_init(pec, slave);

                    for (osal_uint32_t sm_idx = 0u; sm_idx < 2u; ++sm_idx) {
                        ec_log(10, get_transition_string(transition), "slave %2d: "
                                "sm%u, adr 0x%04X, len %3d, enable_sm "
                                        "0x%X, control_register 0x%X\n",
                                slave, sm_idx, slv->sm[sm_idx].adr, 
                                slv->sm[sm_idx].len, slv->sm[sm_idx].enable_sm,
                                slv->sm[sm_idx].control_register);

                        (void)ec_fpwr(pec, slv->fixed_address, 0x800u + (sm_idx * 8u),
                                &slv->sm[sm_idx], sizeof(ec_slave_sm_t), &wkc);
                    }

                }

                (void)ec_eeprom_to_pdi(pec, slave);

                // write state to slave
                if (transition == INIT_2_BOOT) {
                    ret = ec_slave_set_state(pec, slave, EC_STATE_BOOT);
                    //                break;
                } else {
                    ret = ec_slave_set_state(pec, slave, EC_STATE_PREOP);
                }

#if LIBETHERCAT_MBX_SUPPORT_EOE == 1
                // apply eoe settings if any
                if ((ret == EC_OK) && (slv->eoe.use_eoe != 0)) {
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d: applying EoE settings\n", slave);

                    if (check_null(slv->eoe.mac, LEC_EOE_MAC_LEN) != OSAL_TRUE) {
                        ec_log(10, get_transition_string(transition), 
                                "slave %2d:         MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                                slave, slv->eoe.mac[0], slv->eoe.mac[1], slv->eoe.mac[2],
                                slv->eoe.mac[3], slv->eoe.mac[4], slv->eoe.mac[5]);
                    }
                    if (check_null(slv->eoe.ip_address, LEC_EOE_IP_ADDRESS_LEN) != OSAL_TRUE) {
                        ec_log(10, get_transition_string(transition), 
                                "slave %2d:         IP     %d.%d.%d.%d\n",
                                slave, slv->eoe.ip_address[3], slv->eoe.ip_address[2], 
                                slv->eoe.ip_address[1], slv->eoe.ip_address[0]);
                    }
                    if (check_null(slv->eoe.subnet, LEC_EOE_SUBNET_LEN) != OSAL_TRUE) {
                        ec_log(10, get_transition_string(transition), 
                                "slave %2d:         Subnet  %d.%d.%d.%d\n",
                                slave, slv->eoe.subnet[3], slv->eoe.subnet[2], 
                                slv->eoe.subnet[1], slv->eoe.subnet[0]);
                    }
                    if (check_null(slv->eoe.gateway, LEC_EOE_GATEWAY_LEN) != OSAL_TRUE) {
                        ec_log(10, get_transition_string(transition), 
                                "slave %2d:         Gateway %d.%d.%d.%d\n",
                                slave, slv->eoe.gateway[3], slv->eoe.gateway[2], 
                                slv->eoe.gateway[1], slv->eoe.gateway[0]);
                    }                        
                    if (check_null(slv->eoe.dns, LEC_EOE_DNS_NAME_LEN) != OSAL_TRUE) {
                        ec_log(10, get_transition_string(transition), 
                                "slave %2d:         DNS     %d.%d.%d.%d\n",
                                slave, slv->eoe.dns[3], slv->eoe.dns[2], 
                                slv->eoe.dns[1], slv->eoe.dns[0]);
                    }
                    if (slv->eoe.dns_name[0] != '\0') {
                        ec_log(10, get_transition_string(transition), 
                                "slave %2d:         DNSname %s\n",
                                slave, slv->eoe.dns_name);
                    }

                    ret = ec_eoe_set_ip_parameter(pec, slave, slv->eoe.mac, slv->eoe.ip_address, 
                            slv->eoe.subnet, slv->eoe.gateway, slv->eoe.dns, slv->eoe.dns_name);
                }
#endif

                if ((ret != EC_OK) || (transition == INIT_2_PREOP) || (transition == INIT_2_BOOT)) {
                    break;
                }
            }
            // cppcheck-suppress misra-c2012-16.3
            case PREOP_2_SAFEOP: 
            case PREOP_2_OP: {
                // configure distributed clocks if needed 
                if (pec->dc.have_dc && slv->dc.use_dc) {
                    if (slv->dc.cycle_time_0 == 0u) {
                        slv->dc.cycle_time_0 = pec->main_cycle_interval; 
                    }
                    ec_log(10, get_transition_string(transition), 
                        "slave %2d: configuring actiavtion reg. %d, "
                        "cycle_times %d/%d, cycle_shift %d\n", 
                        slave, slv->dc.activation_reg, slv->dc.cycle_time_0, 
                        slv->dc.cycle_time_1, slv->dc.cycle_shift);
                    ec_dc_sync(pec, slave, slv->dc.activation_reg, slv->dc.cycle_time_0, 
                            slv->dc.cycle_time_1, slv->dc.cycle_shift); 
                } else {
                    ec_dc_sync(pec, slave, 0, 0, 0, 0);
                }

                int start_sm = slv->eeprom.mbx_supported ? 2 : 0;

                for (osal_uint32_t sm_idx = start_sm; sm_idx < slv->sm_ch; ++sm_idx) {
                    if (!slv->sm[sm_idx].adr) {
                        continue;
                    }

                    ec_log(10, get_transition_string(transition), "slave %2d: "
                            "sm%d, adr 0x%04X, len %3d, enable_sm "
                                "0x%X, control_register 0x%X\n",
                            slave, sm_idx, slv->sm[sm_idx].adr, 
                            slv->sm[sm_idx].len, slv->sm[sm_idx].enable_sm,
                            slv->sm[sm_idx].control_register);

                    (void)ec_fpwr(pec, slv->fixed_address, 0x800u + (sm_idx * 8u),
                            &slv->sm[sm_idx], sizeof(ec_slave_sm_t), &wkc);

                    if (!wkc) {
                        ec_log(10, get_transition_string(transition), 
                                "slave %2d: no answer on "
                                "writing sm%d settings\n", slave, sm_idx);
                    }
                }

                for (osal_uint32_t fmmu_idx = 0; fmmu_idx < slv->fmmu_ch; ++fmmu_idx) { 
                    if (!slv->fmmu[fmmu_idx].active) {
                        continue;
                    }

                    // safeop to op stuff 
                    ec_log(10, get_transition_string(transition), 
                            "slave %2d: log%d 0x%08X/%d/%d, len %3d, phys "
                            "0x%04X/%d, type %d, active %d\n", slave, fmmu_idx,
                            slv->fmmu[fmmu_idx].log, 
                            slv->fmmu[fmmu_idx].log_bit_start,
                            slv->fmmu[fmmu_idx].log_bit_stop, 
                            slv->fmmu[fmmu_idx].log_len,
                            slv->fmmu[fmmu_idx].phys, 
                            slv->fmmu[fmmu_idx].phys_bit_start,
                            slv->fmmu[fmmu_idx].type, 
                            slv->fmmu[fmmu_idx].active);

                    (void)ec_fpwr(pec, slv->fixed_address, 0x600u + (16u * fmmu_idx),
                            (osal_uint8_t *)&slv->fmmu[fmmu_idx], 
                            sizeof(ec_slave_fmmu_t), &wkc);

                }

                // write state to slave
                ret = ec_slave_set_state(pec, slave, EC_STATE_SAFEOP);

                if ((ret != EC_OK) || (transition == INIT_2_SAFEOP) || (transition == PREOP_2_SAFEOP)) {
                    break;
                }
            }
            // cppcheck-suppress misra-c2012-16.3
            case SAFEOP_2_OP: {
                // write state to slave
                ret = ec_slave_set_state(pec, slave, EC_STATE_OP);            
                break;
            }
            case OP_2_SAFEOP:
            case OP_2_PREOP:
                // write state to slave
                ret = ec_slave_set_state(pec, slave, state);

                if ((ret != EC_OK) || (transition == OP_2_SAFEOP)) {
                    break;
                }
            // cppcheck-suppress misra-c2012-16.3
            case OP_2_INIT:
            case SAFEOP_2_PREOP:
            case SAFEOP_2_INIT:
            case PREOP_2_INIT: {
                osal_uint8_t dc_active = 0;
                (void)ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_DCSYNCACT, 
                        &dc_active, sizeof(dc_active), &wkc);

                // write state to slave
                ret = ec_slave_set_state(pec, slave, state);

                if ((ret != EC_OK) || (transition == OP_2_PREOP) || (transition == SAFEOP_2_PREOP)) {
                    break;
                }
            }
            case UNKNOWN_2_INIT: {
                // rewrite fixed address
                (void)ec_apwr(pec, slv->auto_inc_address, EC_REG_STADR, 
                        (osal_uint8_t *)&slv->fixed_address, 
                        sizeof(slv->fixed_address), &wkc); 

                // disable ditributed clocks
                osal_uint8_t dc_active = 0;
                (void)ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_DCSYNCACT, 
                        &dc_active, sizeof(dc_active), &wkc);
                
                // write state to slave
                ret = ec_slave_set_state(pec, slave, state);

                if (ret != EC_OK) {
                    break;
                }
            }
            // cppcheck-suppress misra-c2012-16.3
            case BOOT_2_INIT:
            case INIT_2_INIT: {
                ec_log(10, get_transition_string(transition), "slave %2d rewriting fixed address\n", slave);

                // rewrite fixed address
                (void)ec_apwr(pec, slv->auto_inc_address, EC_REG_STADR, 
                        (osal_uint8_t *)&slv->fixed_address, 
                        sizeof(slv->fixed_address), &wkc); 

                ec_log(10, get_transition_string(transition), "slave %2d disable dcs\n", slave);

                // disable ditributed clocks
                osal_uint8_t dc_active = 0;
                (void)ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_DCSYNCACT, 
                        &dc_active, sizeof(dc_active), &wkc);

                // free resources
                ec_slave_free(pec, slave);

                ec_log(10, get_transition_string(transition), "slave %2d get number of sm\n", slave);

                // get number of sync managers
                ec_reg_read(EC_REG_SM_CH, &slv->sm_ch, 1);
                if (slv->sm_ch > LEC_MAX_SLAVE_SM) {
                    ec_log(10, get_transition_string(transition), "slave %2d got %d sync manager, but can only use %" PRId64 "\n",
                            slave, slv->sm_ch, LEC_MAX_SLAVE_SM);

                    slv->sm_ch = LEC_MAX_SLAVE_SM;
                }

                if (slv->sm_ch != 0u) {
                    for (osal_uint32_t i = 0u; i < slv->sm_ch; ++i) {
                        (void)ec_transmit_no_reply(pec, EC_CMD_FPWR, 
                                ec_to_adr(slv->fixed_address, 0x800u + (8u * i)),
                                (osal_uint8_t *)&slv->sm[i], sizeof(ec_slave_sm_t));
                    }
                }
                
                ec_log(10, get_transition_string(transition), "slave %2d get number of fmmu\n", slave);

                // get number of fmmus
                ec_reg_read(EC_REG_FMMU_CH, &slv->fmmu_ch, 1);
                if (slv->fmmu_ch > LEC_MAX_SLAVE_FMMU) {
                    ec_log(10, get_transition_string(transition), "slave %2d got %d fmmus, but can only use %" PRId64 "\n",
                            slave, slv->fmmu_ch, LEC_MAX_SLAVE_FMMU);

                    slv->fmmu_ch = LEC_MAX_SLAVE_FMMU;
                }

                if (slv->fmmu_ch != 0u) {
                    for (osal_uint32_t i = 0; i < slv->fmmu_ch; ++i) {
                        (void)ec_transmit_no_reply(pec, EC_CMD_FPWR, 
                                ec_to_adr(slv->fixed_address, 0x600u + (16u * i)),
                                (osal_uint8_t *)&slv->fmmu[i], sizeof(ec_slave_fmmu_t));
                    }
                }

                // get ram size
                osal_uint8_t ram_size = 0;
                ec_reg_read(EC_REG_RAM_SIZE, &ram_size, sizeof(ram_size));
                slv->ram_size = (osal_uint32_t)ram_size << 10u;

                // get pdi control 
                ec_reg_read(EC_REG_PDICTL, &slv->pdi_ctrl, sizeof(slv->pdi_ctrl));
                // get features
                ec_reg_read(EC_REG_ESCSUP, &slv->features, sizeof(slv->features));

                ec_log(10, get_transition_string(transition), 
                        "slave %2d: pdi ctrl 0x%04X, fmmus %d, syncm %d, features 0x%X\n", 
                        slave, slv->pdi_ctrl, slv->fmmu_ch, slv->sm_ch, slv->features);

                // init to preop stuff
                slv->eeprom.read_eeprom = 0;
                ec_eeprom_dump(pec, slave);

                // allocate sub device structures
                if (slv->eeprom.general.ds402_channels > 0u) {
                    slv->subdev_cnt = slv->eeprom.general.ds402_channels;
                } 
#if LIBETHERCAT_MBX_SUPPORT_SOE == 1
                else if (slv->eeprom.general.soe_channels > 0u) {
                    slv->subdev_cnt = slv->eeprom.general.soe_channels;
                } 
#endif
                else {
                    slv->subdev_cnt = 0;
                }

                if (slv->subdev_cnt != 0u) {
                    if (slv->subdev_cnt > LEC_MAX_DS402_SUBDEVS) {
                        ec_log(5, get_transition_string(transition), "slave %2d: got %" PRIu64 " ds402 sub devices but can "
                                "only handle %" PRIu64 "!\n", slave, slv->subdev_cnt, LEC_MAX_DS402_SUBDEVS);
                        slv->subdev_cnt = LEC_MAX_DS402_SUBDEVS;
                    }

                    for (osal_uint32_t q = 0u; q < slv->subdev_cnt; q++) {
                        slv->subdevs[q].pdin.pd = NULL;
                        slv->subdevs[q].pdout.pd = NULL;
                        slv->subdevs[q].pdin.len = 0;
                        slv->subdevs[q].pdout.len = 0;
                    }
                }

                ec_log(10, get_transition_string(transition), 
                        "slave %2d: vendor 0x%08X, product 0x%08X, mbx 0x%04X\n",
                        slave, slv->eeprom.vendor_id, slv->eeprom.product_code, 
                        slv->eeprom.mbx_supported);
            }
            // cppcheck-suppress misra-c2012-16.3
            case PREOP_2_PREOP:
            case SAFEOP_2_SAFEOP:
            case OP_2_OP:
                // write state to slave
                ret = ec_slave_set_state(pec, slave, state);
                break;
            default:
                ret = ec_slave_set_state(pec, slave, EC_STATE_INIT);
                ec_log(10, __func__, "unknown state transition for slave %2d -> %04X\n", slave, transition);
                break;
        };
    }
    
    osal_mutex_unlock(&slv->transition_mutex);

    return ret;
}

#if LIBETHERCAT_MBX_SUPPORT_EOE == 1

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
void ec_slave_set_eoe_settings(struct ec *pec, osal_uint16_t slave,
        osal_uint8_t *mac, osal_uint8_t *ip_address, osal_uint8_t *subnet, osal_uint8_t *gateway, 
        osal_uint8_t *dns, osal_char_t *dns_name) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    slv->eoe.use_eoe = 1;

#define EOE_SET(field, sz) {                      \
    if ((field) != NULL) {                            \
        (void)memcpy(&slv->eoe.field[0], (field), (sz));    \
    } else { (void)memset(&slv->eoe.field[0], 0, sz); } }

    // cppcheck-suppress misra-c2012-21.3
    EOE_SET(mac, LEC_EOE_MAC_LEN);
    EOE_SET(ip_address, LEC_EOE_IP_ADDRESS_LEN);
    EOE_SET(subnet, LEC_EOE_SUBNET_LEN);
    EOE_SET(gateway, LEC_EOE_GATEWAY_LEN);
    EOE_SET(dns, LEC_EOE_DNS_LEN);
    size_t tmp_len = LEC_EOE_DNS_NAME_LEN;
    if (dns_name != NULL) {
    	tmp_len = min(LEC_EOE_DNS_NAME_LEN, strlen(dns_name));
    }
    EOE_SET(dns_name, tmp_len);

// cppcheck-suppress misra-c2012-20.5
#undef EOE_SET

}

#endif

