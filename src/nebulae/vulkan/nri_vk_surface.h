#pragma once

#include "nri_vk_common.h"
#include "nri_vk_instance.h"

#include "common/memory/arc.h"

namespace Warp::nri::vk
{

    /// @brief An enumeration of NRI-recognized surface types that can be created for presentation
    enum class ESurfaceType : uint16_t
    {
        /// For reference see VK_KHR_surface extension
        None,
        Win32,
    };
    
    struct NriSurfaceInfo
    {
        Arc<NriInstance> instance;

        /// Surface type to be used for presentation. If no presentation is needed, ESurfaceType::None must be specified.
        /// TODO: And @NOTE - currently in Warp Vulkan NRI implementation, surface is always required as headless rendering is not supported yet.
        /// So ESurfaceType::None is treated as invalid surface type for now.
        ESurfaceType type = ESurfaceType::None;

        /// Native handle to a platform-specific window. This is only needed if surfaceType is not ESurfaceType::None.
        void* nativeHandle = nullptr;
    };

    /// It is important to note that NriSurface is not atomically ref counted NRI handle!
    class NriSurface : public ArcMark<NriSurface>
    {
    public:
        NriSurface() = delete; // deleted default constructor, surface must be created with proper info
        NriSurface(const NriSurfaceInfo& surfaceInfo);

        NriSurface(const NriSurface&) = delete;
        NriSurface& operator=(const NriSurface&) = delete;

        ~NriSurface();

        inline constexpr VkSurfaceKHR GetNativeHandle() const noexcept { return m_nativeHandle; }

        /// @brief Based on the platform (specified as ESurfaceType provided during this NriSurface creation)
        /// this method will try to obtain current window's (width, height) using platform-specific handle
        VkExtent2D QueryCurrentWindowExtent() const noexcept;


    private:
        VkSurfaceKHR m_nativeHandle = VK_NULL_HANDLE;
        Arc<NriInstance> m_instance; /// instance that was used to create this surface
    };
    
} // Warp::nri::vk namespace