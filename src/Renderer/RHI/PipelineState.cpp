#include "PipelineState.h"

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

        RHIPipelineElementDesc(const ElementType& element)
            : Type(SubobjectType)
            , Element(element)
        {
        }

        inline constexpr ElementType& operator->() { return Element; }

        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type;
        ElementType Element;
    };

    using RHIPipelineElement_RootSignature = RHIPipelineElementDesc<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature*>;
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

    struct GpuGraphicsPipelineStream
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

    struct GpuMeshPipelineStream
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

    struct GpuComputePipelineStream
    {
        RHIPipelineElement_RootSignature RootSignature;
        RHIPipelineElement_CS CS;
        RHIPipelineElement_Flags Flags;
    };

    template<typename PipelineDesc>
    struct PipelineDescTraits
    {
    };

    template<>
    struct PipelineDescTraits<RHIGraphicsPipelineDesc>
    {
        static GpuGraphicsPipelineStream ToRHIStream(const RHIGraphicsPipelineDesc& desc)
        {
            GpuGraphicsPipelineStream stream;
            stream.RootSignature = desc.RootSignature.GetD3D12RootSignature();
            stream.VS = desc.VS;
            stream.PS = desc.PS;
            stream.BlendState = desc.BlendState;
            stream.Rasterizer = desc.Rasterizer;
            stream.DepthStencil = desc.DepthStencil;
            stream.PrimitiveType = desc.PrimitiveType;
            stream.RTVFormats = CD3DX12_RT_FORMAT_ARRAY(desc.RTVFormats, desc.NumRTVs);
            stream.DSVFormat = desc.DSVFormat;
            stream.Flags = desc.Flags;
            return stream;
        }
    };

    template<>
    struct PipelineDescTraits<RHIMeshPipelineDesc>
    {
        static GpuMeshPipelineStream ToRHIStream(const RHIMeshPipelineDesc& desc)
        {
            GpuMeshPipelineStream stream;
            stream.RootSignature = desc.RootSignature.GetD3D12RootSignature();
            stream.AS = desc.AS;
            stream.MS = desc.MS;
            stream.PS = desc.PS;
            stream.BlendState = desc.BlendState;
            stream.Rasterizer = desc.Rasterizer;
            stream.DepthStencil = desc.DepthStencil;
            stream.PrimitiveType = desc.PrimitiveType;
            stream.RTVFormats = CD3DX12_RT_FORMAT_ARRAY(desc.RTVFormats, desc.NumRTVs);
            stream.DSVFormat = desc.DSVFormat;
            stream.Flags = desc.Flags;
            return stream;
        }
    };

    template<>
    struct PipelineDescTraits<RHIComputePipelineDesc>
    {
        static GpuComputePipelineStream ToRHIStream(const RHIComputePipelineDesc& desc)
        {
            GpuComputePipelineStream stream;
            stream.RootSignature = desc.RootSignature.GetD3D12RootSignature();
            stream.CS = desc.CS;
            stream.Flags = desc.Flags;
            return stream;
        }
    };

    RHIGraphicsPipelineState::RHIGraphicsPipelineState(RHIDevice* device, const RHIGraphicsPipelineDesc& desc)
        : RHIPipelineState(device)
        , m_desc(desc)
    {
        auto stream = PipelineDescTraits<RHIGraphicsPipelineDesc>::ToRHIStream(desc);

        D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc;
        StreamDesc.SizeInBytes = sizeof(decltype(stream));
        StreamDesc.pPipelineStateSubobjectStream = &stream;
        WARP_RHI_VALIDATE(device->GetD3D12Device()->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(m_D3D12PipelineState.GetAddressOf())));
    }

    RHIMeshPipelineState::RHIMeshPipelineState(RHIDevice* device, const RHIMeshPipelineDesc& desc)
        : RHIPipelineState(device)
        , m_desc(desc)
    {
        auto stream = PipelineDescTraits<RHIMeshPipelineDesc>::ToRHIStream(desc);

        D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc;
        StreamDesc.SizeInBytes = sizeof(decltype(stream));
        StreamDesc.pPipelineStateSubobjectStream = &stream;
        WARP_RHI_VALIDATE(device->GetD3D12Device()->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(m_D3D12PipelineState.GetAddressOf())));
    }

    RHIComputePipelineState::RHIComputePipelineState(RHIDevice* device, const RHIComputePipelineDesc& desc)
        : RHIPipelineState(device)
        , m_desc(desc)
    {
        auto stream = PipelineDescTraits<RHIComputePipelineDesc>::ToRHIStream(desc);

        D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc;
        StreamDesc.SizeInBytes = sizeof(decltype(stream));
        StreamDesc.pPipelineStateSubobjectStream = &stream;
        WARP_RHI_VALIDATE(device->GetD3D12Device()->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(m_D3D12PipelineState.GetAddressOf())));
    }

    void RHIPipelineState::SetName(std::wstring_view name)
    {
        WARP_SET_RHI_NAME(m_D3D12PipelineState.Get(), name);
    }

}