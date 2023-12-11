#pragma once

#include "../Assets/Asset.h"
#include "../Assets/AssetManager.h"

namespace Warp
{

	struct MeshMaterial
	{
		AssetProxy BaseColorProxy;
		AssetProxy NormalMapProxy;
		AssetProxy RoughnessMapProxy;
		AssetProxy MetalnessMapProxy;
		AssetManager* Manager;
	};

	struct Mesh
	{
		AssetProxy MeshProxy;
		AssetManager* Manager;
	};

}