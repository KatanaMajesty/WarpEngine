#pragma once

#include <cstdint>

namespace Warp
{

    // Encapsulates a typed array view into a blob of data.
    template <typename T>
    class ArrayView
    {
    public:
        ArrayView() = default;
        ArrayView(T* data, uint32_t count)
            : m_data(data)
            , m_count(count)
        {
        }

        // std library container interface
        T* data() { return m_data; }
        const T* data() const { return m_data; }

        T& back() { return *(m_data + m_count - 1); }
        const T& back() const { return *(m_data + m_count - 1); }

        size_t size() const { return m_count; }

        // Iterator interface
        T* begin() { return m_data; }
        T* end() { return m_data + m_count; }

        T& operator[](uint32_t i) { return *(m_data + i); }
        const T& operator[](uint32_t i) const { return *(m_data + i); }

    private:
        T* m_data = nullptr;
        uint32_t m_count = 0;
    };

    template <typename T>
    ArrayView<T> MakeArrayView(T* data, uint32_t size) { return ArrayView<T>(data, size); }

}