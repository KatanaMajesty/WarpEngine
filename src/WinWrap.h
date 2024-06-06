#pragma once

#include "WinAPI.h"
#include <vector>
#include <string_view>
#include <filesystem>

// WinWrap namespace is a wrapper around WinAPI for main Warp's features
namespace Warp::WinWrap
{

    static constexpr std::string_view WindowDefaultClassName    = "WarpEngine_Class";
    static constexpr std::string_view WindowDefaultName         = "Warp Engine";

    // wrapper over COM Library initialization context
    // Only one initialized context can be present at a time, no more needed now
    // More info -> https://learn.microsoft.com/en-us/windows/win32/api/objbase/nf-objbase-coinitialize
    class ScopedCOMLibrary
    {
    public:
        ScopedCOMLibrary();

        ScopedCOMLibrary(const ScopedCOMLibrary&) = delete;
        ScopedCOMLibrary& operator=(const ScopedCOMLibrary&) = delete;

        ~ScopedCOMLibrary() noexcept;

        static inline BOOL IsInitialized() { return s_isInitialized; }

    private:
        static inline BOOL s_isInitialized = FALSE;
    };

    // For more info on keyboards - https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
    // For more info on mouses - https://learn.microsoft.com/en-us/windows/win32/inputdev/mouse-input
    void WinProc_ProcessInput(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void InitInputMappings();

    // returns formatted command line which is easier to work with. Returns empty vector if failed
    std::vector<std::string> ConvertCmdLineArguments(std::wstring_view cmdLine);
    std::vector<std::string> ConvertCmdLineArguments(std::string_view cmdLine);

    // Retrieves the fully qualified path for the file that contains the current module
    // TODO: Be careful with using this as it needs testing. Using shortcuts is not tested
    std::filesystem::path GetModuleDirectory();

    // Allocates/Destroyes a console for the calling process.
    // A process can be associated with only one console,
    // so the InitConsole function returns FALSE if the calling process already has a console
    BOOL InitConsole();
    BOOL StopConsole();

}