#include "Logger.h"

#include "spdlog/sinks/stdout_color_sinks.h"

namespace Warp::Logging
{

	constexpr spdlog::level::level_enum ConvertSeverity(Severity severity)
	{
		using namespace spdlog::level;
		switch (severity)
		{
		case Severity::WARP_SEVERITY_INFO: return level_enum::info;
		case Severity::WARP_SEVERITY_WARNING: return level_enum::warn;
		case Severity::WARP_SEVERITY_ERROR: return level_enum::err;
		case Severity::WARP_SEVERITY_FATAL: return level_enum::critical;
		WARP_ATTR_UNLIKELY default: return level_enum::off;
		}
	}

	void Init(Severity severity)
	{
		auto defaultLogger = spdlog::stdout_color_mt("WARP");
		defaultLogger->set_pattern("%^[%H:%M:%S] [%n](%l): %v%$");
		defaultLogger->set_level(ConvertSeverity(severity));
		spdlog::set_default_logger(defaultLogger);
	}

}