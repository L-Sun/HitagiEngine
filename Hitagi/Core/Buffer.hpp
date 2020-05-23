#pragma once
#include <cstdint>

namespace Hitagi::Core {

class Buffer {
public:
    Buffer() = default;
    Buffer(size_t size, size_t alignment = 4);
    Buffer(const void* initialData, size_t size, size_t alignment = 4);
    Buffer(const Buffer& buffer);
    Buffer(Buffer&& buffer);
    Buffer& operator=(const Buffer& rhs);
    Buffer& operator=(Buffer&& rhs);
    ~Buffer();

    uint8_t*       GetData();
    const uint8_t* GetData() const;
    size_t         GetDataSize() const;

    operator bool() const { return m_Data == nullptr || m_Size == 0; }

private:
    uint8_t* m_Data      = nullptr;
    size_t   m_Size      = 0;
    size_t   m_Alignment = 0;
};
}  // namespace Hitagi::Core
