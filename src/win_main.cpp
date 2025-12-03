#include "warp_win32_entry_point.h"

#include <Windows.h>

#include <cstdint>
#include <vector>
#include <string_view>
#include <stdexcept>



int32_t main(int32_t argc, char* argv[])
{
    // provides the executable's module handle, which is the same as the hInstance
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    
    

    std::vector<std::string_view> args(argc);
    for (int32_t i = 0; i < argc; ++i)
        args[i] = std::string_view(argv[i]);

    Warp::EFinishCode result = Warp::Main(hInstance, args);
    if (result != Warp::EFinishCode::Success)
    {
        throw std::runtime_error("Unsuccessful run of Warp");
    }
}