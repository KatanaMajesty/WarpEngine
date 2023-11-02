#pragma once

#include <queue>
#include <array>
#include <vector>
#include <unordered_map>

#include "stdafx.h"
#include "Resource.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include "PipelineState.h"

namespace Warp
{

	class RHIRootSignature;

	class RHICommandContext
	{
	public:
		RHICommandContext() = default;
		RHICommandContext(RHICommandQueue* queue);

		inline constexpr D3D12_COMMAND_LIST_TYPE GetType() const { return m_queue->GetType(); }
		inline ID3D12GraphicsCommandList6* GetD3D12CommandList() const { return m_commandList.GetD3D12CommandList(); }
		inline ID3D12GraphicsCommandList6* operator->() const { return GetD3D12CommandList(); } // TODO: Will be removed

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

	private:
		static constexpr UINT NumResourceBarriersPerBatch = 16;

		RHICommandQueue* m_queue = nullptr;
		RHICommandList m_commandList;
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		RHICommandAllocatorPool m_commandAllocatorPool;
	};

}