#pragma once

#include <cstdint>
#include <functional>

namespace Warp
{

	// A GUID is a 128-bit value consisting of one group of 8 hexadecimal digits, 
	// followed by three groups of 4 hexadecimal digits each, followed by one group of 12 hexadecimal digits. 
	// The following example GUID shows the groupings of hexadecimal digits in a GUID: 6B29FC40-CA47-1067-B31D-00DD010662DA.
	struct Guid
	{
		static Guid Create();

		constexpr bool IsValid() const { return Data1 != 0 || Data2 != 0 || Data3 != 0 || Data4 != 0; }

		constexpr bool operator==(const Guid& other) const { return Data1 == other.Data1 && Data2 == other.Data2 && Data3 == other.Data3 && Data4 == other.Data4; }
		constexpr bool operator!=(const Guid& other) const { return !(operator==(other)); }

		constexpr bool operator<(const Guid& other) const { return Data1 < other.Data1&& Data2 < other.Data2&& Data3 < other.Data3&& Data4 < other.Data4; }
		constexpr bool operator>(const Guid& other) const { return Data1 > other.Data1 && Data2 > other.Data2 && Data3 > other.Data3 && Data4 > other.Data4; }

		constexpr bool operator<=(const Guid& other) const { return operator<(other) || operator==(other); }
		constexpr bool operator>=(const Guid& other) const { return operator>(other) || operator==(other); }

		uint32_t Data1 = 0;
		uint16_t Data2 = 0;
		uint16_t Data3 = 0;

		// Array of 8 bytes. The first 2 bytes contain the third group of 4 hexadecimal digits. 
		// The remaining 6 bytes contain the final 12 hexadecimal digits.
		uint64_t Data4 = 0;
	};

}

// Standard C++ Library support

template<>
struct std::hash<Warp::Guid>
{
	constexpr std::size_t operator()(const Warp::Guid& guid) const
	{
		std::size_t r = 0;
		HashCombine(r, guid.Data1);
		HashCombine(r, guid.Data2);
		HashCombine(r, guid.Data3);
		HashCombine(r, guid.Data4);
		return r;
	}

private:
	// We like doing what boost once did
	// TODO: Maybe this should be a separate piece of art?
	template<typename T>
	static constexpr void HashCombine(std::size_t& seed, const T& t)
	{
		seed ^= std::hash<T>()(t) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
};