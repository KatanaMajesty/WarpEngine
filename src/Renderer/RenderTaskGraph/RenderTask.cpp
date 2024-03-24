#include "RenderTask.h"

#include "Defines.h"

namespace Warp
{

    RTGRenderTask& RTGRenderTask::AddResourceDependency(const RTGResourceProxy& resource, ERTGDependencyType type)
    {
        switch (type)
        {
        case ERTGDependencyType::Read:
            AddReadDependency(resource);
            break;

        case ERTGDependencyType::Write:
            AddWriteDependency(resource);
            break;

        case ERTGDependencyType::ReadWrite:
            AddReadDependency(resource);
            AddWriteDependency(resource);
            break;

        WARP_ATTR_UNLIKELY default :
            WARP_RTG_ASSERT(false, "Should not happen! Unhandled dependency type in code");
            break;
        }

        return *this;
    }

    bool RTGRenderTask::HasResourceDependency(const RTGResourceProxy& resource, ERTGDependencyType type) const
    {
        switch (type)
        {
        case ERTGDependencyType::Read:
            return HasReadDependency(resource);

        case ERTGDependencyType::Write:
            return HasWriteDependency(resource);

        case ERTGDependencyType::ReadWrite:
            return HasReadDependency(resource) && HasWriteDependency(resource);

        WARP_ATTR_UNLIKELY default:
            WARP_RTG_ASSERT(false, "Should not happen! Unhandled dependency type in code");
            return false;
        }
    }

}