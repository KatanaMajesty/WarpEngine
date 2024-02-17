#include "Resource.h"

#include <cmath>
#include "Device.h"

#include "../../Core/Logger.h"

namespace Warp
{

	CResourceState::CResourceState(UINT numSubresources, D3D12_RESOURCE_STATES initialState)
		: m_perSubresource(numSubresources > 1) // If there are more than 1 subresource, then resource state is perSubresource
		, m_resourceState(initialState)
		, m_subresourceStates(numSubresources, initialState)
		, m_numSubresources(numSubresources)
	{
	}

	void CResourceState::SetSubresourceState(UINT subresourceIndex, D3D12_RESOURCE_STATES state)
	{
		// If setting all subresources, or the resource only has a single subresource, set the per-resource state
		if (subresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES || m_subresourceStates.size() == 1)
		{
			m_perSubresource = false;
			m_resourceState = state;
		}
		else
		{
			// If we previously tracked resource per resource level, we need to update all subresource states before proceeding
			// we need that because before switching to perSubresource we were using m_resourceState, but not m_subresourceStates array
			// thus it needs to be updated with the relevant m_resourceState value
			if (!IsPerSubresource())
			{
				m_perSubresource = true;
				for (D3D12_RESOURCE_STATES& subresourceState : m_subresourceStates)
				{
					subresourceState = m_resourceState;
				}
			}
			m_subresourceStates[subresourceIndex] = state;
		}
	}

	WARP_ATTR_NODISCARD D3D12_RESOURCE_STATES CResourceState::GetSubresourceState(UINT subresourceIndex) const
	{
		if (IsPerSubresource())
		{
			WARP_ASSERT(subresourceIndex < GetNumSubresources(), "Out of bounds subresource index (apparently ALL_SUBRESOURCES?)");
			return m_subresourceStates[subresourceIndex];
		}
		return m_resourceState;
	}

	//inline constexpr bool RHIResourceState::IsValid() const
	//{
	//	if (IsPerSubresource())
	//	{
	//		return m_numSubresources == static_cast<UINT>(m_subresourceStates.size());
	//	}
	//	return m_numSubresources > 0; // Actually, only 1 is valid, but whatever
	//}

	RHIResource::RHIResource(RHIDevice* device,
		D3D12_HEAP_TYPE heapType,
		D3D12_RESOURCE_STATES initialState,
		const D3D12_RESOURCE_DESC& desc, 
		const D3D12_CLEAR_VALUE* optimizedClearValue)
		: RHIDeviceChild(device)
		, m_desc(desc)
		, m_numPlanes(D3D12GetFormatPlaneCount(device->GetD3D12Device(), desc.Format))
		, m_numSubresources(QueryNumSubresources())
		, m_state(m_numSubresources, initialState)
	{
		D3D12MA::ALLOCATION_DESC allocDesc{
			.Flags = D3D12MA::ALLOCATION_FLAG_NONE,
			.HeapType = heapType
		};

		WARP_RHI_VALIDATE(device->GetResourceAllocator()->CreateResource(&allocDesc,
			&desc,
			initialState,
			optimizedClearValue,
			m_D3D12Allocation.GetAddressOf(),
			IID_PPV_ARGS(m_D3D12Resource.ReleaseAndGetAddressOf())
		));
	}

	RHIResource::RHIResource(RHIDevice* device, ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState)
		: RHIDeviceChild(device)
		, m_D3D12Resource(resource)
		, m_desc(resource->GetDesc())
		, m_numPlanes(D3D12GetFormatPlaneCount(device->GetD3D12Device(), m_desc.Format))
		, m_numSubresources(QueryNumSubresources())
		, m_state(m_numSubresources, initialState)
	{
	}

	WARP_ATTR_NODISCARD D3D12_GPU_VIRTUAL_ADDRESS RHIResource::GetGpuVirtualAddress() const
	{
		WARP_ASSERT(IsValid());
		return GetD3D12Resource()->GetGPUVirtualAddress();
	}

	bool RHIResource::IsStateImplicitlyPromotableTo(D3D12_RESOURCE_STATES states, UINT subresourceIndex) const
	{
		// For more info, we chill here 
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#common-state-promotion

		WARP_ASSERT(IsValid());

		// 1) In D3D12_RESOURCE_STATE_COMMON at the moment of calling
		if (GetState().GetSubresourceState(subresourceIndex) != D3D12_RESOURCE_STATE_COMMON)
		{
			return false;
		}

		// 2) If it is buffer or a non-depth SimultanousAccess texture
		if (GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			return true;
		}
		else // Textures
		{
			if (GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)
			{
				static constexpr D3D12_RESOURCE_STATES depthStates = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE;
				return !(states & depthStates);
			}
			else // 3) If it is a non-SimultaneousAccess NON_PIXEL_SHADER_RESOURCE, PIXEL_SHADER_RESOURCE, COPY_DEST or COPY_SOURCE
			{
				static constexpr D3D12_RESOURCE_STATES implicitStates =
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
					D3D12_RESOURCE_STATE_COPY_DEST |
					D3D12_RESOURCE_STATE_COPY_SOURCE;

				// Ensure that we have promotable state flags AND we do not have any other non-promotable flags
				return (states & implicitStates) && !(states & ~implicitStates);
			}
		}
	}

	bool RHIResource::IsStateImplicitlyDecayableFrom(D3D12_RESOURCE_STATES states, D3D12_COMMAND_LIST_TYPE type, UINT subresourceIndex) const
	{
		// For more info, we chill here 
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#state-decay-to-common

		WARP_ASSERT(IsValid());

		// 1) Resource is being accessed on a copy queue
		if (type == D3D12_COMMAND_LIST_TYPE_COPY)
		{
			return true;
		}

		// 2) Buffer resource on any queue type
		if (GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			return true;
		}
		// 3) Texture resources on any queue type that have the D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS flag set
		else if (GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)
		{
			return true;
		}
		else // 4) Any resource implicitly promoted to a read-only state
		{
			// Now we are left with the READ states that are non-buffer and are only implicitly promotable in 
			// non-SimultaneousAccess textures. Those are:
			static constexpr D3D12_RESOURCE_STATES readOnlyNonSimultaneousAccessStates =
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
				D3D12_RESOURCE_STATE_COPY_SOURCE;

			// Ensure that we have read-only decayable flags AND we do not have any other non-decayable flags
			return ((states & readOnlyNonSimultaneousAccessStates) && !(states & ~readOnlyNonSimultaneousAccessStates));
		}
	}

	void RHIResource::SetName(std::wstring_view name)
	{
		WARP_ASSERT(IsValid());
		WARP_SET_RHI_NAME(m_D3D12Resource.Get(), name);
	}

	UINT RHIResource::QueryNumSubresources()
	{
		if (m_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			WARP_ASSERT(m_desc.MipLevels == 1);
			WARP_ASSERT(m_desc.DepthOrArraySize == 1);
			// If a resource contains a buffer, then it simply contains one subresource with an index of 0.
			return 1;
		}
		else
		{
			return m_desc.MipLevels * m_desc.DepthOrArraySize * m_numPlanes;
		}
	}

	RHIBuffer::RHIBuffer(
		RHIDevice* device,
		D3D12_HEAP_TYPE heapType, 
		D3D12_RESOURCE_STATES initialState,
		D3D12_RESOURCE_FLAGS flags,
		UINT64 sizeInBytes)
		: RHIResource(device,
			heapType,
			initialState,
			D3D12_RESOURCE_DESC{
				.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
				.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
				.Width = sizeInBytes,
				.Height = 1,
				.DepthOrArraySize = 1,
				.MipLevels = 1,
				.Format = DXGI_FORMAT_UNKNOWN,
				.SampleDesc = DXGI_SAMPLE_DESC {.Count = 1, .Quality = 0},
				// buffer memory layouts are understood by applications and row-major texture data is commonly marshaled through buffers
				.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
				.Flags = flags,
			}, nullptr)
		//, m_strideInBytes(strideInBytes)
		, m_sizeInBytes(sizeInBytes)
	{
		// TODO: Add HEAP_TYPE_READBACK aswell
		if (heapType == D3D12_HEAP_TYPE_UPLOAD)
		{
			WARP_MAYBE_UNUSED HRESULT hr = GetD3D12Resource()->Map(0, nullptr, &m_cpuMapping);
			if (FAILED(hr))
			{
				m_cpuMapping = nullptr;
				WARP_LOG_WARN("Failed to map resource to CPU");
			}
		}
	}

	RHIVertexBufferView RHIBuffer::GetVertexBufferView(UINT strideInBytes) const
	{
		WARP_ASSERT(IsValid() && IsSrvAllowed(), 
			"The resource is either invalid or is not allowed to be represented as shader resource");

		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = GetD3D12Resource()->GetGPUVirtualAddress();
		vbv.SizeInBytes = static_cast<UINT>(GetSizeInBytes());
		vbv.StrideInBytes = strideInBytes;
		return vbv;
	}

	RHIIndexBufferView RHIBuffer::GetIndexBufferView(DXGI_FORMAT format) const
	{
		WARP_ASSERT(IsValid() && IsSrvAllowed(),
			"The resource is either invalid or is not allowed to be represented as shader resource");

		// TODO: Add check if m_strideInBytes is equal to DXGI_FORMAT's stride

		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = GetD3D12Resource()->GetGPUVirtualAddress();
		ibv.Format = format;
		ibv.SizeInBytes = static_cast<UINT>(GetSizeInBytes());
		return ibv;
	}

	RHITexture::RHITexture(RHIDevice* device,
		D3D12_HEAP_TYPE heapType, 
		D3D12_RESOURCE_STATES initialState, 
		const D3D12_RESOURCE_DESC& desc, 
		const D3D12_CLEAR_VALUE* optimizedClearValue)
		: RHIResource(device,
			heapType,
			initialState,
			desc,
			optimizedClearValue)
	{
		QueryNumMipLevels();

#ifdef WARP_DEBUG
		// TODO: Perform healthy-checks and yield warnings in debug-only
#endif
	}

	RHITexture::RHITexture(RHIDevice* device, ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState)
		: RHIResource(device, resource, initialState)
	{
		QueryNumMipLevels();
	}

	bool RHITexture::IsViewableAsTextureCube() const
	{
		if (!IsTexture2D())
		{
			return false;
		}

		return GetDepthOrArraySize() % 6 == 0;
	}

	UINT RHITexture::GetSubresourceIndex(UINT mipSlice, UINT arraySlice, UINT planeSlice) const
	{
		return D3D12CalcSubresource(mipSlice, arraySlice, planeSlice, GetNumMipLevels(), GetDepthOrArraySize());
	}

	void RHITexture::QueryNumMipLevels()
	{
		if (m_desc.MipLevels == 0)
		{
			m_desc.MipLevels = GetD3D12Resource()->GetDesc().MipLevels;
		}
	}

	UINT RHITexture::GetNumMaxMipLevels()
	{
		ULONG highBit;
		_BitScanReverse(&highBit, GetWidth() | GetHeight());
		return UINT(highBit) + 1;
	}

}