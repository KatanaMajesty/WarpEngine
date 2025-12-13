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

} // Warp::nri namespace

#define NRI_SLANG_CHECK_RESULT(result, ...) Warp::nri::SlangCheckResult(result, __VA_ARGS__)