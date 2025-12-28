#pragma once

#include "common/assert.h"
#include "common/attr_defs.h"

#include <filesystem>
#include <string>
#include <vector>
#include <span>
#include <cstdint>
#include <type_traits>
#include <ranges>

namespace Warp::fsio
{

    /// @brief Returns true if filepath is valid (exists) or false otherwise.
    /// Works with directories and files and uses std::filesystem::exists under the hood.
    bool IsValidFilepath(const std::filesystem::path& filepath) noexcept;

    /// @brief Returns true if filepath is a valid file, checked by a call to std::filesystem::is_regular_file
    bool IsValidFile(const std::filesystem::path& filepath) noexcept;

    static constexpr uint64_t kInvalidFileSize = UINT64_MAX;

    /// @brief Queries size of a file path provided.
    ///
    /// If filepath does not represent a file then kInvalidFileSize is returned.
    /// If filepath does not exist then kInvalidFileSize is returned as well.
    /// @returns Otherwise returns a size of file in bytes.
    WARP_A_NODISCARD("Query on a file handle")
    uint64_t FileSizeInBytes(const std::filesystem::path& filepath) noexcept;

    std::string ReadFile(const std::filesystem::path& filepath) noexcept;

    std::vector<std::byte> ReadBinaryFile(const std::filesystem::path& filepath) noexcept;

    /// @brief Almost the same as ReadBinaryFile for std::byte output, but also performs a check
    /// to make sure that file size is suitable with alignment of requested output type T.
    template<typename T>
        requires(std::is_integral_v<T>)
    std::vector<T> ReadBinaryFile(const std::filesystem::path& filepath) noexcept
    {
        WARP_A_MAYBE_UNUSUED std::uintmax_t fs = FileSizeInBytes(filepath);
        WARP_ASSERT(fs != kInvalidFileSize, "Failed to query filesize for {}", filepath.string());
        WARP_ASSERT(fs % sizeof(T) == 0,
                    "Filesize of {} is not properly aligned to type T. Filesize is {}, alignment of T is {}",
                    filepath.string(),
                    fs,
                    sizeof(T));

        std::ifstream f(filepath, std::ios::binary);
        WARP_ASSERT(!f.fail(), "Failed to open file at path: {}", filepath.string());
        return std::vector<T>(std::istreambuf_iterator<T>(), {});
    }

    void WriteBinaryFile(const std::filesystem::path& filepath, std::span<const std::byte> bytes) noexcept;

    template<typename ArrayType>
        requires(std::ranges::contiguous_range<ArrayType> &&                // so that we can safely use std::ranges::data
                 std::ranges::sized_range<ArrayType> &&                     // so that we can safely use std::ranges::size
                 std::is_integral_v<std::ranges::range_value_t<ArrayType>>) // require range value type to be integral
    void WriteBinaryFile(const std::filesystem::path& filepath, ArrayType&& array) noexcept
    {
        std::span s = { std::ranges::data(array), std::ranges::size(array) };
        WriteBinaryFile(filepath, std::as_bytes(s));
    }

} // Warp::fsio namespace