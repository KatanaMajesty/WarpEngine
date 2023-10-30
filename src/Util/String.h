#pragma once

#include <string>
#include <sstream>

namespace Warp
{
	std::string WStringToString(const wchar_t* wstr) noexcept;
	std::string WStringToString(const std::wstring& wstr) noexcept;

	std::wstring StringToWString(std::string_view view);
}