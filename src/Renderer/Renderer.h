#pragma once

#include <vector>
#include <array>

#include "../Core/Defines.h"
#include "RHI/stdafx.h"
#include "RHI/Device.h"
#include "RHI/Descriptor.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/PhysicalDevice.h"
#include "RHI/Resource.h"
#include "RHI/ResourceTrackingContext.h"
#include "RHI/CommandContext.h"
#include "RHI/PipelineState.h"
#include "RHI/Swapchain.h"
#include "RHI/RootSignature.h"
#include "ShaderCompiler.h"
#include "../Math/Math.h"

namespace Warp
{

    class World;

    // TODO: Temporarily moved out of Renderer to allow to use with RenderOpts
    enum EGbufferType
    {
        eGbufferType_Albedo,
        eGbufferType_Normal,
        eGbufferType_RoughnessMetalness,
        eGbufferType_NumTypes,
    };

    // TODO: Temporary, remove. Just to player around with old plain renderer
    struct RenderOpts
    {
        EGbufferType ViewGbuffer = EGbufferType::eGbufferType_NumTypes;
    };

    class Renderer
    {
    public:
        Renderer() = default;
        Renderer(HWND hwnd);

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        ~Renderer();

        void Resize(uint32_t width, uint32_t height);
        void Render(World* world, const RenderOpts& opts);

        static constexpr uint32_t SimultaneousFrames = RHISwapchain::BackbufferCount;

        // TODO: Remove these to private to start rewriting bad code
        inline RHIPhysicalDevice* GetPhysicalDevice() const { return m_physicalDevice.get(); }
        inline RHIDevice* GetDevice() const { return m_device.get(); }

        inline RHICommandContext& GetGraphicsContext() { return m_graphicsContext; }
        inline RHICommandContext& GetComputeContext() { return m_computeContext; }
        inline RHICopyCommandContext& GetCopyContext() { return m_copyContext; }

    private:
        // Waits for graphics queue to finish executing on the particular specified frame or on all frames
        void WaitForGfxOnFrameToFinish(uint32_t frameIndex);
        void WaitForGfxToFinish();

        std::unique_ptr<RHIPhysicalDevice> m_physicalDevice;
        std::unique_ptr<RHIDevice> m_device;
        std::unique_ptr<RHISwapchain> m_swapchain;

        UINT64 m_frameFenceValues[SimultaneousFrames];
        RHICommandContext m_graphicsContext;
        RHICommandContext m_computeContext;
        RHICopyCommandContext m_copyContext;

        CShaderCompiler m_shaderCompiler;

        // TODO: Remove/move
        void InitSceneDepth();
        void ResizeSceneDepth();
        RHITexture m_sceneDepth;
        RHIDepthStencilView m_sceneDepthDsv;
        RHIShaderResourceView m_sceneDepthSrv;

        void InitBase();
        RHIRootSignature m_baseRootSignature;
        RHIMeshPipelineState m_basePSO;
        CShader m_MSBase;
        CShader m_PSBase;

        // TODO: Remove entirely when rendergraph
        void InitGbuffers();
        void ResizeGbuffers();
        struct Gbuffer
        {
            RHITexture Buffer;
            RHIRenderTargetView Rtv;
            RHIShaderResourceView Srv;
        };
        std::array<Gbuffer, eGbufferType_NumTypes> m_gbuffers;
        RHIDescriptorAllocation m_gbufferRtvs;
        RHIDescriptorAllocation m_gbufferSrvs;

        void InitDirectionalShadowmapping();
        RHIDescriptorAllocation m_directionalShadowingSrvs;
        RHIRootSignature m_directionalShadowingSignature;
        RHIMeshPipelineState m_directionalShadowingPSO;
        CShader m_MSDirectionalShadowing;

        void InitDeferredLighting();
        RHIRootSignature m_deferredLightingSignature;
        RHIGraphicsPipelineState m_deferredLightingPSO;
        CShader m_VSDeferredLighting;
        CShader m_PSDeferredLighting;

        void InitGbufferView();
        RHIRootSignature m_gbufferViewSignature;
        RHIGraphicsPipelineState m_gbufferViewPSO;
        CShader m_VSGbufferView;
        CShader m_PSGbufferView;

        void AllocateGlobalCbuffers();
        static constexpr size_t SizeOfGlobalCb = 64 * 16384;
        RHIBuffer m_constantBuffers[SimultaneousFrames];
    };
}