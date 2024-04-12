#pragma once

// Warp Engine
// Reference Counting namespace

#include <cstdint>
#include <concepts>
#include <type_traits>

namespace Warp::rc
{

    using RefCountType = uint64_t;

    // IsRefCountable concept 
    // It is fully compatible with WinAPIs Component Object Model, thus any WinAPI class that is COM object
    // can be safely used as a ref-counted object and is compatible with Warp::rc namespace
    //
    // A ref-counted object uses a per-interface reference-counting mechanism to ensure
    // that the object doesn't outlive references to it.
    // You use T::AddRef to stabilize a copy of an interface pointer.
    // It can also be called when the life of a cloned pointer must extend beyond the lifetime of the original pointer.
    // The cloned pointer must be released by calling T::Release on it.
    template<typename T>
    concept IsRefCountable = requires(T a) 
    { 
        { 
            a.AddRef() 
        }   -> std::convertible_to<RefCountType>;
        {
            a.Release()
        }   -> std::convertible_to<RefCountType>;
    };

    // this is not used yet, but may be used in future for our own ref-counting techniques
    // this would also require a separate implementation of a RcMemoryArena or some sort of
    // ref-count manager that would deal with object lifetime
    class RefCounted
    {
    public:
        virtual ~RefCounted() = default;

        // These methods return the new/current reference count. Should not be used in most cases
        RefCountType AddRef() { return ++m_Ref; }
        RefCountType Release() { return --m_Ref; }
        RefCountType GetRefCount() const { return m_Ref; }

    private:
        RefCountType m_Ref;
    };

} // Warp::rc namespace