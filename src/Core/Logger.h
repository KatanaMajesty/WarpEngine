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
#else
#define WARP_LOG_INFO(message, ...)
#endif