/*
 * macros.h - small preprocessor utilities and assertions shared across the
 * library.
 *
 * Token helpers
 * -------------
 *   RC_CONCAT(a, b)    paste two tokens together, expanding macros first
 *   RC_STRINGIFY(x)    convert a token to a string literal, expanding first
 *
 * Sentinels
 * ---------
 *   RC_INDEX_NONE      "not found" / "invalid" index value (== UINT32_MAX)
 *
 * Assertions
 * ----------
 *   RC_ASSERT(cond)    expression-form assert; in debug builds a false cond
 *                      triggers a debug break, in release builds (NDEBUG)
 *                      cond is evaluated and discarded.
 *   RC_PANIC(cond)     always-active assertion (debug and release); a false
 *                      cond triggers __builtin_trap(). Use for unrecoverable
 *                      invariants such as out-of-memory.
 *
 * RC_ASSERT is an expression (void), not a statement, so it can appear as the
 * left operand of the comma operator:
 *
 *   *(RC_ASSERT(i < v.num), v.data + i)  // bounds-checked lvalue
 *
 * Debug-break intrinsic by compiler:
 *   MSVC        __debugbreak()           - software breakpoint instruction
 *   Clang       __builtin_debugtrap()    - detected via __has_builtin
 *   Other       __builtin_trap()         - illegal instruction / SIGILL
 */
#ifndef RC_MACROS_H_
#define RC_MACROS_H_

#include <stdint.h>

/* ---- token helpers ---- */

/* RC_CONCAT(a, b) pastes two tokens together, expanding macros first. */
#define RC_CONCAT_(a, b) a##b
#define RC_CONCAT(a, b)  RC_CONCAT_(a, b)

/* RC_STRINGIFY(x) converts a token to a string literal, expanding macros first. */
#define RC_STRINGIFY_(x) #x
#define RC_STRINGIFY(x)  RC_STRINGIFY_(x)

/* ---- sentinels ---- */

/* RC_INDEX_NONE: sentinel "not found" / "invalid" index value (== UINT32_MAX). */
#define RC_INDEX_NONE ((uint32_t)-1)

/* ---- debug break intrinsic (RC_ASSERT: pauses in debugger) ---- */

#if defined(_MSC_VER)
#  define RC_DEBUG_BREAK_() __debugbreak()
#elif defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
#  define RC_DEBUG_BREAK_() __builtin_debugtrap()
#else
#  define RC_DEBUG_BREAK_() __builtin_trap()
#endif

/* ---- unconditional trap intrinsic (RC_PANIC: always terminates) ---- */

#if defined(_MSC_VER)
#  define RC_TRAP_() __debugbreak()
#else
#  define RC_TRAP_() __builtin_trap()
#endif

/* ---- RC_ASSERT ---- */

#ifndef NDEBUG
#  define RC_ASSERT(cond) \
    ((cond) ? (void)0 : (RC_DEBUG_BREAK_(), (void)0))
#else
#  define RC_ASSERT(cond) ((void)(cond))
#endif

/* ---- RC_PANIC ---- */

#define RC_PANIC(cond) \
    ((cond) ? (void)0 : (RC_TRAP_(), (void)0))

#endif /* RC_MACROS_H_ */
