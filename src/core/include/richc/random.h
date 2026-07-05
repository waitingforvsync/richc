#ifndef RC_RANDOM_H_
#define RC_RANDOM_H_

#include <stdint.h>

/*
 * random.h - a tiny, fast pseudo-random generator.
 *
 * splitmix32: one word of state, an add and two multiply-mixes per draw, good
 * statistical quality, and - unlike xorshift - no bad seeds (any seed, including
 * 0, is fine). Deterministic: the same seed always replays the same stream. Not
 * for cryptography.
 */
typedef struct rc_random {
    uint32_t state;
} rc_random;

/* Make a generator seeded to `seed`. */
rc_random rc_random_make(uint32_t seed);

/* The next 32-bit draw, advancing the state. */
uint32_t rc_random_next(rc_random *p);

#endif // RC_RANDOM_H_
