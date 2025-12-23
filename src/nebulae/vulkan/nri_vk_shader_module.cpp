#include "nri_vk_shader_module.h"

#include "common/assert.h"

namespace Warp::nri::vk
{

    NriShaderModule::NriShaderModule(VkDevice nativeDevice, const NriShaderModuleInfo& info)
        : m_nativeDevice(nativeDevice)
    {
        WARP_ASSERT(nativeDevice != VK_NULL_HANDLE, "Native device cannot be nullptr");

        auto createInfo = NRI_VK_STRUCT(VkShaderModuleCreateInfo);
        createInfo.flags = 0;
        createInfo.codeSize = info.spirvWords.size_bytes();
        createInfo.pCode = info.spirvWords.data();
        NRI_VK_CHECK_RESULT(vkCreateShaderModule(nativeDevice, &createInfo, nullptr, &m_nativeHandle), 
            "Failed to create native shader module");
    }

    NriShaderModule::~NriShaderModule()
    {
        vkDestroyShaderModule(m_nativeDevice, GetNativeHandle(), nullptr);
    }

} // Warp::nri::vk namespace