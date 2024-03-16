#pragma once

#include <span>

#include "stdafx.h"
#include "Resource.h"
#include "ResourceTrackingContext.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include "PipelineState.h"
#include "Descriptor.h"

namespace Warp
{

    class RHIRootSignature;

    class RHICommandContext
    {
    public:
        RHICommandContext() = default;
        RHICommandContext(std::wstring_view name, RHICommandQueue* queue);

        inline constexpr D3D12_COMMAND_LIST_TYPE GetType() const { return m_queue->GetType(); }
        inline ID3D12GraphicsCommandList6* GetD3D12CommandList() const { return m_commandList.GetD3D12CommandList(); }
        inline ID3D12GraphicsCommandList6* operator->() const { return GetD3D12CommandList(); } // TODO: Will be removed
        inline RHICommandQueue* GetQueue() const { return m_queue; }

        void AddTransitionBarrier(RHIResource* resource, D3D12_RESOURCE_STATES state, UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        void AddAliasingBarrier(RHIResource* before, RHIResource* after); // NOIMPL
        void AddUavBarrier(RHIResource* resource); // NOIMPL

        // TODO: This might change from UINT to FLOAT
        void SetViewport(UINT topLeftX, UINT topLeftY, UINT width, UINT height);
        void SetScissorRect(UINT left, UINT top, UINT right, UINT bottom);
        void SetGraphicsRootSignature(const RHIRootSignature& rootSignature);

        void SetPipelineState(const RHIPipelineState& pso);

        void Open();
        void Close();
        UINT64 Execute(bool waitForCompletion);

        void FlushBatchedResourceBarriers();

        // In D3D12 there is no non-instanced draw calls
        // Application should always prefer calling these draw call functions in order to avoid manually flushing batched resource barriers
        void DrawInstanced(
            UINT verticesPerInstance,
            UINT numInstances,
            UINT startVertexLocation,
            UINT startInstanceLocation);
        void DrawIndexedInstanced(
            UINT indicesPerInstance,
            UINT numInstances,
            UINT startIndexLocation,
            INT baseVertexLocation,
            UINT startInstanceLocation);
        void Dispatch(
            UINT numThreadGroupsX,
            UINT numThreadGroupsY,
            UINT numThreadGroupsZ);
        void DispatchMesh(
            UINT numThreadGroupsX,
            UINT numThreadGroupsY,
            UINT numThreadGroupsZ);

        void ClearRtv(const RHIRenderTargetView& descriptor, const float* rgba, UINT numDirtyRects, const D3D12_RECT* dirtyRects);
        void ClearDsv(const RHIDepthStencilView& descriptor, D3D12_CLEAR_FLAGS flags, float depth, UINT8 stencil, UINT numDirtyRects, const D3D12_RECT* dirtyRects);

    protected:
        static constexpr UINT NumResourceBarriersPerBatch = 16;

        RHICommandQueue* m_queue = nullptr;
        RHICommandList m_commandList;
        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        RHICommandAllocatorPool m_commandAllocatorPool;
    };

    class RHICopyCommandContext : public RHICommandContext
    {
    public:
        RHICopyCommandContext() = default;
        RHICopyCommandContext(std::wstring_view name, RHICommandQueue* copyQueue);

        void BeginCopy();
        void EndCopy(UINT64 fenceValue);

        void CopyResource(RHIResource* dest, RHIResource* src);
        void UploadSubresources(RHIResource* dest, std::span<D3D12_SUBRESOURCE_DATA> subresourceData, UINT subresourceOffset);
        void UploadSubresources(RHIResource* dest, std::span<D3D12_SUBRESOURCE_DATA> subresourceData, UINT subresourceOffset, RHIResource* uploadBuffer);
        void UploadToBuffer(RHIBuffer* dest, void* src, size_t numBytes);

    private:
        RHIResourceTrackingContext<RHIBuffer> m_uploadBufferTrackingContext;
    };

}