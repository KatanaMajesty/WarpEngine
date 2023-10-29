#include "String.h"

#include <algorithm>

namespace Warp
{

	void WStringToString(std::string& output, const wchar_t* wstr) noexcept
	{
		WStringToString(output, (std::wstring)wstr);
	}

	void WStringToString(std::string& output, const std::wstring& wstr) noexcept
	{
		output = std::string(wstr.size(), 0);
		std::ranges::transform(wstr, output.begin(), [](wchar_t c) { return static_cast<char>(c); });
	}

	std::wstring StringToWString(std::string_view view)
	{
		return std::wstring(view.begin(), view.end());
	}

}