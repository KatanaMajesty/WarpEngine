#pragma once

#include "common/attributes.h"
#include "common/breakpoint.h"
#include "common/cross_log_colorizer.h"
#include "common/print.h"

#include <format>
#include <print>
#include <source_location>
#include <string_view>

namespace Warp::common
{

template <typename... Args>
inline void AssertPrint(std::string_view payload, const std::format_string<Args...> fmt, Args&&... args)
{
    // TODO: Replace with logger
    PrintLn_Err("{}Assertion at {} failed: {}", ELogFgColor::Bright_Red, payload,
                std::format(fmt, std::forward<Args>(args)...));
}

inline void AssertPrint(std::string_view payload, std::string_view message)
{
    // TODO: Replace with logger
    PrintLn_Err("{}Assertion at {} failed: {}", ELogFgColor::Bright_Red, payload, message);
}

inline void AssertPrint(std::string_view payload)
{
    // TODO: Replace with logger
    PrintLn_Err("{}Assertion at {} failed", ELogFgColor::Bright_Red, payload);
}

} // namespace Warp::common

#if defined(WARP_DEBUG)
#define WARP_ASSERT(expr, ...)                                                                                \
    do                                                                                                        \
    {                                                                                                         \
        if (!(expr))                                                                                          \
        {                                                                                                     \
            ::Warp::common::AssertPrint(std::format("{}:{}", std::source_location::current().function_name(), \
                                                    std::source_location::current().line()),                  \
                                        ##__VA_ARGS__);                                                       \
            WARP_DEBUG_BREAK();                                                                               \
        }                                                                                                     \
    } while (false)
#else // !defined(WARP_DEBUG)
#define WARP_ASSERT(expr, ...)
#endif // defined(WARP_DEBUG)