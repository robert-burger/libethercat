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
 * If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#undef malloc
     
#include <sys/types.h>

void *malloc();

#if HAVE_MALLOC == 0

// wrapper around malloc to avoid allocation of 0 bytes
// 
// allocate an n-byte block of memory from the heap.
// if n is zero, allocate a 1-byte block.
void *rpl_malloc(size_t n) {
  if (n == 0)
    n = 1;
  return malloc(n);
}

#endif

