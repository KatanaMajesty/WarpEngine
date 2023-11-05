#pragma once

#ifndef _WIN32
#error Windows platform is required
#endif				// Abort if not Windows

#if _MSC_VER < 1931
#error At least VS2022 version 17.1 is required
#endif

#ifdef _DEBUG
#define WARP_DEBUG
#endif

// If C++ standard is more or equal to C++20 (/std:c++20 or /std:c++latest)
#if _MSVC_LANG >= 202002L

// indicates that the compiler should optimize for the case where a path of execution through a statement is more or less likely than any other path of execution
#define WARP_ATTR_LIKELY [[likely]]
#define WARP_ATTR_UNLIKELY [[unlikely]]

// indicates that the fall through from the previous case label is intentional and should not be diagnosed by a compiler that warns on fall-through
#define WARP_ATTR_FALLTHROUGH [[fallthrough]]

// indicates that the use of the name or entity declared with this attribute is allowed, but discouraged for some reason
#define WARP_ATTR_NODISCARD [[nodiscard]]
#define WARP_DEPRECATED [[deprecated]]

// To suppress unused warnings
#define WARP_MAYBE_UNUSED [[maybe_unused]]

#else

#define WARP_ATTR_LIKELY
#define WARP_ATTR_UNLIKELY
#define WARP_ATTR_FALLTHROUGH
#define WARP_ATTR_NODISCARD
#define WARP_DEPRECATED
#define WARP_MAYBE_UNUSED

#endif

// Additions for stdafx.h mostly
#define WARP_STRINGIFY(x) #x