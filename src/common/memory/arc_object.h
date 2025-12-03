#pragma once

#include "common/assert.h"
#include "common/attr_defs.h"

#include <memory>
#include <atomic>

namespace Warp
{

    /// @brief Atomically reference counted objects are maintaining their current reference count internally.
    /// Each Arc object also has associated Allocator (that can be obtained by a call to static member function 'GetAllocator()').
    /// Allocator type **MUST** be default-constructible, so that it could be instantiated lazily.
    /// 
    // TODO: Maybe address the requirement of default-constructible allocator types, as this implies stateless allocators only...
    template<typename Self>
    class AtomicallyRefCounted
    {
    public:
        using RefType = uint32_t;

        static_assert(std::is_integral_v<RefType> && std::is_unsigned_v<RefType>,
            "Ref counter type needs to an unsigned integer");

        static_assert(std::atomic<RefType>::is_always_lock_free,
            "Ref counter type needs to support lock-free atomic instructions");

        AtomicallyRefCounted() = default;

        AtomicallyRefCounted(const AtomicallyRefCounted&) = delete;
        AtomicallyRefCounted& operator=(const AtomicallyRefCounted&) = delete;

        AtomicallyRefCounted(AtomicallyRefCounted&&) = delete;
        AtomicallyRefCounted& operator=(AtomicallyRefCounted&&) = delete;

        ~AtomicallyRefCounted() noexcept
        {
#if defined(WARP_ENGINE_DEBUG)
            WARP_A_MAYBE_UNUSUED RefType refCount = m_refCount.load();
            WARP_ASSERT(refCount == 0, "Lifetime memory leak (Arc obj) -> Reference count of an object at address \'{}\' was {} during destruction!",
                reinterpret_cast<std::uintptr_t>(this), refCount);
#endif // defined(WARP_ENGINE_DEBUG)
        }

        /// @returns new reference count of this ref-counted instance
        WARP_A_MAYBE_UNUSUED RefType IncrementRef() noexcept
        {
            return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        /// @returns new reference count of this ref-counted instance
        /// Return result from this function **SHOULD NOT** be discarded and must instead be used to destroy the object.
        WARP_A_NODISCARD("Returned reference count must be used to destroy the object (if it is 0)")
        RefType DecrementRef() noexcept
        {
            // When decrementing the reference count, there's a potential object destruction if the count hits zero
            // thus std::memory_order_acq_rel
            RefType prev = m_refCount.fetch_sub(1, std::memory_order_acq_rel);
            WARP_ASSERT(prev > 0, "Arc object -> Decrement performed on instance with refcount 0! Undefined behavior");
            return prev - 1;
        }

    private:
        std::atomic<RefType> m_refCount = 0;
    };

} // Warp namespace