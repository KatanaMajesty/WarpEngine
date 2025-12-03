#if defined(_WIN32)

#define NOMINMAX
#include <Windows.h>

#include <span>
#include <string_view>

namespace Warp
{

    enum class EFinishCode
    {
        Success = 0,
        Error,
    };

    EFinishCode Main(HINSTANCE instance, std::span<std::string_view> args);

} // Warp namespace

#endif // defined(_WIN32)