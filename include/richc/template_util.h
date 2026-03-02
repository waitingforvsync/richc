/*
 * template_util.h - helpers shared by template headers.
 *
 * Include this from template headers, not directly from user code.
 */
#ifndef RC_TEMPLATE_UTIL_H_
#define RC_TEMPLATE_UTIL_H_

#include <stdint.h>

/* RC_CONCAT(a, b) pastes two tokens together, expanding macros first. */
#define RC_CONCAT_(a, b) a##b
#define RC_CONCAT(a, b)  RC_CONCAT_(a, b)

/* RC_STRINGIFY(x) converts a token to a string literal, expanding macros first. */
#define RC_STRINGIFY_(x) #x
#define RC_STRINGIFY(x)  RC_STRINGIFY_(x)

/* RC_INDEX_NONE: sentinel "not found" / "invalid" index value (== UINT32_MAX). */
#define RC_INDEX_NONE ((uint32_t)-1)

#endif /* RC_TEMPLATE_UTIL_H_ */
