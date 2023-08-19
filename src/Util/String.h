#pragma once

#include <string>
#include <sstream>

namespace Warp
{
	void WStringToString(std::string& output, const wchar_t* wstr) noexcept;
	void WStringToString(std::string& output, const std::wstring& wstr) noexcept;
}