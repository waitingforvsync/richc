/*
 * template_util.h - helpers shared by template headers.
 *
 * Include this from template headers, not directly from user code.
 */
#ifndef RC_TEMPLATE_UTIL_H_
#define RC_TEMPLATE_UTIL_H_

/* RC_CONCAT(a, b) pastes two tokens together, expanding macros first. */
#define RC_CONCAT_(a, b) a##b
#define RC_CONCAT(a, b)  RC_CONCAT_(a, b)

/* RC_STRINGIFY(x) converts a token to a string literal, expanding macros first. */
#define RC_STRINGIFY_(x) #x
#define RC_STRINGIFY(x)  RC_STRINGIFY_(x)

#endif /* RC_TEMPLATE_UTIL_H_ */
