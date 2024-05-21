#pragma once

#include <cstdint>
#include <cstring>

namespace Warp
{

    void* Memset(void* dest, uint8_t value, size_t bytes) noexcept
    {
        return std::memset(dest, value, bytes);
    }

    void* Memcpy(void* dest, const void* src, size_t bytes) noexcept
    {
        return std::memcpy(dest, src, bytes);
    }

    int32_t Memcmp(const void* lhs, const void* rhs, size_t bytes) noexcept
    {
        return std::memcmp(lhs, rhs, bytes);
    }

}