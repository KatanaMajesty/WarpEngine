#pragma once

#include "D3D12MA/D3D12MemAlloc.h"

#include "../stdafx.h"
#include "../../Core/Defines.h"
#include "../../Core/Assert.h"

namespace Warp
{

	class GpuResource
	{
	public:
		GpuResource() = default;
		GpuResource(ID3D12Device* device,
			D3D12MA::Allocator* allocator,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			const D3D12_RESOURCE_DESC& desc);

		inline D3D12MA::Allocation* GetAllocation() const { return m_allocation.Get(); }
		inline bool IsValid() const { return GetAllocation() != nullptr; }

		// Returns a D3D12 Resource
		// One must ensure that the GpuResource object is valid by calling IsValid() method before issuing this call
		WARP_ATTR_NODISCARD ID3D12Resource* GetD3D12Resource() const;

	protected:
		ComPtr<D3D12MA::Allocation> m_allocation;
		D3D12_RESOURCE_DESC m_desc{};
	};

	class GpuBuffer final : public GpuResource
	{
	public:
		GpuBuffer() = default;
		GpuBuffer(ID3D12Device* device,
			D3D12MA::Allocator* allocator,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			D3D12_RESOURCE_FLAGS flags,
			UINT strideInBytes,
			UINT64 sizeInBytes);

		inline constexpr UINT GetStrideInBytes() const { return m_strideInBytes; }
		inline constexpr UINT64 GetSizeInBytes() const { return m_sizeInBytes; }

		inline constexpr bool IsUavAllowed() const { return m_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; }
		inline constexpr bool IsSrvAllowed() const { return !(m_desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE); }

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView(DXGI_FORMAT format = DXGI_FORMAT_R32_UINT) const;

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
		GpuTexture(ID3D12Device* device,
			D3D12MA::Allocator* allocator,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			const D3D12_RESOURCE_DESC& desc);

		inline constexpr UINT GetNumSubresources() const { return m_numSubresources; }
		inline constexpr UINT GetWidth() const { return static_cast<UINT>(m_desc.Width); }
		inline constexpr UINT GetHeight() const { return m_desc.Height; }
		inline constexpr UINT GetNumMipLevels() const { return m_desc.MipLevels; }
		inline constexpr UINT GetDepthOrArraySize() const { return m_desc.DepthOrArraySize; }
		
		UINT GetSubresourceIndex(UINT mipSlice, UINT arraySlice, UINT planeSlice) const;

	private:
		void QueryNumSubresources();
		void QueryNumMipLevels();

		UINT m_numSubresources = 0;
	};

}