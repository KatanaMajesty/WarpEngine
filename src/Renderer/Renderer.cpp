#include "Renderer.h"

#include <cmath>
#include "../Util/String.h"
#include "../Core/Logger.h"
#include "../Core/Assert.h"
#include "../Core/Application.h"

// TODO: Temp, remove
#include <DirectXMath.h>

namespace Warp
{

	struct alignas(256) ConstantBuffer
	{
		DirectX::XMMATRIX model;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
	};

	Renderer::Renderer(HWND hwnd)
		: m_physicalDevice(
			[hwnd]() -> std::unique_ptr<GpuPhysicalDevice>
			{
				GpuPhysicalDeviceDesc physicalDeviceDesc;
				physicalDeviceDesc.hwnd = hwnd;
#ifdef WARP_DEBUG
				physicalDeviceDesc.EnableDebugLayer = true;
				physicalDeviceDesc.EnableGpuBasedValidation = true;
#else
				physicalDeviceDesc.EnableDebugLayer = false;
				physicalDeviceDesc.EnableGpuBasedValidation = false;
#endif
				return std::make_unique<GpuPhysicalDevice>(physicalDeviceDesc);
			}()
		)
		, m_device(m_physicalDevice->GetAssociatedLogicalDevice())
		, m_commandContext(m_device->GetGraphicsQueue())
		, m_swapchain(std::make_unique<RHISwapchain>(m_physicalDevice.get()))
	{

		D3D12_FEATURE_DATA_D3D12_OPTIONS7 feature; 
		m_device->CheckLogicalDeviceFeatureSupport<D3D12_FEATURE_D3D12_OPTIONS7>(feature);
		WARP_ASSERT(feature.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1, "Cannot run with no mesh shader support");

		// TODO: Temp, remove
		if (!InitAssets())
		{
			WARP_LOG_ERROR("Failed to init assets");
			return;
		}

		WARP_LOG_INFO("Renderer was initialized successfully");
	}
	
	Renderer::~Renderer()
	{
		WaitForGfxToFinish();
		m_device->GetComputeQueue()->HostWaitIdle();
		m_device->GetCopyQueue()->HostWaitIdle();
	}

	void Renderer::Resize(uint32_t width, uint32_t height)
	{
		// Wait for all frames to be completed before resizing the backbuffers
		// as they may still be used
		WaitForGfxToFinish();
		m_swapchain->Resize(width, height);
	}

	void Renderer::RenderFrame()
	{
		// Wait for inflight frame of the currently used backbuffer
		UINT frameIndex = m_swapchain->GetCurrentBackbufferIndex();
		WaitForGfxOnFrameToFinish(frameIndex);

		m_device->BeginFrame();
		{
			// Populate command lists
			// TODO: Remove this
			PopulateCommandList();

			m_frameFenceValues[frameIndex] = m_commandContext.Execute(false);
			m_swapchain->Present(true);
		}
		m_device->EndFrame();
	}

	void Renderer::Update(float timestep)
	{
		m_timeElapsed += timestep;
		ConstantBuffer cb;
		cb.model = DirectX::XMMatrixAffineTransformation(
			DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f),
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMQuaternionRotationRollPitchYaw(
				m_timeElapsed * 0.5f,
				m_timeElapsed * 1.0f,
				m_timeElapsed * 0.75f
			),
			DirectX::XMVectorSet(0.0f, 0.0f, 2.0f, 0.0f)
		);
		cb.view = DirectX::XMMatrixIdentity();

		uint32_t width = m_swapchain->GetWidth();
		uint32_t height = m_swapchain->GetHeight();
		float aspectRatio = static_cast<float>(width) / height;
		cb.proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.0f), aspectRatio, 0.1f, 100.0f);

		ConstantBuffer* data = m_constantBuffer.GetCpuVirtualAddress<ConstantBuffer>();
		memcpy(data, &cb, sizeof(ConstantBuffer));
	}

	void Renderer::WaitForGfxOnFrameToFinish(uint32_t frameIndex)
	{
		m_device->GetGraphicsQueue()->HostWaitForValue(m_frameFenceValues[frameIndex]);
	}

	void Renderer::WaitForGfxToFinish()
	{
		m_device->GetGraphicsQueue()->HostWaitIdle();
	}

	bool Renderer::InitAssets()
	{
		// TODO: Temporary
		ID3D12Device* d3dDevice = m_device->GetD3D12Device();

		if (!InitShaders())
		{
			WARP_LOG_ERROR("Failed to init shaders");
			return false;
		}

		m_rootSignature = RHIRootSignature(m_device, 
			RHIRootSignatureDesc(1, 0).AddConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL)); // TODO: Figure out why MESH visibility wont work

		RHIMeshPipelineDesc cubePsoDesc = {};
		cubePsoDesc.RootSignature = m_rootSignature;
		cubePsoDesc.MS = m_cubeMs.GetBinaryBytecode();
		cubePsoDesc.PS = m_cubePs.GetBinaryBytecode();
		cubePsoDesc.DepthStencil.DepthEnable = FALSE;
		cubePsoDesc.DepthStencil.StencilEnable = FALSE;
		cubePsoDesc.NumRTVs = 1;
		cubePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_cubePso = RHIMeshPipelineState(m_device, cubePsoDesc);

		WARP_ASSERT(DirectX::XMVerifyCPUSupport(), "Cannot use DirectXMath for the provided CPU");
		
		ConstantBuffer cb;
		cb.model = DirectX::XMMatrixIdentity();
		cb.view = DirectX::XMMatrixIdentity();
		cb.proj = DirectX::XMMatrixIdentity();

		m_constantBuffer = m_device->CreateBuffer(sizeof(ConstantBuffer), sizeof(ConstantBuffer));
		WARP_ASSERT(m_constantBuffer.IsValid());

		ConstantBuffer* data = m_constantBuffer.GetCpuVirtualAddress<ConstantBuffer>();
		memcpy(data, &cb, sizeof(ConstantBuffer));

		WaitForGfxToFinish();
		return true;
	}

	bool Renderer::InitShaders()
	{
		std::string filepath = (Application::Get().GetShaderPath() / "Triangle.hlsl").string();
		std::string badCubeShader = (Application::Get().GetShaderPath() / "CubeBad.hlsl").string();
		ShaderCompilationDesc msShaderDesc = ShaderCompilationDesc("MSMain", EShaderModel::sm_6_5, EShaderType::Mesh)
			.AddDefine(ShaderDefine("MAX_OUTPUT_VERTICES", "256"))
			.AddDefine(ShaderDefine("MAX_OUTPUT_PRIMITIVES", "256")); // TODO: Defines do not work as expected

		ShaderCompilationDesc psShaderDesc = ShaderCompilationDesc("PSMain", EShaderModel::sm_6_5, EShaderType::Pixel);

		m_cubeMs = m_shaderCompiler.CompileShader(badCubeShader, msShaderDesc);
		m_cubePs = m_shaderCompiler.CompileShader(badCubeShader, psShaderDesc);
		return m_cubeMs.GetBinaryPointer() && m_cubePs.GetBinaryPointer();
	}

	void Renderer::PopulateCommandList()
	{
		m_commandContext.Open();
		m_commandContext.SetPipelineState(m_cubePso);

		UINT width = m_swapchain->GetWidth();
		UINT height = m_swapchain->GetHeight();
		m_commandContext.SetViewport(0, 0, width, height);
		m_commandContext.SetScissorRect(0, 0, width, height);

		// Indicate that the back buffer will be used as a render target.
		UINT currentBackbufferIndex = m_swapchain->GetCurrentBackbufferIndex();
		GpuTexture* backbuffer = m_swapchain->GetBackbuffer(currentBackbufferIndex);
		m_commandContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE backbufferRtv = m_swapchain->GetRtvDescriptor(currentBackbufferIndex);
		m_commandContext->OMSetRenderTargets(1, &backbufferRtv, FALSE, nullptr);

		const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_commandContext->ClearRenderTargetView(backbufferRtv, clearColor, 0, nullptr);

		m_commandContext.SetGraphicsRootSignature(m_rootSignature);
		m_commandContext->SetGraphicsRootConstantBufferView(0, m_constantBuffer.GetGpuVirtualAddress());
		m_commandContext.DispatchMesh(1, 1, 1);

		// Indicate that the back buffer will now be used to present.
		m_commandContext.AddTransitionBarrier(backbuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_commandContext.Close();
	}

}