#pragma once

#include "../../Core/Defines.h"

#include "stdafx.h"
#include "GpuDeviceChild.h"
#include "GpuDevice.h"
#include "RootSignature.h"

namespace Warp
{
	// https://youtu.be/CFXKTXtil34?t=1495 for better understanding. Might also want to look into D3DX12.h

	// Basically a representation of a pipeline object for pipeline stream
	// PSO stream expects elements to be aligned to the natural word alignment of the system, thus alignas(void*)
	template<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE SubobjectType, typename ElementType>
	struct alignas(void*) RHIPipelineElementDesc
	{
		RHIPipelineElementDesc()
			: Type(SubobjectType)
			, Element(ElementType())
		{
		}

		explicit RHIPipelineElementDesc(const ElementType& element)
			: Type(SubobjectType)
			, Element(element)
		{
		}

		inline constexpr ElementType& operator->() { return Element; }

		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type;
		ElementType Element;
	};

	using RHIPipelineElement_RootSignature = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, D3D12_ROOT_SIGNATURE_DESC>;
	using RHIPipelineElement_VS = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, D3D12_SHADER_BYTECODE>;
	using RHIPipelineElement_PS = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE>;
	using RHIPipelineElement_MS = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, D3D12_SHADER_BYTECODE>;
	using RHIPipelineElement_AS = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, D3D12_SHADER_BYTECODE>;
	using RHIPipelineElement_CS = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, D3D12_SHADER_BYTECODE>;
	using RHIPipelineElement_BlendState = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, D3D12_BLEND_DESC>;
	using RHIPipelineElement_Rasterizer = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, D3D12_RASTERIZER_DESC>;
	using RHIPipelineElement_DepthStencil = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, D3D12_DEPTH_STENCIL_DESC>;
	using RHIPipelineElement_PrimitiveType = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE>;
	using RHIPipelineElement_RenderTargetFormat = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY>;
	using RHIPipelineElement_DepthStencilFormat = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT>;
	using RHIPipelineElement_Flags = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS, D3D12_PIPELINE_STATE_FLAGS>;

	struct RHIGraphicsPipelineStream
	{
		RHIPipelineElement_RootSignature RootSignature;
		RHIPipelineElement_VS VS;
		RHIPipelineElement_PS PS;
		RHIPipelineElement_BlendState BlendState;
		RHIPipelineElement_Rasterizer Rasterizer;
		RHIPipelineElement_DepthStencil DepthStencil;
		RHIPipelineElement_PrimitiveType PrimitiveType;
		RHIPipelineElement_RenderTargetFormat RTVFormats;
		RHIPipelineElement_DepthStencilFormat DSVFormat;
		RHIPipelineElement_Flags Flags;
	};

	struct RHIMeshPipelineStream
	{
		RHIPipelineElement_RootSignature RootSignature;
		RHIPipelineElement_AS AS;
		RHIPipelineElement_MS MS;
		RHIPipelineElement_PS PS;
		RHIPipelineElement_BlendState BlendState;
		RHIPipelineElement_Rasterizer Rasterizer;
		RHIPipelineElement_DepthStencil DepthStencil;
		RHIPipelineElement_PrimitiveType PrimitiveType;
		RHIPipelineElement_RenderTargetFormat RTVFormats;
		RHIPipelineElement_DepthStencilFormat DSVFormat;
		RHIPipelineElement_Flags Flags;
	};

	struct RHIComputePipelineStream
	{
		RHIPipelineElement_RootSignature RootSignature;
		RHIPipelineElement_CS CS;
		RHIPipelineElement_Flags Flags;
	};

	// TODO: Noimpl
	struct GpuGraphicsPipelineDesc
	{
		RHIRootSignature RootSignature;
	};

	// TODO: Noimpl
	struct GpuMeshPipelineDesc
	{
		RHIRootSignature RootSignature;
	};

	// TODO: Noimpl
	struct GpuComputePipelineDesc
	{
		RHIRootSignature RootSignature;
	};

	template<typename PipelineDesc>
	struct PipelineDescTraits
	{
	};

	template<>
	struct PipelineDescTraits<GpuGraphicsPipelineDesc>
	{
		static RHIGraphicsPipelineStream ToRHIStream(const GpuGraphicsPipelineDesc& desc)
		{
			WARP_YIELD_NOIMPL();
			RHIGraphicsPipelineStream stream;
			return stream;
		}
	};

	template<>
	struct PipelineDescTraits<GpuMeshPipelineDesc>
	{
		static RHIMeshPipelineStream ToRHIStream(const GpuMeshPipelineDesc& desc)
		{
			WARP_YIELD_NOIMPL();
			RHIMeshPipelineStream stream;
			return stream;
		}
	};

	template<>
	struct PipelineDescTraits<GpuComputePipelineDesc>
	{
		static RHIComputePipelineStream ToRHIStream(const GpuComputePipelineDesc& desc)
		{
			WARP_YIELD_NOIMPL();
			RHIComputePipelineStream stream;
			return stream;
		}
	};

	// TODO: add concept for desc?

	template<typename PipelineDesc>
	class GpuPipelineState final : public GpuDeviceChild
	{
	public:
		GpuPipelineState() = default;
		explicit GpuPipelineState(GpuDevice* device, const PipelineDesc& desc)
			: GpuDeviceChild(device)
			, m_D3D12PipelineState(
				[device, &desc]()
				{
					auto stream = PipelineDescTraits<PipelineDesc>::ToRHIStream(desc);

					D3D12_PIPELINE_STATE_STREAM_DESC desc;
					desc.SizeInBytes = sizeof(decltype(stream));
					desc.pPipelineStateSubobjectStream = &stream;

					ComPtr<ID3D12PipelineState> pso;
					WARP_RHI_VALIDATE(device->GetD3D12Device()->CreatePipelineState(&desc, IID_PPV_ARGS(pso.GetAddressOf())));
					return pso;
				}()
			)
			, m_desc(desc)
		{
		}

		WARP_ATTR_NODISCARD inline ID3D12PipelineState* GetD3D12PipelineState() const { return m_D3D12PipelineState.Get(); }
		WARP_ATTR_NODISCARD inline constexpr const PipelineDesc& GetDesc() const { return m_desc; }

	private:
		ComPtr<ID3D12PipelineState> m_D3D12PipelineState;
		PipelineDesc m_desc;
	};

}