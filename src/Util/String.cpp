#include "String.h"

#include <algorithm>

namespace Warp
{

	std::string WStringToString(const wchar_t* wstr) noexcept
	{
		return WStringToString((std::wstring)wstr);
	}

	std::string WStringToString(const std::wstring& wstr) noexcept
	{
		std::string output = std::string(wstr.size(), 0);
		std::ranges::transform(wstr, output.begin(), [](wchar_t c) { return static_cast<char>(c); });
		return output;
	}

	std::wstring StringToWString(std::string_view view)
	{
		return std::wstring(view.begin(), view.end());
	}

}