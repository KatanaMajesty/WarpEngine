#pragma once

#include <string>
#include <sstream>

// TODO: [URGENT 07.05.24] Deprecate these! Until its too late

namespace Warp
{
    std::string WStringToString(std::wstring_view wstr) noexcept;
    std::wstring StringToWString(std::string_view view);
}