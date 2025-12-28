#pragma once

#include "common/cross_log.h"
#include "common/breakpoint.h"

#include <slang/slang.h>

namespace Warp::nri
{

    inline constexpr void SlangCheckResult(SlangResult result) noexcept
    {
        if (SLANG_FAILED(result))
        {
            WARP_LOG_ERROR(Warp::ELoggerType::NriLogger, "SlangCheckResult: '{}'", result);
            WARP_DEBUG_BREAK();
        }
    }

    template<typename... Args>
    constexpr void SlangCheckResult(SlangResult result, std::format_string<Args...> fmt, Args&&... args) noexcept
    {
        if (SLANG_FAILED(result))
        {
            WARP_LOG_ERROR(Warp::ELoggerType::NriLogger, "SlangCheckResult: '{}'. {}", result, std::format(fmt, std::forward<Args>(args)...));
            WARP_DEBUG_BREAK();
        }
    }

    template<typename... Args> constexpr bool SlangCheckAndLogFailure(SlangResult result, std::format_string<Args...> fmt, Args&&... args) {
        if (SLANG_FAILED(result))
        {
            WARP_LOG_ERROR(Warp::ELoggerType::NriLogger, "SlangCheckAndLogFailure: '{}'. {}", result, std::format(fmt, std::forward<Args>(args)...));
            return true;
        }
        return false;
    }

} // Warp::nri namespace

#define NRI_SLANG_CHECK_RESULT(result, ...) Warp::nri::SlangCheckResult(result, __VA_ARGS__)

#define NRI_SLANG_RETURN_IF_FAILED(retVal, result, ...)                                                                                    \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (Warp::nri::SlangCheckAndLogFailure(result, __VA_ARGS__))                                                                       \
        {                                                                                                                                  \
            return retVal;                                                                                                                 \
        }                                                                                                                                  \
    } while (false)