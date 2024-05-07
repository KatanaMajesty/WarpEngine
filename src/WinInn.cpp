#include "WinWrap.h"

#include <iterator>
#include <stdexcept>

#include "Core/Defines.h"
#include "Util/String.h"

namespace Warp::WinWrap::Inn
{
        
        std::vector<std::string> ConvertCmdLineArguments(std::wstring_view cmdLine)
        {
            std::string args = Warp::WStringToString(cmdLine);
            return ConvertCmdLineArguments(cmdLine);
        }

        std::vector<std::string> ConvertCmdLineArguments(std::string_view cmdLine)
        {
            using IStreamIterator_type = std::istream_iterator<std::string>;

            auto iss    = std::istringstream(std::string(cmdLine));
            auto args   = std::vector<std::string>(IStreamIterator_type(iss), IStreamIterator_type());
            return args;
        }

        std::filesystem::path GetModuleDirectory()
        {
            char rawPath[MAX_PATH];

            // this will always return null-termination character
            if (GetModuleFileNameA(nullptr, rawPath, MAX_PATH) == MAX_PATH && 
                GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                // Handle insufficient buffer
                throw std::runtime_error("Insufficient buffer space for path to be retrieved");
                return {};
            }

            // This std::string constructor will seek for the null-termination character;
            std::filesystem::path path = std::filesystem::path(std::string(rawPath));

            // get directory now
            return path.parent_path(); 
        }

        BOOL InitConsole()
        {
            if (!AllocConsole())
            {
                // Console is already allocated perhaps?
                return FALSE;
            }
            return TRUE;
        }

        BOOL StopConsole()
        {
            return FreeConsole();
        }

}