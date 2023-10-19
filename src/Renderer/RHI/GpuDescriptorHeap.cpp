#include "GpuDescriptorHeap.h"

#include "GpuDevice.h"
#include "../../Core/Logger.h"

namespace Warp
{

    WARP_ATTR_NODISCARD D3D12_CPU_DESCRIPTOR_HANDLE RHIDescriptorAllocation::GetCpuAddress(UINT index) const
    {
        WARP_ASSERT(index < NumDescriptors);
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(CpuHandle, index, DescriptorIncrementSize);
    }

    WARP_ATTR_NODISCARD D3D12_GPU_DESCRIPTOR_HANDLE RHIDescriptorAllocation::GetGpuAddress(UINT index) const
    {
        WARP_ASSERT(index < NumDescriptors && IsShaderVisible());
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(GpuHandle, index, DescriptorIncrementSize);
    }

    GpuDescriptorHeap::GpuDescriptorHeap(GpuDevice* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible)
        : m_type(type)
        , m_descriptorIncrementSize(device->GetD3D12Device()->GetDescriptorHandleIncrementSize(type))
        , m_numDescriptors(numDescriptors)
        , m_shaderVisible(shaderVisible)
    {
        // TODO: Implement feature check and max allowed numDescriptors check
        // https://learn.microsoft.com/en-gb/windows/win32/direct3d12/hardware-support?redirectedfrom=MSDN
        
        if (m_shaderVisible && !IsTypeShaderVisible(type))
        {
            WARP_LOG_WARN("Descriptor heap was created with shaderVisible = true, but its type cannot be shader visible");
            m_shaderVisible = false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = type;
        desc.NumDescriptors = numDescriptors;
        desc.NodeMask = 0;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (m_shaderVisible)
        {
            desc.Flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        }

        WARP_RHI_VALIDATE(
            device->GetD3D12Device9()->CreateDescriptorHeap(
                &desc, 
                IID_PPV_ARGS(m_D3D12DescriptorHeap.ReleaseAndGetAddressOf())
            ));

        m_baseCpuDescriptorHandle = m_D3D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_baseGpuDescriptorHandle = IsShaderVisible() ? 
            m_D3D12DescriptorHeap->GetGPUDescriptorHandleForHeapStart() : 
            D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

        // Just add the whole heap into the available range
        m_availableRanges.push_back(RHIDescriptorAllocation{
                .CpuHandle = m_baseCpuDescriptorHandle,
                .GpuHandle = m_baseGpuDescriptorHandle,
                .NumDescriptors = numDescriptors,
                .DescriptorIncrementSize = m_descriptorIncrementSize
            });
    }

    RHIDescriptorAllocation GpuDescriptorHeap::Allocate(UINT numDescriptors)
    {
        std::lock_guard guard(m_allocationMutex);

        auto allocationIt = m_availableRanges.end();
        for (auto it = m_availableRanges.begin();
            it != m_availableRanges.end() && allocationIt == m_availableRanges.end();
            ++it)
        {
            // If number of descriptors in available allocation is more than we need we can use it
            RHIDescriptorAllocation& allocation = *it;
            if (allocation.NumDescriptors >= numDescriptors)
            {
                allocationIt = it;
            }
        }
        
        // If we found no available to use allocations, we return null-allocation
        if (allocationIt == m_availableRanges.end())
        {
            return RHIDescriptorAllocation();
        }

        // Copy the allocation we need and remove it from the available list
        RHIDescriptorAllocation allocationToUse = *allocationIt;
        m_availableRanges.erase(allocationIt);
        WARP_ASSERT(!allocationToUse.IsNull());

        // Now we want to split the allocation range into two parts. First part will contain the "reserved" memory for
        // requested descriptors and the other range will contain the memory that is still available
        RHIDescriptorAllocation reservedAllocation = RHIDescriptorAllocation{
            .CpuHandle = allocationToUse.CpuHandle,
            .GpuHandle = allocationToUse.GpuHandle,
            .NumDescriptors = numDescriptors,
            .DescriptorIncrementSize = m_descriptorIncrementSize
        };

        // It is guaranteed for allocationToUse.NumDescriptors to be more or equal than requested numDescriptors at this point
        // If there will be no available descriptors left after "reservation" then there is no need to do further calculations
        UINT numStillAvailableDescriptors = allocationToUse.NumDescriptors - numDescriptors;
        if (numStillAvailableDescriptors > 0)
        {
            UINT offsetScaledByIncrementSize = numDescriptors * GetDescriptorIncrementSize();
            D3D12_CPU_DESCRIPTOR_HANDLE availableCpuOffset = CD3DX12_CPU_DESCRIPTOR_HANDLE(allocationToUse.CpuHandle, offsetScaledByIncrementSize);
            D3D12_GPU_DESCRIPTOR_HANDLE availableGpuOffset = { 0 };
            if (IsShaderVisible())
            {
                availableGpuOffset = CD3DX12_GPU_DESCRIPTOR_HANDLE(allocationToUse.GpuHandle, offsetScaledByIncrementSize);
            }

            RHIDescriptorAllocation availableAllocation = RHIDescriptorAllocation{
                .CpuHandle = availableCpuOffset,
                .GpuHandle = availableGpuOffset,
                .NumDescriptors = numStillAvailableDescriptors,
                .DescriptorIncrementSize = m_descriptorIncrementSize
            };
            WARP_ASSERT(availableAllocation.IsValid());
            m_availableRanges.push_back(availableAllocation);
        }

        return reservedAllocation;
    }

    void GpuDescriptorHeap::Free(RHIDescriptorAllocation&& allocation)
    {
        std::lock_guard guard(m_allocationMutex);

        WARP_ASSERT(IsValidAllocation(allocation));
        m_availableRanges.push_back(allocation);
        allocation = RHIDescriptorAllocation();
    }

    bool GpuDescriptorHeap::IsValidAllocation(const RHIDescriptorAllocation& allocation)
    {
        UINT offsetScaledByIncrementSize = m_numDescriptors * m_descriptorIncrementSize;
        D3D12_CPU_DESCRIPTOR_HANDLE endCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_baseCpuDescriptorHandle, offsetScaledByIncrementSize);
        return allocation.IsValid() && !allocation.IsNull()
            && allocation.CpuHandle.ptr >= m_baseCpuDescriptorHandle.ptr
            && allocation.CpuHandle.ptr <= endCpuHandle.ptr;
    }

    bool GpuDescriptorHeap::IsTypeShaderVisible(D3D12_DESCRIPTOR_HEAP_TYPE type) const
    {
        switch (type)
        {
        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: WARP_ATTR_FALLTHROUGH;
        case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
            return true;
        case D3D12_DESCRIPTOR_HEAP_TYPE_RTV: WARP_ATTR_FALLTHROUGH;
        case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
            return false;
        default: WARP_ASSERT(false); return false;
        }
    }

}