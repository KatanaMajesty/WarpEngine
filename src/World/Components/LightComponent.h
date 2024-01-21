#pragma once

#include "../../Math/Math.h"
#include "../../Renderer/RHI/Resource.h"
#include "../../Renderer/RHI/Descriptor.h"

namespace Warp
{

	struct DirectionalLightComponent
	{
		float Intensity = 0.0f;
		Math::Vector3 Direction;
		Math::Vector3 Radiance;
		bool CastsShadow = true;
	};

	struct DirectionalLightShadowmappingComponent
	{
		Math::Matrix LightView;
		Math::Matrix LightProj;
		Math::Matrix LightInvView;
		Math::Matrix LightInvProj;
		RHITexture DepthMap;
		RHIDepthStencilView DepthMapView;
		RHIShaderResourceView DepthMapSrv;
	};

	// TODO: Unused still. Will we even use this before stochastic shadows?
	struct OmnidirectionalLightShadowmappingComponent
	{
		Math::Matrix LightMatrix[6];
		Math::Matrix LightInvMatrix[6];
		RHITexture DepthCubemap;
	};

}