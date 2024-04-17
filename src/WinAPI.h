#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif					// Exclude rarely-used stuff from Windows headers.

#include <Windows.h>
#include <Objbase.h>
#include <stdexcept>

namespace Warp::winapi
{
 
    // wrapper over https://learn.microsoft.com/en-us/windows/win32/api/objbase/nf-objbase-coinitialize   
    struct ComLibraryWrapper
    {
        ComLibraryWrapper()
        {
            if (ComLibraryWrapper::s_isInitialized)
            {
                return;
            }

            HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
            if (FAILED(hr))
            {
                throw std::runtime_error("Could not initialize COM library");
            }
            ComLibraryWrapper::s_isInitialized = TRUE;
        }

        ~ComLibraryWrapper()
        {
            if (ComLibraryWrapper::s_isInitialized)
            {
                ComLibraryWrapper::s_isInitialized = FALSE;
                CoUninitialize();
            }
        }

        static inline BOOL IsInitialized() { return s_isInitialized; }

    private:
        static inline BOOL s_isInitialized = FALSE;
    };

}