//! ethercat master common functions
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

#include <config.h>
#include <sys/types.h>
#ifdef __VXWORKS__
#include <string.h>
#endif // __VXWORKS__

#include "libethercat/common.h"

// cppcheck-suppress misra-c2012-20.9
#if HAVE_MALLOC == 0

// wrapper around malloc to avoid allocation of 0 bytes
// 
// allocate an n-byte block of memory from the heap.
// if n is zero, allocate a 1-byte block.
void *rpl_malloc(osal_size_t n) {
    osal_size_t local_n = n;

    if (local_n == 0u) {
        local_n = 1u;
    }

    // cppcheck-suppress misra-c2012-21.3
    return malloc(local_n);
}

#endif

#ifdef __VXWORKS__ 


osal_char_t *strndup(const osal_char_t *s, osal_size_t n) {
    const osal_char_t* cp = s;
    osal_size_t i = 0;
    while((*cp) != '\0') {
        i++;
        if(i >= n) {
            break; // enough osal_char_ts
        }

        cp++;
    }
    i ++;
    // cppcheck-suppress misra-c2012-21.3
    osal_char_t* result = (osal_char_t*)malloc(i);
    (void)memcpy(result, s, i);
    result[i - 1u] = 0;
    return result;
}
#endif

