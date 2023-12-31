#pragma once

#include "../../Math/Math.h"

namespace Warp
{

	struct DirectionalLightComponent
	{
		float Intensity = 0.0f;
		Math::Vector3 Direction;
		Math::Vector3 Radiance;
	};

}