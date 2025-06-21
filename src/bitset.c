#include "richc/bitset.h"
#include "richc/math/math.h"


bitset_t bitset_make(int32_t initial_capacity, arena_t *arena) {
    bitset_t bitset = {0};
    bitset_init(&bitset, initial_capacity, arena);
    return bitset;
}


void bitset_init(bitset_t *bitset, int32_t initial_capacity, arena_t *arena) {
    check(bitset);
    check(initial_capacity > 0);
    int32_t element_num = ((uint32_t)initial_capacity + 63) / 64;
    bitset->bits = arena_alloc(arena, element_num * ssizeof(*bitset->bits));
    bitset->num = 0;
    bitset->capacity = element_num * 64;
}


void bitset_reserve(bitset_t *bitset, int32_t capacity, arena_t *arena) {
    check(bitset);
    check(capacity > 0);
    if (capacity > bitset->capacity) {
        check(arena);
        int32_t element_num = ((uint32_t)capacity + 63) / 64;
        bitset->bits = arena_realloc(arena, bitset->bits, bitset->capacity / 8, element_num * 8);
        bitset->capacity = element_num * 64;
    }
}


void bitset_resize(bitset_t *bitset, int32_t size, arena_t *arena) {
    check(bitset);
    check(size > 0);
    if (size < bitset->num) {
        // If shrinking, clear the top unused bits of the final element
        bitset->bits[(uint32_t)size / 64] &= ~((1ULL << ((uint32_t)size & 63)) - 1);
    }
    else {
        // If growing, clear the newly reserved final elements
        bitset_reserve(bitset, size, arena);
        for (uint32_t i = ((uint32_t)bitset->num + 63) / 64, max_i = ((uint32_t)size + 63) / 64; i < max_i; ++i) {
            bitset->bits[i] = 0;
        }
    }
    bitset->num = size;
}


void bitset_for_each(const bitset_t *bitset, void (*fn)(void *, int32_t), void *ctx) {
    check(bitset);
    check(bitset->bits);
    for (uint32_t i = 0, max_i = ((uint32_t)bitset->num + 63) / 64; i < max_i; ++i) {
        uint64_t b = bitset->bits[i];
        while (b) {
            uint64_t t = b & (~b + 1ULL);
            fn(ctx, (int32_t)(i * 64 + 63 - llclz(t)));
            b ^= t;
        }
    }
}
