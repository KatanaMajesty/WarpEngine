#pragma once

#include <vector>

#include "D3D12MA/D3D12MemAlloc.h"

#include "stdafx.h"
#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "DeviceChild.h"

namespace Warp
{

	// TODO: Maybe Remove?
	struct RHIBufferAddress
	{
		RHIBufferAddress() = default;
		RHIBufferAddress(UINT sizeInBytes, UINT offsetInBytes)
			: SizeInBytes(sizeInBytes)
			, OffsetInBytes(offsetInBytes)
		{
		}

		D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const { return bufferLocation + OffsetInBytes; }
		void* GetCpuAddress(void* mapping) const { return static_cast<uint8_t*>(mapping) + OffsetInBytes; }

		UINT SizeInBytes = 0;
		UINT OffsetInBytes = 0;
	};

	struct RHIVertexBufferView
	{
		RHIVertexBufferView() = default;
		RHIVertexBufferView(D3D12_GPU_VIRTUAL_ADDRESS bufferLocation, UINT sizeInBytes, UINT strideInBytes)
			: Handle(D3D12_VERTEX_BUFFER_VIEW{
					.BufferLocation = bufferLocation,
					.SizeInBytes = sizeInBytes,
					.StrideInBytes = strideInBytes
				})
		{
		}
		RHIVertexBufferView(D3D12_VERTEX_BUFFER_VIEW vbv)
			: Handle(vbv)
		{
		}

		operator D3D12_VERTEX_BUFFER_VIEW() const { return Handle; }

		D3D12_VERTEX_BUFFER_VIEW Handle = D3D12_VERTEX_BUFFER_VIEW();
	};

	struct RHIIndexBufferView
	{
		RHIIndexBufferView() = default;
		RHIIndexBufferView(D3D12_GPU_VIRTUAL_ADDRESS bufferLocation, UINT sizeInBytes, DXGI_FORMAT format)
			: Handle(D3D12_INDEX_BUFFER_VIEW{
					.BufferLocation = bufferLocation,
					.SizeInBytes = sizeInBytes,
					.Format = format
				})
		{
		}
		RHIIndexBufferView(D3D12_INDEX_BUFFER_VIEW ibv)
			: Handle(ibv)
		{
		}

		operator D3D12_INDEX_BUFFER_VIEW() const { return Handle; }

		D3D12_INDEX_BUFFER_VIEW Handle = D3D12_INDEX_BUFFER_VIEW();
	};

	constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_UNKNOWN = static_cast<D3D12_RESOURCE_STATES>(-1);
	constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_INVALID = static_cast<D3D12_RESOURCE_STATES>(-2);

	// https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html#subresource-state-tracking
	class CResourceState
	{
	public:
		// The default-constructed CResourceState object should not be considered valid, as it tracks 0 subresources
		// Although, it is guaranteed to store D3D12_RESOURCE_STATE_UNKNOWN as a tracked state
		CResourceState() = default;
		CResourceState(UINT numSubresources, D3D12_RESOURCE_STATES initialState);

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

	class RHIResource : public RHIDeviceChild
	{
	public:
		RHIResource() = default;
		RHIResource(RHIDevice* device,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			const D3D12_RESOURCE_DESC& desc,
			const D3D12_CLEAR_VALUE* optimizedClearValue);
		RHIResource(RHIDevice* device,
			ID3D12Resource* resource,
			D3D12_RESOURCE_STATES initialState);

		RHIResource(const RHIResource&) = default;
		RHIResource& operator=(const RHIResource&) = default;

		RHIResource(RHIResource&&) = default;
		RHIResource& operator=(RHIResource&&) = default;

		void RecreateInPlace(D3D12_RESOURCE_STATES initialState,
			const D3D12_RESOURCE_DESC& desc,
			const D3D12_CLEAR_VALUE* optimizedClearValue);

		inline ID3D12Resource* GetD3D12Resource() const { return m_D3D12Resource.Get(); }
		inline bool IsValid() const { return GetD3D12Resource() != nullptr; }

		// Returns a virtual address of a resource in GPU memory
		WARP_ATTR_NODISCARD D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const;

		WARP_ATTR_NODISCARD CResourceState& GetState() { return m_state; }
		WARP_ATTR_NODISCARD const CResourceState& GetState() const { return m_state; }
		WARP_ATTR_NODISCARD UINT GetNumSubresources() const { return m_numSubresources; }
		WARP_ATTR_NODISCARD const D3D12_RESOURCE_DESC& GetDesc() const { return m_desc; }

		// Subresource is implicitly promotable if and only if it is:
		// 1) In D3D12_RESOURCE_STATE_COMMON at the moment of calling
		// 2) If it is buffer or a non-depth SimultanousAccess texture
		// 3) If it is a non-SimultaneousAccess NON_PIXEL_SHADER_RESOURCE, PIXEL_SHADER_RESOURCE, COPY_DEST or COPY_SOURCE
		bool IsStateImplicitlyPromotableTo(D3D12_RESOURCE_STATES states, UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) const;

		// IMPORTANT NOTE:
		//	Decay does not occur between command lists executed together in the same ExecuteCommandLists call.
		// 
		// Subresource is implicitly decayable if and only if it is:
		// 1) Resource is being accessed on a copy queue
		// 2) Buffer resource on any queue type
		// 3) Texture resources on any queue type that have the D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS flag set
		// 4) Any resource implicitly promoted to a read-only state
		bool IsStateImplicitlyDecayableFrom(D3D12_RESOURCE_STATES states, D3D12_COMMAND_LIST_TYPE type, UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) const;

		void SetName(std::wstring_view name);

	protected:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/subresources
		WARP_ATTR_NODISCARD UINT QueryNumSubresources();

		ComPtr<ID3D12Resource> m_D3D12Resource;
		ComPtr<D3D12MA::Allocation> m_D3D12Allocation; // may be null, should always be checked!
		D3D12_RESOURCE_DESC m_desc{};
		UINT m_numPlanes = 0;
		UINT m_numSubresources = 0;
		CResourceState m_state;
	};

	class RHIBuffer final : public RHIResource
	{
	public:
		RHIBuffer() = default;
		RHIBuffer(RHIDevice* device,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			D3D12_RESOURCE_FLAGS flags,
			UINT64 sizeInBytes);

		RHIBuffer(const RHIBuffer&) = default;
		RHIBuffer& operator=(const RHIBuffer&) = default;

		RHIBuffer(RHIBuffer&&) = default;
		RHIBuffer& operator=(RHIBuffer&&) = default;

		//inline constexpr UINT GetStrideInBytes() const { return m_strideInBytes; }
		inline constexpr UINT64 GetSizeInBytes() const { return m_sizeInBytes; }

		inline constexpr bool IsUavAllowed() const { return m_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; }
		inline constexpr bool IsSrvAllowed() const { return !(m_desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE); }

		RHIVertexBufferView GetVertexBufferView(UINT strideInBytes) const;
		RHIIndexBufferView	GetIndexBufferView(DXGI_FORMAT format = DXGI_FORMAT_R32_UINT) const;

		// It may be nullptr, if the type of the HEAP where the buffer resides is not UPLOAD_HEAP
		template<typename T = void>
		WARP_ATTR_NODISCARD T* GetCpuVirtualAddress() const
		{
			WARP_ASSERT(m_cpuMapping && "CPU mapping is not supported for the resource");
			return reinterpret_cast<T*>(m_cpuMapping);
		}

	private:
		//UINT m_strideInBytes = 0;
		UINT64 m_sizeInBytes = 0;
		void* m_cpuMapping = nullptr;
	};

	class RHITexture final : public RHIResource
	{
	public:
		RHITexture() = default;
		RHITexture(RHIDevice* device,
			D3D12_HEAP_TYPE heapType,
			D3D12_RESOURCE_STATES initialState,
			const D3D12_RESOURCE_DESC& desc,
			const D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
		RHITexture(RHIDevice* device,
			ID3D12Resource* resource,
			D3D12_RESOURCE_STATES initialState);

		RHITexture(const RHITexture&) = default;
		RHITexture& operator=(const RHITexture&) = default;

		RHITexture(RHITexture&&) = default;
		RHITexture& operator=(RHITexture&&) = default;

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
		UINT GetNumMaxMipLevels();
	};

}