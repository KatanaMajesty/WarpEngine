#pragma once

#include "../../Core/Defines.h"

namespace Warp
{

	class GpuDevice;

	class GpuDeviceChild
	{
	public:
		GpuDeviceChild() = default;
		explicit GpuDeviceChild(GpuDevice* device)
			: m_device(device)
		{
		}

		// Returns a logical device, associated with the child
		WARP_ATTR_NODISCARD inline constexpr GpuDevice* GetDevice() const { return m_device; }

	protected:
		GpuDevice* m_device;
	};

}