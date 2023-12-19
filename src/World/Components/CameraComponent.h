#pragma once

#include "../../Math/Math.h"

namespace Warp
{

	struct CameraComponent
	{
		inline void SetView()
		{
			Math::Vector3 EyeTarget = EyePos + EyeDir;
			EyeTarget.Normalize();

			ViewMatrix = Math::Matrix::CreateLookAt(EyePos, EyeTarget, UpDir);
			ViewMatrix.Invert(ViewInvMatrix);
		}

		inline void SetProjection(uint32_t width, uint32_t height)
		{
			float AspectRatio = static_cast<float>(width) / height;
			ProjMatrix = Math::Matrix::CreatePerspectiveFieldOfView(Math::ToRadians(Fov), AspectRatio, NearPlane, FarPlane);
			ProjMatrix.Invert(ProjInvMatrix);
		}

		Math::Vector3 EyePos;
		Math::Vector3 EyeDir;
		Math::Vector3 UpDir;

		Math::Matrix ViewMatrix;
		Math::Matrix ViewInvMatrix;

		float Fov = 90.0f;
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;

		Math::Matrix ProjMatrix;
		Math::Matrix ProjInvMatrix;
	};

	// struct CameraQuaternionComponent
	// {
	// 	
	// };

}