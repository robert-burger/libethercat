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

#ifndef __LIBETHERCAT_SLAVE_H__
#define __LIBETHERCAT_SLAVE_H__

#include <stdint.h>

#include "libethercat/common.h"
#include "libethercat/slave.h"
#include "libethercat/ec.h"

typedef enum ec_state_transition {
    BOOT_2_INIT      = 0x0301,
    INIT_2_BOOT      = 0x0103,
    INIT_2_INIT      = 0x0101,
    INIT_2_PREOP     = 0x0102,
    INIT_2_SAFEOP    = 0x0104,
    INIT_2_OP        = 0x0108,
    PREOP_2_INIT     = 0x0201,
    PREOP_2_PREOP    = 0x0202,
    PREOP_2_SAFEOP   = 0x0204,
    PREOP_2_OP       = 0x0208,
    SAFEOP_2_INIT    = 0x0401,
    SAFEOP_2_PREOP   = 0x0402,
    SAFEOP_2_SAFEOP  = 0x0404,
    SAFEOP_2_OP      = 0x0408,
    OP_2_INIT        = 0x0801,
    OP_2_PREOP       = 0x0802,
    OP_2_SAFEOP      = 0x0804,
    OP_2_OP          = 0x0808,
} ec_state_transition_t;

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! set ethercat state on slave 
/*!
 * \param pec ethercat master pointer
 * \param slave number
 * \param state new ethercat state
 * \return wkc
 */
int ec_slave_set_state(struct ec *pec, uint16_t slave, ec_state_t state);

//! get ethercat state from slave 
/*!
 * \param pec ethercat master pointer
 * \param slave number
 * \param state return ethercat state
 * \param alstatcode return alstatcode (maybe NULL)
 * \return wkc
 */
int ec_slave_get_state(struct ec *pec, uint16_t slave, 
        ec_state_t *state, uint16_t *alstatcode);

//! generate pd mapping
/*!
 * \param pec ethercat master pointer
 * \param slave slave number
 * \return wkc
 */
int ec_slave_generate_mapping(struct ec *pec, uint16_t slave);

//! prepare state transition on ethercat slave
/*!
 * \param pec ethercat master pointer
 * \param slave slave number
 * \param state switch to state
 * \return wkc
 */
int ec_slave_prepare_state_transition(ec_t *pec, uint16_t slave, ec_state_t state);

//! state transition on ethercat slave
/*!
 * \param pec ethercat master pointer
 * \param slave slave number
 * \param state switch to state
 * \return wkc
 */
int ec_slave_state_transition(struct ec *pec, uint16_t slave, ec_state_t state);

//! add master init command
/*!
 * \param ring master ring structure
 * \param atn drive atn
 * \param direction service direction
 * \param element service element
 * \param idn id number
 * \param value pointer to values
 * \param vallen legnth of values
 * \param desc null-terminated description (maybe NULL)
 */
void ec_slave_add_init_cmd(ec_t *pec, uint16_t slave,
        int type, int transition, int id, int si_el, int ca_atn,
        char *data, size_t datalen);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_SLAVE_H__

