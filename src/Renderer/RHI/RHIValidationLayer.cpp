#include "RHIValidationLayer.h"

#include "../../Core/Assert.h"
#include "../../Core/Defines.h"
#include "../../Core/Logger.h"

namespace Warp::ValidationLayer
{

	static std::string_view GetMessageCategoryAsStr(D3D12_MESSAGE_CATEGORY category)
	{
		switch (category)
		{
		case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED: return "APP_DEFINED";
		case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS: return "MISC";
		case D3D12_MESSAGE_CATEGORY_INITIALIZATION: return "INIT";
		case D3D12_MESSAGE_CATEGORY_CLEANUP: return "CLEANUP";
		case D3D12_MESSAGE_CATEGORY_COMPILATION: return "COMPILATION";
		case D3D12_MESSAGE_CATEGORY_STATE_CREATION: return "STATE_CREATION";
		case D3D12_MESSAGE_CATEGORY_STATE_SETTING: return "STATE_SETTING";
		case D3D12_MESSAGE_CATEGORY_STATE_GETTING: return "STATE_GETTING";
		case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION: return "RESOURCE_MANIP";
		case D3D12_MESSAGE_CATEGORY_EXECUTION: return "EXECUTION";
		case D3D12_MESSAGE_CATEGORY_SHADER: return "SHADER";
		WARP_ATTR_UNLIKELY default: WARP_ASSERT(false); return "UNKNOWN";
		}
	}

	static std::string_view GetMessageSeverityAsStr(D3D12_MESSAGE_SEVERITY severity)
	{
		switch (severity)
		{
		case D3D12_MESSAGE_SEVERITY_CORRUPTION: return "CORRUPTION";
		case D3D12_MESSAGE_SEVERITY_ERROR: return "ERROR";
		case D3D12_MESSAGE_SEVERITY_WARNING: return "WARNING";
		case D3D12_MESSAGE_SEVERITY_INFO: return "INFO";
		case D3D12_MESSAGE_SEVERITY_MESSAGE: return "MESSAGE";
		WARP_ATTR_UNLIKELY default: WARP_ASSERT(false); return "UNKNOWN";
		}
	}

	void OnDebugLayerMessage(D3D12_MESSAGE_CATEGORY category,
		D3D12_MESSAGE_SEVERITY severity,
		D3D12_MESSAGE_ID ID,
		LPCSTR description,
		void* vpDevice)
	{
		std::string_view pCategory = GetMessageCategoryAsStr(category);
		std::string_view pSeverity = GetMessageSeverityAsStr(severity);
		std::string Message = std::format("(Severity: {}) [Category: {}], ID {}: {}",
			pSeverity,
			pCategory,
			static_cast<UINT>(ID),
			description);

		switch (severity)
		{
		case D3D12_MESSAGE_SEVERITY_CORRUPTION:
		case D3D12_MESSAGE_SEVERITY_ERROR:
		{
			WARP_LOG_ERROR(Message);
		}; break;
		case D3D12_MESSAGE_SEVERITY_WARNING:
		{
			WARP_LOG_WARN(Message);
		}; break;
		case D3D12_MESSAGE_SEVERITY_INFO:
		case D3D12_MESSAGE_SEVERITY_MESSAGE:
		{
			WARP_LOG_INFO(Message);
		}; break;
		WARP_ATTR_UNLIKELY default: WARP_ASSERT(false); break;
		}
	}

}