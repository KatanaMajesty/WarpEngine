#pragma once

#include <mutex>
#include "../../Core/Defines.h"
#include "stdafx.h"
#include "DeviceChild.h"

namespace Warp
{

	struct RHIDescriptorAllocation
	{
		D3D12_CPU_DESCRIPTOR_HANDLE	CpuHandle = { 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = { 0 };
		UINT NumDescriptors = 0;
		UINT DescriptorIncrementSize = 0;

		WARP_ATTR_NODISCARD D3D12_CPU_DESCRIPTOR_HANDLE GetCpuAddress(UINT index = 0) const;
		WARP_ATTR_NODISCARD D3D12_GPU_DESCRIPTOR_HANDLE GetGpuAddress(UINT index = 0) const;

		inline constexpr bool IsValid() const { return NumDescriptors > 0 && DescriptorIncrementSize > 0 && !IsNull(); }
		inline constexpr bool IsNull() const { return CpuHandle.ptr == 0; }
		inline constexpr bool IsShaderVisible() const { return GpuHandle.ptr != 0; }
		inline constexpr operator bool() const { return !IsNull(); }
	};

	// RHIDescriptorHeap represents a simple wrapper over ID3D12DescriptorHeap class
	// It acts somewhat simillar to LinearAllocator. Allocating a certain amount of descriptors makes it so
	// that you shift the available range to the right by numDescriptors * size; More details in Allocate() method
	class RHIDescriptorHeap : public RHIDeviceChild
	{
	public:
		RHIDescriptorHeap() = default;
		RHIDescriptorHeap(RHIDevice* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible);

		// We use mutex, thus no copies nor moves allowed
		RHIDescriptorHeap(const RHIDescriptorHeap&) = delete;
		RHIDescriptorHeap& operator=(const RHIDescriptorHeap&) = delete;

		// Allocate() method requests a "reservation" of memory for numDescriptors descriptors in the heap
		// If there is not enough of free memory in the heap, the null-allocation is returned (RHIDescriptorAllocation::IsNull() to check)
		// The allocation should be returned to the heap after it is no longer used. This will push the range into the m_availableRanges vector
		// It will then be used for other allocations.
		RHIDescriptorAllocation Allocate(UINT numDescriptors);

		// RHIDescriptorAllocation becomes invalid after the Free() method
		void Free(RHIDescriptorAllocation&& allocation);

		WARP_ATTR_NODISCARD inline ID3D12DescriptorHeap* GetD3D12Heap() const { return m_D3D12DescriptorHeap.Get(); }
		WARP_ATTR_NODISCARD inline constexpr D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return m_type; }
		WARP_ATTR_NODISCARD inline constexpr UINT GetDescriptorIncrementSize() const { return m_descriptorIncrementSize; }
		WARP_ATTR_NODISCARD inline constexpr bool IsShaderVisible() const { return m_shaderVisible; }

		void SetName(std::wstring_view name);

	private:
		bool IsValidAllocation(const RHIDescriptorAllocation& allocation);
		bool IsTypeShaderVisible(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

		ComPtr<ID3D12DescriptorHeap> m_D3D12DescriptorHeap;
		std::mutex m_allocationMutex;
		std::vector<RHIDescriptorAllocation> m_availableRanges;

		D3D12_DESCRIPTOR_HEAP_TYPE m_type;
		D3D12_CPU_DESCRIPTOR_HANDLE m_baseCpuDescriptorHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE m_baseGpuDescriptorHandle;
		UINT m_descriptorIncrementSize;
		UINT m_numDescriptors;
		bool m_shaderVisible;
	};

}