/*
 * hash.h - hashing functions for richc types.
 *
 * All functions are static inline and return uint32_t, matching the hash
 * type expected by the hash_map and hash_set templates.
 *
 * Algorithms
 * ----------
 * Integer types   : Murmur3 32-bit finalizer (u32, i32, f32) or splitmix64
 *                   finalizer XOR-folded to 32 bits (u64, i64, f64, ptr).
 * Byte sequences  : FNV-1a 32-bit (bytes, str).  Simple and effective for
 *                   the short keys typical in hash tables.
 * Composition     : rc_hash_combine — mix a new hash into a running seed,
 *                   using the Boost hash_combine formula.
 *
 * Float edge cases
 * ----------------
 * -0.0f and +0.0f compare equal under ==, so they are normalised to the
 * same bit pattern before hashing.  All other special values (infinity,
 * NaN) are hashed by bit pattern; NaN keys are unsupported in hash tables
 * that use == for equality anyway, since NaN != NaN.
 *
 * Composing hashes for structs
 * ----------------------------
 *   uint32_t h = rc_hash_i32(point.x);
 *   h = rc_hash_combine(h, rc_hash_i32(point.y));
 *
 * Integrating with hash_map / hash_set templates
 * -----------------------------------------------
 *   #define MAP_KEY_T   rc_str
 *   #define MAP_HASH(k) rc_hash_str(k)
 *   #define MAP_EQUAL(a,b) rc_str_is_equal(a, b)
 *   #include "richc/template/hash_map.h"
 */

#ifndef RC_HASH_H_
#define RC_HASH_H_

#include <stdint.h>
#include <string.h>
#include "richc/str.h"
#include "richc/math/rational.h"
#include "richc/math/vec2i.h"
#include "richc/math/vec3i.h"
#include "richc/math/vec2f.h"
#include "richc/math/vec3f.h"
#include "richc/math/vec4f.h"

/* ---- integer types ---- */

/*
 * Hash a uint32_t.  Uses the Murmur3 32-bit finalizer, which has excellent
 * avalanche properties and passes SMHasher.
 */
static inline uint32_t rc_hash_u32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

/* Hash an int32_t. */
static inline uint32_t rc_hash_i32(int32_t x)
{
    return rc_hash_u32((uint32_t)x);
}

/*
 * Hash a uint64_t to uint32_t.  Uses the splitmix64 finalizer for thorough
 * mixing of all 64 input bits, then XOR-folds the result to 32 bits.
 */
static inline uint32_t rc_hash_u64(uint64_t x)
{
    x ^= x >> 33;
    x *= UINT64_C(0xff51afd7ed558ccd);
    x ^= x >> 33;
    x *= UINT64_C(0xc4ceb9fe1a85ec53);
    x ^= x >> 33;
    return (uint32_t)x ^ (uint32_t)(x >> 32);
}

/* Hash an int64_t. */
static inline uint32_t rc_hash_i64(int64_t x)
{
    return rc_hash_u64((uint64_t)x);
}

/* ---- floating-point types ---- */

/*
 * Hash a float by bit pattern.  -0.0f and +0.0f are normalised to 0
 * before hashing so that equal values always produce equal hashes.
 */
static inline uint32_t rc_hash_f32(float x)
{
    if (x == 0.0f) return rc_hash_u32(0u);
    uint32_t bits;
    memcpy(&bits, &x, sizeof bits);
    return rc_hash_u32(bits);
}

/*
 * Hash a double by bit pattern.  -0.0 and +0.0 are normalised to 0
 * before hashing so that equal values always produce equal hashes.
 */
static inline uint32_t rc_hash_f64(double x)
{
    if (x == 0.0) return rc_hash_u64(0u);
    uint64_t bits;
    memcpy(&bits, &x, sizeof bits);
    return rc_hash_u64(bits);
}

/* ---- pointer ---- */

/* Hash a pointer value (not the pointed-to data). */
static inline uint32_t rc_hash_ptr(const void *p)
{
    return rc_hash_u64((uint64_t)(uintptr_t)p);
}

/* ---- byte sequences ---- */

/*
 * Hash an arbitrary byte sequence using FNV-1a 32-bit.
 * data may be NULL when len is 0.
 */
static inline uint32_t rc_hash_bytes(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = 2166136261u;   /* FNV-1a 32-bit offset basis */
    for (uint32_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619u;          /* FNV prime */
    }
    return h;
}

/*
 * Hash an rc_str.  An invalid (NULL) rc_str hashes to the FNV-1a basis
 * value (same as an empty string with data==NULL, len==0).
 */
static inline uint32_t rc_hash_str(rc_str s)
{
    return rc_hash_bytes(s.data, s.len);
}

/* ---- composition ---- */

/*
 * Mix a new hash value into a running seed.
 *
 * Based on the Boost hash_combine formula.  Use this to build a combined
 * hash for a struct field by field:
 *
 *   uint32_t h = rc_hash_i32(point.x);
 *   h = rc_hash_combine(h, rc_hash_i32(point.y));
 */
static inline uint32_t rc_hash_combine(uint32_t seed, uint32_t hash)
{
    return seed ^ (hash + 0x9e3779b9u + (seed << 6) + (seed >> 2));
}

/* ---- rational ---- */

/*
 * Hash an rc_rational.  Because rationals are always in canonical form,
 * equal values have identical num and denom fields, so combining the two
 * field hashes is both correct and consistent with rc_rational_is_equal.
 * Asserts that r is valid (denom > 0).
 */
static inline uint32_t rc_hash_rational(rc_rational r)
{
    RC_ASSERT(rc_rational_is_valid(r));
    uint32_t h = rc_hash_i64(r.num);
    return rc_hash_combine(h, rc_hash_i64(r.denom));
}

/* ---- integer vector types ---- */

static inline uint32_t rc_hash_vec2i(rc_vec2i v)
{
    uint32_t h = rc_hash_i32(v.x);
    return rc_hash_combine(h, rc_hash_i32(v.y));
}

static inline uint32_t rc_hash_vec3i(rc_vec3i v)
{
    uint32_t h = rc_hash_i32(v.x);
    h = rc_hash_combine(h, rc_hash_i32(v.y));
    return rc_hash_combine(h, rc_hash_i32(v.z));
}

/* ---- float vector types ---- */

static inline uint32_t rc_hash_vec2f(rc_vec2f v)
{
    uint32_t h = rc_hash_f32(v.x);
    return rc_hash_combine(h, rc_hash_f32(v.y));
}

static inline uint32_t rc_hash_vec3f(rc_vec3f v)
{
    uint32_t h = rc_hash_f32(v.x);
    h = rc_hash_combine(h, rc_hash_f32(v.y));
    return rc_hash_combine(h, rc_hash_f32(v.z));
}

static inline uint32_t rc_hash_vec4f(rc_vec4f v)
{
    uint32_t h = rc_hash_f32(v.x);
    h = rc_hash_combine(h, rc_hash_f32(v.y));
    h = rc_hash_combine(h, rc_hash_f32(v.z));
    return rc_hash_combine(h, rc_hash_f32(v.w));
}

#endif /* RC_HASH_H_ */
