#include "Device.h"

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "../../Core/Logger.h"
#include "PhysicalDevice.h"

#include "ValidationLayer.h"
#include "PIXRuntime.h"

namespace Warp
{

	RHIDevice::RHIDevice(RHIPhysicalDevice* physicalDevice)
		: m_physicalDevice(physicalDevice)
		, m_frameID(0)
	{
		WARP_ASSERT(physicalDevice);
		WARP_RHI_VALIDATE(
			D3D12CreateDevice(
				physicalDevice->GetAdapter(), 
				D3D_FEATURE_LEVEL_12_0, 
				IID_PPV_ARGS(m_device.ReleaseAndGetAddressOf()
				)));

		// Setup validation layer messages callbacks
		if (physicalDevice->IsDebugLayerEnabled())
		{
			WARP_MAYBE_UNUSED HRESULT hr = m_device->QueryInterface(IID_PPV_ARGS(m_debugInfoQueue.ReleaseAndGetAddressOf()));
			if (FAILED(hr))
			{
				WARP_LOG_WARN("Failed to initialize debug info queue. Validation layer message callback won't be initialized");
			}
			else
			{
				D3D12_MESSAGE_ID deniedIDs[] = {
					// Suppress D3D12 ERROR: ID3D12CommandQueue::Present: Resource state (0x800: D3D12_RESOURCE_STATE_COPY_SOURCE)
					// As it thats a bug in the DXGI Debug Layer interaction with the DX12 Debug Layer w/ Windows 11.
					// The issue comes from debug layer interacting with hybrid graphics, such as integrated and discrete laptop GPUs
					D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
				};

				D3D12_INFO_QUEUE_FILTER_DESC denyFilterDesc{
					.NumIDs = _countof(deniedIDs),
					.pIDList = deniedIDs
				};

				D3D12_INFO_QUEUE_FILTER filter{
					.AllowList = {},
					.DenyList = {denyFilterDesc}
				};

				WARP_RHI_VALIDATE(m_debugInfoQueue->AddStorageFilterEntries(&filter));
				WARP_RHI_VALIDATE(
					m_debugInfoQueue->RegisterMessageCallback(
						ValidationLayer::OnDebugLayerMessage, 
						D3D12_MESSAGE_CALLBACK_FLAG_NONE, 
						this, 
						&m_messageCallbackCookie
					));
			}
		}

		D3D12MA::ALLOCATOR_DESC resourceAllocatorDesc{};
		resourceAllocatorDesc.pAdapter = m_physicalDevice->GetAdapter();
		resourceAllocatorDesc.pDevice = GetD3D12Device();
		WARP_RHI_VALIDATE(D3D12MA::CreateAllocator(&resourceAllocatorDesc, m_resourceAllocator.ReleaseAndGetAddressOf()));

		InitCommandQueues();

		// Associate a logical device with physical device!
		physicalDevice->AssociateLogicalDevice(this);
	}

	RHIDevice::~RHIDevice()
	{
		m_graphicsQueue->HostWaitIdle();
		m_computeQueue->HostWaitIdle();
		m_copyQueue->HostWaitIdle();

		if (m_debugDevice)
		{
			WARP_MAYBE_UNUSED HRESULT hr = m_debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			WARP_ASSERT(SUCCEEDED(hr));
		}
		
		if (m_debugInfoQueue)
		{
			WARP_LOG_INFO("Unregistering validation layer message callback");
			WARP_MAYBE_UNUSED HRESULT hr = m_debugInfoQueue->UnregisterMessageCallback(m_messageCallbackCookie);
			WARP_ASSERT(SUCCEEDED(hr));
		}
	}

	void RHIDevice::BeginFrame()
	{
	}

	void RHIDevice::EndFrame()
	{
		++m_frameID;
		m_resourceAllocator->SetCurrentFrameIndex(m_frameID);
	}

	RHICommandQueue* RHIDevice::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type)
	{
		switch (type)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: return GetGraphicsQueue();
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: return GetComputeQueue();
		case D3D12_COMMAND_LIST_TYPE_COPY: return GetCopyQueue();
		default: WARP_ASSERT(false); return nullptr;
		}
	}

	RHIBuffer RHIDevice::CreateBuffer(UINT strideInBytes, UINT64 sizeInBytes, D3D12_RESOURCE_FLAGS flags)
	{
		// Work remarks:
		// Applications should stick to the heap type abstractions of UPLOAD, DEFAULT, and READBACK, 
		// in order to support all adapter architectures reasonably well.
		RHIBuffer buffer(this,
			D3D12_HEAP_TYPE_UPLOAD, 		   // TODO: Rewrite this, request a heap type from the user
			D3D12_RESOURCE_STATE_GENERIC_READ, // TODO: Rewrite this, avoid using generic read, start using transitions
			flags, 
			strideInBytes, sizeInBytes);

		// TODO: Add validity checks
		return buffer;
	}

	bool RHIDevice::CheckMeshShaderSupport() const
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS7 feature;
		WARP_RHI_VALIDATE(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &feature, sizeof(decltype(feature))));
		return feature.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
	}

	void RHIDevice::InitCommandQueues()
	{
		m_graphicsQueue.reset(new RHICommandQueue(this, D3D12_COMMAND_LIST_TYPE_DIRECT));
		m_graphicsQueue->SetName(L"RHIDevice_GraphicsQueue");

		m_computeQueue.reset(new RHICommandQueue(this, D3D12_COMMAND_LIST_TYPE_COMPUTE));
		m_computeQueue->SetName(L"RHIDevice_ComputeQueue");

		m_copyQueue.reset(new RHICommandQueue(this, D3D12_COMMAND_LIST_TYPE_COPY));
		m_copyQueue->SetName(L"RHIDevice_CopyQueue");
	}

}