#include "Device.h"

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "../../Util/Logger.h"
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

        InitCapabilitiesAndAssociateDevice();

        D3D12MA::ALLOCATOR_DESC resourceAllocatorDesc{};
        resourceAllocatorDesc.pAdapter = m_physicalDevice->GetAdapter();
        resourceAllocatorDesc.pDevice = GetD3D12Device();
        WARP_RHI_VALIDATE(D3D12MA::CreateAllocator(&resourceAllocatorDesc, m_resourceAllocator.ReleaseAndGetAddressOf()));

        InitCommandQueues();
        InitDescriptorHeaps();
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

    UINT64 RHIDevice::GetCopyableBytes(RHIResource* res, UINT subresourceOffset, UINT numSubresources)
    {
        UINT64 TotalBytes = 0;
        GetD3D12Device()->GetCopyableFootprints(
            &res->GetDesc(),
            subresourceOffset,
            numSubresources, 0, nullptr, nullptr, nullptr, &TotalBytes);
        return TotalBytes;
    }

    void RHIDevice::InitCapabilitiesAndAssociateDevice()
    {
        WARP_ASSERT(m_device, "RHIDevice should be initialized before initializing its capabilities and device association")
            m_deviceCapabilities = RHIDeviceCapabilities();
        m_deviceCapabilities.MeshShaderSupportTier = QueryMeshShaderSupportTier();
        m_deviceCapabilities.RayTracingSupportTier = QueryRaytracingSupportTier();

        // Associate a logical device with physical device!
        m_physicalDevice->AssociateLogicalDevice(this);
    }

    ERHIMeshShaderSupportTier RHIDevice::QueryMeshShaderSupportTier() const
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 featureSupportData = {};
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &featureSupportData, sizeof(featureSupportData))))
        {
            return ERHIMeshShaderSupportTier::NotSupported;
        }

        switch (featureSupportData.MeshShaderTier)
        {
        case D3D12_MESH_SHADER_TIER_NOT_SUPPORTED: return ERHIMeshShaderSupportTier::NotSupported;
        case D3D12_MESH_SHADER_TIER_1:             return ERHIMeshShaderSupportTier::SupportTier_1_0;

            // We should not get here! This probably means we did not handle some switch-case and our code is outdated
        WARP_ATTR_UNLIKELY default:
            WARP_ASSERT(false, "Outdated code! Handle all cases of featureSupportData.MeshShaderTier");
            return ERHIMeshShaderSupportTier::NotSupported;
        }
    }

    ERHIRaytracingSupportTier RHIDevice::QueryRaytracingSupportTier() const
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData))))
        {
            return ERHIRaytracingSupportTier::NotSupported;
        }

        switch (featureSupportData.RaytracingTier)
        {
        case D3D12_RAYTRACING_TIER_NOT_SUPPORTED: return ERHIRaytracingSupportTier::NotSupported;
        case D3D12_RAYTRACING_TIER_1_0:           return ERHIRaytracingSupportTier::SupportTier_1_0;
        case D3D12_RAYTRACING_TIER_1_1:           return ERHIRaytracingSupportTier::SupportTier_1_1;

            // We should not get here! This probably means we did not handle some switch-case and our code is outdated
        WARP_ATTR_UNLIKELY default:
            WARP_ASSERT(false, "Outdated code! Handle all cases of featureSupportData.RaytracingTier");
            return ERHIRaytracingSupportTier::NotSupported;
        }
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

    void RHIDevice::InitDescriptorHeaps()
    {
        m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].NumDescriptors = 32768;
        m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].ShaderVisible = true;

        m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].NumDescriptors = 512;
        m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].ShaderVisible = true;

        m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].NumDescriptors = 512;
        m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].ShaderVisible = false;

        m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].NumDescriptors = 512;
        m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].ShaderVisible = false;

        for (UINT i = 0; i < NumDescriptorHeaps; ++i)
        {
            DescriptorHeapData& data = m_descriptorHeaps[i];
            D3D12_DESCRIPTOR_HEAP_TYPE type = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
            data.Heap.reset(new RHIDescriptorHeap(this, type, data.NumDescriptors, data.ShaderVisible));
        }
    }

}