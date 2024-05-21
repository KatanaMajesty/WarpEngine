#pragma once

#include "spdlog/spdlog.h"

#include "../Core/Defines.h"

#include <cassert> // use cassert here instead of Warp's assert to avoid circular includes
#include <memory>
#include <unordered_map>

namespace Warp::Log
{
    enum class ESeverity
    {
        Info,
        Warn,
        Error,
        Fatal,
    };

    inline constexpr spdlog::level::level_enum ConvertSeverity(ESeverity severity) noexcept
    {
        using namespace spdlog::level;
        switch (severity)
        {
        case ESeverity::Info: return level_enum::info;
        case ESeverity::Warn: return level_enum::warn;
        case ESeverity::Error: return level_enum::err;
        case ESeverity::Fatal: return level_enum::critical;

        WARP_ATTR_UNLIKELY default: 
            return level_enum::off;
        }
    }
    
    class Logger
    {
    public:
        template<typename... Args>
        using FormatString_type = spdlog::format_string_t<Args...>;

        static constexpr std::string_view MainLoggerName = "WarpMain";

        // Creates a main Logger instance
        static bool Create(std::string_view name = MainLoggerName) noexcept;
        static bool Delete(std::string_view name = MainLoggerName) noexcept;

        static Logger* Get(std::string_view name = MainLoggerName) noexcept;

        Logger(std::string_view name) noexcept;

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        bool IsInitialized() const noexcept { return m_handle != nullptr; }

        // Not noexcept in spdlog although under the hood is still noexcept
        void SetSeverity(ESeverity severity) noexcept;

        template<typename T>
        void Log(ESeverity severity, const T& t) noexcept
        {
            assert(m_handle != nullptr && "Logger should be initialized before Log");
            m_handle->log(ConvertSeverity(severity), t);
        }
    
        template<typename... Args>
        void Log(ESeverity severity, FormatString_type<Args...> fmt, Args&&... args) noexcept
        {
            assert(m_handle != nullptr && "Logger should be initialized before Log");
            m_handle->log(ConvertSeverity(severity), fmt, std::forward<Args>(args)...);
        }

    private:
        // static instance map of all loggers (basically on top of spdlog's registry)
        // they can be allocated same as main instance of logger but with provided name as a parameter
        static std::unordered_map<std::string_view, std::unique_ptr<Logger>> s_instanceMap;

        std::shared_ptr<spdlog::logger> m_handle;
    };

}

#if defined(WARP_DEBUG) || 0 // for release-crash debugging
    #define WARP_LOG_INFO(msg, ...) (Warp::Log::Logger::Get()->Log(Warp::Log::ESeverity::Info, msg, ##__VA_ARGS__))
    #define WARP_LOG_WARN(msg, ...) (Warp::Log::Logger::Get()->Log(Warp::Log::ESeverity::Warn, msg, ##__VA_ARGS__))
    #define WARP_LOG_ERROR(msg, ...) (Warp::Log::Logger::Get()->Log(Warp::Log::ESeverity::Error, msg, ##__VA_ARGS__))
    #define WARP_LOG_FATAL(msg, ...) (Warp::Log::Logger::Get()->Log(Warp::Log::ESeverity::Fatal, msg, ##__VA_ARGS__))
#else
    #define WARP_LOG_INFO(msg, ...)
    #define WARP_LOG_WARN(msg, ...)
    #define WARP_LOG_ERROR(msg, ...)
    #define WARP_LOG_FATAL(msg, ...)
#endif