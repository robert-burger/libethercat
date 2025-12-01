//! ethercat canopen over ethercat for master mailbox handling
/*!
 * author: Robert Burger
 *
 * $Id$
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

#ifdef HAVE_CONFIG_H
#include <libethercat/config.h>
#endif

#include "libethercat/coe.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"
#include "libethercat/mbx.h"

// cppcheck-suppress misra-c2012-21.6
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

struct ec_coe_object;

typedef int (*ec_coe_cb_read_t)(ec_t* pec, const struct ec_coe_object* coe_obj, osal_uint16_t index,
                                osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                osal_size_t* len, osal_uint32_t* abort_code);
typedef int (*ec_coe_cb_write_t)(ec_t* pec, struct ec_coe_object* coe_obj, osal_uint16_t index,
                                 osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                 osal_size_t len, osal_uint32_t* abort_code);

typedef struct ec_coe_object {
    osal_uint16_t index;
    osal_uint16_t index_mask;
    ec_coe_sdo_desc_t* obj_desc;
    ec_coe_sdo_entry_desc_t* entry_desc;
    osal_uint8_t* data;
    ec_coe_cb_read_t read;
    ec_coe_cb_write_t write;
} ec_coe_object_t;

#define HW_VERSION "0.0.0"

#define BUF_PUT(type, datap)                             \
    if ((*len) >= sizeof(type)) {                        \
        (void)memcpy(buf, (void*)(datap), sizeof(type)); \
        (*len) = sizeof(type);                           \
    }

/****************************************************************************
 * 0x1000   Device Type
 */

static ec_coe_sdo_desc_t obj_desc_master_0x1000 = {
    DEFTYPE_UNSIGNED32, OBJCODE_VAR, 0, {"Device Type"}, 11};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1000 = {0,           DEFTYPE_UNSIGNED32, 32,
                                                           ACCESS_READ, {"Device Type"},    11};
static osal_uint32_t data_master_0x1000 = 0;

/****************************************************************************
 * 0x1008   Device Name
 */

static ec_coe_sdo_desc_t obj_desc_master_0x1008 = {
    DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, {"Device Name"}, 11};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1008 = {
    0,           DEFTYPE_VISIBLESTRING, (sizeof(LIBETHERCAT_PACKAGE_NAME) - 1) << 3,
    ACCESS_READ, {"Device Name"},       11};
static osal_char_t data_master_0x1008[] = LIBETHERCAT_PACKAGE_NAME;

/****************************************************************************
 * 0x1009   Manufacturer Hardware Version
 */

static ec_coe_sdo_desc_t obj_desc_master_0x1009 = {
    DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, {"Manufacturer Hardware Version"}, 29};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1009 = {0,
                                                           DEFTYPE_VISIBLESTRING,
                                                           (sizeof(HW_VERSION) - 1) << 3,
                                                           ACCESS_READ,
                                                           {"Manufacturer Hardware Version"},
                                                           29};
static osal_char_t data_master_0x1009[] = HW_VERSION;

/****************************************************************************
 * 0x100A   Manufacturer Software Version
 */

static ec_coe_sdo_desc_t obj_desc_master_0x100A = {
    DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, {"Manufacturer Software Version"}, 29};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x100A = {0,
                                                           DEFTYPE_VISIBLESTRING,
                                                           (sizeof(LIBETHERCAT_PACKAGE_VERSION) - 1)
                                                               << 3,
                                                           ACCESS_READ,
                                                           {"Manufacturer Software Version"},
                                                           29};
static osal_char_t data_master_0x100A[] = LIBETHERCAT_PACKAGE_VERSION;

/****************************************************************************
 * 0x1018   Identity
 */

static ec_coe_sdo_desc_t obj_desc_master_0x1018 = {DEFTYPE_RECORD, OBJCODE_REC, 4, {"Identity"}, 8};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1018[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},         // 0
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Vendor ID"}, 9},         // 1
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Product Code"}, 12},     // 1
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Revision Number"}, 15},  // 1
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Serial Number"}, 13}};
static struct PACKED {
    osal_uint16_t subindex_0;
    osal_uint32_t vendor_id;
    osal_uint32_t product_code;
    osal_uint32_t revision_number;
    osal_uint32_t serial_number;
} data_master_0x1018 = {4, 0x1616, 0x11BECA7, 0x0, 0x0};

/****************************************************************************
 * 0x20nn   Configuration Cyclic Group
 */

static ec_coe_sdo_desc_t obj_desc_master_0x20nn = {
    DEFTYPE_RECORD, OBJCODE_REC, 13, {"Configuration Cyclic Group"}, 26};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x20nn[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},  // 0
    {0, DEFTYPE_DWORD, 32, ACCESS_READ, {"Logical Address"}, 15},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Logical Length"}, 14},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Output Length"}, 13},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Input Length"}, 12},
    {0, DEFTYPE_BOOLEAN, 8, ACCESS_READ, {"Overlapping"}, 11},
    {0, DEFTYPE_BOOLEAN, 8, ACCESS_READ, {"Use LRW"}, 7},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Expected Working Counter LRW"}, 28},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Expected Working Counter LRD"}, 28},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Expected Working Counter LWR"}, 28},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Receive Missed LRW"}, 18},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Receive Missed LRD"}, 18},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Receive Missed LWR"}, 18},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Timer Divisor"}, 13}};

static int callback_master_0x20nn(ec_t* pec, const ec_coe_object_t* coe_obj, osal_uint16_t index,
                                  osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                  osal_size_t* len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(coe_obj != NULL);

    // unused
    (void)coe_obj;
    (void)abort_code;
    (void)complete;

    // osal_uint8_t *tmp_buf = buf;
    // osal_size_t tmp_len = (*len);
    //(*len) = 0u; // returns read length
    int ret = EC_OK;

    osal_uint16_t group = (index & 0x00FEu) >> 1u;

    if (sub_index == 0u) {
        osal_uint8_t tmp_val = 11u;
        BUF_PUT(osal_uint8_t, &tmp_val);
    } else if (sub_index == 1u) {
        BUF_PUT(osal_uint32_t, &pec->pd_groups[group].log);
    } else if (sub_index == 2u) {
        BUF_PUT(osal_uint32_t, &pec->pd_groups[group].log_len);
    } else if (sub_index == 3u) {
        BUF_PUT(osal_uint32_t, &pec->pd_groups[group].pdout_len);
    } else if (sub_index == 4u) {
        BUF_PUT(osal_uint32_t, &pec->pd_groups[group].pdin_len);
    } else if (sub_index == 5u) {
        BUF_PUT(osal_uint8_t, &pec->pd_groups[group].overlapping);
    } else if (sub_index == 6u) {
        BUF_PUT(osal_uint8_t, &pec->pd_groups[group].use_lrw);
    } else if (sub_index == 7u) {
        BUF_PUT(osal_uint16_t, &pec->pd_groups[group].wkc_expected_lrw);
    } else if (sub_index == 8u) {
        BUF_PUT(osal_uint16_t, &pec->pd_groups[group].wkc_expected_lrd);
    } else if (sub_index == 9u) {
        BUF_PUT(osal_uint16_t, &pec->pd_groups[group].wkc_expected_lwr);
    } else if (sub_index == 10u) {
        BUF_PUT(osal_uint32_t, &pec->pd_groups[group].recv_missed_lrw);
    } else if (sub_index == 11u) {
        BUF_PUT(osal_uint32_t, &pec->pd_groups[group].recv_missed_lrd);
    } else if (sub_index == 12u) {
        BUF_PUT(osal_uint32_t, &pec->pd_groups[group].recv_missed_lwr);
    } else if (sub_index == 13u) {
        BUF_PUT(osal_uint32_t, &pec->pd_groups[group].divisor);
    } else {
        ret = EC_ERROR_MAILBOX_COE_SUBINDEX_NOT_FOUND;
    }

    return ret;
}

/****************************************************************************
 * 0x20nm   Assigned Slaves Cyclic Group
 */

static ec_coe_sdo_desc_t obj_desc_master_0x20nm = {
    DEFTYPE_ARRAY_OF_INT, OBJCODE_ARR, 255, {"Assigned Slaves Cyclic Group"}, 28};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x20nm[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Slave"}, 5},
};

static int callback_master_0x20nm(ec_t* pec, const ec_coe_object_t* coe_obj, osal_uint16_t index,
                                  osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                  osal_size_t* len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(coe_obj != NULL);

    // unused
    (void)coe_obj;
    (void)abort_code;
    (void)complete;

    int ret = EC_OK;

    osal_uint16_t group = (index & 0x00FEu) >> 1u;
    int value = -1;
    int need_value = sub_index - 1u;

    for (uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
        if (pec->slaves[slave].assigned_pd_group == (int)group) {
            value++;

            if (sub_index > 0u) {
                if (value == need_value) {
                    BUF_PUT(osal_uint16_t, &slave);
                }
            }
        }
    }

    if (sub_index == 0u) {
        if ((*len) >= 1) {
            (*(osal_uint8_t*)buf) = value + 1;
            (*len) = sizeof(osal_uint8_t);
        }
    }

    return ret;
}

/****************************************************************************
 * 0x30nn   Configuration Distributied Clock Slave
 */

static ec_coe_sdo_desc_t obj_desc_master_0x3nnn = {
    DEFTYPE_RECORD, OBJCODE_REC, 13, {"Configuration Distributed Clock Slave"}, 37};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x3nnn[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},  // 0
    {0, DEFTYPE_BOOLEAN, 8, ACCESS_READ, {"Enabled"}, 7},
    {0, DEFTYPE_INTEGER32, 32, ACCESS_READ, {"Next Slave"}, 10},
    {0, DEFTYPE_INTEGER32, 32, ACCESS_READ, {"Previous Slave"}, 14},
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Active Ports"}, 12},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Receive Time Port 0"}, 19},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Receive Time Port 1"}, 19},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Receive Time Port 2"}, 19},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Receive Time Port 3"}, 19},
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Sync Type"}, 9},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Cycle Time 0"}, 12},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Cycle Time 1"}, 12},
    {0, DEFTYPE_INTEGER32, 32, ACCESS_READ, {"Cycle Shift"}, 11},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"System Delay"}, 12}};

static int callback_master_0x3nnn(ec_t* pec, const ec_coe_object_t* coe_obj, osal_uint16_t index,
                                  osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                  osal_size_t* len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(coe_obj != NULL);

    // unused
    (void)coe_obj;
    (void)abort_code;
    (void)complete;

    // osal_uint8_t *tmp_buf = buf;

    int ret = EC_OK;

    osal_uint16_t slave = (index & 0x0FFFu);
    if (sub_index == 0u) {
        if ((*len) >= 1) {
            (*buf) = 13;
            (*len) = 1;
        }
    } else if (sub_index == 1u) {
        BUF_PUT(osal_uint8_t, &pec->slaves[slave].dc.use_dc);
    } else if (sub_index == 2u) {
        BUF_PUT(osal_int32_t, &pec->slaves[slave].dc.next);
    } else if (sub_index == 3u) {
        BUF_PUT(osal_int32_t, &pec->slaves[slave].dc.prev);
    } else if (sub_index == 4u) {
        BUF_PUT(osal_uint8_t, &pec->slaves[slave].active_ports);
    } else if (sub_index == 5u) {
        BUF_PUT(osal_uint32_t, &pec->slaves[slave].dc.receive_times[0]);
    } else if (sub_index == 6u) {
        BUF_PUT(osal_uint32_t, &pec->slaves[slave].dc.receive_times[1]);
    } else if (sub_index == 7u) {
        BUF_PUT(osal_uint32_t, &pec->slaves[slave].dc.receive_times[2]);
    } else if (sub_index == 8u) {
        BUF_PUT(osal_uint32_t, &pec->slaves[slave].dc.receive_times[3]);
    } else if (sub_index == 9u) {
        BUF_PUT(osal_uint8_t, &pec->slaves[slave].dc.activation_reg);
    } else if (sub_index == 10u) {
        BUF_PUT(osal_uint32_t, &pec->slaves[slave].dc.cycle_time_0);
    } else if (sub_index == 11u) {
        BUF_PUT(osal_uint32_t, &pec->slaves[slave].dc.cycle_time_1);
    } else if (sub_index == 12u) {
        BUF_PUT(osal_int32_t, &pec->slaves[slave].dc.cycle_shift);
    } else if (sub_index == 13u) {
        BUF_PUT(osal_uint32_t, &pec->slaves[slave].pdelay);
    } else {
        ret = EC_ERROR_MAILBOX_COE_SUBINDEX_NOT_FOUND;
    }

    return ret;
}

/****************************************************************************
 * 0x8nnn   Configuration Data Slave
 */

static ec_coe_sdo_desc_t obj_desc_master_0x8nnn = {
    DEFTYPE_RECORD, OBJCODE_REC, 35, {"Configuration Data Slave"}, 24};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x8nnn[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},               // 0
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Fixed Station Address"}, 21},  // 1
    {0, DEFTYPE_VISIBLESTRING, 8, ACCESS_READ, {"Type"}, 4},                  // 2
    {0, DEFTYPE_VISIBLESTRING, 8, ACCESS_READ, {"Name"}, 4},                  // 3
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Device Type"}, 11},            // 4
    {0, DEFTYPE_DWORD, 32, ACCESS_READ, {"Vendor Id"}, 9},                    // 5
    {0, DEFTYPE_DWORD, 32, ACCESS_READ, {"Product Code"}, 12},                // 6
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Revision Number"}, 15},        // 7
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Serial Number"}, 13},          // 8
    {0, 0, 0, 0, {""}, 0},                                                    // 9
    {0, 0, 0, 0, {""}, 0},                                                    // 10
    {0, 0, 0, 0, {""}, 0},                                                    // 11
    {0, 0, 0, 0, {""}, 0},                                                    // 12
    {0, 0, 0, 0, {""}, 0},                                                    // 13
    {0, 0, 0, 0, {""}, 0},                                                    // 14
    {0, 0, 0, 0, {""}, 0},                                                    // 15
    {0, 0, 0, 0, {""}, 0},                                                    // 16
    {0, 0, 0, 0, {""}, 0},                                                    // 17
    {0, 0, 0, 0, {""}, 0},                                                    // 18
    {0, 0, 0, 0, {""}, 0},                                                    // 19
    {0, 0, 0, 0, {""}, 0},                                                    // 20
    {0, 0, 0, 0, {""}, 0},                                                    // 21
    {0, 0, 0, 0, {""}, 0},                                                    // 22
    {0, 0, 0, 0, {""}, 0},                                                    // 23
    {0, 0, 0, 0, {""}, 0},                                                    // 24
    {0, 0, 0, 0, {""}, 0},                                                    // 25
    {0, 0, 0, 0, {""}, 0},                                                    // 26
    {0, 0, 0, 0, {""}, 0},                                                    // 27
    {0, 0, 0, 0, {""}, 0},                                                    // 28
    {0, 0, 0, 0, {""}, 0},                                                    // 29
    {0, 0, 0, 0, {""}, 0},                                                    // 30
    {0, 0, 0, 0, {""}, 0},                                                    // 31
    {0, 0, 0, 0, {""}, 0},                                                    // 32
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Mailbox Out Size"}, 16},       // 33
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Mailbox In Size"}, 15},        // 34
    {0, DEFTYPE_BYTE, 8, ACCESS_READ, {"Link Status"}, 11},                   // 35
};

static int callback_master_0x8nnn(ec_t* pec, const ec_coe_object_t* coe_obj, osal_uint16_t index,
                                  osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                  osal_size_t* len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(coe_obj != NULL);

    (void)coe_obj;

    int ret = EC_OK;

    osal_uint16_t slave = index & 0x0FFFu;
    if (sub_index == 0u) {
        if ((*len) >= 1) {
            (*buf) = 35;
            (*len) = 1;
        }
    } else if (sub_index == 1u) {
        if ((*len) >= 2) {
            (*(osal_uint16_t*)buf) = pec->slaves[slave].fixed_address;
            (*len) = sizeof(osal_uint16_t);
        }
    } else if (sub_index == 2u) {
        if (pec->slaves[slave].eeprom.mbx_supported != 0u) {
            ret = ec_coe_sdo_read(pec, slave, 0x100A, 0, complete, buf, len, abort_code);
        }
    } else if (sub_index == 3u) {
        if (pec->slaves[slave].eeprom.mbx_supported != 0u) {
            ret = ec_coe_sdo_read(pec, slave, 0x1008, 0, complete, buf, len, abort_code);
        }
    } else if (sub_index == 4u) {
        if (pec->slaves[slave].eeprom.mbx_supported != 0u) {
            ret = ec_coe_sdo_read(pec, slave, 0x1000, 0, complete, buf, len, abort_code);
        }
    } else if ((sub_index == 5u) || (sub_index == 6u) || (sub_index == 7u) || (sub_index == 8u)) {
        ret = ec_coe_sdo_read(pec, slave, 0x1018, sub_index - 4u, complete, buf, len, abort_code);
    } else if ((sub_index == 33u) || (sub_index == 34u)) {
        if ((*len) >= 2) {
            if (pec->slaves[slave].eeprom.mbx_supported != 0u) {
                (void)memcpy(buf, &pec->slaves[slave].sm[sub_index - 33u].len,
                             sizeof(osal_uint16_t));
                (*len) = sizeof(osal_uint16_t);
            } else {
                osal_uint16_t tmp_value = 0u;
                (void)memcpy(buf, (void*)&tmp_value, sizeof(osal_uint16_t));
                (*len) = sizeof(osal_uint16_t);
            }
        }
    } else {
        ret = EC_ERROR_MAILBOX_COE_SUBINDEX_NOT_FOUND;
    }

    return ret;
}

/****************************************************************************
 * 0x9nnn   Information Data Slave
 */

static ec_coe_sdo_desc_t obj_desc_master_0x9nnn = {
    DEFTYPE_RECORD, OBJCODE_REC, 32, {"Information Data Slave"}, 22};
static ec_coe_sdo_entry_desc_t entry_desc_master_0x9nnn[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},               // 0
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Fixed Station Address"}, 21},  // 1
    {0, 0, 0, 0, {""}, 0},                                                    // 2
    {0, 0, 0, 0, {""}, 0},                                                    // 3
    {0, 0, 0, 0, {""}, 0},                                                    // 4
    {0, DEFTYPE_DWORD, 32, ACCESS_READ, {"Vendor Id"}, 9},                    // 5
    {0, DEFTYPE_DWORD, 32, ACCESS_READ, {"Product Code"}, 12},                // 6
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Revision Number"}, 15},        // 7
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"Serial Number"}, 13},          // 8
    {0, 0, 0, 0, {""}, 0},                                                    // 9
    {0, 0, 0, 0, {""}, 0},                                                    // 10
    {0, 0, 0, 0, {""}, 0},                                                    // 11
    {0, 0, 0, 0, {""}, 0},                                                    // 12
    {0, 0, 0, 0, {""}, 0},                                                    // 13
    {0, 0, 0, 0, {""}, 0},                                                    // 14
    {0, 0, 0, 0, {""}, 0},                                                    // 15
    {0, 0, 0, 0, {""}, 0},                                                    // 16
    {0, 0, 0, 0, {""}, 0},                                                    // 17
    {0, 0, 0, 0, {""}, 0},                                                    // 18
    {0, 0, 0, 0, {""}, 0},                                                    // 19
    {0, 0, 0, 0, {""}, 0},                                                    // 20
    {0, 0, 0, 0, {""}, 0},                                                    // 21
    {0, 0, 0, 0, {""}, 0},                                                    // 22
    {0, 0, 0, 0, {""}, 0},                                                    // 23
    {0, 0, 0, 0, {""}, 0},                                                    // 24
    {0, 0, 0, 0, {""}, 0},                                                    // 25
    {0, 0, 0, 0, {""}, 0},                                                    // 26
    {0, 0, 0, 0, {""}, 0},                                                    // 27
    {0, 0, 0, 0, {""}, 0},                                                    // 28
    {0, 0, 0, 0, {""}, 0},                                                    // 29
    {0, 0, 0, 0, {""}, 0},                                                    // 30
    {0, 0, 0, 0, {""}, 0},                                                    // 31
    {0, DEFTYPE_WORD, 16, ACCESS_READ, {"DL Status Register"}, 18},           // 32
};

static int callback_master_0x9nnn(ec_t* pec, const ec_coe_object_t* coe_obj, osal_uint16_t index,
                                  osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                  osal_size_t* len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(coe_obj != NULL);

    (void)coe_obj;

    int ret = EC_OK;

    osal_uint16_t slave = index & 0x0FFFu;
    if (sub_index == 0u) {
        if ((*len) >= 1) {
            (*buf) = 32;
            (*len) = 1;
        }
    } else if (sub_index == 1u) {
        BUF_PUT(osal_uint16_t, &pec->slaves[slave].fixed_address);
    } else if ((sub_index == 5u) || (sub_index == 6u) || (sub_index == 7u) || (sub_index == 8u)) {
        ret = ec_coe_sdo_read(pec, slave, 0x1018, sub_index - 4u, complete, buf, len, abort_code);
    } else if (sub_index == 32u) {
        if ((*len) >= 2) {
            osal_uint16_t wkc = 0;
            (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x110, buf, sizeof(osal_uint16_t),
                          &wkc);

            if (wkc != 0u) {
                (*len) = sizeof(osal_uint16_t);
            }
        }
    } else {
        ret = EC_ERROR_MAILBOX_ABORT;
    }
    return ret;
}

/****************************************************************************
 * 0xAnnn   Diagnosis Data Slave
 */

static ec_coe_sdo_desc_t obj_desc_master_0xAnnn = {
    DEFTYPE_RECORD, OBJCODE_REC, 2, {"Diagnosis Data Slave"}, 20};
static ec_coe_sdo_entry_desc_t entry_desc_master_0xAnnn[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"AL Status"}, 9},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"AL Control"}, 10},
};

static int callback_master_0xAnnn(ec_t* pec, const ec_coe_object_t* coe_obj, osal_uint16_t index,
                                  osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                  osal_size_t* len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(coe_obj != NULL);

    // unused
    (void)coe_obj;
    (void)abort_code;
    (void)complete;

    int ret = EC_OK;

    osal_uint16_t slave = index & 0x0FFFu;
    if (sub_index == 0u) {
        if ((*len) >= 1) {
            (*buf) = 2;
            (*len) = 1;
        }
    } else if (sub_index == 1u) {
        if ((*len) >= 2) {
            osal_uint16_t wkc = 0u;
            (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x130, buf, sizeof(osal_uint16_t),
                          &wkc);

            if (wkc != 0u) {
                (*len) = sizeof(osal_uint16_t);
            }
        }
    } else if (sub_index == 2u) {
        if ((*len) >= 2) {
            osal_uint16_t wkc = 0u;
            (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x120, buf, sizeof(osal_uint16_t),
                          &wkc);

            if (wkc != 0u) {
                (*len) = sizeof(osal_uint16_t);
            }
        }
    } else {
        ret = EC_ERROR_MAILBOX_ABORT;
    }

    return ret;
}

/****************************************************************************
 * 0xF000   Modular Device Profile
 */

static ec_coe_sdo_desc_t obj_desc_master_0xF000 = {
    DEFTYPE_RECORD, OBJCODE_REC, 4, {"Modular Device Profile"}, 22};
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF000[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Module Index Distance"}, 21},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Maximum Number of Modules"}, 25},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"General Configuration"}, 21},
    {0, DEFTYPE_UNSIGNED32, 32, ACCESS_READ, {"General Information"}, 19},
};
static struct PACKED {
    osal_uint16_t subindex_0;
    osal_uint16_t module_index_distance;
    osal_uint16_t maximum_number_of_modules;
    osal_uint32_t general_configuration;
    osal_uint32_t general_information;
} data_master_0xF000 = {4, 0x0001, 4080, 0x000000FF, 0x000000F1};

/****************************************************************************
 * 0xF002   Detect Modules Command
 */

static ec_coe_sdo_desc_t obj_desc_master_0xF002 = {
    DEFTYPE_RECORD, OBJCODE_REC, 3, {"Detect Modules Command"}, 22};
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF002[] = {
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Subindex 0"}, 10},
    {0, DEFTYPE_OCTETSTRING, 16, ACCESS_READWRITE, {"Scan Command Request"}, 20},
    {0, DEFTYPE_UNSIGNED8, 8, ACCESS_READ, {"Scan Command Status"}, 19},
    {0, DEFTYPE_OCTETSTRING, 48, ACCESS_READ, {"Scan Command Response"}, 21}};
static struct PACKED {
    osal_uint16_t subindex_0;
    osal_uint8_t scan_command_request[2];
    osal_uint8_t scan_command_status;
    osal_uint8_t scan_command_response[6];
} data_master_0xF002 = {3, {0}, 0, {0}};

/****************************************************************************
 * 0xF02n   Configured Address List Slaves
 */

static ec_coe_sdo_desc_t obj_desc_master_0xF02n = {
    DEFTYPE_ARRAY_OF_INT, OBJCODE_ARR, 255, {"Configured Address List Slaves"}, 30};
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF02n[] = {
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Subindex 0"}, 10},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Slave"}, 5},
};

/****************************************************************************
 * 0xF04n   Detected Address List Slaves
 */

static ec_coe_sdo_desc_t obj_desc_master_0xF04n = {
    DEFTYPE_ARRAY_OF_INT, OBJCODE_ARR, 255, {"Detected Address List Slaves"}, 28};
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF04n[] = {
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Subindex 0"}, 10},
    {0, DEFTYPE_UNSIGNED16, 16, ACCESS_READ, {"Slave"}, 5},
};

static int callback_master_0xF0nn(ec_t* pec, const ec_coe_object_t* coe_obj, osal_uint16_t index,
                                  osal_uint8_t sub_index, int complete, osal_uint8_t* buf,
                                  osal_size_t* len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(coe_obj != NULL);

    // unused
    (void)coe_obj;
    (void)abort_code;
    (void)complete;

    int ret = EC_OK;

    osal_uint16_t slave_range = index & 0x000Fu;
    osal_uint16_t slave = (slave_range * 255u) + (sub_index - 1u);

    if ((*len) >= 2) {
        if (slave < pec->slave_cnt) {
            (void)memcpy(buf, &pec->slaves[slave].fixed_address, sizeof(osal_uint16_t));
        } else {
            osal_uint16_t value = 0u;
            (void)memcpy(buf, &value, sizeof(osal_uint16_t));
        }

        (*len) = sizeof(osal_uint16_t);
    }

    return ret;
}

#define EC_COE_OBJECT_INDEX_MASK_ALL ((osal_uint16_t)0xFFFFu)

ec_coe_object_t ec_coe_master_dict[] = {
    {0x1000, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x1000, &entry_desc_master_0x1000,
     (osal_uint8_t*)&data_master_0x1000, NULL, NULL},
    {0x1008, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x1008, &entry_desc_master_0x1008,
     (osal_uint8_t*)data_master_0x1008, NULL, NULL},
    {0x1009, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x1009, &entry_desc_master_0x1009,
     (osal_uint8_t*)data_master_0x1009, NULL, NULL},
    {0x100A, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x100A, &entry_desc_master_0x100A,
     (osal_uint8_t*)data_master_0x100A, NULL, NULL},
    {0x1018, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x1018, &entry_desc_master_0x1018[0],
     (osal_uint8_t*)&data_master_0x1018, NULL, NULL},
    {0x2000, 0xFF01u, &obj_desc_master_0x20nn, &entry_desc_master_0x20nn[0], NULL,
     callback_master_0x20nn, NULL},
    {0x2001, 0xFF01u, &obj_desc_master_0x20nm, &entry_desc_master_0x20nm[0], NULL,
     callback_master_0x20nm, NULL},
    {0x3000, 0xF000u, &obj_desc_master_0x3nnn, &entry_desc_master_0x3nnn[0], NULL,
     callback_master_0x3nnn, NULL},
    {0x8000, 0xF000u, &obj_desc_master_0x8nnn, &entry_desc_master_0x8nnn[0], NULL,
     callback_master_0x8nnn, NULL},
    {0x9000, 0xF000u, &obj_desc_master_0x9nnn, &entry_desc_master_0x9nnn[0], NULL,
     callback_master_0x9nnn, NULL},
    {0xA000, 0xF000u, &obj_desc_master_0xAnnn, &entry_desc_master_0xAnnn[0], NULL,
     callback_master_0xAnnn, NULL},
    {0xF000, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0xF000, &entry_desc_master_0xF000[0],
     (osal_uint8_t*)&data_master_0xF000, NULL, NULL},
    {0xF002, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0xF002, &entry_desc_master_0xF002[0],
     (osal_uint8_t*)&data_master_0xF002, NULL, NULL},
    {0xF020, 0xFFF0u, &obj_desc_master_0xF02n, &entry_desc_master_0xF02n[0], NULL,
     callback_master_0xF0nn, NULL},
    {0xF040, 0xFFF0u, &obj_desc_master_0xF04n, &entry_desc_master_0xF04n[0], NULL,
     callback_master_0xF0nn, NULL},
    {0xFFFF, EC_COE_OBJECT_INDEX_MASK_ALL, NULL, NULL, NULL, NULL, NULL}};

//! Calculate complete object length either with or without subindex 0.
/*!
 * \param[in]   coe_obj             Pointer to object to calculate length from
 * \param[in]   with_sub_index_0    Include subindex 0 in calculated length.
 * \param[out]  obj_len             Return calculated object length.
 * \return EC_OK on success.
 */
static int ec_coe_master_get_object_length(ec_coe_object_t* coe_obj, osal_bool_t with_sub_index_0,
                                           osal_size_t* obj_len) {
    assert(coe_obj != NULL);
    assert(obj_len != NULL);

    int ret = EC_OK;
    osal_size_t tmp = 0u;

    osal_uint16_t i = 1u;
    if (with_sub_index_0 == OSAL_TRUE) {
        tmp += 2u;  // subindex 0 always is stored as 2-byte-unsigned
    }

    while (i <= coe_obj->obj_desc->max_subindices) {
        tmp += coe_obj->entry_desc[i].bit_length >> 3;

        i++;
    }

    (*obj_len) = tmp;

    return ret;
}

static int ec_coe_master_get_object(osal_uint16_t index, ec_coe_object_t** coe_obj) {
    assert(coe_obj != NULL);

    int ret = EC_ERROR_MAILBOX_COE_INDEX_NOT_FOUND;
    ec_coe_object_t* tmp_coe_obj = &ec_coe_master_dict[0];

    while (tmp_coe_obj->index != 0xFFFFu) {
        if (tmp_coe_obj->index == (index & tmp_coe_obj->index_mask)) {
            *coe_obj = tmp_coe_obj;
            ret = EC_OK;
            break;
        }

        tmp_coe_obj++;
    }

    if (ret != EC_OK) {
        (*coe_obj) = NULL;
    }

    return ret;
}

static int ec_coe_master_get_object_data(ec_coe_object_t* coe_obj, osal_uint8_t sub_index,
                                         int complete, osal_uint8_t** data, osal_size_t* data_len) {
    assert(coe_obj != NULL);
    assert(data != NULL);
    assert(data_len != NULL);

    int ret = EC_ERROR_MAILBOX_ABORT;

    if (complete == 0) {
        if (sub_index <= coe_obj->obj_desc->max_subindices) {
            if (coe_obj->data != NULL) {
                osal_off_t pos = 0u;

                for (osal_uint16_t i = 0; i <= sub_index; ++i) {
                    if (i == sub_index) {
                        (*data) = &coe_obj->data[pos];
                        (*data_len) = coe_obj->entry_desc[i].bit_length >> 3;
                        ret = EC_OK;
                        break;
                    }

                    pos += coe_obj->entry_desc[i].bit_length >> 3;
                }
            }
        }
    } else {  // complete != 0
        osal_size_t complete_len = 0u;
        osal_bool_t with_sub_index_0 = (sub_index == 0u) ? OSAL_TRUE : OSAL_FALSE;
        ret = ec_coe_master_get_object_length(coe_obj, with_sub_index_0, &complete_len);

        if (ret == EC_OK) {
            osal_off_t pos = 0u;

            if (sub_index != 0u) {
                pos += 2;  // skip subindex 0
            }

            (*data) = &coe_obj->data[pos];
            (*data_len) = complete_len;
        }
    }

    return ret;
}

// Read CoE service data object (SDO) of master
int ec_coe_master_sdo_read(ec_t* pec, osal_uint16_t index, osal_uint8_t sub_index, int complete,
                           osal_uint8_t* buf, osal_size_t* len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);

    int ret = EC_OK;

    ec_coe_object_t* coe_obj = NULL;
    ret = ec_coe_master_get_object(index, &coe_obj);

    if (ret == EC_OK) {
        if ((coe_obj != NULL) && (coe_obj->read != NULL)) {
            ret = (*coe_obj->read)(pec, coe_obj, index, sub_index, complete, buf, len, abort_code);
        } else if ((coe_obj != NULL) && (coe_obj->data != NULL)) {
            osal_uint8_t* data = NULL;
            osal_size_t data_len = 0;
            ret = ec_coe_master_get_object_data(coe_obj, sub_index, complete, &data, &data_len);

            if ((ret == EC_OK) && (data != NULL)) {
                if ((*len) >= data_len) {
                    (void)memcpy(buf, data, data_len);
                }

                (*len) = data_len;
            }
        } else {
            ret = EC_ERROR_MAILBOX_ABORT;
        }
    }

    return ret;
}

// Write CoE service data object (SDO) of master
int ec_coe_master_sdo_write(ec_t* pec, osal_uint16_t index, osal_uint8_t sub_index, int complete,
                            osal_uint8_t* buf, osal_size_t len, osal_uint32_t* abort_code) {
    assert(pec != NULL);
    assert(buf != NULL);

    int ret = EC_OK;

    ec_coe_object_t* coe_obj = NULL;
    ret = ec_coe_master_get_object(index, &coe_obj);

    if (ret == EC_OK) {
        if ((coe_obj != NULL) && (coe_obj->write != NULL)) {
            ret = (*coe_obj->write)(pec, coe_obj, index, sub_index, complete, buf, len, abort_code);
        } else if ((coe_obj != NULL) && (coe_obj->data != NULL)) {
            osal_uint8_t* data = NULL;
            osal_size_t data_len = 0;
            ret = ec_coe_master_get_object_data(coe_obj, sub_index, complete, &data, &data_len);

            if ((ret == EC_OK) && (data != NULL) &&
                (sub_index <= coe_obj->obj_desc->max_subindices)) {
                ec_coe_sdo_entry_desc_t* entry_desc = &coe_obj->entry_desc[sub_index];

                if ((entry_desc->obj_access & ACCESS_WRITE) != 0u) {
                    if (len <= data_len) {
                        (void)memcpy(data, buf, len);
                    }
                }
            }
        } else {
            ret = EC_ERROR_MAILBOX_ABORT;
        }
    }

    return ret;
}

// read coe object dictionary list of master
int ec_coe_master_odlist_read(ec_t* pec, osal_uint8_t* buf, osal_size_t* len) {
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);
    int ret = EC_OK;

    osal_off_t pos = 0u;

#define BUF_ASSIGN(data)                                           \
    {                                                              \
        if (pos < *len) {                                          \
            osal_uint16_t value = (data);                          \
            (void)memcpy(&buf[pos], (void*)&value, sizeof(value)); \
        }                                                          \
        pos += sizeof(osal_uint16_t);                              \
    }

    BUF_ASSIGN(0x1000u);
    BUF_ASSIGN(0x1008u);
    BUF_ASSIGN(0x1009u);
    BUF_ASSIGN(0x100Au);
    BUF_ASSIGN(0x1018u);

    for (osal_uint16_t i = 0; (i < pec->pd_group_cnt); ++i) {
        BUF_ASSIGN(0x2000u | ((osal_uint16_t)i << 1u));
        BUF_ASSIGN(0x2001u | ((osal_uint16_t)i << 1u));
    }

    for (osal_uint16_t i = 0; (i < pec->slave_cnt); ++i) {
        BUF_ASSIGN(0x3000u | (osal_uint16_t)i);
        BUF_ASSIGN(0x8000u | (osal_uint16_t)i);
        BUF_ASSIGN(0x9000u | (osal_uint16_t)i);
        BUF_ASSIGN(0xA000u | (osal_uint16_t)i);
    }

    BUF_ASSIGN(0xF000u);
    BUF_ASSIGN(0xF002u);

    for (osal_uint16_t i = 0u; (i < ((pec->slave_cnt / 255u) + 1u)); ++i) {
        BUF_ASSIGN(0xF020u | (osal_uint16_t)i);
        BUF_ASSIGN(0xF040u | (osal_uint16_t)i);
    }

    if (pos > (*len)) {
        ret = EC_ERROR_MAILBOX_BUFFER_TOO_SMALL;
    }

    (*len) = pos;

    return ret;
}

// Read CoE SDO description of master
int ec_coe_master_sdo_desc_read(const ec_t* pec, osal_uint16_t index, ec_coe_sdo_desc_t* desc,
                                osal_uint32_t* error_code) {
    assert(pec != NULL);
    assert(desc != NULL);
    (void)error_code;

    int ret = EC_OK;

    ec_coe_object_t* coe_obj = NULL;
    ret = ec_coe_master_get_object(index, &coe_obj);

    if (coe_obj != NULL) {
        (void)memcpy(desc, coe_obj->obj_desc, sizeof(ec_coe_sdo_desc_t));

        if (index >= 0xF000u) {
            if ((index != 0xF000u) && (index != 0xF002u)) {
                osal_uint16_t slave_range = index & 0x000Fu;
                osal_uint16_t slave_begin = slave_range * 256u;
                osal_uint16_t slave_end = slave_begin + 255u;

                (void)snprintf(&desc->name[strlen(coe_obj->obj_desc->name)],
                               CANOPEN_MAXNAME - strlen(coe_obj->obj_desc->name), " %hu-%hu",
                               slave_begin, slave_end);
                desc->name_len = strlen(desc->name);

                if (slave_begin < pec->slave_cnt) {
                    desc->max_subindices = pec->slave_cnt - slave_begin;
                } else {
                    desc->max_subindices = 0;
                }
            }
        } else if ((index >= 0x8000u) || ((index & coe_obj->index_mask) == 0x3000u)) {
            osal_uint16_t slave = index & ~coe_obj->index_mask;
            (void)snprintf(&desc->name[strlen(coe_obj->obj_desc->name)],
                           CANOPEN_MAXNAME - strlen(coe_obj->obj_desc->name), " %hu", slave);
            desc->name_len = strlen(desc->name);
        } else if ((index & 0xFF01u) == 0x2000u) {
            osal_uint16_t group = (index & 0x0FEu) >> 1u;
            (void)snprintf(&desc->name[strlen(coe_obj->obj_desc->name)],
                           CANOPEN_MAXNAME - strlen(coe_obj->obj_desc->name), " %hu", group);
            desc->name_len = strlen(desc->name);
        } else if ((index & 0xFF01u) == 0x2001u) {
            osal_uint16_t group = (index & 0x00FEu) >> 1;
            (void)snprintf(&desc->name[strlen(coe_obj->obj_desc->name)],
                           CANOPEN_MAXNAME - strlen(coe_obj->obj_desc->name), " %hu", group);
            desc->name_len = strlen(desc->name);
            desc->max_subindices = 0;

            for (uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
                if (pec->slaves[slave].assigned_pd_group == (int)group) {
                    desc->max_subindices++;
                }
            }
        } else {
        }
    }

    return ret;
}

// Read CoE SDO entry description of master
int ec_coe_master_sdo_entry_desc_read(const ec_t* pec, osal_uint16_t index, osal_uint8_t sub_index,
                                      osal_uint8_t value_info, ec_coe_sdo_entry_desc_t* desc,
                                      osal_uint32_t* error_code) {
    assert(pec != NULL);
    assert(desc != NULL);

    (void)pec;
    (void)value_info;
    (void)error_code;

    int ret = EC_OK;

    ec_coe_object_t* coe_obj = NULL;
    ret = ec_coe_master_get_object(index, &coe_obj);

    if (coe_obj != NULL) {
        if (sub_index <= coe_obj->obj_desc->max_subindices) {
            if ((sub_index != 0) && (coe_obj->obj_desc->obj_code == OBJCODE_ARR)) {
                ec_coe_sdo_entry_desc_t* entry_desc = &coe_obj->entry_desc[1];
                (void)memcpy(desc, entry_desc, sizeof(ec_coe_sdo_entry_desc_t));

                if (((index & 0xFFF0u) == 0xF020u) || ((index & 0xFFF0u) == 0xF040u)) {
                    osal_uint16_t slave_range = index & 0x000Fu;
                    osal_uint16_t slave = (slave_range * 255) + (sub_index - 1);

                    (void)snprintf((char*)&desc->data[strlen((char*)(entry_desc->data))],
                                   CANOPEN_MAXNAME - strlen((char*)(entry_desc->data)), " %hu",
                                   slave);
                    desc->data_len = strlen((char*)&desc->data[0]);
                }
            } else {
                (void)memcpy(desc, &coe_obj->entry_desc[sub_index],
                             sizeof(ec_coe_sdo_entry_desc_t));
            }
        }
    }

    return ret;
}
