#include "Logger.h"

#include "spdlog/sinks/stdout_color_sinks.h"

namespace Warp::Log
{

    std::unordered_map<std::string_view, std::unique_ptr<Logger>> Logger::s_instanceMap = {};

    bool Logger::Create(std::string_view name) noexcept
    {
        if (Get(name) != nullptr)
        {
            // do not create again if already exists
            return true;
        }
    
        // allocate new logger
        s_instanceMap[name] = std::make_unique<Logger>(name);
        return true;
    }

    bool Logger::Delete(std::string_view name) noexcept
    {
        // erase returns number of elements removed (0 or 1).
        return s_instanceMap.erase(name) > 0;
    }

    Logger* Logger::Get(std::string_view name) noexcept
    {
        auto it = s_instanceMap.find(name);
        return it == s_instanceMap.end() ? nullptr : it->second.get();
    }

    Logger::Logger(std::string_view name) noexcept
    {
        try
        {
            m_handle = spdlog::stdout_color_mt(name.empty() ? MainLoggerName.data() : name.data());
            m_handle->set_pattern("%^[%H:%M:%S] [%n](%l): %v%$");

            if (name.empty())
            {
                // handle main logger. Recreate main instance basically
                spdlog::drop(MainLoggerName.data());
                spdlog::set_default_logger(m_handle);
            }
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            m_handle = nullptr;
            // do nothing
            // (IsInitialized == false) here
        }
    }

    void Logger::SetSeverity(ESeverity severity) noexcept
    {
        if (m_handle)
            m_handle->set_level(ConvertSeverity(severity));
    }

}