/*
 * debug.h - expression-based assertion with debug-break on failure.
 *
 * RC_ASSERT(cond)
 *   In debug builds (NDEBUG not defined): if cond is false, triggers a
 *   debug break — trapping into an attached debugger, or crashing with a
 *   useful signal if none is attached.
 *
 *   Unlike <assert.h>, RC_ASSERT is an expression (void), not a
 *   statement, so it can appear as the left operand of the comma operator:
 *
 *     *(RC_ASSERT(i < v.num), v.data + i)  // bounds-checked lvalue
 *
 *   In release builds (NDEBUG defined): expands to ((void)(cond)); the
 *   condition is evaluated and discarded so that variables referenced only
 *   inside assertions do not cause "unused variable" warnings.
 *
 * RC_PANIC(cond)
 *   Always-active assertion: fires in both debug and release builds.
 *   Use for invariants whose violation is always an unrecoverable error
 *   regardless of build configuration — e.g. arena OOM, internal
 *   consistency failures.
 *
 *   Triggers __builtin_trap() (illegal instruction / SIGILL) rather than
 *   a debugbreak, so the process is terminated unconditionally.
 *
 * Platform detection
 * ------------------
 * MSVC        __debugbreak()           — software breakpoint instruction
 * Clang       __builtin_debugtrap()    — detected via __has_builtin
 * Other       __builtin_trap()         — illegal instruction / SIGILL
 */

#ifndef RC_DEBUG_H_
#define RC_DEBUG_H_

/* ---- debug break intrinsic ---- */

#if defined(_MSC_VER)
#  define RC_DEBUG_BREAK_() __debugbreak()
#elif defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
#  define RC_DEBUG_BREAK_() __builtin_debugtrap()
#else
#  define RC_DEBUG_BREAK_() __builtin_trap()
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
    ((cond) ? (void)0 : (__builtin_trap(), (void)0))

#endif /* RC_DEBUG_H_ */
