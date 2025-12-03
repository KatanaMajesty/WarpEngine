#pragma once

#if defined(_MSC_VER)
#define WARP_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#if defined(__has_builtin)
#if __has_builtin(__builtin_debugtrap)
#define WARP_DEBUG_BREAK() __builtin_debugtrap()
#elif __has_builtin(__builtin_trap)
#define WARP_DEBUG_BREAK() __builtin_trap()
#endif
#endif
#ifndef WARP_DEBUG_BREAK
#include <signal.h>
#define WARP_DEBUG_BREAK() raise(SIGTRAP)
#endif
#else
#include <signal.h>
#define WARP_DEBUG_BREAK() raise(SIGTRAP)
#endif

namespace Warp::common
{
} // Warp::common namespace