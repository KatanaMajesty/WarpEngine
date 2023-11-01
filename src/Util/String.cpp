#include "String.h"

#include <algorithm>

namespace Warp
{

	std::string WStringToString(std::wstring_view view) noexcept
	{
		std::string output = std::string(view.size(), 0);
		std::ranges::transform(view, output.begin(), [](wchar_t c) { return static_cast<char>(c); });
		return output;
	}

	std::wstring StringToWString(std::string_view view)
	{
		return std::wstring(view.begin(), view.end());
	}

}