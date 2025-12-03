#pragma once

#include <utility> // for std::to_underlying

namespace Warp
{

    template<typename Enum>
    concept scoped_enum = std::is_scoped_enum_v<Enum>;

    template<typename Enum>
    constexpr std::underlying_type_t<Enum> EnumValue(Enum e) noexcept
    {
        return std::to_underlying<Enum>(e);
    }

} // warp namespace