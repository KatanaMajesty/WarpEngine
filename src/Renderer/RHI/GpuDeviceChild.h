#pragma once

namespace Warp
{

	class GpuDevice;

	class GpuDeviceChild
	{
	public:
		GpuDeviceChild() = default;
		GpuDeviceChild(GpuDevice* device)
			: m_device(device)
		{
		}

		inline constexpr GpuDevice* GetDevice() const { return m_device; }

	private:
		GpuDevice* m_device;
	};

}