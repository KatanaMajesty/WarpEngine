#pragma once

#include "../../Assets/Asset.h"
#include "../../Assets/AssetManager.h"
#include "../../Assets/MeshAsset.h"

namespace Warp
{

	// TODO: Currently its just a mesh component. In future it might become different mesh components
	// like static mesh component, skeletal mesh component, etc
	struct MeshComponent
	{
		MeshComponent() = default;
		MeshComponent(AssetManager* manager, AssetProxy proxy)
			: Manager(manager)
			, Proxy(proxy)
		{
		}

		MeshAsset* GetMesh() { return Manager->GetAs<MeshAsset>(Proxy); }

		AssetManager* Manager;
		AssetProxy Proxy;
	};

}