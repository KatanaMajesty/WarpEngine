#pragma once

#include <cstdint>
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
    concept IsRefCountable = requires(T a) {
        {
            a.AddRef()
        } -> std::convertible_to<RefCountType>;
        {
            a.Release()
        } -> std::convertible_to<RefCountType>;
    };

    // Weak pointer
    template<IsRefCountable T>
    class WeakPtr
    {
        //static_assert(false && "noimpl");
    };

    // Auto Reference-Counted pointer
    // TODO: Add pointer convertion support with templates
    // TODO: It memory-leaks currently! Fixit
    template<IsRefCountable T>
    class ArcPtr
    {
    public:
        ArcPtr() = default;
        ArcPtr(std::nullptr_t) noexcept
            : m_Ptr(nullptr)
        {
        }

        ArcPtr(T* ptr) noexcept
            : m_Ptr(ptr)
        {
            AddRefSelf();
        }

        ArcPtr(const ArcPtr& other) noexcept
        {
            Attach(other.m_Ptr);
        }
        ArcPtr& operator=(const ArcPtr& other) noexcept
        {
            if (this != &other)
            {
                Attach(other.m_Ptr);
            }
            return *this;
        }

        ArcPtr(ArcPtr&& other) noexcept
            : m_Ptr(other.Detach())
        {
        }
        ArcPtr& operator=(ArcPtr&& other) noexcept
        {
            m_Ptr = other.Detach();
            return *this;
        }

        ~ArcPtr()
        {
            ReleaseSelf();
        }

        // Associates the ptr with the ArcPtr instance, adds reference to the ptr
        void Attach(T* ptr) noexcept
        {
            DetachAndRelease();
            m_Ptr = ptr;
            AddRefSelf();
        }

        // Disassociates the ptr with the ArcPtr instance, does not affect internal reference count
        T* Detach() noexcept
        {
            T* prev = m_Ptr;
            m_Ptr = nullptr;
            return prev;
        }

        T* DetachAndRelease() noexcept
        {
            ReleaseSelf();
            return Detach();
        }

        T* Get() const noexcept { return m_Ptr; }
        T* operator->() const noexcept { return m_Ptr; }

        // Get Address of a field
        T* const* GetAddressOf() const noexcept { return &m_Ptr; }
        T** GetAddressOf() noexcept { return &m_Ptr; }

        T** ReleaseAndGetAddressOf() noexcept
        {
            ReleaseSelf();
            return GetAddressOf();
        }

        T** operator&() noexcept
        {
            return ReleaseAndGetAddressOf();
        }

        void Reset() noexcept { ReleaseSelf(); }

        bool operator==(std::nullptr_t) const { return m_Ptr == nullptr; }
        bool operator!=(std::nullptr_t) const { return m_Ptr != nullptr; }
        bool operator==(const T* ptr) const { return m_Ptr == ptr; }
        bool operator!=(const T* ptr) const { return m_Ptr != ptr; }
        operator bool() const { return operator!=(nullptr); }

    private:
        void ReleaseSelf() noexcept
        {
            if (m_Ptr)
            {
                m_Ptr = (m_Ptr->Release() == 0) ? nullptr : m_Ptr;
            }
        }

        void AddRefSelf() noexcept
        {
            if (m_Ptr)
            {
                m_Ptr->AddRef();
            }
        }

        T* m_Ptr = nullptr;
    };
}