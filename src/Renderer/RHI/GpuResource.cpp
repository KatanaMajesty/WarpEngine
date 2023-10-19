#include "GpuResource.h"

#include <cmath>
#include "GpuDevice.h"

#include "../../Core/Logger.h"

namespace Warp
{

	GpuResourceState::GpuResourceState(UINT numSubresources, D3D12_RESOURCE_STATES initialState)
		: m_perSubresource(numSubresources > 1) // If there are more than 1 subresource, then resource state is perSubresource
		, m_resourceState(initialState)
		, m_subresourceStates(numSubresources, initialState)
		, m_numSubresources(numSubresources)
	{
	}

	void GpuResourceState::SetSubresourceState(UINT subresourceIndex, D3D12_RESOURCE_STATES state)
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

	WARP_ATTR_NODISCARD D3D12_RESOURCE_STATES GpuResourceState::GetSubresourceState(UINT subresourceIndex) const
	{
		if (IsPerSubresource())
		{
			WARP_ASSERT(subresourceIndex < GetNumSubresources());
			return m_subresourceStates[subresourceIndex];
		}
		else return m_resourceState;
	}

	//inline constexpr bool GpuResourceState::IsValid() const
	//{
	//	if (IsPerSubresource())
	//	{
	//		return m_numSubresources == static_cast<UINT>(m_subresourceStates.size());
	//	}
	//	return m_numSubresources > 0; // Actually, only 1 is valid, but whatever
	//}

	GpuResource::GpuResource(GpuDevice* device,
		D3D12_HEAP_TYPE heapType,
		D3D12_RESOURCE_STATES initialState,
		const D3D12_RESOURCE_DESC& desc)
		: GpuDeviceChild(device)
		, m_desc(desc)
		, m_numPlanes(D3D12GetFormatPlaneCount(device->GetD3D12Device(), desc.Format))
		, m_numSubresources(QueryNumSubresources())
		, m_state(m_numSubresources, initialState)
	{
		D3D12MA::ALLOCATION_DESC allocDesc{
			.Flags = D3D12MA::ALLOCATION_FLAG_NONE,
			.HeapType = heapType
		};

		ComPtr<D3D12MA::Allocation> allocation;
		WARP_RHI_VALIDATE(device->GetResourceAllocator()->CreateResource(&allocDesc,
			&desc,
			initialState,
			nullptr, // TODO: Handle clear value
			allocation.GetAddressOf(),
			IID_PPV_ARGS(m_D3D12Resource.ReleaseAndGetAddressOf())
		));
	}

	GpuResource::GpuResource(GpuDevice* device, ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState)
		: GpuDeviceChild(device)
		, m_D3D12Resource(resource)
		, m_desc(resource->GetDesc())
		, m_numPlanes(D3D12GetFormatPlaneCount(device->GetD3D12Device(), m_desc.Format))
		, m_numSubresources(QueryNumSubresources())
		, m_state(m_numSubresources, initialState)
	{
	}

	WARP_ATTR_NODISCARD D3D12_GPU_VIRTUAL_ADDRESS GpuResource::GetGpuVirtualAddress() const
	{
		WARP_ASSERT(IsValid());
		return GetD3D12Resource()->GetGPUVirtualAddress();
	}

	// ID3D12Resource* GpuResource::GetD3D12Resource() const
	// {
	// 	WARP_ASSERT(m_allocation);
	// 	return m_allocation->GetResource();
	// }

	UINT GpuResource::QueryNumSubresources()
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

	GpuBuffer::GpuBuffer(
		GpuDevice* device,
		D3D12_HEAP_TYPE heapType, 
		D3D12_RESOURCE_STATES initialState,
		D3D12_RESOURCE_FLAGS flags,
		UINT strideInBytes, 
		UINT64 sizeInBytes)
		: GpuResource(device,
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
			})
		, m_strideInBytes(strideInBytes)
		, m_sizeInBytes(sizeInBytes)
	{
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

	D3D12_VERTEX_BUFFER_VIEW GpuBuffer::GetVertexBufferView() const
	{
		WARP_ASSERT(IsValid() && IsSrvAllowed(), 
			"The resource is either invalid or is not allowed to be represented as shader resource");

		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = GetD3D12Resource()->GetGPUVirtualAddress();
		vbv.SizeInBytes = static_cast<UINT>(GetSizeInBytes());
		vbv.StrideInBytes = GetStrideInBytes();
		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW GpuBuffer::GetIndexBufferView(DXGI_FORMAT format) const
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

	GpuTexture::GpuTexture(GpuDevice* device,
		D3D12_HEAP_TYPE heapType, 
		D3D12_RESOURCE_STATES initialState, 
		const D3D12_RESOURCE_DESC& desc)
		: GpuResource(device,
			heapType,
			initialState,
			desc)
	{
		QueryNumMipLevels();

#ifdef WARP_DEBUG
		// TODO: Perform healthy-checks and yield warnings in debug-only
#endif
	}

	GpuTexture::GpuTexture(GpuDevice* device, ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState)
		: GpuResource(device, resource, initialState)
	{
		QueryNumMipLevels();
	}

	bool GpuTexture::IsViewableAsTextureCube() const
	{
		if (!IsTexture2D())
		{
			return false;
		}

		return GetDepthOrArraySize() % 6 == 0;
	}

	UINT GpuTexture::GetSubresourceIndex(UINT mipSlice, UINT arraySlice, UINT planeSlice) const
	{
		return D3D12CalcSubresource(mipSlice, arraySlice, planeSlice, GetNumMipLevels(), GetDepthOrArraySize());
	}

	void GpuTexture::QueryNumMipLevels()
	{
		if (m_desc.MipLevels == 0)
		{
			m_desc.MipLevels = GetD3D12Resource()->GetDesc().MipLevels;
		}
	}

	UINT GpuTexture::GetNumMaxMipLevels()
	{
		ULONG highBit;
		_BitScanReverse(&highBit, GetWidth() | GetHeight());
		return UINT(highBit) + 1;
	}

}