#pragma once

#include "common/assert.h"
#include "common/attr_defs.h"

#include <atomic>
#include <memory>
#include <type_traits>
#include <cstddef> // for std::byte

namespace Warp
{

    namespace detail
    {
        /// @brief ArcMarkInternal (or base-level arc mark) is an empty struct-marker for Arc pointer to "enable" Arc-pointer functionality for
        /// types derived from this mark. This should not be used directly and instead ArcMark is to be used by the client - see below
        ///
        /// A type T can only be used with Arc<T> if T is derived from ArcMark (base-level arc mark)
        struct ArcMarkInternal
        {
        };
    } // detail namespace

    /// @brief ArcMark (hint-marker) is an empty struct-marker for Arc pointers that derives from base-level arc mark (see struct ArcMark)
    /// but also provides some compile-time metadata on allocators, basically providing some hints on how to allocate/deallocate underlying Arc handle memory.
    ///
    /// The small catch here is that with modern C++20 std-based allocators should no longer provide an API to construct/destroy objects,
    /// but instead just manage underlying memory.
    /// For that reason to remove "self-dependency" all allocators used by Arc **MUST** have std::byte as underlying type
    ///
    /// Also keep in mind that Arc will allocate more bytes than the client might expect when creating new Arc handle. For rules of allocation read more on Arc
    /// 4 bytes are required to place control block
    template<typename Self, typename AllocatorHint = std::allocator<std::byte>>
        requires(
            std::is_trivially_copyable_v<AllocatorHint> &&
            std::is_same_v<typename AllocatorHint::value_type, std::byte>)
    struct ArcMark : detail::ArcMarkInternal
    {
        using AllocatorType = AllocatorHint;

        // enable default comparison operators for derived classes
        constexpr bool operator==(const ArcMark&) const noexcept { return true; }
        constexpr bool operator!=(const ArcMark&) const noexcept { return false; }
    };

    /// @brief Arc control block is a replacement of std::shared_ptr control block implementation to allow for lower-level control of memory.
    /// Control block type can be changed when creating Arc pointer if an addition layer of memory debug interface is required
    class ArcControlBlock
    {
    public:
        static_assert(std::atomic_uint32_t::is_always_lock_free, "Arc reference counter must be an always lock-free atomic!");

        ArcControlBlock() = delete; // no default constructors for control block

        /// @brief sets up a new reference count for this control block. Should only ever be used during
        /// control block initialization
        inline void SetRefCount(uint32_t value) noexcept
        {
            m_refCount = value;
        }

        WARP_A_NODISCARD("Getting ref count is an atomic operation which might affect scheduling")
        inline uint32_t GetRefCount() const noexcept
        {
            return m_refCount.load(std::memory_order_relaxed);
        }

        /// @returns new reference count of this ref-counted instance
        WARP_A_MAYBE_UNUSUED inline uint32_t IncrementRefCount() noexcept
        {
            return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        /// @returns new reference count of this control block
        /// Return result from this function **SHOULD NOT** be discarded and must instead be used to destroy the object.
        WARP_A_NODISCARD("Returned reference count must be used to destroy the object (if it is 0)")
        inline uint32_t DecrementRefCount() noexcept
        {
            // When decrementing the reference count, there's a potential object destruction if the count hits zero
            // thus std::memory_order_acq_rel
            uint32_t prev = m_refCount.fetch_sub(1, std::memory_order_acq_rel);
            WARP_ASSERT(prev > 0, "Arc object -> Decrement performed on instance with refcount 0! Undefined behavior");
            return prev - 1;
        }

    private:
        std::atomic_uint32_t m_refCount{}; // constexpr since C++11. The initialization is not atomic.
    };

    /// Arc pointer allocation has the following rules:
    /// - If underlying type T is derived from ArcMark a default std::allocator<std::byte> will be used to allocate underlying memory
    /// - If underlying type T is derived from ArcMark2 an allocator specified as AllocatorHint will be used instead
    /// - A size of allocation done by Arc is sizeof(ArcControlBlock) + sizeof(T)
    /// - Control block **ALWAYS** comes first in the memory!
    template<typename T>
        requires(std::is_base_of_v<detail::ArcMarkInternal, T>)
    class Arc
    {
    private:
        /// make all Arc<U> specializations friends of each other
        template<typename U>
            requires(std::is_base_of_v<detail::ArcMarkInternal, U>)
        friend class Arc;

    public:
        static_assert(!std::is_array_v<T>, "Arrays are not supported and can be ill-formed, so avoid!");

        /// @brief To avoid compiler confusion a static Make function is required when creating new Arc pointers.
        template<typename... Args>
        static Arc<T> Make(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        {
            Arc<T> ptr(nullptr);
            ptr.AllocateMemoryBlock();
            ptr.ConstructMemoryBlock(std::forward<Args>(args)...);
            return ptr;
        }

        constexpr Arc(std::nullptr_t = nullptr) noexcept
            : m_allocedMemoryBlock(nullptr)
            , m_ptr(nullptr)
        {
        }

        // ignore clang format for now. TODO: Fix clang-format for one-line class operators
        // clang-format off

        // copy constructors
        constexpr Arc(const Arc& other) noexcept { CopyInit(other); }
        constexpr Arc& operator=(const Arc& other) noexcept { CopyInit(other); return *this; }

        template<typename Derived> requires(std::is_base_of_v<T, Derived>)
        Arc(const Arc<Derived>& other) noexcept { CopyInit(other); }

        template<typename Derived> requires(std::is_base_of_v<T, Derived>)
        Arc& operator=(const Arc<Derived>& other) noexcept { CopyInit(other); return *this; }

        // move constructors
        // all move constructors must evaluate to constexpr
        constexpr Arc(Arc&& other) noexcept { MoveInit(std::move(other)); }
        constexpr Arc& operator=(Arc&& other) noexcept { MoveInit(std::move(other)); return *this; }

        template<typename Derived> requires(std::is_base_of_v<T, Derived>)
        constexpr Arc(Arc<Derived>&& other) noexcept { MoveInit(std::move(other)); }

        template<typename Derived> requires(std::is_base_of_v<T, Derived>)
        constexpr Arc& operator=(Arc<Derived>&& other) noexcept { MoveInit(std::move(other)); return *this; }
        // clang-format on
        
        ~Arc() noexcept
        {
            if (IsValidReference())
            {
                ReleaseReference();
                InvalidateMemoryBlock();
            }
        }

        WARP_A_NODISCARD("Getting ref count is an atomic operation which might affect scheduling")
        constexpr uint32_t GetReferenceCount() const noexcept { return IsValidReference() ? m_controlBlock->GetRefCount() : 0u; }

        /// @brief Checks whether this pointer holds a valid pointer and a valid reference for this pointer.
        /// Latter should always be true really, so this is just a sanity check here.
        constexpr bool IsValid() const noexcept { return IsValidReference() && m_ptr != nullptr; }

        /// @brief Compares this arc pointer with another arc pointer.
        /// Depending on whether underlying memory block pointer address is the same returns either true or false
        constexpr bool operator==(const Arc<T>& other) const noexcept { return m_allocedMemoryBlock == other.m_allocedMemoryBlock; }
        constexpr bool operator!=(const Arc<T>& other) const noexcept { return m_allocedMemoryBlock != other.m_allocedMemoryBlock; }

        constexpr bool operator==(std::nullptr_t) const noexcept { return m_allocedMemoryBlock == nullptr; }
        constexpr bool operator!=(std::nullptr_t) const noexcept { return m_allocedMemoryBlock != nullptr; }

        constexpr bool operator==(const T*) const noexcept = delete;
        constexpr bool operator!=(const T*) const noexcept = delete;

        constexpr operator bool() const noexcept { return IsValid(); }

        constexpr T* Get() const noexcept { return m_ptr; }
        constexpr T* operator->() const noexcept { return this->Get(); }
        constexpr T* operator&() const noexcept { return this->Get(); }

    private:
        constexpr bool IsValidReference() const noexcept { return m_controlBlock != nullptr; }

        /// @brief Releases handle of this arc pointer by decrementing internal ref count of BaseType.
        /// If after the decrement internal ref count is 0 then the object will be destroyed
        void ReleaseReference() noexcept
        {
            if (m_controlBlock->DecrementRefCount() == 0)
            {
                DestroyMemoryBlock();
                DeallocateMemoryBlock();
            }
        }

        template<typename U>
            requires(std::is_base_of_v<T, U>) // if U == T will also evaluate to true
        void CopyInit(const Arc<U>& other) noexcept
        {
            if (*this == other)
            {
                return;
            }
            // check if we currently have a valid pointer and if after releasing the refcount is 0.
            // if all is true then destroy the memory block
            if (IsValidReference())
            {
                // just release the reference - dont invalidate pointers
                ReleaseReference();
            }
            // only assign one of union fields, as control block must be at the exact same address
            // as the beginning of memory block as per mem layout spec
            m_allocedMemoryBlock = other.m_allocedMemoryBlock;
            m_ptr = static_cast<T*>(other.m_ptr); // upcasting should be safe here if requirements are satisfied

            // after copying pointers check whether our new memory block is now valid
            if (IsValidReference())
            {
                // increment ref count as we've just added a new valid reference
                m_controlBlock->IncrementRefCount();
            }
        }

        template<typename U>
            requires(std::is_base_of_v<T, U>) // if U == T will also evaluate to true
        constexpr void MoveInit(Arc<U>&& other) noexcept
        {
            if (*this == other)
            {
                return;
            }
            // check if we currently have a valid reference and if after releasing the refcount is 0.
            // if all is true then destroy the object
            if (IsValidReference())
            {
                // just release the reference - dont invalidate pointers
                ReleaseReference();
            }
            // only assign one of union fields, as control block must be at the exact same address
            // as the beginning of memory block as per mem layout spec
            m_allocedMemoryBlock = other.m_allocedMemoryBlock;
            m_ptr = static_cast<T*>(other.m_ptr);
            // invalidate other's memory block after move
            other.InvalidateMemoryBlock();
        }

        // helper function to properly construct allocated memory (control block and type T)
        template<typename... Args>
        constexpr void ConstructMemoryBlock(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        {
            // there is currently no way to execute constexpr default-construct at a pointer, so control block needs to
            // be initialized explicitly
            // Proposal for default_construct_at is here https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2283r2.pdf
            m_controlBlock->SetRefCount(1u); // now we are at 1 reference
            std::construct_at(m_ptr, std::forward<Args>(args)...); // (constexpr since C++20)
        }

        // helper function to properly destruct allocated memory (control block and type T)
        constexpr void DestroyMemoryBlock() noexcept
        {
            // Always assume that destructor is nothrow!
            // it is dangerous to have throwable destructors overall as it causes std::terminate on almost all compilers
            // (whereas others might even be UB)
            static_assert(std::is_nothrow_destructible_v<T>, "Destructors MUST be marked with nothrow to be used with Arc!");
            std::destroy_at(m_controlBlock);
            std::destroy_at(m_ptr);
        }

        // Helper function to align memory allocations
        static constexpr size_t AlignUp(size_t size, size_t alignment) noexcept
        {
            return (size + (alignment - 1ull)) & ~(alignment - 1ull);
        }

        /// all handle allocations should be word-aligned.
        /// This is to handle empty-struct cases where control block + sizeof(T) would result in 4 + 1 bytes allocated
        static constexpr size_t AllocationWordAlignment = sizeof(uint32_t);
        static constexpr size_t AllocationSizeInBytes = sizeof(ArcControlBlock) + AlignUp(sizeof(T), AllocationWordAlignment);
        static constexpr size_t AllocationHandleOffset = sizeof(ArcControlBlock);

        // It is assumed that all pointers will properly be constructed after the call to this mem-fn
        // Should qualify for constexpr since C++20 if allocator is std-based (which we assume it to be)
        constexpr void AllocateMemoryBlock() noexcept
        {
            using StdAllocatorType = typename T::AllocatorType;
            // this should return std::byte* as per Arc spec
            m_allocedMemoryBlock = StdAllocatorType{}.allocate(AllocationSizeInBytes);
            // according to alloc rules first in mem layout comes control block
            m_controlBlock = reinterpret_cast<ArcControlBlock*>(m_allocedMemoryBlock);
            // according to alloc rules second in mem layout comes T at offset defined as sizeof(ControlBlock)
            m_ptr = reinterpret_cast<T*>(m_allocedMemoryBlock + AllocationHandleOffset);
        }

        // If Arc pointer is out-of-scope but lifetime should still be preserved - dont deallocate memory block
        // but still invalidate pointers
        constexpr void InvalidateMemoryBlock() noexcept
        {
            m_controlBlock = nullptr;
            m_ptr = nullptr;
        }

        // If is assumed that all pointers were properly destructed prior to this mem-fn call
        constexpr void DeallocateMemoryBlock() noexcept
        {
            using StdAllocatorType = typename T::AllocatorType;
            // assert that memory block pointers are valid!
            WARP_ASSERT(m_controlBlock && m_ptr, "Invalidated memory block pointers!");
            // we simply deallocate for AllocationSizeInBytes and clean-up pointers
            StdAllocatorType{}.deallocate(m_allocedMemoryBlock, AllocationSizeInBytes);
            m_controlBlock = nullptr;
            m_ptr = nullptr;
        }

        /// We always guarantee control block to be first in memory layout, so we can treat it as a beginning of
        /// underlying allocation block - prefer to unionise it here.
        union
        {
            std::byte* m_allocedMemoryBlock = nullptr;
            ArcControlBlock* m_controlBlock;
        };
        T* m_ptr = nullptr;
    };

} // Warp namespace