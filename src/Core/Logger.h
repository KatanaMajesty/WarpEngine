#pragma once

#include "spdlog/spdlog.h"

#include "Defines.h"

namespace Warp::Logging
{
    enum class Severity
    {
        WARP_SEVERITY_INFO,
        WARP_SEVERITY_WARNING,
        WARP_SEVERITY_ERROR,
        WARP_SEVERITY_FATAL,
    };

    void Init(Severity severity);
}

#ifdef WARP_DEBUG
#define WARP_LOG_INFO(message, ...) (spdlog::info(message, ##__VA_ARGS__))
#define WARP_LOG_WARN(message, ...) (spdlog::warn(message, ##__VA_ARGS__))
#define WARP_LOG_ERROR(message, ...) (spdlog::error(message, ##__VA_ARGS__))
#define WARP_LOG_FATAL(message, ...) (spdlog::critical(message, ##__VA_ARGS__))
#else

#if 1 // for release-crash debugging
#define WARP_LOG_INFO(message, ...)
#define WARP_LOG_WARN(message, ...)
#define WARP_LOG_ERROR(message, ...)
#define WARP_LOG_FATAL(message, ...)
#else
#define WARP_LOG_INFO(message, ...) (spdlog::info(message, ##__VA_ARGS__))
#define WARP_LOG_WARN(message, ...) (spdlog::warn(message, ##__VA_ARGS__))
#define WARP_LOG_ERROR(message, ...) (spdlog::error(message, ##__VA_ARGS__))
#define WARP_LOG_FATAL(message, ...) (spdlog::critical(message, ##__VA_ARGS__))
#endif // 1

#endif