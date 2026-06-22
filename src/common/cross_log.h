#pragma once

#include "cross_log_colorizer.h"
#include "enum_utils.h"
#include "print.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Warp
{

// Cross logger is a part of common namespace
// It implements a robust logging tooling without any third-parties (thats the goal at least)

enum class ELogLevel
{
    Trace,
    Info,
    Warn,
    Error,
    Disabled, // This value should be max, as logging is determined based on largest set enum
              // value of log level
};

// A logging instance (can be created per-system)
//
// An idea is to have multiple simple loggers, one for a system. All of those would be stored
// somewhere in a light container A container would store logger objects inside of a linear
// array (that can be accessed using indices). Keeping track of indices - is application's
// responsibility
class Logger
{
public:
    Logger() = default;

    Logger(const Logger&) = default;
    Logger& operator=(const Logger&) = default;

    void Init(ELogLevel logLevel)
    {
        m_isInitialized = true;
        m_logTitle = "";
        m_logLevel = logLevel;
    }
    void Init(ELogLevel logLevel, std::string_view logTitle)
    {
        m_isInitialized = true;
        m_logTitle = logTitle;
        m_logLevel = logLevel;
    }

    template <ELogLevel Level, ELogFgColor ForegroundColor, ELogBgColor BackgroundColor, typename... Args>
    void Log(std::string_view levelPrefix, std::format_string<Args...> fmt, Args&&... args) const
    {
        // check if the logger is initialized using c-style assertion (not common/assert.h)
        assert(IsInitialized());

        if (EnumValue(Level) >= EnumValue(m_logLevel))
        {
            std::string logTitleFormatted = "";
            if (!m_logTitle.empty())
            {
                logTitleFormatted =
                    (BackgroundColor != ELogBgColor::Reset)
                        ? std::format("{}{}<{}> {}", ForegroundColor, ELogStyle::Italic, m_logTitle, ELogStyle::Reset)
                        : // If background color is present dont apply dim style, use foreground
                          // color
                        std::format("{}{}<{}> {}", ELogStyle::Dim, ELogStyle::Italic, m_logTitle, ELogStyle::Reset);
            }

            static constexpr bool IsError = (Level == ELogLevel::Error);
            if constexpr (IsError)
            {
                // Error
                common::PrintLn_Err("{}{}{}{}", BackgroundColor, logTitleFormatted, BackgroundColor,
                                    std::format("{}[{}]: {} {}", ForegroundColor, levelPrefix,
                                                std::format(fmt, std::forward<Args>(args)...), ELogStyle::Reset));
            }
            else
            {
                // Not an error
                common::PrintLn("{}{}{}{}", BackgroundColor, logTitleFormatted, BackgroundColor,
                                std::format("{}[{}]: {} {}", ForegroundColor, levelPrefix,
                                            std::format(fmt, std::forward<Args>(args)...), ELogStyle::Reset));
            }
        }
    }

    template <typename... Args>
    void Trace(std::format_string<Args...> fmt, Args&&... args) const noexcept
    {
        this->Log<ELogLevel::Trace, ELogFgColor::Bright_Black, ELogBgColor::Reset>("TRACE", fmt,
                                                                                   std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Info(std::format_string<Args...> fmt, Args&&... args) const noexcept
    {
        this->Log<ELogLevel::Info, ELogFgColor::Bright_Cyan, ELogBgColor::Reset>("INFO", fmt,
                                                                                 std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Warn(std::format_string<Args...> fmt, Args&&... args) const noexcept
    {
        this->Log<ELogLevel::Warn, ELogFgColor::Bright_Yellow, ELogBgColor::Reset>("WARN", fmt,
                                                                                   std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args) const noexcept
    {
        this->Log<ELogLevel::Error, ELogFgColor::White, ELogBgColor::Bright_Red>("ERROR", fmt,
                                                                                 std::forward<Args>(args)...);
    }

    // TODO: change this to be a proper log avoiding string copies
    inline void Trace(std::string_view msg) const noexcept { this->Trace("{}", msg); }
    inline void Info(std::string_view msg) const noexcept { this->Info("{}", msg); }
    inline void Warn(std::string_view msg) const noexcept { this->Warn("{}", msg); }
    inline void Error(std::string_view msg) const noexcept { this->Error("{}", msg); }

    inline constexpr bool IsInitialized() const { return m_isInitialized; }

private:
    bool m_isInitialized = false;
    std::string m_logTitle;
    ELogLevel m_logLevel = ELogLevel::Disabled;
};

enum ELoggerType
{
    LOGGER_DEFAULT,
    LOGGER_NRI,
    LOGGER_PLATFORM,
    LOGGER_UNIVERSE,
    NUM_LOGGERS, // this should be last and is reserved, used by crosslog::LoggerContainer
};

// N - number of loggers inside of a container
//
// Logger container is a singleton instance
//      Motivation is coming from having a properly instanced N amount of loggers with a
//      convenient way of retrieving them
template <typename E, size_t N = EnumValue(E::NumLoggers)>
    requires(std::is_enum_v<E>)
class LoggerContainer
{
private:
    LoggerContainer() = default;

public:
    LoggerContainer(const LoggerContainer&) = delete;
    LoggerContainer& operator=(const LoggerContainer&) = delete;

    static LoggerContainer* Get() noexcept
    {
        static LoggerContainer instance;
        return &instance;
    }

    inline constexpr Logger* GetLogger(E index) noexcept { return &m_loggerArray.at(EnumValue(index)); }
    inline constexpr const Logger* GetLogger(E index) const noexcept { return &m_loggerArray.at(EnumValue(index)); }

private:
    std::array<Logger, N> m_loggerArray;
};

using DefaultLoggerContainer = LoggerContainer<ELoggerType, EnumValue(ELoggerType::NUM_LOGGERS)>;

} // namespace Warp

#define WARP_INIT_LOGGER(LogType, ...) ::Warp::DefaultLoggerContainer::Get()->GetLogger(LogType)->Init(__VA_ARGS__)

#define WARP_LOG_TRACE(LogType, ...) ::Warp::DefaultLoggerContainer::Get()->GetLogger(LogType)->Trace(__VA_ARGS__)
#define WARP_LOG_INFO(LogType, ...) ::Warp::DefaultLoggerContainer::Get()->GetLogger(LogType)->Info(__VA_ARGS__)
#define WARP_LOG_WARN(LogType, ...) ::Warp::DefaultLoggerContainer::Get()->GetLogger(LogType)->Warn(__VA_ARGS__)
#define WARP_LOG_ERROR(LogType, ...) ::Warp::DefaultLoggerContainer::Get()->GetLogger(LogType)->Error(__VA_ARGS__)