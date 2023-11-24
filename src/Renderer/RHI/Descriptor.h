#pragma once

#include <optional>
#include <functional>

#include <d3d12.h>
#include "Device.h"
#include "DeviceChild.h"
#include "DescriptorHeap.h"
#include "Resource.h"

#include "../../Core/Assert.h"
#include "../../Core/Defines.h"

namespace Warp
{

	template<typename T>
	struct RHIViewTraits
	{
	};

	template<>
	struct RHIViewTraits<D3D12_SHADER_RESOURCE_VIEW_DESC>
	{
		static constexpr bool IsShaderVisible = true;
		static constexpr auto CreateFunc() { return &ID3D12Device::CreateShaderResourceView; };
	};

	template<>
	struct RHIViewTraits<D3D12_UNORDERED_ACCESS_VIEW_DESC>
	{
		static constexpr bool IsShaderVisible = true;
		static constexpr auto CreateFunc() { return &ID3D12Device::CreateUnorderedAccessView; };
	};

	template<>
	struct RHIViewTraits<D3D12_DEPTH_STENCIL_VIEW_DESC>
	{
		static constexpr bool IsShaderVisible = false;
		static constexpr auto CreateFunc() { return &ID3D12Device::CreateDepthStencilView; };
	};

	template<>
	struct RHIViewTraits<D3D12_RENDER_TARGET_VIEW_DESC>
	{
		static constexpr bool IsShaderVisible = false;
		static constexpr auto CreateFunc() { return &ID3D12Device::CreateRenderTargetView; };
	};

	template<typename ViewType>
	class RHIDescriptor : public RHIDeviceChild
	{
	public:
		using Traits = RHIViewTraits<ViewType>;

		RHIDescriptor() = default;
		RHIDescriptor(RHIDevice* device, 
			const RHIResource* resource,
			const ViewType* view, 
			const RHIDescriptorAllocation& allocation, uint32_t allocationIndex = 0u)
			: RHIDeviceChild(device)
			, m_allocation(allocation)
			, m_allocationIndex(allocationIndex)
		{
			WARP_ASSERT(Traits::CreateFunc() && m_allocation.IsValid());
			if constexpr (Traits::IsShaderVisible)
			{
				WARP_ASSERT(m_allocation.IsShaderVisible());
			}

			// TODO: Maybe a bit hacky, but a way to set view property correctly
			if (view)
			{
				m_view = *view;
			}

			// Init everything else before calling this
			Init(resource);
		}
		RHIDescriptor(RHIDevice* device,
			const RHIResource* resource,
			const RHIResource* counterResource,
			const ViewType* view,
			const RHIDescriptorAllocation& allocation, uint32_t allocationIndex = 0u)
			: RHIDeviceChild(device)
			, m_allocation(allocation)
			, m_allocationIndex(allocationIndex)
		{
			WARP_ASSERT(Traits::DescriptorCreateFunc && m_allocation.IsValid());
			if constexpr (Traits::IsShaderVisible)
			{
				WARP_ASSERT(m_allocation.IsShaderVisible());
			}

			// TODO: Maybe a bit hacky, but a way to set view property correctly
			if (view)
			{
				m_view = *view;
			}
			
			// Init everything else before calling this
			InitWithCounter(resource, counterResource);
		}

		uint32_t GetAllocationIndex() const { return m_allocationIndex; }
		const RHIDescriptorAllocation& GetAllocation() const { return m_allocation; }
		const ViewType GetView() const { return m_view; }

		void RecreateDescriptor(RHIResource* resource, uint32_t allocationIndex = 0)
		{
			// set this before initting
			m_allocationIndex = allocationIndex;
			Init(resource);
		}

		void RecreateDescriptor(RHIResource* resource, RHIResource* counterResource, uint32_t allocationIndex = 0)
		{
			// set this before initting
			m_allocationIndex = allocationIndex;
			InitWithCounter(resource, counterResource);
		}

		bool IsValid() const { return m_allocation.IsValid() && m_allocationIndex != uint32_t(-1); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuAddress() const { return m_allocation.GetCpuAddress(m_allocationIndex); }

	protected:
		uint32_t m_allocationIndex = uint32_t(-1);
		RHIDescriptorAllocation m_allocation;
		std::optional<ViewType> m_view;

	private:
		void Init(const RHIResource* resource)
		{
			WARP_ASSERT(resource && resource->IsValid());

			ID3D12Device* D3D12Device = GetDevice()->GetD3D12Device();
			ID3D12Resource* D3D12Resource = resource->GetD3D12Resource();
			const ViewType* D3D12View = m_view.has_value() ? &m_view.value() : nullptr;
#if 0
			std::invoke(Traits::DescriptorCreateFunc, D3D12Device,
				resource->GetD3D12Resource(),
				D3D12View,
				GetCpuAddress());
#else
			(D3D12Device->*Traits::CreateFunc())(D3D12Resource, D3D12View, GetCpuAddress());
#endif
		}

		void InitWithCounter(
			const RHIResource* resource,
			const RHIResource* counterResource)
		{
			WARP_ASSERT(resource && resource->IsValid());

			ID3D12Device* D3D12Device = GetDevice()->GetD3D12Device();
			ID3D12Resource* D3D12Resource = resource->GetD3D12Resource();
			ID3D12Resource* D3D12Counter = counterResource ? counterResource->GetD3D12Resource() : nullptr;
			const ViewType* D3D12View = m_view.has_value() ? &m_view.value() : nullptr;
#if 0
			std::invoke(Traits::DescriptorCreateFunc, D3D12Device,
				resource->GetD3D12Resource(),
				D3D12Counter,
				D3D12View,
				GetCpuAddress());
#else
			(D3D12Device->*Traits::CreateFunc())(D3D12Resource, D3D12Counter, D3D12View, GetCpuAddress());
#endif
		}
	};

	//enum class ERHIShaderResourceType
	//{
	//	StructuredBuffer,
	//	ByteAddressBuffer,
	//	Texture1D,
	//	Texture2D,
	//	Texture3D,
	//	TextureCube,
	//	RaytracingAccelerationStruct
	//};
	
	using RHIDepthStencilView = RHIDescriptor<D3D12_DEPTH_STENCIL_VIEW_DESC>;
	using RHIRenderTargetView = RHIDescriptor<D3D12_RENDER_TARGET_VIEW_DESC>;
	using RHIShaderResource = RHIDescriptor<D3D12_SHADER_RESOURCE_VIEW_DESC>;
	using RHIUnorderedAccess = RHIDescriptor<D3D12_UNORDERED_ACCESS_VIEW_DESC>;

}