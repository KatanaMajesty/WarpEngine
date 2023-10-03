#pragma once

#include "Defines.h"

#ifdef WARP_DEBUG
#define WARP_ENABLE_ASSERTIONS
#endif

#ifdef WARP_ENABLE_ASSERTIONS

// Defines.h assures that the specified platform is Win32
// If its not then we don't even care
#include <filesystem>
#include "Logger.h"

#define WARP_INTERNAL_EXPAND(x) x
#define WARP_INTERNAL_DEBUGBREAK() __debugbreak();
#define WARP_INTERNAL_ASSERT_(expression) \
	{ if (!(expression)) \
	{ \
		WARP_LOG_FATAL("Assertion \"{}\" failed at {}:{}", #expression, std::filesystem::path(__FILE__).filename().string(), __LINE__); \
		WARP_INTERNAL_DEBUGBREAK(); \
	} }

#define WARP_INTERNAL_ASSERT_MSG_(expression, msg) \
	{ if (!(expression)) \
	{ \
		WARP_LOG_FATAL("Assertion \"{}\" failed at {}:{}. Error message: {}", #expression, std::filesystem::path(__FILE__).filename().string(), __LINE__, msg); \
		WARP_INTERNAL_DEBUGBREAK(); \
	} }

#define WARP_INTERNAL_GET_ASSERT_SPEC(UnusedArg1, UnusedArg2, macro, ...) macro
#define WARP_INTERNAL_GET_ASSERT(...) WARP_INTERNAL_EXPAND(WARP_INTERNAL_GET_ASSERT_SPEC(__VA_ARGS__, WARP_INTERNAL_ASSERT_MSG_, WARP_INTERNAL_ASSERT_))

// WARP_ASSERT assumes there are at least 1 argument, which is an expression
// There can also be more arguments, which will be treated as messages and format
#define WARP_ASSERT(...) WARP_INTERNAL_EXPAND(WARP_INTERNAL_GET_ASSERT(__VA_ARGS__)(__VA_ARGS__))

#else

#define WARP_ASSERT(...)

#endif

#define WARP_YIELD_NOIMPL() WARP_ASSERT(false, "No implementation!")