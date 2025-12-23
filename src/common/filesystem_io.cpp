#include "filesystem_io.h"

#include <fstream>
#include <streambuf>

namespace Warp::fsio
{

    bool IsValidFilepath(const std::filesystem::path& filepath) noexcept
    {
        std::error_code ec;
        bool e = std::filesystem::exists(filepath, ec);
        WARP_ASSERT(!ec, "Failed to check filepath for validity: {}", ec.message());
        return e;
    }

    bool IsValidFile(const std::filesystem::path& filepath) noexcept
    {
        if (!IsValidFilepath(filepath))
            return false;

        std::error_code ec;
        bool isFile = std::filesystem::is_regular_file(filepath, ec);
        WARP_ASSERT(!ec, "Failed to check file for validity: {}", ec.message());
        return isFile;
    }

    uint64_t FileSizeInBytes(const std::filesystem::path& filepath) noexcept
    {
        if (!IsValidFilepath(filepath))
        {
            // return invalid file size if filepath is not valid
            return kInvalidFileSize;
        }
        std::error_code ec;
        uint64_t fs = std::filesystem::file_size(filepath, ec);
        WARP_ASSERT(!ec, "Failed to get filesize of {}: {}", filepath.string(), ec.message());
        return fs;
    }

    std::string ReadFile(const std::filesystem::path& filepath) noexcept
    {
        std::ifstream f(filepath);
        WARP_ASSERT(!f.fail(), "Failed to open file at path: {}", filepath.string());

        return std::string(std::istreambuf_iterator<char>(f), {});
    }

    std::vector<std::byte> ReadBinaryFile(const std::filesystem::path& filepath) noexcept
    {
        std::ifstream f(filepath, std::ios::binary);
        WARP_ASSERT(!f.fail(), "Failed to open file at path: {}", filepath.string());
        
        return std::vector<std::byte>(std::istreambuf_iterator<std::byte>(), {});
    }

} // Warp::fsio namespace