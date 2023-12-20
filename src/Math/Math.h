#pragma once

#include <numbers>
#include <cmath>
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

	inline constexpr float Clamp(float v, float lo, float hi)
	{
		if (v < lo)
			return lo;
		if (v > hi)
			return hi;
		return v;
	}
}