//! ethercat canopen over ethercat for master mailbox handling
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

#include "libethercat/ec.h"
#include "libethercat/mbx.h"
#include "libethercat/coe.h"
#include "libethercat/error_codes.h"

// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

struct ec_coe_object;

typedef int (*ec_coe_cb_read_t)(ec_t *pec, struct ec_coe_object *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, 
        int complete, osal_uint8_t *buf, osal_size_t *len, osal_uint32_t *abort_code);
typedef int (*ec_coe_cb_write_t)(ec_t *pec, struct ec_coe_object *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, 
        int complete, osal_uint8_t *buf, osal_size_t len, osal_uint32_t *abort_code);

typedef struct ec_coe_object {
    osal_uint16_t               index;
    osal_uint16_t               index_mask;
    ec_coe_sdo_desc_t *         obj_desc;
    ec_coe_sdo_entry_desc_t *   entry_desc;
    osal_uint8_t *              data;
    ec_coe_cb_read_t            read;
    ec_coe_cb_write_t           write;
} ec_coe_object_t;

#define HW_VERSION      "0.0.0"

/****************************************************************************
 * 0x1000   Device Type
 */

static ec_coe_sdo_desc_t obj_desc_master_0x1000 = { DEFTYPE_UNSIGNED32, OBJCODE_VAR, 0, { "Device Type" }, 11 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1000 =
    { 0, DEFTYPE_UNSIGNED32,    32, ACCESS_READ, { "Device Type" },       11 };
static osal_uint32_t data_master_0x1000 = 0;

/****************************************************************************
 * 0x1008   Device Name
 */

static ec_coe_sdo_desc_t obj_desc_master_0x1008 = { DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, { "Device Name" }, 11 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1008 =
    { 0, DEFTYPE_VISIBLESTRING, strlen(LIBETHERCAT_PACKAGE_NAME) << 3, ACCESS_READ, { "Device Name" },       11 };
static osal_char_t data_master_0x1008[] = LIBETHERCAT_PACKAGE_NAME;

/****************************************************************************
 * 0x1009   Manufacturer Hardware Version
 */

static ec_coe_sdo_desc_t obj_desc_master_0x1009 = { DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, { "Manufacturer Hardware Version" }, 29 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1009 =
    { 0, DEFTYPE_VISIBLESTRING , strlen(HW_VERSION) << 3, ACCESS_READ, { "Manufacturer Hardware Version" },       29 };
static osal_char_t data_master_0x1009[] = HW_VERSION;

/****************************************************************************
 * 0x100A   Manufacturer Software Version
 */

static ec_coe_sdo_desc_t obj_desc_master_0x100A = { DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, { "Manufacturer Software Version" }, 29 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x100A =
    { 0, DEFTYPE_VISIBLESTRING , strlen(LIBETHERCAT_PACKAGE_VERSION) << 3, ACCESS_READ, { "Manufacturer Software Version" },       29 };
static osal_char_t data_master_0x100A[] = LIBETHERCAT_PACKAGE_VERSION;

/****************************************************************************
 * 0x1018   Identity
 */

static ec_coe_sdo_desc_t obj_desc_master_0x1018 = { DEFTYPE_RECORD, OBJCODE_REC, 4, { "Identity" }, 8 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1018[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, { "Subindex 0" },       10 }, // 0
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Vendor ID" },         9 }, // 1
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Product Code" },     12 }, // 1
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Revision Number" },  15 }, // 1
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Serial Number" },    13 } };
static struct PACKED {
    osal_uint8_t subindex_0;
    osal_uint32_t vendor_id;
    osal_uint32_t product_code;
    osal_uint32_t revision_number;
    osal_uint32_t serial_number;
} data_master_0x1018 = { 4, 0x1616, 0x11BECA7, 0x0, 0x0 };

/****************************************************************************
 * 0x20nn   Configuration Cyclic Group
 */

static ec_coe_sdo_desc_t obj_desc_master_0x20nn = { DEFTYPE_RECORD, OBJCODE_REC, 8, { "Configuration Cyclic Group" }, 26 };
static ec_coe_sdo_entry_desc_t entry_desc_master_0x20nn[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, { "Subindex 0" }              , 10 }, // 0
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, { "Logical Address" }         , 15 },
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Logical Length" }          , 14 },
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Output Length" }           , 13 },
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Input Length" }            , 12 },
    { 0, DEFTYPE_UNSIGNED8,     8, ACCESS_READ, { "Overlapping" }             , 11 },
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, { "Expected Working Counter" }, 24 }, 
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Receive Missed" }          , 14 }, 
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Timer Divisor" }           , 13 } };

static int callback_master_0x20nn(ec_t *pec, ec_coe_object_t *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, 
        int complete, osal_uint8_t *buf, osal_size_t *len, osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(coe_obj != NULL);
    
    int ret = EC_OK;

    osal_uint16_t group = (index & 0x00FE) >> 1u;
    if (sub_index == 0) {
        if ((*len) >= 1) {
            (*buf) = 8;
            (*len) = 1;
        }
    } else if (sub_index == 1) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->pd_groups[group].log;
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 2) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->pd_groups[group].log_len;
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 3) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->pd_groups[group].pdout_len;
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 4) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->pd_groups[group].pdin_len;
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 5) {
        if ((*len) >= 1) {
            (*(osal_uint8_t *)buf) = pec->pd_groups[group].use_lrw;
            (*len) = sizeof(osal_uint8_t);
        }
    } else if (sub_index == 6) {
        if ((*len) >= 2) {
            (*(osal_uint16_t *)buf) = pec->pd_groups[group].wkc_expected;
            (*len) = sizeof(osal_uint16_t);
        }
    } else if (sub_index == 7) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->pd_groups[group].recv_missed;
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 8) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->pd_groups[group].divisor;
            (*len) = sizeof(osal_uint32_t);
        }
    }

    return ret;
}

/****************************************************************************
 * 0x20nm   Assigned Slaves Cyclic Group
 */

static ec_coe_sdo_desc_t obj_desc_master_0x20nm = { DEFTYPE_ARRAY_OF_INT, OBJCODE_ARR, 255, { "Assigned Slaves Cyclic Group" }, 28 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x20nm[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      { "Subindex 0" },            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "Slave" },                  5 },
};

static int callback_master_0x20nm(ec_t *pec, ec_coe_object_t *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, 
        int complete, osal_uint8_t *buf, osal_size_t *len, osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(coe_obj != NULL);
    
    int ret = EC_OK;

    osal_uint16_t group = (index & 0x00FE) >> 1u;
    int value = -1;

    for (uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
        if (pec->slaves[slave].assigned_pd_group == group) {
            value++;

            if (sub_index > 0) {
                if (value == (sub_index - 1)) {
                    if ((*len) >= 2) {
                        (*(osal_uint32_t *)buf) = slave;
                        (*len) = sizeof(osal_uint16_t);
                    }
                }
            }
        }
    }

    if (sub_index == 0) {
        if ((*len) >= 1) {
            (*(osal_uint8_t *)buf) = value + 1;
            (*len) = sizeof(osal_uint8_t);
        }
    }

    return ret;
}

/****************************************************************************
 * 0x30nn   Configuration Distributied Clock Slave
 */

static ec_coe_sdo_desc_t obj_desc_master_0x3nnn = { DEFTYPE_RECORD, OBJCODE_REC, 13, { "Configuration Distributed Clock Slave" }, 37 };
static ec_coe_sdo_entry_desc_t entry_desc_master_0x3nnn[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, { "Subindex 0" }              , 10 }, // 0
    { 0, DEFTYPE_BOOLEAN,       8, ACCESS_READ, { "Enabled" }                 ,  7 },
    { 0, DEFTYPE_INTEGER32,    32, ACCESS_READ, { "Next Slave" }              , 10 },
    { 0, DEFTYPE_INTEGER32,    32, ACCESS_READ, { "Previous Slave" }          , 14 },
    { 0, DEFTYPE_UNSIGNED8,     8, ACCESS_READ, { "Active Ports" }            , 12 },
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Receive Time Port 0" }     , 19 },
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Receive Time Port 1" }     , 19 },
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Receive Time Port 2" }     , 19 },
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Receive Time Port 3" }     , 19 },
    { 0, DEFTYPE_UNSIGNED8,     8, ACCESS_READ, { "Sync Type" }               ,  9 }, 
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Cycle Time 0" }            , 12 }, 
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Cycle Time 1" }            , 12 }, 
    { 0, DEFTYPE_INTEGER32,    32, ACCESS_READ, { "Cycle Shift" }             , 11 },
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "System Delay" }            , 12 } };

static int callback_master_0x3nnn(ec_t *pec, ec_coe_object_t *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, int complete, osal_uint8_t *buf,
        osal_size_t *len, osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(coe_obj != NULL);
    
    int ret = EC_OK;

    osal_uint16_t slave = (index & 0x0FFF);
    if (sub_index == 0) {
        if ((*len) >= 1) {
            (*buf) = 13;
            (*len) = 1;
        }
    } else if (sub_index == 1) {
        if ((*len) >= 1) {
            (*(osal_uint8_t *)buf) = pec->slaves[slave].dc.use_dc;
            (*len) = sizeof(osal_uint8_t);
        }
    } else if (sub_index == 2) {
        if ((*len) >= 4) {
            (*(osal_int32_t *)buf) = pec->slaves[slave].dc.next;
            (*len) = sizeof(osal_int32_t);
        }
    } else if (sub_index == 3) {
        if ((*len) >= 4) {
            (*(osal_int32_t *)buf) = pec->slaves[slave].dc.prev;
            (*len) = sizeof(osal_int32_t);
        }
    } else if (sub_index == 4) {
        if ((*len) >= 1) {
            (*(osal_uint8_t *)buf) = pec->slaves[slave].active_ports;
            (*len) = sizeof(osal_uint8_t);
        }
    } else if (sub_index == 5) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->slaves[slave].dc.receive_times[0];
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 6) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->slaves[slave].dc.receive_times[1];
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 7) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->slaves[slave].dc.receive_times[2];
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 8) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->slaves[slave].dc.receive_times[3];
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 9) {
        if ((*len) >= 1) {
            (*(osal_uint8_t *)buf) = pec->slaves[slave].dc.type;
            (*len) = sizeof(osal_uint8_t);
        }
    } else if (sub_index == 10) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->slaves[slave].dc.cycle_time_0;
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 11) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->slaves[slave].dc.cycle_time_1;
            (*len) = sizeof(osal_uint32_t);
        }
    } else if (sub_index == 12) {
        if ((*len) >= 4) {
            (*(osal_int32_t *)buf) = pec->slaves[slave].dc.cycle_shift;
            (*len) = sizeof(osal_int32_t);
        }
    } else if (sub_index == 13) {
        if ((*len) >= 4) {
            (*(osal_uint32_t *)buf) = pec->slaves[slave].pdelay;
            (*len) = sizeof(osal_uint32_t);
        }
    }

    return ret;
}

/****************************************************************************
 * 0x8nnn   Configuration Data Slave
 */

static ec_coe_sdo_desc_t obj_desc_master_0x8nnn = { DEFTYPE_RECORD, OBJCODE_REC, 35, { "Configuration Data Slave" }, 24 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x8nnn[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, { "Subindex 0" }           , 10 }, // 0
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, { "Fixed Station Address" }, 21 }, // 1
    { 0, DEFTYPE_VISIBLESTRING, 8, ACCESS_READ, { "Type" },                   4 }, // 2
    { 0, DEFTYPE_VISIBLESTRING, 8, ACCESS_READ, { "Name" },                   4 }, // 3
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Device Type" },           11 }, // 4
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, { "Vendor Id" },              9 }, // 5
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, { "Product Code" },          12 }, // 6
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Revision Number" },       15 }, // 7
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Serial Number" },         13 }, // 8 
    { 0,                  0,    0,           0, { "" },                       0 }, // 9
    { 0,                  0,    0,           0, { "" },                       0 }, // 10
    { 0,                  0,    0,           0, { "" },                       0 }, // 11
    { 0,                  0,    0,           0, { "" },                       0 }, // 12
    { 0,                  0,    0,           0, { "" },                       0 }, // 13
    { 0,                  0,    0,           0, { "" },                       0 }, // 14
    { 0,                  0,    0,           0, { "" },                       0 }, // 15
    { 0,                  0,    0,           0, { "" },                       0 }, // 16
    { 0,                  0,    0,           0, { "" },                       0 }, // 17
    { 0,                  0,    0,           0, { "" },                       0 }, // 18
    { 0,                  0,    0,           0, { "" },                       0 }, // 19
    { 0,                  0,    0,           0, { "" },                       0 }, // 20
    { 0,                  0,    0,           0, { "" },                       0 }, // 21
    { 0,                  0,    0,           0, { "" },                       0 }, // 22
    { 0,                  0,    0,           0, { "" },                       0 }, // 23
    { 0,                  0,    0,           0, { "" },                       0 }, // 24
    { 0,                  0,    0,           0, { "" },                       0 }, // 25
    { 0,                  0,    0,           0, { "" },                       0 }, // 26
    { 0,                  0,    0,           0, { "" },                       0 }, // 27
    { 0,                  0,    0,           0, { "" },                       0 }, // 28
    { 0,                  0,    0,           0, { "" },                       0 }, // 29
    { 0,                  0,    0,           0, { "" },                       0 }, // 30
    { 0,                  0,    0,           0, { "" },                       0 }, // 31
    { 0,                  0,    0,           0, { "" },                       0 }, // 32
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, { "Mailbox Out Size" },      16 }, // 33
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, { "Mailbox In Size" },       15 }, // 34
    { 0, DEFTYPE_BYTE,          8, ACCESS_READ, { "Link Status" },           11 }, // 35
};

static int callback_master_0x8nnn(ec_t *pec, ec_coe_object_t *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, 
        int complete, osal_uint8_t *buf, osal_size_t *len, osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(coe_obj != NULL);
    
    int ret = EC_OK;

    osal_uint16_t slave = index & 0x0FFF;
    if (sub_index == 0) {
        if ((*len) >= 1) {
            (*buf) = 35;
            (*len) = 1;
        }
    } else if (sub_index == 1) {
        if ((*len) >= 2) {
            (*(osal_uint16_t *)buf) = pec->slaves[slave].fixed_address;
            (*len) = sizeof(osal_uint16_t);
        }
    } else if (sub_index == 2) {
        if (pec->slaves[slave].eeprom.mbx_supported != 0) {
            ret = ec_coe_sdo_read(pec, slave, 0x100A, 0, complete, buf, len, abort_code);
        }
    } else if (sub_index == 3) {
        if (pec->slaves[slave].eeprom.mbx_supported != 0) {
            ret = ec_coe_sdo_read(pec, slave, 0x1008, 0, complete, buf, len, abort_code);
        }
    } else if (sub_index == 4) {
        if (pec->slaves[slave].eeprom.mbx_supported != 0) {
            ret = ec_coe_sdo_read(pec, slave, 0x1000, 0, complete, buf, len, abort_code);
        }
    } else if ((sub_index == 5) || (sub_index == 6) || (sub_index == 7) || (sub_index == 8)) {
        ret = ec_coe_sdo_read(pec, slave, 0x1018, sub_index - 4, complete, buf, len, abort_code);
    } else if ((sub_index == 33) || (sub_index == 34)) {
        if ((*len) >= 2) {
            if (pec->slaves[slave].eeprom.mbx_supported != 0) {
                (*(osal_uint16_t *)buf) = pec->slaves[slave].sm[sub_index-33].len;
                (*len) = sizeof(osal_uint16_t);
            } else {
                (*(osal_uint16_t *)buf) = 0;
                (*len) = sizeof(osal_uint16_t);
            }
        }
    }

    return ret;
}

/****************************************************************************
 * 0x9nnn   Information Data Slave
 */

static ec_coe_sdo_desc_t obj_desc_master_0x9nnn = { DEFTYPE_RECORD, OBJCODE_REC, 32, { "Information Data Slave" }, 22 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x9nnn[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, { "Subindex 0" }           , 10 }, // 0
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, { "Fixed Station Address" }, 21 }, // 1
    { 0,                  0,    0,           0, { "" },                       0 }, // 2
    { 0,                  0,    0,           0, { "" },                       0 }, // 3
    { 0,                  0,    0,           0, { "" },                       0 }, // 4
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, { "Vendor Id" },              9 }, // 5
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, { "Product Code" },          12 }, // 6
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Revision Number" },       15 }, // 7
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, { "Serial Number" },         13 }, // 8 
    { 0,                  0,    0,           0, { "" },                       0 }, // 9
    { 0,                  0,    0,           0, { "" },                       0 }, // 10
    { 0,                  0,    0,           0, { "" },                       0 }, // 11
    { 0,                  0,    0,           0, { "" },                       0 }, // 12
    { 0,                  0,    0,           0, { "" },                       0 }, // 13
    { 0,                  0,    0,           0, { "" },                       0 }, // 14
    { 0,                  0,    0,           0, { "" },                       0 }, // 15
    { 0,                  0,    0,           0, { "" },                       0 }, // 16
    { 0,                  0,    0,           0, { "" },                       0 }, // 17
    { 0,                  0,    0,           0, { "" },                       0 }, // 18
    { 0,                  0,    0,           0, { "" },                       0 }, // 19
    { 0,                  0,    0,           0, { "" },                       0 }, // 20
    { 0,                  0,    0,           0, { "" },                       0 }, // 21
    { 0,                  0,    0,           0, { "" },                       0 }, // 22
    { 0,                  0,    0,           0, { "" },                       0 }, // 23
    { 0,                  0,    0,           0, { "" },                       0 }, // 24
    { 0,                  0,    0,           0, { "" },                       0 }, // 25
    { 0,                  0,    0,           0, { "" },                       0 }, // 26
    { 0,                  0,    0,           0, { "" },                       0 }, // 27
    { 0,                  0,    0,           0, { "" },                       0 }, // 28
    { 0,                  0,    0,           0, { "" },                       0 }, // 29
    { 0,                  0,    0,           0, { "" },                       0 }, // 30
    { 0,                  0,    0,           0, { "" },                       0 }, // 31
    { 0, DEFTYPE_WORD,         16, ACCESS_READ, { "DL Status Register" },    18 }, // 32
};

static int callback_master_0x9nnn(ec_t *pec, ec_coe_object_t *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, 
        int complete, osal_uint8_t *buf, osal_size_t *len, osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(coe_obj != NULL);
    
    int ret = EC_OK;

    osal_uint16_t slave = index & 0x0FFF;
    if (sub_index == 0) {
        if ((*len) >= 1) {
            (*buf) = 32;
            (*len) = 1;
        }
    } else if (sub_index == 1) {
        if ((*len) >= 2) {
            (*(osal_uint16_t *)buf) = pec->slaves[slave].fixed_address;
            (*len) = sizeof(osal_uint16_t);
        }
    } else if ((sub_index == 5) || (sub_index == 6) || (sub_index == 7) || (sub_index == 8)) {
        ret = ec_coe_sdo_read(pec, slave, 0x1018, sub_index - 4, complete, buf, len, abort_code);
    } else if (sub_index == 32) {
        if ((*len) >= 2) {
            osal_uint16_t wkc = 0;
            (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x110, buf, sizeof(osal_uint16_t), &wkc);

            if (wkc != 0) {
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

static ec_coe_sdo_desc_t obj_desc_master_0xAnnn = { DEFTYPE_RECORD, OBJCODE_REC, 2, { "Diagnosis Data Slave" }, 20 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xAnnn[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      { "Subindex 0" },            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "AL Status" },              9 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "AL Control" },            10 },
};

static int callback_master_0xAnnn(ec_t *pec, ec_coe_object_t *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, 
        int complete, osal_uint8_t *buf, osal_size_t *len, osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(coe_obj != NULL);
    
    int ret = EC_OK;

    osal_uint16_t slave = index & 0x0FFF;
    if (sub_index == 0) {
        if ((*len) >= 1) {
            (*buf) = 2;
            (*len) = 1;
        }
    } else if (sub_index == 1) {
        if ((*len) >= 2) {
            osal_uint16_t wkc = 0;
            (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x130, buf, sizeof(osal_uint16_t), &wkc);

            if (wkc != 0) {
                (*len) = sizeof(osal_uint16_t);
            }
        }
    } else if (sub_index == 2) {
        if ((*len) >= 2) {
            osal_uint16_t wkc = 0;
            (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x120, buf, sizeof(osal_uint16_t), &wkc);

            if (wkc != 0) {
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

static ec_coe_sdo_desc_t obj_desc_master_0xF000 = { DEFTYPE_RECORD, OBJCODE_REC, 4, { "Modular Device Profile" }, 22 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF000[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      { "Subindex 0" },            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "Module Index Distance" }, 21 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "Maximum Number of Modules" }, 25 },
    { 0, DEFTYPE_UNSIGNED32,  32, ACCESS_READ,      { "General Configuration" }, 21 },
    { 0, DEFTYPE_UNSIGNED32,  32, ACCESS_READ,      { "General Information" },   19 },
};
static struct PACKED {
    osal_uint8_t  subindex_0; 
    osal_uint16_t module_index_distance;
    osal_uint16_t maximum_number_of_modules;
    osal_uint32_t general_configuration;
    osal_uint32_t general_information;
} data_master_0xF000 = { 4, 0x0001, 4080, 0x000000FF, 0x000000F1 };

/****************************************************************************
 * 0xF002   Detect Modules Command
 */

static ec_coe_sdo_desc_t obj_desc_master_0xF002 = { DEFTYPE_RECORD, OBJCODE_REC, 3, { "Detect Modules Command" }, 22 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF002[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      { "Subindex 0" },            10 },
    { 0, DEFTYPE_OCTETSTRING, 16, ACCESS_READWRITE, { "Scan Command Request" },  20 },
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      { "Scan Command Status" },   19 },
    { 0, DEFTYPE_OCTETSTRING, 48, ACCESS_READ,      { "Scan Command Response" }, 21 }
};
static struct PACKED {
    osal_uint8_t  subindex_0;
    osal_uint8_t  scan_command_request[2];
    osal_uint8_t  scan_command_status;
    osal_uint8_t  scan_command_response[6];
} data_master_0xF002 = { 3, { 0 }, 0, { 0 } };

/****************************************************************************
 * 0xF02n   Configured Address List Slaves
 */

static ec_coe_sdo_desc_t obj_desc_master_0xF02n = { DEFTYPE_ARRAY_OF_INT, OBJCODE_ARR, 255, { "Configured Address List Slaves" }, 30 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF02n[] = {
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "Subindex 0" },            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "Slave" },                  5 },
};

/****************************************************************************
 * 0xF04n   Detected Address List Slaves
 */

static ec_coe_sdo_desc_t obj_desc_master_0xF04n = { DEFTYPE_ARRAY_OF_INT, OBJCODE_ARR, 255, { "Detected Address List Slaves" }, 28 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF04n[] = {
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "Subindex 0" },            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      { "Slave" },                  5 },
};

static int callback_master_0xF0nn(ec_t *pec, ec_coe_object_t *coe_obj, osal_uint16_t index, osal_uint8_t sub_index, 
        int complete, osal_uint8_t *buf, osal_size_t *len, osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(coe_obj != NULL);
    
    int ret = EC_OK;

    osal_uint16_t slave_range = index & 0x000Fu;
    osal_uint16_t slave = (slave_range * 255) + (sub_index - 1);

    if ((*len) >= 2) {
        if (slave < pec->slave_cnt) {
            (*(osal_uint16_t *)buf) = pec->slaves[slave].fixed_address;
        } else {
            (*(osal_uint16_t *)buf) = 0;
        }

        (*len) = sizeof(osal_uint16_t);
    }

    return ret; 
}

#define     EC_COE_OBJECT_INDEX_MASK_ALL        ((osal_uint16_t)0xFFFFu)

ec_coe_object_t ec_coe_master_dict[] = {
    { 0x1000, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x1000, &entry_desc_master_0x1000   , (osal_uint8_t *)&data_master_0x1000, NULL, NULL },
    { 0x1008, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x1008, &entry_desc_master_0x1008   , (osal_uint8_t *) data_master_0x1008, NULL, NULL },
    { 0x1009, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x1009, &entry_desc_master_0x1009   , (osal_uint8_t *) data_master_0x1009, NULL, NULL },
    { 0x100A, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x100A, &entry_desc_master_0x100A   , (osal_uint8_t *) data_master_0x100A, NULL, NULL },
    { 0x1018, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0x1018, &entry_desc_master_0x1018[0], (osal_uint8_t *)&data_master_0x1018, NULL, NULL },
    { 0x2000, 0xFF01u,                      &obj_desc_master_0x20nn, &entry_desc_master_0x20nn[0], NULL,                                callback_master_0x20nn, NULL },
    { 0x2001, 0xFF01u,                      &obj_desc_master_0x20nm, &entry_desc_master_0x20nm[0], NULL,                                callback_master_0x20nm, NULL },
    { 0x3000, 0xF000u,                      &obj_desc_master_0x3nnn, &entry_desc_master_0x3nnn[0], NULL,                                callback_master_0x3nnn, NULL },
    { 0x8000, 0xF000u,                      &obj_desc_master_0x8nnn, &entry_desc_master_0x8nnn[0], NULL,                                callback_master_0x8nnn, NULL },
    { 0x9000, 0xF000u,                      &obj_desc_master_0x9nnn, &entry_desc_master_0x9nnn[0], NULL,                                callback_master_0x9nnn, NULL },
    { 0xA000, 0xF000u,                      &obj_desc_master_0xAnnn, &entry_desc_master_0xAnnn[0], NULL,                                callback_master_0xAnnn, NULL },
    { 0xF000, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0xF000, &entry_desc_master_0xF000[0], (osal_uint8_t *)&data_master_0xF000, NULL, NULL },
    { 0xF002, EC_COE_OBJECT_INDEX_MASK_ALL, &obj_desc_master_0xF002, &entry_desc_master_0xF002[0], (osal_uint8_t *)&data_master_0xF002, NULL, NULL },
    { 0xF020, 0xFFF0u,                      &obj_desc_master_0xF02n, &entry_desc_master_0xF02n[0], NULL,                                callback_master_0xF0nn, NULL },
    { 0xF040, 0xFFF0u,                      &obj_desc_master_0xF04n, &entry_desc_master_0xF04n[0], NULL,                                callback_master_0xF0nn, NULL },
    { 0xFFFF, EC_COE_OBJECT_INDEX_MASK_ALL, NULL, NULL, NULL, NULL, NULL } };

static int ec_coe_master_get_object(osal_uint16_t index, ec_coe_object_t **coe_obj) {
    assert(coe_obj != NULL);

    int ret = EC_ERROR_MAILBOX_COE_INDEX_NOT_FOUND;
    ec_coe_object_t *tmp_coe_obj = &ec_coe_master_dict[0];

    while (tmp_coe_obj->index != 0xFFFFu) {
        if (tmp_coe_obj->index == (index & tmp_coe_obj->index_mask)) {
            *coe_obj = tmp_coe_obj;
            ret = EC_OK;
            break;
        }

        tmp_coe_obj++;
    }

    return ret;
}

static int ec_coe_master_get_object_data(ec_coe_object_t *coe_obj, osal_uint8_t sub_index, 
        osal_uint8_t **data, osal_size_t *data_len) 
{
    assert(coe_obj != NULL);
    assert(data != NULL);
    assert(data_len != NULL);

    int ret = EC_ERROR_MAILBOX_ABORT;

    if (sub_index <= coe_obj->obj_desc->max_subindices) {
        if (coe_obj->data != NULL) {
            osal_uint8_t *tmp = coe_obj->data;

            for (osal_uint16_t i = 0; i <= sub_index; ++i) {
                if (i == sub_index) {
                    (*data) = tmp;
                    (*data_len) = coe_obj->entry_desc[i].bit_length >> 3;
                    ret = EC_OK;
                    break;
                }

                tmp += coe_obj->entry_desc[i].bit_length >> 3;
            }
        }
    }

    return ret;
}

// Read CoE service data object (SDO) of master
int ec_coe_master_sdo_read(ec_t *pec, osal_uint16_t index, 
        osal_uint8_t sub_index, int complete, osal_uint8_t *buf, osal_size_t *len, 
        osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);

    int ret = EC_OK;
        
    ec_coe_object_t *coe_obj = NULL;
    ret = ec_coe_master_get_object(index, &coe_obj);

    if ((coe_obj != NULL) && (coe_obj->read != NULL)) {
        ret = (*coe_obj->read)(pec, coe_obj, index, sub_index, complete, buf, len, abort_code);
    } else if ((coe_obj != NULL) && (coe_obj->data != NULL)) {
        osal_uint8_t *data = NULL;
        osal_size_t data_len = 0;
        ret = ec_coe_master_get_object_data(coe_obj, sub_index, &data, &data_len);

        if ((ret == EC_OK) && (data != NULL)) {
            if ((*len) >= data_len) {
                memcpy(buf, data, data_len);
            }

            (*len) = data_len;
        }
    } else {
        ret = EC_ERROR_MAILBOX_ABORT;
    }

    return ret;
}

// Write CoE service data object (SDO) of master
int ec_coe_master_sdo_write(ec_t *pec, osal_uint16_t index, 
        osal_uint8_t sub_index, int complete, osal_uint8_t *buf, osal_size_t len,
        osal_uint32_t *abort_code) 
{
    assert(pec != NULL);
    assert(buf != NULL);

    int ret = EC_OK;

    ec_coe_object_t *coe_obj = NULL;
    ret = ec_coe_master_get_object(index, &coe_obj);

    if ((coe_obj != NULL) && (coe_obj->write != NULL)) {
        ret = (*coe_obj->write)(pec, coe_obj, index, sub_index, complete, buf, len, abort_code);
    } else if ((coe_obj != NULL) && (coe_obj->data != NULL)) {
        osal_uint8_t *data = NULL;
        osal_size_t data_len = 0;
        ret = ec_coe_master_get_object_data(coe_obj, sub_index, &data, &data_len);

        if ((ret == EC_OK) && (data != NULL) && (sub_index <= coe_obj->obj_desc->max_subindices)) {
            ec_coe_sdo_entry_desc_t *entry_desc = &coe_obj->entry_desc[sub_index];

            if (entry_desc->obj_access & ACCESS_WRITE) {
                if (len <= data_len) {
                    memcpy(data, buf, len);
                }
            }
        }
    } else {
        ret = EC_ERROR_MAILBOX_ABORT;
    }

    return ret;
}

// read coe object dictionary list of master
int ec_coe_master_odlist_read(ec_t *pec, osal_uint8_t *buf, osal_size_t *len) {
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);
    int ret = EC_OK;

    // return just an uint16_t array of indices
    //uint16_t test_indices[] = { 0x8000, 0x9000, 0xA000, 0xF000, 0xF002, 0xF020, 0xF040 };
    //osal_size_t od_len = sizeof(uint16_t) * 7;//sizeof(test_indices);

    osal_uint8_t *tmp = buf;
    osal_uint8_t *end = buf + *len;

    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x1000;
        tmp += sizeof(osal_uint16_t);
    }
    
    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x1008;
        tmp += sizeof(osal_uint16_t);
    }
    
    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x1009;
        tmp += sizeof(osal_uint16_t);
    }

    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x100A;
        tmp += sizeof(osal_uint16_t);
    }
    
    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x1018;
        tmp += sizeof(osal_uint16_t);
    }
    
    for (int i = 0; (i < pec->pd_group_cnt) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0x2000 | (i << 1u);
        tmp += sizeof(osal_uint16_t);
        *(osal_uint16_t *)tmp = 0x2001 | (i << 1u);
        tmp += sizeof(osal_uint16_t);
    }
    
    for (int i = 0; (i < pec->slave_cnt) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0x3000 | i;
        tmp += sizeof(osal_uint16_t);
    }

    for (int i = 0; (i < pec->slave_cnt) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0x8000 | i;
        tmp += sizeof(osal_uint16_t);
    }
    
    for (int i = 0; (i < pec->slave_cnt) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0x9000 | i;
        tmp += sizeof(osal_uint16_t);
    }

    for (int i = 0; (i < pec->slave_cnt) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0xA000 | i;
        tmp += sizeof(osal_uint16_t);
    }

    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0xF000;
        tmp += sizeof(osal_uint16_t);
    }

    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0xF002;
        tmp += sizeof(osal_uint16_t);
    }
    
    for (int i = 0; (i < ((pec->slave_cnt/255) + 1)) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0xF020 | i;
        tmp += sizeof(osal_uint16_t);
    }

    for (int i = 0; (i < ((pec->slave_cnt/255) + 1)) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0xF040 | i;
        tmp += sizeof(osal_uint16_t);
    }

    if (tmp >= end) {
        ret = EC_ERROR_MAILBOX_BUFFER_TOO_SMALL;
    } else {
        (*len) = tmp - buf;
    }

    return ret;
}

// Read CoE SDO description of master
int ec_coe_master_sdo_desc_read(const ec_t *pec, osal_uint16_t index, 
        ec_coe_sdo_desc_t *desc, osal_uint32_t *error_code) 
{
    assert(pec != NULL);
    assert(desc != NULL);
    (void)error_code;

    int ret = EC_OK;

    ec_coe_object_t *coe_obj = NULL;
    ret = ec_coe_master_get_object(index, &coe_obj);

    if (coe_obj) {
        (void)memcpy(desc, coe_obj->obj_desc, sizeof(ec_coe_sdo_desc_t));

        if (index >= 0xF000) {
            if ((index != 0xF000) && (index != 0xF002)) {
                osal_uint16_t slave_range = index & 0x000F;
                osal_uint16_t slave_begin = slave_range * 256;
                osal_uint16_t slave_end = slave_begin + 255;

                (void)snprintf(&desc->name[strlen(coe_obj->obj_desc->name)], 
                        CANOPEN_MAXNAME - strlen(coe_obj->obj_desc->name), " %hu-%hu", slave_begin, slave_end);
                desc->name_len = strlen(desc->name);

                if (slave_begin < pec->slave_cnt) {
                    desc->max_subindices = pec->slave_cnt - slave_begin;
                } else {
                    desc->max_subindices = 0;
                }
            }
        } else if (index >= 0x8000) {
            osal_uint16_t slave = index & 0x0FFF;
            (void)snprintf(&desc->name[strlen(coe_obj->obj_desc->name)], 
                    CANOPEN_MAXNAME - strlen(coe_obj->obj_desc->name), " %hu", slave);
            desc->name_len = strlen(desc->name);
        } else if ((index & 0xFF01) == 0x2000) {
            osal_uint16_t group = (index & 0x0FE) >> 1;
            (void)snprintf(&desc->name[strlen(coe_obj->obj_desc->name)], 
                    CANOPEN_MAXNAME - strlen(coe_obj->obj_desc->name), " %hu", group);
            desc->name_len = strlen(desc->name);
        } else if ((index & 0xFF01) == 0x2001) {
            osal_uint16_t group = (index & 0x0FE) >> 1;
            (void)snprintf(&desc->name[strlen(coe_obj->obj_desc->name)], 
                    CANOPEN_MAXNAME - strlen(coe_obj->obj_desc->name), " %hu", group);
            desc->name_len = strlen(desc->name);
            desc->max_subindices = 0;

            for (uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
                if (pec->slaves[slave].assigned_pd_group == group) {
                    desc->max_subindices++;
                }
            }
        }
    }

    return ret;
}

// Read CoE SDO entry description of master
int ec_coe_master_sdo_entry_desc_read(const ec_t *pec, osal_uint16_t index,
        osal_uint8_t sub_index, osal_uint8_t value_info, ec_coe_sdo_entry_desc_t *desc, 
        osal_uint32_t *error_code) 
{
    assert(pec != NULL);
    assert(desc != NULL);
    (void)value_info;
    (void)error_code;

    int ret = EC_OK; 

    ec_coe_object_t *coe_obj = NULL;
    ret = ec_coe_master_get_object(index, &coe_obj);

    if (coe_obj) {
        if (sub_index <= coe_obj->obj_desc->max_subindices) {
            if ((sub_index != 0) && (coe_obj->obj_desc->obj_code == OBJCODE_ARR)) {
                ec_coe_sdo_entry_desc_t *entry_desc = &coe_obj->entry_desc[1];
                (void)memcpy(desc, entry_desc, sizeof(ec_coe_sdo_entry_desc_t));

                if (((index & 0xFFF0) == 0xF020) || ((index & 0xFFF0) == 0xF040)) {
                    osal_uint16_t slave_range = index & 0x000Fu;
                    osal_uint16_t slave = (slave_range * 255) + (sub_index - 1);

                    (void)snprintf((char *)&desc->data[strlen((char *)(entry_desc->data))], 
                            CANOPEN_MAXNAME - strlen((char *)(entry_desc->data)), " %hu", slave);
                    desc->data_len = strlen((char *)&desc->data[0]);
                }
            } else {
                (void)memcpy(desc, &coe_obj->entry_desc[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
            }

        }
    }

    return ret;
}

