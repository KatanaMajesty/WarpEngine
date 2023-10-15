#pragma once

#include "stdafx.h"
#include "GpuDeviceChild.h"

namespace Warp
{

	class GpuDescriptorHeap : public GpuDeviceChild
	{
	public:
		GpuDescriptorHeap() = default;
		GpuDescriptorHeap(GpuDevice* device, D3D12_DESCRIPTOR_HEAP_TYPE type);
	};

}