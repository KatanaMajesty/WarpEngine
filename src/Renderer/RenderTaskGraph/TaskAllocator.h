#pragma once

#include <vector>
#include <concepts>

#include "Defines.h"

namespace Warp
{

    template<std::default_initializable TaskType>
    class RTGTaskAllocator
    {
    private:
        struct MemoryPage
        {
            using ByteType = std::byte;
            using ByteContainer = std::vector<ByteType>;

            // Default contructor constructs invalid page that cannot be used to allocate memory
            MemoryPage() = default;
            MemoryPage(size_t sizeInBytes)
                : Bytes(sizeInBytes)
                , Offset(0)
            {
                WARP_RTG_ASSERT(sizeInBytes % TaskSize == 0, "should be multiples of TaskSize");
            }

            ByteContainer Bytes;
            size_t Offset = 0;
        };

    public:
        static constexpr size_t TaskSize = sizeof(TaskType);
        static constexpr size_t DefaultPageSize = TaskSize * 32;
        RTGTaskAllocator(size_t pageSize = DefaultPageSize)
            : m_PageSize(pageSize)
        {
        }

        ~RTGTaskAllocator()
        {
            Cleanup();
        }

        template<typename... Args>
        TaskType* AllocateTask(Args&&... args)
        {
            void* bytes = Allocate(TaskSize);
            TaskType* task = new (bytes) TaskType;
            return task;
        }

        // Cleans up all allocated tasks
        // All of them!
        void Cleanup()
        {
            // Explicitly call every destructor
            for (MemoryPage& page : m_MemoryPages)
            {
                WARP_RTG_ASSERT(page->Offset % TaskSize == 0, "should be multiples of TaskSize");
                TaskType* begin = reinterpret_cast<TaskType*>(page->Bytes.data());
                TaskType* end   = reinterpret_cast<TaskType*>(page->Bytes.data() + page->Offset);
                for (begin; begin != end; ++begin)
                {
                    begin->~TaskType();
                }
            }
        }

    private:
        MemoryPage::ByteType* Allocate(size_t sizeInBytes)
        {
            WARP_RTG_ASSERT(sizeInBytes % TaskSize == 0, "should be multiples of TaskSize");
            MemoryPage* pPage = nullptr;

            // try to find a suitable memory page
            for (MemoryPage& page : m_MemoryPages)
            {
                const size_t numBytes = page.Bytes.size();
                const size_t numBytesLeft = numBytes - page.Offset;
                if (numBytesLeft >= sizeInBytes)
                {
                    pPage = &page;
                    break;
                }
            }

            // Add new page if none was found
            if (!pPage)
            {
                pPage = &m_MemoryPages.emplace_back(m_PageSize);
            }

            MemoryPage::ByteType* begin = pPage->Bytes.data();
            MemoryPage::ByteType* alloc = begin + pPage->Offset;
            pPage->Offset += sizeInBytes;

            return alloc;
        }

        size_t m_PageSize;
        std::vector<MemoryPage> m_MemoryPages;
    };

}