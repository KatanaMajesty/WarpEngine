#pragma once

#include "stdafx.h"

namespace Warp
{

	class Renderer
	{
	public:
		Renderer() = default;
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		~Renderer() = default;

		static bool Init();
		static void Deinit();
		static inline constexpr Renderer& GetInstance() { return *s_instance; }

	private:
		bool InitD3D12Api();

		static inline Renderer* s_instance = nullptr;
	};

}