/*
 * time.h - monotonic clock.
 *
 * A single steady, monotonic nanosecond counter. It never goes backwards and is
 * unaffected by wall-clock changes, so only differences between two readings are
 * meaningful (the absolute value has an arbitrary, platform-defined origin). Use
 * it for elapsed-time measurement and to derive timeout deadlines.
 */

#ifndef RC_TIME_H_
#define RC_TIME_H_

#include <stdint.h>

/* Current value of the monotonic clock, in nanoseconds. */
uint64_t rc_time_now_ns(void);

#endif /* RC_TIME_H_ */
