#pragma once

#include <string>
#include <string_view>
#include "DirectXTex.h"

#include "../../../Core/Defines.h"

namespace Warp::ImageLoader
{

	using EImageFlags = uint16_t;
	enum  EImageFlag
	{
		eImageFlag_None = 0,

		// States that the Filepath field of an Image is not an actual path in the filesystem
		// But rather a fake-generated (still unique though) path inside of another file
		// 
		// For example, if a texture is stored inside of a .glb file along with other textures then
		// they might have a following filepath: ./assets/scene.glb/1, ./assets/scene.glb/2 and so on
		//
		// (14.02.2024) -> As of now this flag is not yet used and the intended use-cases would probably be a lie.
		// The flag would still be set for internal resources but filepaths would probably just be empty in this case
		eImageFlag_InternalStorage = 1,
	};

	struct Image
	{
		bool IsValid() const { return DxImage.GetImages() != nullptr; }

		// Filepath cannot be empty. It does not necessarily represent a valid filesystem's path though
		// If an image has eImageFlag_InternalStorage selected it will not represent a valid filesystem's path
		// For more info see eImageFlag_InternalStorage
		//
		// (14.02.2024) -> This is probably obsolete now as assets no more use filepaths but rather use randomly created guids.
		// This is soon to be removed
		/*WARP_ATTR_DEPRECATED */std::string Filepath;
		DirectX::ScratchImage DxImage;

		// Custom Image flags
		EImageFlags Flags = eImageFlag_None;
	};

	Image LoadWICFromFile(std::string_view filepath, bool generateMips);
	Image LoadDDSFromFile(std::string_view filepath, bool generateMips);

}