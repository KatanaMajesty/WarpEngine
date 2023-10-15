#pragma once

#include <vector>

#include "D3D12MA/D3D12MemAlloc.h"

#include "stdafx.h"
#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "GpuDeviceChild.h"

namespace Warp
{
	
	constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_UNKNOWN = static_cast<D3D12_RESOURCE_STATES>(-1);
	constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_INVALID = static_cast<D3D12_RESOURCE_STATES>(-2);

	// https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html#subresource-state-tracking
	class GpuResourceState
	{
	public:
		// The default-constructed GpuResourceState object should not be considered valid, as it tracks 0 subresources
		// Although, it is guaranteed to store D3D12_RESOURCE_STATE_UNKNOWN as a tracked state
		GpuResourceState() = default;
		GpuResourceState(UINT numSubresources, D3D12_RESOURCE_STATES initialState);

		// If subresourceIndex is D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, then all subresources will be updated
		void SetSubresourceState(UINT subresourceIndex, D3D12_RESOURCE_STATES state);
		WARP_ATTR_NODISCARD D3D12_RESOURCE_STATES GetSubresourceState(UINT subresourceIndex) const;

		inline constexpr bool IsValid() const { return m_resourceState != D3D12_RESOURCE_STATE_INVALID; }
		inline constexpr bool IsUnknown() const { return m_resourceState == D3D12_RESOURCE_STATE_UNKNOWN; }
		inline constexpr bool IsPerSubresource() const { return m_perSubresource; }
		inline constexpr UINT GetNumSubresources() const { return m_numSubresources; }

	private:
		// It is important to keep the default value of m_perSubresource as false due to the default constructor
		// We want a default-constructed object to behave as an unknown state object. For more detail see the default constructor
		bool m_perSubresource = false;

		D3D12_RESOURCE_STATES m_resourceState = D3D12_RESOURCE_STATE_INVALID;
		std::vector<D3D12_RESOURCE_STATES> m_subresourceStates;
		UINT m_numSubresources = 0;
	};

	class GpuResource : public GpuDeviceChild
	{
	public:
		GpuResource() = default;
		GpuResource(GpuDevice* device,
			D3D12MA::Allocator* allocator,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			const D3D12_RESOURCE_DESC& desc);

		GpuResource(const GpuResource&) = default;
		GpuResource& operator=(const GpuResource&) = default;

		GpuResource(GpuResource&&) = default;
		GpuResource& operator=(GpuResource&&) = default;

		inline D3D12MA::Allocation* GetAllocation() const { return m_allocation.Get(); }
		inline bool IsValid() const { return GetAllocation() != nullptr; }

		// Returns a virtual address of a resource in GPU memory
		WARP_ATTR_NODISCARD D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const;

		// Returns a D3D12 Resource
		// One must ensure that the GpuResource object is valid by calling IsValid() method before issuing this call
		WARP_ATTR_NODISCARD ID3D12Resource* GetD3D12Resource() const;

		WARP_ATTR_NODISCARD inline constexpr GpuResourceState& GetState() { return m_state; }
		WARP_ATTR_NODISCARD inline constexpr const GpuResourceState& GetState() const { return m_state; }
		WARP_ATTR_NODISCARD inline constexpr UINT GetNumSubresources() const { return m_numSubresources; }

	protected:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/subresources
		WARP_ATTR_NODISCARD UINT QueryNumSubresources();

		ComPtr<D3D12MA::Allocation> m_allocation;
		D3D12_RESOURCE_DESC m_desc{};
		GpuResourceState m_state;
		UINT m_numPlanes = 0;
		UINT m_numSubresources = 0;
	};

	class GpuBuffer final : public GpuResource
	{
	public:
		GpuBuffer() = default;
		GpuBuffer(GpuDevice* device,
			D3D12MA::Allocator* allocator,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			D3D12_RESOURCE_FLAGS flags,
			UINT strideInBytes,
			UINT64 sizeInBytes);

		GpuBuffer(const GpuBuffer&) = default;
		GpuBuffer& operator=(const GpuBuffer&) = default;

		GpuBuffer(GpuBuffer&&) = default;
		GpuBuffer& operator=(GpuBuffer&&) = default;

		inline constexpr UINT GetStrideInBytes() const { return m_strideInBytes; }
		inline constexpr UINT64 GetSizeInBytes() const { return m_sizeInBytes; }

		inline constexpr bool IsUavAllowed() const { return m_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; }
		inline constexpr bool IsSrvAllowed() const { return !(m_desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE); }

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView(DXGI_FORMAT format = DXGI_FORMAT_R32_UINT) const;

		// It may be nullptr, if the type of the HEAP where the buffer resides is not UPLOAD_HEAP
		template<typename T>
		WARP_ATTR_NODISCARD T* GetCpuVirtualAddress() const
		{
			WARP_ASSERT(m_cpuMapping && "CPU mapping is not supported for the resource");
			return reinterpret_cast<T*>(m_cpuMapping);
		}

	private:
		UINT m_strideInBytes = 0;
		UINT64 m_sizeInBytes = 0;
		void* m_cpuMapping = nullptr;
	};

	class GpuTexture final : public GpuResource
	{
	public:
		GpuTexture() = default;
		GpuTexture(GpuDevice* device,
			D3D12MA::Allocator* allocator,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			const D3D12_RESOURCE_DESC& desc);

		GpuTexture(const GpuTexture&) = default;
		GpuTexture& operator=(const GpuTexture&) = default;

		GpuTexture(GpuTexture&&) = default;
		GpuTexture& operator=(GpuTexture&&) = default;

		inline constexpr UINT GetWidth() const { return static_cast<UINT>(m_desc.Width); }
		inline constexpr UINT GetHeight() const { return m_desc.Height; }
		inline constexpr UINT GetNumMipLevels() const { return m_desc.MipLevels; }
		inline constexpr UINT GetDepthOrArraySize() const { return m_desc.DepthOrArraySize; }

		inline bool IsTexture1D() const { WARP_ASSERT(IsValid()); return m_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D; }
		inline bool IsTexture2D() const { WARP_ASSERT(IsValid()); return m_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D; }
		inline bool IsTexture3D() const { WARP_ASSERT(IsValid()); return m_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D; }
		bool IsViewableAsTextureCube() const;
		
		WARP_ATTR_NODISCARD UINT GetSubresourceIndex(UINT mipSlice, UINT arraySlice, UINT planeSlice) const;

	private:
		void QueryNumMipLevels();
	};

}