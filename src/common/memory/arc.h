#pragma once

#include "arc_object.h"

#include <memory>
#include <type_traits>

namespace Warp
{

    template<typename T, typename Allocator = std::allocator<T>>
        requires(
            std::is_base_of_v<AtomicallyRefCounted<T>, T> && 
            std::is_trivially_copyable_v<Allocator>)
    class Arc
    {
    public:
        static_assert(!std::is_array_v<T>, "Arrays are not supported and can be ill-formed, so avoid!");
        
        using BaseType = AtomicallyRefCounted<T>;
        using PointerType = T*;

        /// @brief A static member function to properly create Arc objects
        template<typename... Args>
        static Arc<T, Allocator> Make(Args&&... args) noexcept
        {
            PointerType ptr = std::construct_at(Allocator().allocate(1), std::forward<Args>(args)...); // (constexpr since C++20)
            return Arc(ptr);                                                                           // copy elision
        }

    private:
        /// @brief A static member function to destroy Arc objects. This function is not for external use.
        static constexpr void Destroy(PointerType ptr) noexcept
        {
            std::destroy_at(ptr);
            Allocator().deallocate(ptr, 1); // (constexpr since C++20)
        }

    public:
        constexpr Arc(std::nullptr_t = nullptr)
            : m_ptr(nullptr)
        {
        }

        Arc(T* ptr) noexcept
            : m_ptr(ptr)
        {
            if (m_ptr)
                m_ptr->IncrementRef();
        }

        Arc(const Arc& other) noexcept
            : m_ptr(other.m_ptr)
        {
            if (m_ptr)
                m_ptr->IncrementRef();
        }

        Arc& operator=(const Arc& other) noexcept
        {
            if (this->Get() != other.Get())
            {
                // check if we currently have a valid pointer and if after releasing the refcount is 0.
                // if all is true then destroy the object
                if (m_ptr)
                    ReleaseHandle();

                m_ptr = other.m_ptr;
                if (m_ptr)
                    m_ptr->IncrementRef();
            }
            return *this;
        }

        constexpr Arc(Arc&& other) noexcept
            : m_ptr(other.m_ptr)
        {
            other.m_ptr = nullptr;
        }

        Arc& operator=(Arc&& other) noexcept
        {
            if (this->Get() != other.Get())
            {
                if (m_ptr)
                    ReleaseHandle();

                m_ptr = other.m_ptr;
                other.m_ptr = nullptr;
            }
            return *this;
        }

        ~Arc() noexcept
        {
            if (m_ptr)
            {
                ReleaseHandle();
                m_ptr = nullptr;
            }
        }

        constexpr PointerType Get() const noexcept { return static_cast<PointerType>(m_ptr); }
        constexpr PointerType operator->() const noexcept { return this->Get(); }
        constexpr PointerType operator&() const noexcept { return this->Get(); }
        constexpr operator PointerType() const noexcept { return this->Get(); }

    private:
        /// @brief Releases handle of this arc pointer by decrementing internal ref count of BaseType.
        /// If after the decrement internal ref count is 0 then the object will be destroyed
        constexpr void ReleaseHandle() noexcept
        {
            if (m_ptr->DecrementRef() == 0)
            {
                Destroy(Get());
            }
        }

        BaseType* m_ptr;
    };

} // Warp namespace