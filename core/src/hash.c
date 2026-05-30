#include "richc/hash.h"

#include "richc/math/vec4f.h"
#include "richc/ops.h"
#include "richc/str.h"

/* ---- integer types ---- */

// Murmur3 32-bit finalizer (good avalanche).
uint32_t rc_hash_u32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

uint32_t rc_hash_i32(int32_t x)
{
    return rc_hash_u32((uint32_t)x);
}

// splitmix64 finalizer, XOR-folded to 32 bits.
uint32_t rc_hash_u64(uint64_t x)
{
    x ^= x >> 33;
    x *= UINT64_C(0xff51afd7ed558ccd);
    x ^= x >> 33;
    x *= UINT64_C(0xc4ceb9fe1a85ec53);
    x ^= x >> 33;
    return (uint32_t)x ^ (uint32_t)(x >> 32);
}

uint32_t rc_hash_i64(int64_t x)
{
    return rc_hash_u64((uint64_t)x);
}

/* ---- floating-point types ---- */

uint32_t rc_hash_f32(float x)
{
    if (x == 0.0f) return rc_hash_u32(0u);   // normalise -0.0f and +0.0f
    return rc_hash_u32(rc_bitcast_f32(x));
}

uint32_t rc_hash_f64(double x)
{
    if (x == 0.0) return rc_hash_u64(0u);    // normalise -0.0 and +0.0
    return rc_hash_u64(rc_bitcast_f64(x));
}

/* ---- pointer ---- */

uint32_t rc_hash_ptr(const void *p)
{
    return rc_hash_u64((uint64_t)(uintptr_t)p);
}

/* ---- byte sequences ---- */

uint32_t rc_hash_bytes(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = 2166136261u;        // FNV-1a 32-bit offset basis
    for (uint32_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619u;              // FNV prime
    }
    return h;
}

uint32_t rc_hash_str(rc_str s)
{
    return rc_hash_bytes(s.data, s.len);
}

/* ---- composition ---- */

uint32_t rc_hash_combine(uint32_t seed, uint32_t hash)
{
    return seed ^ (hash + 0x9e3779b9u + (seed << 6) + (seed >> 2));
}

/* ---- integer vector types ---- */

uint32_t rc_hash_vec2i(rc_vec2i v)
{
    uint32_t h = rc_hash_i32(v.x);
    return rc_hash_combine(h, rc_hash_i32(v.y));
}

uint32_t rc_hash_vec3i(rc_vec3i v)
{
    uint32_t h = rc_hash_i32(v.x);
    h = rc_hash_combine(h, rc_hash_i32(v.y));
    return rc_hash_combine(h, rc_hash_i32(v.z));
}

/* ---- float vector types ---- */

uint32_t rc_hash_vec2f(rc_vec2f v)
{
    uint32_t h = rc_hash_f32(v.x);
    return rc_hash_combine(h, rc_hash_f32(v.y));
}

uint32_t rc_hash_vec3f(rc_vec3f v)
{
    uint32_t h = rc_hash_f32(v.x);
    h = rc_hash_combine(h, rc_hash_f32(v.y));
    return rc_hash_combine(h, rc_hash_f32(v.z));
}

uint32_t rc_hash_vec4f(rc_vec4f v)
{
    uint32_t h = rc_hash_f32(v.x);
    h = rc_hash_combine(h, rc_hash_f32(v.y));
    h = rc_hash_combine(h, rc_hash_f32(v.z));
    return rc_hash_combine(h, rc_hash_f32(v.w));
}
