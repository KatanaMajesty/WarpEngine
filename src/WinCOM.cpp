#include "WinWrap.h"

#include <Objbase.h>
#include <stdexcept>

namespace Warp::WinWrap
{

    ScopedCOMLibrary::ScopedCOMLibrary()
    {
        // No need to initialize more than once
        if (s_isInitialized)
        {
            return;
        }

        HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
        if (FAILED(hr))
        {
            throw std::runtime_error("Could not initialize COM library");
        }
        s_isInitialized = TRUE;
    }

    ScopedCOMLibrary::~ScopedCOMLibrary() noexcept
    {
        if (s_isInitialized)
        {
            s_isInitialized = FALSE;
            CoUninitialize();
        }
    }

}