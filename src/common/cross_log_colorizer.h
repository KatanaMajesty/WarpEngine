#pragma once

#include "enum_utils.h"

#include <concepts>
#include <format>

namespace Warp
{

enum class ELogFgColor
{
    Black = 30,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    Bright_Black = 90,
    Bright_Red,
    Bright_Green,
    Bright_Yellow,
    Bright_Blue,
    Bright_Magenta,
    Bright_Cyan,
    Bright_White,
    Reset = 39
};

enum class ELogBgColor
{
    Black = 40,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    Bright_Black = 100,
    Bright_Red,
    Bright_Green,
    Bright_Yellow,
    Bright_Blue,
    Bright_Magenta,
    Bright_Cyan,
    Bright_White,
    Reset = 49
};

enum class ELogStyle
{
    Reset = 0, // clears all
    Bold = 1,
    Dim = 2,
    Italic = 3,
    Underline = 4,
    Blink = 5,
    Reverse = 7,
    Hidden = 8,
    Strike = 9
};

template <typename T>
concept LogColorEnum = std::same_as<T, ELogFgColor> || std::same_as<T, ELogBgColor> || std::same_as<T, ELogStyle>;

} // namespace Warp

// std::formatter specialisations so the enums print as escape sequences.
namespace std
{
template <Warp::LogColorEnum E>
struct formatter<E, char>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(E e, format_context& ctx) const { return format_to(ctx.out(), "\x1b[{}m", Warp::EnumValue(e)); }
};
} // namespace std