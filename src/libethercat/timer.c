/**
 * \file timer.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 23 Nov 2016
 *
 * \brief ethercat master timer routines
 *
 * 
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

#include "libethercat/timer.h"
#include <stdio.h>

#define timer_cmp(a, b, CMP)          \
    (((a)->sec == (b)->sec) ?         \
     ((a)->nsec CMP (b)->nsec) :      \
     ((a)->sec CMP (b)->sec))

#define timer_add(a, b, result)                 \
  do {                                          \
    (result)->sec  = (a)->sec  + (b)->sec;      \
    (result)->nsec = (a)->nsec + (b)->nsec;     \
    if ((result)->nsec >= NSEC_PER_SEC)         \
      {                                         \
        ++(result)->sec;                        \
        (result)->nsec -= NSEC_PER_SEC;         \
      }                                         \
  } while (0)

//! sleep in nanoseconds
/*!
 * \param nsec time to sleep in nanoseconds
 */
void ec_sleep(uint64_t nsec) {
    struct timespec ts = { 
        (nsec / NSEC_PER_SEC), (nsec % NSEC_PER_SEC) }, rest;
    
    while (1) {
        int ret = nanosleep(&ts, &rest);
        if (ret == 0)
            break;

        ts = rest;
    }
}

//! gets timer 
/*!
 * \param timer pointer to timer struct
 * \return 0 on success, -1 on error and errno set
 */
int ec_timer_gettime(ec_timer_t *timer) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        return -1;
    }

    timer->sec = ts.tv_sec;
    timer->nsec = ts.tv_nsec;

    return 0;
}

//! gets timer in nanoseconds
/*!
 * \param timer pointer to timer struct
 * \return 0 on success, -1 on error and errno set
 */
uint64_t ec_timer_gettime_nsec() {
    ec_timer_t tmr;
    ec_timer_gettime(&tmr);

    return (tmr.sec * 1E9 + tmr.nsec);
}

//! initialize timer with timeout 
/*!
 * \parma timer pointer to timer to initialize
 * \param timeout in nanoseconds
 */
void ec_timer_init(ec_timer_t *timer, uint64_t timeout) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        perror("clock_gettime");

    ec_timer_t a, b;
    a.sec = ts.tv_sec;
    a.nsec = ts.tv_nsec;

    b.sec = (timeout / NSEC_PER_SEC);
    b.nsec = (timeout % NSEC_PER_SEC);

    timer_add(&a, &b, timer);
}

//! checks if timer is expired
/*!
 * \param timer timer to check 
 * \return 1 if expired, 0 if not
 */
int ec_timer_expired(ec_timer_t *timer) {
    ec_timer_t act;
    ec_timer_gettime(&act);    

    return !timer_cmp(&act, timer, <);
}

