/*
 * bytes.h - byte containers: rc_view_bytes, rc_span_bytes, rc_array_bytes.
 *
 * A convenience instantiation of the array template (richc/template/array.h)
 * for uint8_t.  Include it once to get the generated types and their operations
 * (rc_array_bytes_make, rc_array_bytes_push, rc_span_bytes_get, ...).
 */
#ifndef RC_BYTES_H_
#define RC_BYTES_H_

#define RC_ARRAY_TYPE uint8_t
#define RC_ARRAY_NAME bytes
#include "richc/template/array.h"

#endif /* RC_BYTES_H_ */
