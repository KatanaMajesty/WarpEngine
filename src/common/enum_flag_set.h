#pragma once

#include "enum_utils.h"

#include <bitset>
#include <type_traits>
#include <limits>
#include <cstdint>
#include <format>

namespace Warp
{

    /// @brief represents a strongly-typed bit mask
    ///
    /// FlagSet works with an assumption that very first enum value defined would be 0
    template<scoped_enum Enum>
    class FlagSet
    {
    public:
        using UnderlyingType = std::underlying_type_t<Enum>;
        static constexpr uint32_t NumBits = std::numeric_limits<UnderlyingType>::digits;
    
        // constructors
        constexpr FlagSet() = default;
        constexpr FlagSet(Enum e)
        {
            Set(e);
        }
        constexpr FlagSet(std::bitset<NumBits> bits)
            : m_bits(bits)
        {
        }

        constexpr UnderlyingType ToUnderlying() const noexcept
        {
            return static_cast<UnderlyingType>(m_bits.to_ullong());
        }

        constexpr operator UnderlyingType() const noexcept
        {
            return ToUnderlying();
        }

        constexpr FlagSet& Set(Enum e, bool v = true) noexcept { m_bits.set(EnumValue(e), v); return *this; }
        constexpr FlagSet& Flip(Enum e) noexcept { m_bits.flip(EnumValue(e)); return *this; }
        constexpr bool IsSet(Enum e) const noexcept { return m_bits.test(EnumValue(e)); }

        constexpr FlagSet& operator|=(FlagSet other) noexcept
        {
            m_bits |= other.m_bits;
            return *this;
        }
        constexpr FlagSet& operator&=(FlagSet other) noexcept
        {
            m_bits &= other.m_bits;
            return *this;
        }
        constexpr FlagSet& operator^=(FlagSet other) noexcept
        {
            m_bits ^= other.m_bits;
            return *this;
        }
        constexpr FlagSet operator|(FlagSet other) const noexcept { return FlagSet(m_bits | other.m_bits); }
        constexpr FlagSet operator&(FlagSet other) const noexcept { return FlagSet(m_bits & other.m_bits); }
        constexpr FlagSet operator^(FlagSet other) const noexcept { return FlagSet(m_bits ^ other.m_bits); }

        /// @brief Returns the number of set bits in this flag-set
        constexpr size_t NumSetBits() const noexcept { return m_bits.count(); }
        constexpr size_t Size() const noexcept { return m_bits.size(); }
        constexpr bool Any() const noexcept { return m_bits.any(); }
        constexpr operator bool() const noexcept { return Any(); }

        constexpr bool operator==(FlagSet other) const noexcept { return m_bits == other.m_bits; }
        constexpr bool operator!=(FlagSet other) const noexcept { return m_bits != other.m_bits; }

        constexpr bool AllOf(FlagSet other) const noexcept { return this->operator&(other) == other; }
        constexpr bool AnyOf(FlagSet other) const noexcept { return this->operator&(other).Any(); }
        constexpr bool NoneOf(FlagSet other) const noexcept { return !AnyOf(other); }
    
    private:
        std::bitset<NumBits> m_bits = {};
    };

} // Warp namespace

// Utility functions to help properly initialize flag-sets
template<Warp::scoped_enum Enum> constexpr Warp::FlagSet<Enum> operator|(Enum lhs, Enum rhs) noexcept { return Warp::FlagSet<Enum>(lhs) | Warp::FlagSet<Enum>(rhs); }
template<Warp::scoped_enum Enum> constexpr Warp::FlagSet<Enum> operator&(Enum lhs, Enum rhs) noexcept { return Warp::FlagSet<Enum>(lhs) & Warp::FlagSet<Enum>(rhs); }
template<Warp::scoped_enum Enum> constexpr Warp::FlagSet<Enum> operator^(Enum lhs, Enum rhs) noexcept { return Warp::FlagSet<Enum>(lhs) ^ Warp::FlagSet<Enum>(rhs); }
template<Warp::scoped_enum Enum> constexpr Warp::FlagSet<Enum> operator~(Enum e) noexcept { return ~Warp::FlagSet<Enum>(e); }

namespace std
{

    template<Warp::scoped_enum Enum>
    struct formatter<Warp::FlagSet<Enum>, char>
    {
        constexpr auto parse(format_parse_context& ctx) const noexcept
        { 
            return ctx.begin(); 
        }

        template<class FCtx>
        constexpr auto format(const Warp::FlagSet<Enum>& fs, FCtx& ctx) const
        {
            return std::format_to(ctx.out(), "{:0{}b}", fs.ToUnderlying(), fs.Size());
        }
    };

} // namespace std