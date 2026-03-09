/*
 * math/array_box2i.h - convenience include that generates rc_view_box2i,
 *                      rc_span_box2i, and rc_array_box2i.
 *
 * Include this header once to get:
 *   rc_view_box2i  { const rc_box2i *data; uint32_t num; }              read-only slice
 *   rc_span_box2i  {       rc_box2i *data; uint32_t num; }              mutable  slice
 *   rc_array_box2i {       rc_box2i *data; uint32_t num; uint32_t cap; } growable array
 */

#ifndef RC_MATH_ARRAY_BOX2I_H_
#define RC_MATH_ARRAY_BOX2I_H_

#include "richc/math/box2i.h"

#define ARRAY_T    rc_box2i
#define ARRAY_NAME rc_array_box2i
#define ARRAY_VIEW rc_view_box2i
#define ARRAY_SPAN rc_span_box2i
#include "richc/template/array.h"

#endif /* RC_MATH_ARRAY_BOX2I_H_ */
