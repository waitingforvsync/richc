#ifndef RICHC_BITSET_H_
#define RICHC_BITSET_H_

#include "richc/arena.h"


typedef struct bitset_t {
    uint64_t *bits;
    int32_t num;
    int32_t capacity;
} bitset_t;


bitset_t bitset_make(int32_t initial_capacity, arena_t *arena);
void bitset_init(bitset_t *bitset, int32_t initial_capacity, arena_t *arena);
void bitset_reserve(bitset_t *bitset, int32_t capacity, arena_t *arena);
void bitset_resize(bitset_t *bitset, int32_t size, arena_t *arena);
void bitset_for_each(const bitset_t *bitset, void (*fn)(void *, int32_t), void *ctx);

static inline void bitset_reset(bitset_t *bitset) {
    bitset->num = 0;
    bitset->bits[0] = 0;
}

static inline void bitset_set(bitset_t *bitset, int32_t index) {
    check(index >= 0 && index < bitset->num);
    bitset->bits[(uint32_t)index / 64] |= (1ULL << ((uint32_t)index & 63));
}

static inline void bitset_clear(bitset_t *bitset, int32_t index) {
    check(index >= 0 && index < bitset->num);
    bitset->bits[(uint32_t)index / 64] &= ~(1ULL << ((uint32_t)index & 63));
}

static inline bool bitset_is_set(const bitset_t *bitset, int32_t index) {
    check(index >= 0 && index < bitset->num);
    return (bitset->bits[(uint32_t)index / 64] & (1ULL << ((uint32_t)index & 63)));
}




#endif // ifndef RICHC_BITSET_H_
