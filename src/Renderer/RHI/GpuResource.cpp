#include "GpuResource.h"

#include <cmath>

namespace Warp
{

	GpuResource::GpuResource(ID3D12Device* device,
		D3D12MA::Allocator* allocator,
		D3D12_HEAP_TYPE heapType,
		D3D12_RESOURCE_STATES initialState,
		const D3D12_RESOURCE_DESC& desc)
		: m_desc(desc)
	{
		WARP_ASSERT(device && allocator);

		D3D12MA::ALLOCATION_DESC allocDesc{
			.Flags = D3D12MA::ALLOCATION_FLAG_NONE,
			.HeapType = heapType
		};

		WARP_MAYBE_UNUSED HRESULT hr = allocator->CreateResource(&allocDesc,
			&desc,
			initialState,
			nullptr, // TODO: Handle clear value
			&m_allocation, __uuidof(ID3D12Resource), nullptr);
		WARP_ASSERT(SUCCEEDED(hr) && IsValid(), "Failed to create a resource");
	}

	ID3D12Resource* GpuResource::GetD3D12Resource() const
	{
		WARP_ASSERT(m_allocation);
		return m_allocation->GetResource();
	}

	GpuBuffer::GpuBuffer(
		ID3D12Device* device, 
		D3D12MA::Allocator* allocator, 
		D3D12_HEAP_TYPE heapType, 
		D3D12_RESOURCE_STATES initialState,
		D3D12_RESOURCE_FLAGS flags,
		UINT strideInBytes, 
		UINT64 sizeInBytes)
		: GpuResource(device,
			allocator,
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

	GpuTexture::GpuTexture(ID3D12Device* device, 
		D3D12MA::Allocator* allocator, 
		D3D12_HEAP_TYPE heapType, 
		D3D12_RESOURCE_STATES initialState, 
		const D3D12_RESOURCE_DESC& desc)
		: GpuResource(device,
			allocator,
			heapType,
			initialState,
			desc)
	{
		QueryNumMipLevels();
		QueryNumSubresources();

#ifdef WARP_DEBUG
		// TODO: Perform healthy-checks and yield warnings in debug-only
#endif
	}

	UINT GpuTexture::GetSubresourceIndex(UINT mipSlice, UINT arraySlice, UINT planeSlice) const
	{
		return D3D12CalcSubresource(mipSlice, arraySlice, planeSlice, GetNumMipLevels(), GetDepthOrArraySize());
	}

	void GpuTexture::QueryNumSubresources()
	{
		m_numSubresources = m_desc.MipLevels * m_desc.DepthOrArraySize;
	}

	void GpuTexture::QueryNumMipLevels()
	{
		if (m_desc.MipLevels == 0)
		{
			m_desc.MipLevels = GetD3D12Resource()->GetDesc().MipLevels;
		}
	}

}