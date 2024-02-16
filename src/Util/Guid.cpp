#include "Guid.h"

// WinApis GUID and CoCreateGuid
#include <combaseapi.h>

namespace Warp
{

	Guid Guid::Create()
	{
		GUID winGuid;
		HRESULT hr = CoCreateGuid(&winGuid);
		if (FAILED(hr))
		{
			return Guid();
		}

		Guid result;
		result.Data1 = winGuid.Data1;
		result.Data2 = winGuid.Data2;
		result.Data3 = winGuid.Data3;
		for (uint32_t i = 0; i < 8; ++i)
		{
			result.Data4 |= static_cast<uint64_t>(winGuid.Data4[i] << (8 * i));
		}

		return result;
	}

}