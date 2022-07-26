/**
 * \file timer.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 23 Nov 2016
 *
 * \brief EtherCAT master timer routines
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

#ifndef LIBETHERCAT_TIMER_H
#define LIBETHERCAT_TIMER_H

#define NSEC_PER_SEC                1000000000

#define EC_SHORT_TIMEOUT_MBX        10000000
#define EC_DEFAULT_TIMEOUT_MBX      1000000000
#define EC_DEFAULT_DELAY            2000000

#include <stdint.h>
// cppcheck-suppress misra-c2012-21.10
#include <time.h>
#include <sys/time.h>

//! timer structure
typedef struct ec_timer {
    int64_t sec;       //!< seconds
    int64_t nsec;      //!< nanoseconds
} ec_timer_t;

# define ec_timer_add(a, b, result)                                 \
    do {                                                            \
        (result)->sec = (a)->sec + (b)->sec;                        \
        (result)->nsec = (a)->nsec + (b)->nsec;                     \
        if ((result)->nsec >= 1E9)                                  \
        {                                                           \
            ++(result)->sec;                                        \
            (result)->nsec -= 1E9;                                  \
        }                                                           \
    } while (0)

#define ec_timer_cmp(a, b, CMP)                                     \
    (((a)->sec == (b)->sec) ?                                       \
     ((a)->nsec CMP (b)->nsec) :                                    \
     ((a)->sec CMP (b)->sec))

#ifdef __cplusplus
extern "C" {
#endif

//! Sleep in nanoseconds
/*!
 * \param[in] nsec      Time to sleep in nanoseconds.
 */
void ec_sleep(int64_t nsec);

//! Gets filled timer struct with current time.
/*!
 * \param[out] timer    Pointer to timer struct which will be initialized
 *                      with current time.
 *
 * \retval EC_OK                    On success.
 * \retval EC_ERROR_UNAVAILABLE     On error and errno set.
 */
int ec_timer_gettime(ec_timer_t *timer);

//! Gets time in nanoseconds.
/*!
 * \return              Current timer in nanosecond.
 */
int64_t ec_timer_gettime_nsec(void);

//! Initialize timer with timeout.
/*!
 * \param[out] timer    Pointer to timer struct which will be initialized
 *                      with current time plus an optional \p timeout.
 * \param[in] timeout   Timeout in nanoseconds. If set to 0, then this function
 *                      will do the same as \link ec_timer_gettime \endlink.
 */
void ec_timer_init(ec_timer_t *timer, int64_t timeout);

//! Checks if timer is expired.
/*!
 * \param[out] timer    Timer to check if it is expired.
 *
 * \retval EC_ERROR_TIMER_EXPIRED   If \p timer is expired
 * \retval EC_OK                    If \p timer is not expired
 */
int ec_timer_expired(ec_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_TIMER_H

