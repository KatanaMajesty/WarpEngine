#pragma once

#include "common/assert.h"
#include "common/attr_defs.h"
#include "common/cross_log.h"
#include "common/cross_log_colorizer.h"
#include "common/breakpoint.h"
#include "common/enum_flag_set.h"
#include "common/enum_utils.h"

// TODO: Change this to be under ./nebulae/vulkan/ !!!
#include "generated/nri_vk_stype_helper.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <string_view>

// https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
#define NRI_CPP __cplusplus
#if NRI_CPP < 202302L
#error "NRI requires C++23 or newer
#endif

namespace Warp::nri::vk
{

    template<typename T>
    constexpr T InitVulkanChainableStruct(void* pNext = nullptr) noexcept
    {
        return T{ .sType = GetVkSType<T>(), .pNext = pNext };
    }

    inline constexpr void CheckVulkanResult(VkResult result) noexcept
    {
        // https://registry.khronos.org/vulkan/specs/latest/man/html/VkResult.html
        // - All successful completion codes are non-negative values.
        // - All runtime error codes are negative values.
        if (EnumValue(result) < 0)
        {
            WARP_LOG_ERROR(Warp::ELoggerType::NriLogger, "CheckVulkanResult: '{}'", string_VkResult(result));
            WARP_DEBUG_BREAK();
        }
    }

    template<typename... Args>
    constexpr void CheckVulkanResult(VkResult result, std::format_string<Args...> fmt, Args&&... args) noexcept
    {
        if (EnumValue(result) < 0)
        {
            WARP_LOG_ERROR(Warp::ELoggerType::NriLogger, "CheckVulkanResult: '{}'. {}", string_VkResult(result), std::format(fmt, std::forward<Args>(args)...));
            WARP_DEBUG_BREAK();
        }
    }

    inline constexpr void CheckBooleanResult(bool result) noexcept
    {
        if (!result)
        {
            WARP_LOG_ERROR(Warp::ELoggerType::NriLogger, "CheckBooleanResult: false");
            WARP_DEBUG_BREAK();
        }
    }

    template<typename... Args>
    constexpr void CheckBooleanResult(bool result, std::format_string<Args...> fmt, Args&&... args) noexcept
    {
        if (!result)
        {
            WARP_LOG_ERROR(Warp::ELoggerType::NriLogger, "CheckBooleanResult: false. {}", std::format(fmt, std::forward<Args>(args)...));
            WARP_DEBUG_BREAK();
        }
    }

    template<typename Function>
    Function GetInstanceProcAddr(VkInstance instance, std::string_view name) noexcept
    {
        return reinterpret_cast<Function>(vkGetInstanceProcAddr(instance, name.data()));
    }


} // Warp::nri::vk namespace

#define NRI_VK_STRUCT(type, ...) Warp::nri::vk::InitVulkanChainableStruct<type>(__VA_ARGS__)
#define NRI_VK_CHECK_RESULT(result, ...) Warp::nri::vk::CheckVulkanResult(result, __VA_ARGS__)
#define NRI_VK_CHECK_BOOL(result, ...) Warp::nri::vk::CheckBooleanResult(result, __VA_ARGS__);
#define NRI_VK_INSTANCE_PROC_ADDR(function, instance) Warp::nri::vk::GetInstanceProcAddr<PFN_##function>(instance, #function);
