#pragma once

/// All Warp Engine's attribute definitions must follow the 'WARP_<NAME>' naming convention
/// ref: https://en.cppreference.com/w/cpp/language/attributes.html

/// indicates that the use of the name or entity declared with this attribute is allowed,
/// but discouraged for some reason (MUST be explained with 'reason')
#define WARP_DEPRECATED(reason) [[deprecated(reason)]]

/// indicates that the fall through from the previous case label is intentional
/// and should not be diagnosed by a compiler that warns on fall-through
#define WARP_FALLTHROUGH [[fallthrough]]

/// suppresses compiler warnings on unused entities, if any
#define WARP_MAYBE_UNUSED [[maybe_unused]]

/// encourages the compiler to issue a warning if the return value is discarded.
/// In Warp Engine this will most definitely mark undefined/unintended behaviour (MUST be
/// explained with 'reason')
#define WARP_NODISCARD(reason) [[nodiscard(reason)]]

/// indicates that the compiler should optimize for the case where a path of
/// execution through a statement is more or less likely than any other path of execution
#define WARP_LIKELY [[likely]]
#define WARP_UNLIKELY [[unlikely]]

/// specifies that the expression will always evaluate to true at a given point
#define WARP_ASSUME(expr) [[assume(expr)]]

#define WARP_OPTIONAL(x) x
