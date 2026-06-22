#pragma once

#include "vulkan/nri_vk_device.h"

namespace Warp::nri
{

struct NriFrameGraphResource
{
    
};
struct NriFrameGraphPassNode
{

};
struct NriFrameGraphCreateInfo
{
};
class NriFrameGraph
{
public:
    NriFrameGraph(Arc<vk::NriDevice> device)
    {
    }
    NriFrameGraph(const NriFrameGraph&) = delete;
    NriFrameGraph& operator=(const NriFrameGraph&) = delete;

    /// @brief Initializes an empty frame graph
    /// @returns true if successfully initialized all device-related data, otherwise false
    bool Init(const NriFrameGraphCreateInfo& createInfo) noexcept;
};

}; // namespace Warp::nri