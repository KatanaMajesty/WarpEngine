#include "common/memory/arc.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace Warp
{

TEST(ArcTest, PreserveHandleSize)
{
    // ArcMark should not affect size of handle, so for empty struct this should still be 1
    class EmptyHandle : public ArcMark<EmptyHandle>
    {
    };
    ASSERT_EQ(sizeof(EmptyHandle), 1);
    // ArcMark should not affect alignment of a handle structure either
    class Handle : public ArcMark<Handle>
    {
        uint32_t i;
    };
    ASSERT_EQ(sizeof(Handle), sizeof(uint32_t));
}

TEST(ArcTest, EmptyHandleDefaultCtor)
{
    class EmptyHandle : public ArcMark<EmptyHandle>
    {
    };
    // Default constructor for Arc must be equal to nullptr
    {
        Arc<EmptyHandle> handle;
        ASSERT_FALSE(handle.IsValid());
        ASSERT_EQ(handle, nullptr);
    }
    // Explicitly specifying nullptr in constructor must also be properly resolved
    {
        Arc<EmptyHandle> handle = nullptr;
        ASSERT_FALSE(handle.IsValid());
        ASSERT_EQ(handle, nullptr);
    }
    // Contrary to constructors when using Arc<T>::Make a created handle should always be valid
    {
        auto handle = Arc<EmptyHandle>::Make();
        ASSERT_TRUE(handle.IsValid());
    }
}

TEST(ArcTest, MakeHandleNoexceptCtor)
{
    struct Handle : ArcMark<Handle>
    {
        Handle(uint32_t x, uint32_t y) noexcept
            : x(x)
            , y(y)
        {
        }
        uint32_t x, y;
    };

    // Arc must guarantee that for noexcept constructors Arc<T>::Make would also be noexcept
    using ArcMakeFnType = decltype(&Arc<Handle>::Make<uint32_t, uint32_t>);
    static constexpr bool isNothrowInvocable = std::is_nothrow_invocable_v<ArcMakeFnType, uint32_t, uint32_t>;
    ASSERT_TRUE(isNothrowInvocable);
}
TEST(ArcTest, MakeHandle)
{
    struct Handle : ArcMark<Handle>
    {
        Handle(uint32_t x, uint32_t y) noexcept
            : x(x)
            , y(y)
        {
        }
        constexpr bool operator==(const Handle& other) const noexcept = default;
        uint32_t x = 0, y = 0;
    };
    // Arc<T>::Make must properly invoke constructor and properly initialize member fields
    auto handle = Arc<Handle>::Make(1, 2);
    Handle lhs = *handle.Get();
    Handle rhs = {1, 2};
    ASSERT_EQ(lhs, rhs);
};

TEST(ArcTest, MakeHandleAggregateCtor)
{
    struct AggregateHandle : ArcMark<AggregateHandle>
    {
        constexpr bool operator==(const AggregateHandle&) const noexcept = default;
        uint32_t i;
    };
    // Since C++17 designator initialization is allowed for classes with public base class
    // so ArcMark should not ruin aggregate initialization rules
    ASSERT_TRUE(std::is_aggregate_v<AggregateHandle>);

    // Arc<T>::Make must properly initialize objects using copy constructors,
    // as underlying std::construct_at allows it
    auto handle = Arc<AggregateHandle>::Make(AggregateHandle{.i = 0});
    ASSERT_EQ(*handle.Get(), AggregateHandle{.i = 0});
}

TEST(ArcTest, CopyCtor)
{
    struct Handle : ArcMark<Handle>
    {
        uint32_t i = 0;
    };

    // after a copy h1 and h2 must share the same underlying memory block address
    Arc<Handle> h1 = Arc<Handle>::Make(Handle{.i = 1});
    Arc<Handle> h2 = h1;
    ASSERT_EQ(h1, h2);
}

TEST(ArcTest, OutOfScopeCopyLifetime)
{
    struct Handle : ArcMark<Handle>
    {
    };

    Arc<Handle> h1 = Arc<Handle>::Make();
    {
        Arc<Handle> h2 = h1;
        ASSERT_EQ(h2.GetReferenceCount(), 2);
    }
    // when out of scope handle must get destroyed
    ASSERT_EQ(h1.GetReferenceCount(), 1);
}

TEST(ArcTest, OutOfScopeMoveLifetime)
{
    struct Handle : ArcMark<Handle>
    {
    };

    Arc<Handle> h1 = Arc<Handle>::Make();
    {
        // after a std::move operation h1 must become invalid
        // and h2 must retain previous ref count without changing it
        Arc<Handle> h2 = std::move(h1);
        ASSERT_FALSE(h1.IsValid());
        ASSERT_EQ(h2.GetReferenceCount(), 1);
    }
}

TEST(ArcTest, DirectPointerAccess)
{
    struct Handle : ArcMark<Handle>
    {
        uint32_t i = 0;
    };

    auto handle = Arc<Handle>::Make();
    Handle handleCopy = *handle.Get();
    handle->i = 1;
    handleCopy.i = 1;
    ASSERT_EQ(handle->i, handleCopy.i);
}

} // namespace Warp