#pragma once

// Warp Engine
// Reference Counting namespace

#include "RcCommon.h"

namespace Warp::rc
{

    // Weak pointer
    template<IsRefCountable T>
    class WeakPtr
    {
    };

    // Reference-Counted pointer
    // TODO: Add noexcept
    // TODO: Add pointer convertion support with templates
    template<IsRefCountable T>
    class RcPtr
    {
    public:
        RcPtr() = default;
        RcPtr(std::nullptr_t)
            : m_Ptr(nullptr)
        {
        }

        RcPtr(T* ptr)
            : m_Ptr(ptr)
        {
            AddRefSelf();
        }

        RcPtr(const RcPtr& other)
        {
            Attach(other.m_Ptr);
        }
        RcPtr& operator=(const RcPtr& other)
        {
            if (this != &other)
            {
                Attach(other.m_Ptr);
            }
            return *this;
        }

        RcPtr(RcPtr&& other)
            : m_Ptr(other.Detach())
        {
        }
        RcPtr& operator=(RcPtr&& other)
        {
            if (this != &other)
            {
                m_Ptr = other.Detach();
            }
            return *this;
        }

        ~RcPtr()
        {
            ReleaseSelf();
        }

        // Associates the ptr with the RcPtr instance, adds reference to the ptr
        void Attach(T* ptr)
        {
            DetachAndRelease();
            m_Ptr = ptr;
            AddRefSelf();
        }

        // Disassociates the ptr with the RcPtr instance, does not affect internal reference count
        T* Detach()
        {
            T* prev = m_Ptr;
            m_Ptr = nullptr;
            return prev;
        }

        T* DetachAndRelease()
        {
            ReleaseSelf();
            return Detach();
        }

        T* Get()        const { return m_Ptr; }
        T* operator->() const { return m_Ptr; }
        
        // Get Address of a field
        T* const*   GetAddressOf() const { return &m_Ptr; }
        T**         GetAddressOf() { return &m_Ptr; }

        T** ReleaseAndGetAddressOf()
        {
            ReleaseSelf();
            return GetAddressOf();
        }

        T** operator&()
        {
            return ReleaseAndGetAddressOf();
        }

        bool operator==(std::nullptr_t) const { return m_Ptr == nullptr; }
        bool operator!=(std::nullptr_t) const { return m_Ptr != nullptr; }
        bool operator==(const T* ptr) const { return m_Ptr == ptr; }
        bool operator!=(const T* ptr) const { return m_Ptr != ptr; }
        operator bool() const { return operator!=(nullptr); }

    private:
        void ReleaseSelf()
        {
            if (m_Ptr)
            {
                m_Ptr = (m_Ptr->Release() == 0) ? nullptr : m_Ptr;
            }
        }

        void AddRefSelf()
        {
            if (m_Ptr)
            {
                m_Ptr->AddRef();
            }
        }

        T* m_Ptr = nullptr;
    };

}