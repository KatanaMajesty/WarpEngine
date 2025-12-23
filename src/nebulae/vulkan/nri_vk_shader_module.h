#pragma once

#include "nri_vk_common.h"

#include "common/memory/arc.h"

#include <span>
#include <cstdint>

namespace Warp::nri::vk
{

    struct NriShaderModuleInfo
    {
        std::span<const uint32_t> spirvWords;
    };

    class NriShaderModule : public ArcMark<NriShaderModule>
    {
    public:
        /// @brief Constructs invalid NRI shader module
        NriShaderModule() = default;
        NriShaderModule(VkDevice nativeDevice, const NriShaderModuleInfo& info);

        NriShaderModule(const NriShaderModule&) = delete;
        NriShaderModule& operator=(const NriShaderModule&) = delete;

        ~NriShaderModule();

        inline constexpr VkShaderModule GetNativeHandle() const noexcept { return m_nativeHandle; }

    private:
        VkDevice m_nativeDevice = VK_NULL_HANDLE;
        VkShaderModule m_nativeHandle = VK_NULL_HANDLE;
    };

} // Warp::nri::vk namespace