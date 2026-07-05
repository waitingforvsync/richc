#include "richc/random.h"

#include <stddef.h>

#include "richc/macros.h"


rc_random rc_random_make(uint32_t seed)
{
    return (rc_random) {.state = seed};
}

/* splitmix32: bump a counter by the golden-ratio odd constant, then run it through two xor-shift-multiply
 * mixes. The multiply constants are the well-tested pair from the hash-prospector search - they give a
 * near-ideal avalanche, so even a 1-bit change in the state scatters across the whole output. */
uint32_t rc_random_next(rc_random *p)
{
    RC_ASSERT(p != NULL);
    uint32_t z = (p->state += 0x9E3779B9u);
    z = (z ^ (z >> 16)) * 0x21F0AAADu;
    z = (z ^ (z >> 15)) * 0x735A2D97u;
    return z ^ (z >> 15);
}
