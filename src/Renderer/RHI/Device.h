#pragma once

#include <memory>
#include <array>

#include "D3D12MA/D3D12MemAlloc.h"

#include "stdafx.h"
#include "Resource.h"
#include "CommandQueue.h"
#include "DescriptorHeap.h"

namespace Warp
{

    class RHIPhysicalDevice;

    enum class ERHIMeshShaderSupportTier
    {
        NotSupported = 0,
        SupportTier_1_0 = 1,
    };

    enum class ERHIRaytracingSupportTier
    {
        NotSupported = 0,
        SupportTier_1_0 = 1,
        SupportTier_1_1 = 2
    };

    struct RHIDeviceCapabilities
    {
        ERHIMeshShaderSupportTier MeshShaderSupportTier = ERHIMeshShaderSupportTier::NotSupported;
        ERHIRaytracingSupportTier RayTracingSupportTier = ERHIRaytracingSupportTier::NotSupported;
    };

    // Warp uses an approach somewhat similar to Vulkan's. There are two types of device representations: physical and logical.
    // RHIDevice is a logical representation of a GPU device, which is responsible for:
    //	- Command Queue creation
    //	- Performance querying and Profiling
    //	- Querying device's features and retrieving information about the physical device's capabilities
    //	- Frame management
    //
    // TODO: Implement feature querying and adapter infromation querying
    class RHIDevice
    {
    public:
        RHIDevice() = default;
        RHIDevice(RHIPhysicalDevice* physicalDevice);

        // No need to copy logical device, as two different logical devices of the same physical device will in fact
        // be in the same memory, thus making them unnecessary or even unsafe in some cases
        RHIDevice(const RHIDevice&) = delete;
        RHIDevice& operator=(const RHIDevice&) = delete;

        ~RHIDevice();

        void BeginFrame();
        void EndFrame();

        WARP_ATTR_NODISCARD RHIPhysicalDevice* GetPhysicalDevice() const { return m_physicalDevice; }
        WARP_ATTR_NODISCARD const RHIDeviceCapabilities& GetDeviceCapabilities() const { return m_deviceCapabilities; }

        WARP_ATTR_NODISCARD ID3D12Device9* GetD3D12Device() const { return m_device.Get(); }
        WARP_ATTR_NODISCARD constexpr UINT GetFrameID() const { return m_frameID; }

        WARP_ATTR_NODISCARD RHICommandQueue* GetCommandQueue(D3D12_COMMAND_LIST_TYPE type);
        WARP_ATTR_NODISCARD RHICommandQueue* GetGraphicsQueue() { return m_graphicsQueue.get(); }
        WARP_ATTR_NODISCARD RHICommandQueue* GetComputeQueue() { return m_computeQueue.get(); }
        WARP_ATTR_NODISCARD RHICommandQueue* GetCopyQueue() { return m_copyQueue.get(); }

        WARP_ATTR_NODISCARD RHIDescriptorHeap* GetDescriptorHeap(uint32_t type) { return m_descriptorHeaps[type].Heap.get(); }
        WARP_ATTR_NODISCARD RHIDescriptorHeap* GetSamplerHeap() { return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER); }
        WARP_ATTR_NODISCARD RHIDescriptorHeap* GetViewHeap() { return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); }
        WARP_ATTR_NODISCARD RHIDescriptorHeap* GetRtvsHeap() { return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); }
        WARP_ATTR_NODISCARD RHIDescriptorHeap* GetDsvsHeap() { return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV); }

        D3D12MA::Allocator* GetResourceAllocator() const { return m_resourceAllocator.Get(); }

        // Returns the amount of TotalBytes needed to copy the subresources of the resource
        // Basically gets a resource's TotalBytes from GetCopyableFootprint structure call
        WARP_ATTR_NODISCARD UINT64 GetCopyableBytes(RHIResource* res, UINT subresourceOffset, UINT numSubresources);

        static constexpr UINT NumDescriptorHeaps = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

    private:
        ComPtr<ID3D12Device9>     m_device;
        ComPtr<ID3D12DebugDevice> m_debugDevice;
        ComPtr<ID3D12InfoQueue1>  m_debugInfoQueue;
        DWORD m_messageCallbackCookie = DWORD(-1);

        void InitCapabilitiesAndAssociateDevice();
        ERHIMeshShaderSupportTier QueryMeshShaderSupportTier() const;
        ERHIRaytracingSupportTier QueryRaytracingSupportTier() const;

        RHIPhysicalDevice* m_physicalDevice;
        RHIDeviceCapabilities m_deviceCapabilities;

        UINT m_frameID = 0;
        ComPtr<D3D12MA::Allocator> m_resourceAllocator;

        void InitCommandQueues();
        // TODO: Make them non-ptr types. Just store as value; Doing that invalidates them due to problems with copy-constructor
        std::unique_ptr<RHICommandQueue> m_graphicsQueue;
        std::unique_ptr<RHICommandQueue> m_computeQueue;
        std::unique_ptr<RHICommandQueue> m_copyQueue;

        struct DescriptorHeapData
        {
            std::unique_ptr<RHIDescriptorHeap> Heap;
            UINT NumDescriptors;
            BOOL ShaderVisible;
        };

        void InitDescriptorHeaps();
        std::array<DescriptorHeapData, NumDescriptorHeaps> m_descriptorHeaps;
    };

}