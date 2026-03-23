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
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>

#include <string_view>

// https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
#define NRI_CPP __cplusplus
#if NRI_CPP < 202302L
#error "NRI requires C++23 or newer
#endif

namespace Warp::nri::vk
{

    static constexpr VkComponentMapping IdentityComponentMapping = VkComponentMapping{
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY
    };

    /// @brief Direct enum wrapper over VK_API_VERSION macros. Should be used for Vulkan API versioning instead of direct uint32_t
    enum class EApiVersion : uint32_t
    {
        Vk_1_0 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 0, 0),
        Vk_1_1 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 1, 0),
        Vk_1_2 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 2, 0),
        Vk_1_3 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 3, 0),
        Vk_1_4 = VK_MAKE_API_VERSION(0, /*major*/ 1, /*minor*/ 4, 0),
        MaxSupported = Vk_1_4, // Specifies the latest version
    };

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

#define NRI_MARK_OPTIONAL(x) x

// namespace Warp::nri::vk 
// {

//     inline void BeginCommandBufferDebugLabel(VkCommandBuffer cmdBuffer, std::string_view labelName, const glm::vec4& color)
//     {
//         auto labelInfo = NRI_VK_STRUCT(VkDebugUtilsLabelEXT);
//         labelInfo.pLabelName = labelName.data();
//         labelInfo.color[0] = color.r;
//         labelInfo.color[1] = color.g;
//         labelInfo.color[2] = color.b;
//         labelInfo.color[3] = color.a;
//         vkCmdBeginDebugUtilsLabelEXT(cmdBuffer, &labelInfo);
//     }

//     inline void EndCommandBufferDebugLabel(VkCommandBuffer cmdBuffer)
//     {

//         vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
//     }

// }; // Warp::nri::vk namespace

// #define NRI_VK_BEGIN_CMD_DEBUG_LABEL(cmdBuffer, labelName, color) Warp::nri::vk::BeginCommandBufferDebugLabel(cmdBuffer, labelName, color)
// #define NRI_VK_END_CMD_DEBUG_LABEL(cmdBuffer) Warp::nri::vk::EndCommandBufferDebugLabel(cmdBuffer)
