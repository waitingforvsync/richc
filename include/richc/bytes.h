/*
 * bytes.h - convenience include that generates rc_view_bytes, rc_span_bytes,
 *           and rc_array_bytes (a growable arena-backed byte array).
 *
 * Include this header once to get:
 *   rc_view_bytes  { const uint8_t *data; uint32_t num; }              read-only slice
 *   rc_span_bytes  {       uint8_t *data; uint32_t num; }              mutable  slice
 *   rc_array_bytes {       uint8_t *data; uint32_t num; uint32_t cap; } growable array
 *
 * Operation functions are named after the container (ARRAY_NAME):
 *   rc_array_bytes_push(&arr, byte, &a)
 *   rc_array_bytes_pop(&arr)
 *   rc_array_bytes_push_n(&arr, src, n, &a)
 *   etc.
 *
 * Conversion helpers:
 *   rc_span_bytes_as_view(span)        → rc_view_bytes
 *   rc_array_bytes_as_view(&arr)       → rc_view_bytes
 *   rc_array_bytes_as_span(&arr)       → rc_span_bytes
 */

#ifndef RC_BYTES_H_
#define RC_BYTES_H_

#include <stdint.h>

#define ARRAY_T    uint8_t
#define ARRAY_NAME rc_array_bytes
#define ARRAY_VIEW rc_view_bytes
#define ARRAY_SPAN rc_span_bytes
#include "richc/template/array.h"

#endif /* RC_BYTES_H_ */
