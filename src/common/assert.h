#pragma once

#include "common/attr_defs.h"
#include "common/breakpoint.h"
#include "common/print.h"
#include "common/cross_log_colorizer.h"

#include <string_view>
#include <source_location>
#include <print>
#include <format>

namespace Warp::common
{

    template<typename... Args>
    inline void AssertPrint(std::string_view payload, const std::format_string<Args...> fmt, Args&&... args)
    {
        // TODO: Replace with logger
        PrintLn_Err("{}Assertion at {} failed: {}", ELogFgColor::Bright_Red, payload, std::format(fmt, std::forward<Args>(args)...));
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

} // Warp::detail namespace

#if defined(WARP_ENGINE_DEBUG)
#define WARP_ASSERT(expr, ...)                                       \
    do                                                               \
    {                                                                \
        if (!(expr))                                                 \
        {                                                            \
            ::Warp::common::AssertPrint(                             \
                std::format("{}:{}",                                 \
                    std::source_location::current().function_name(), \
                    std::source_location::current().line()),         \
                ##__VA_ARGS__);                                      \
            WARP_DEBUG_BREAK();                                      \
        }                                                            \
        WARP_A_ASSUME(expr);                                         \
    } while (false)
#else // !defined(WARP_ENGINE_DEBUG)
#define WARP_ASSERT(expr, ...) WARP_A_ASSUME(expr)
#endif // defined(WARP_ENGINE_DEBUG)