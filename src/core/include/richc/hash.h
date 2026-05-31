/*
 * hash.h - hashing functions for richc types.
 *
 * All functions return uint32_t.
 *
 * Algorithms
 * ----------
 * Integer types   : Murmur3 32-bit finalizer (u32, i32, f32) or the splitmix64
 *                   finalizer XOR-folded to 32 bits (u64, i64, f64, ptr).
 * Byte sequences  : FNV-1a 32-bit (bytes, str).
 * Composition     : rc_hash_combine - mix a new hash into a running seed using
 *                   the Boost hash_combine formula.
 * Vector types    : each component hashed by its scalar hash, then folded
 *                   left-to-right with rc_hash_combine.
 *
 * Float edge cases
 * ----------------
 * -0.0 and +0.0 compare equal under ==, so they are normalised to the same bit
 * pattern before hashing.  Other special values (infinity, NaN) are hashed by
 * bit pattern.
 *
 * Composing hashes for a struct, field by field:
 *
 *   uint32_t h = rc_hash_i32(point.x);
 *   h = rc_hash_combine(h, rc_hash_i32(point.y));
 *
 * Hash functions for further types are added here as those types are ported.
 */

#ifndef RC_HASH_H_
#define RC_HASH_H_

#include <stdint.h>

/* The hashes take their argument by value, but a prototype only needs the type
 * declared, not complete - forward-declare the tags rather than pulling in the
 * defining headers (the definitions in hash.c include them). */
typedef struct rc_str rc_str;
typedef struct rc_vec2i rc_vec2i;
typedef struct rc_vec3i rc_vec3i;
typedef struct rc_vec2f rc_vec2f;
typedef struct rc_vec3f rc_vec3f;
typedef struct rc_vec4f rc_vec4f;

/* ---- integer types ---- */

uint32_t rc_hash_u32(uint32_t x);
uint32_t rc_hash_i32(int32_t x);
uint32_t rc_hash_u64(uint64_t x);
uint32_t rc_hash_i64(int64_t x);

/* ---- floating-point types ---- */

uint32_t rc_hash_f32(float x);
uint32_t rc_hash_f64(double x);

/* ---- pointer ---- */

uint32_t rc_hash_ptr(const void *p);

/* ---- byte sequences ---- */

uint32_t rc_hash_bytes(const void *data, uint32_t len);
uint32_t rc_hash_str(rc_str s);

/* ---- composition ---- */

uint32_t rc_hash_combine(uint32_t seed, uint32_t hash);

/* ---- integer vector types ---- */

uint32_t rc_hash_vec2i(rc_vec2i v);
uint32_t rc_hash_vec3i(rc_vec3i v);

/* ---- float vector types ---- */

uint32_t rc_hash_vec2f(rc_vec2f v);
uint32_t rc_hash_vec3f(rc_vec3f v);
uint32_t rc_hash_vec4f(rc_vec4f v);

#endif /* RC_HASH_H_ */
