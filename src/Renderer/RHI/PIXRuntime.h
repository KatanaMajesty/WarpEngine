#pragma once

#include "stdafx.h"
#include "CommandContext.h"
#include "CommandQueue.h"
#include <type_traits>

#define USE_PIX
#include <WinPixEventRuntime/pix3.h>

namespace Warp::Pix
{

    // For more info https://devblogs.microsoft.com/pix/winpixeventruntime/
    // TODO: Implement programmatic captures https://devblogs.microsoft.com/pix/programmatic-capture/

    template<typename... Args>
    void BeginEvent(RHICommandContext* commandContext, std::string_view name, Args&&... args)
    {
        PIXBeginEvent(commandContext->GetD3D12CommandList(), PIX_COLOR_DEFAULT, name.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    void BeginEvent(RHICommandQueue* commandQueue, std::string_view name, Args&&... args)
    {
        PIXBeginEvent(commandQueue->GetInternalHandle(), PIX_COLOR_DEFAULT, name.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    void BeginEvent(std::string_view name, Args&&... args)
    {
        PIXBeginEvent(PIX_COLOR_DEFAULT, name.data(), std::forward<Args>(args)...);
    }

    inline void EndEvent(RHICommandContext* commandContext) { PIXEndEvent(commandContext->GetD3D12CommandList()); }
    inline void EndEvent(RHICommandQueue* commandQueue) { PIXEndEvent(commandQueue->GetInternalHandle()); }
    inline void EndEvent() { PIXEndEvent(); }

    template<typename... Args>
    void SetMarker(RHICommandContext* commandContext, std::string_view name, Args&&... args)
    {
        PIXSetMarker(commandContext->GetD3D12CommandList(), PIX_COLOR_DEFAULT, name.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    void SetMarker(RHICommandQueue* commandQueue, std::string_view name, Args&&... args)
    {
        PIXSetMarker(commandQueue->GetInternalHandle(), PIX_COLOR_DEFAULT, name.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    void SetMarker(std::string_view name, Args&&... args)
    {
        PIXSetMarker(PIX_COLOR_DEFAULT, name.data(), std::forward<Args>(args)...);
    }

    inline void NotifyFenceWakeup(HANDLE event) { PIXNotifyWakeFromFenceSignal(event); }

    template<class T>
    struct InferEventType
    {
        using Type = void;
    };

    template<>
    struct InferEventType<RHICommandQueue*>
    {
        using Type = RHICommandQueue*;
    };

    template<>
    struct InferEventType<RHICommandContext*>
    {
        using Type = RHICommandContext*;
    };

    template<typename T, typename... Args>
    concept HasBeginEvent = requires(T t, Args&&... args) {
        BeginEvent(t, std::forward<Args>(args)...);
    };

    template<typename T>
    concept HasEndEvent = requires(T t) {
        EndEvent(t);
    };

    template<typename ContextType>
    struct ScopedEvent
    {
        template<typename... Args>
            requires HasBeginEvent<ContextType, Args...>
        ScopedEvent(ContextType context, Args&&... args)
            : Context(context)
        {
            WARP_ASSERT(context != nullptr);
            Pix::BeginEvent(context, std::forward<Args>(args)...);
        }

        ~ScopedEvent()
        {
            if constexpr (HasEndEvent<ContextType>)
            {
                EndEvent(this->Context);
            }
            else
            {
                EndEvent();
            }
        }

        ContextType Context;
    };
}

#define WARP_PIX_GET_SCOPED_EVENT_VARNAME(a, b) a##b
#define WARP_PIX_SCOPED_EVENT(context, ...) \
    ::Warp::Pix::ScopedEvent                \
    WARP_PIX_GET_SCOPED_EVENT_VARNAME(WarpEvent, __LINE__)(context, ##__VA_ARGS__)