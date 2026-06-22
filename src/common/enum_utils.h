#pragma once

#include <utility> // for std::to_underlying
#include <string_view>

namespace Warp
{

template <typename Enum>
constexpr std::underlying_type_t<Enum> EnumValue(Enum e) noexcept
{
    return std::to_underlying<Enum>(e);
}

template <typename Enum>
    requires(std::is_scoped_enum_v<Enum>)
constexpr std::string_view EnumString(Enum e)
{
    return "UnknownEnumValue";
}

} // namespace Warp