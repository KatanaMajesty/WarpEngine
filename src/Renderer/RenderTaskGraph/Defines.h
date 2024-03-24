#pragma once

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "../../Core/Logger.h"

#define WARP_RTG_ASSERT(...) WARP_ASSERT(__VA_ARGS__)
#define WARP_RTG_NOIMPL() WARP_YIELD_NOIMPL()

#define WARP_RTG_LOG_INFO(message, ...) WARP_LOG_INFO(message, __VA_ARGS__)
#define WARP_RTG_LOG_WARN(message, ...) WARP_LOG_WARN(message, __VA_ARGS__)
#define WARP_RTG_LOG_ERROR(message, ...) WARP_LOG_ERROR(message, __VA_ARGS__)
#define WARP_RTG_LOG_FATAL(message, ...) WARP_LOG_FATAL(message, __VA_ARGS__)