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
 * If not, see <www.gnu.org/licenses/>.
 */

#include "config.h"

#include "libethercat/timer.h"
#include "libethercat/error_codes.h"

// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <assert.h>

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

// sleep in nanoseconds
void ec_sleep(int64_t nsec) {
    struct timespec ts = { (nsec / NSEC_PER_SEC), (nsec % NSEC_PER_SEC) };
    struct timespec rest;
    
    while (1) {
        int ret = nanosleep(&ts, &rest);
        if (ret == 0) {
            break;
        }

        ts = rest;
    }
}

//! gets timer 
int ec_timer_gettime(ec_timer_t *timer) {
    assert(timer != NULL);
    int ret = EC_OK;

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        ret = EC_ERROR_UNAVAILABLE;
    } else {
        timer->sec = ts.tv_sec;
        timer->nsec = ts.tv_nsec;
    }

    return ret;
}

// gets time in nanoseconds
int64_t ec_timer_gettime_nsec(void) {
    int64_t ret = 0;
    ec_timer_t tmr = { 0, 0 };
    ret = ec_timer_gettime(&tmr);

    if (ret == EC_OK) {
        ret = ((tmr.sec * 1E9) + tmr.nsec);
    }

    return ret;
}

// initialize timer with timeout 
void ec_timer_init(ec_timer_t *timer, int64_t timeout) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
    }

    assert(timer != NULL);

    ec_timer_t a;
    ec_timer_t b;
    a.sec = ts.tv_sec;
    a.nsec = ts.tv_nsec;

    b.sec = (timeout / NSEC_PER_SEC);
    b.nsec = (timeout % NSEC_PER_SEC);

    timer_add(&a, &b, timer);
}

// checks if timer is expired
int ec_timer_expired(ec_timer_t *timer) {
    ec_timer_t act = { 0, 0 };
    int ret = EC_OK;
    ret = ec_timer_gettime(&act);    

    assert(timer != NULL);

    if (ret == EC_OK) {
        if (timer_cmp(&act, timer, <) == 0) {
            ret = EC_ERROR_TIMER_EXPIRED;
        }
    } 

    return ret;
}

