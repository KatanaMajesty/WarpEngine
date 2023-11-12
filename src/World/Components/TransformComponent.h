#pragma once

#include "../../Math/Math.h"

namespace Warp
{

	// Currently we use yaw-pitch-roll to describe rotation but in future we want quaternions prob
	struct TransformComponent
	{
		TransformComponent() = default;
		TransformComponent(const Math::Vector3& translation, const Math::Vector3& rotation, const Math::Vector3& scaling)
			: Translation(translation)
			, Rotation(rotation)
			, Scaling(scaling)
		{
		}

		Math::Vector3 Translation;
		Math::Vector3 Rotation;
		Math::Vector3 Scaling;
	};

}