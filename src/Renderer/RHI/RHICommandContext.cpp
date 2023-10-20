#include "RHICommandContext.h"

#include "GpuCommandQueue.h"
#include "GpuDevice.h"
#include "RootSignature.h"

namespace Warp
{

	// =======================
	// RHICommandContext
	// =======================

	RHICommandContext::RHICommandContext(GpuCommandQueue* queue)
		: m_queue(queue)
		, m_commandList(queue->GetDevice()->GetD3D12Device9(), queue->GetType())
		, m_commandAllocatorPool(queue)
	{
	}

	void RHICommandContext::AddTransitionBarrier(GpuResource* resource, D3D12_RESOURCE_STATES state, UINT subresourceIndex)
	{
		m_commandList.AddTransitionBarrier(resource, state, subresourceIndex);
	}

	void RHICommandContext::AddAliasingBarrier(GpuResource* before, GpuResource* after)
	{
		m_commandList.AddAliasingBarrier(before, after);
	}

	void RHICommandContext::AddUavBarrier(GpuResource* resource)
	{
		m_commandList.AddUavBarrier(resource);
	}

	void RHICommandContext::SetGraphicsRootSignature(const RHIRootSignature& rootSignature)
	{
		m_commandList->SetGraphicsRootSignature(rootSignature.GetD3D12RootSignature());
	}

	UINT64 RHICommandContext::Execute(bool waitForCompletion)
	{
		WARP_ASSERT(m_commandAllocator);

		GpuCommandList* lists[] = { &m_commandList };
		UINT64 fenceValue = m_queue->ExecuteCommandLists(lists, waitForCompletion);

		m_commandAllocatorPool.DiscardCommandAllocator(std::exchange(m_commandAllocator, nullptr), fenceValue);
		return fenceValue;
	}

	void RHICommandContext::Open()
	{
		if (!m_commandAllocator)
		{
			// We get a command allocator that was already reset
			m_commandAllocator = m_commandAllocatorPool.GetCommandAllocator();
		}
		m_commandList.Open(m_commandAllocator.Get());
	}

	void RHICommandContext::Close()
	{
		m_commandList.Close();
	}

	void RHICommandContext::DrawInstanced(
		UINT verticesPerInstance, 
		UINT numInstances, 
		UINT startVertexLocation, 
		UINT startInstanceLocation)
	{
		m_commandList.FlushBatchedResourceBarriers();
		m_commandList->DrawInstanced(
			verticesPerInstance, 
			numInstances, 
			startVertexLocation, 
			startInstanceLocation);
	}

	void RHICommandContext::DrawIndexedInstanced(
		UINT indicesPerInstance, 
		UINT numInstances, 
		UINT startIndexLocation, 
		INT baseVertexLocation, 
		UINT startInstanceLocation)
	{
		m_commandList.FlushBatchedResourceBarriers();
		m_commandList->DrawIndexedInstanced(
			indicesPerInstance, 
			numInstances, 
			startIndexLocation, 
			baseVertexLocation, 
			startIndexLocation);
	}

	void RHICommandContext::Dispatch(
		UINT numThreadGroupsX, 
		UINT numThreadGroupsY, 
		UINT numThreadGroupsZ)
	{
		m_commandList.FlushBatchedResourceBarriers();
		m_commandList->Dispatch(
			numThreadGroupsX, 
			numThreadGroupsY, 
			numThreadGroupsZ);
	}

}