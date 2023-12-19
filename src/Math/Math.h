#pragma once

#include <numbers>
#include "directxtk12/SimpleMath.h"

namespace Warp::Math
{
	// using namespace DirectX;
	using namespace DirectX::SimpleMath;

	inline constexpr float ToRadians(float degrees)
	{
		constexpr float Pi = std::numbers::pi_v<float>;
		constexpr float ConvertionFactor = Pi / 180.0f;
		return degrees * ConvertionFactor;
	}
}