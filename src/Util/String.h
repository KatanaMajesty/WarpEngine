#pragma once

#include <string>
#include <sstream>

namespace Warp
{
    std::string WStringToString(std::wstring_view wstr) noexcept;
    std::wstring StringToWString(std::string_view view);
}