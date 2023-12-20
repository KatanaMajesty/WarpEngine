#pragma once

#include "../../Math/Math.h"

namespace Warp
{

	struct EulersCameraComponent
	{
		inline void SetView()
		{
			float rYaw = Math::ToRadians(Yaw);
			float rPitch = Math::ToRadians(Pitch);

			EyeDir.x = std::cos(rYaw) * std::cos(rPitch);
			EyeDir.y = std::sin(rPitch);
			EyeDir.z = std::sin(rYaw) * std::cos(rPitch);
			EyeDir.Normalize();

			ViewMatrix = Math::Matrix::CreateLookAt(EyePos, EyePos + EyeDir, UpDir);
			ViewMatrix.Invert(ViewInvMatrix);
		}

		inline void SetProjection(uint32_t width, uint32_t height)
		{
			float AspectRatio = static_cast<float>(width) / height;
			ProjMatrix = Math::Matrix::CreatePerspectiveFieldOfView(Math::ToRadians(Fov), AspectRatio, NearPlane, FarPlane);
			ProjMatrix.Invert(ProjInvMatrix);
		}

		float Pitch = 90.0f;
		float Yaw = 0.0f;
		// float Roll = 0.0f; // No roll for Eulers currently

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