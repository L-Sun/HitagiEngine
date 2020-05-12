#pragma once
#include <cstdint>

namespace Hitagi::Core {

class Buffer {
public:
    Buffer();
    Buffer(size_t size, size_t alignment = 4, const void* srcPtr = nullptr, size_t copySize = 0);
    Buffer(const Buffer& buffer);
    Buffer(Buffer&& buffer);
    Buffer& operator=(const Buffer& rhs);
    Buffer& operator=(Buffer&& rhs);
    ~Buffer();

    uint8_t*       GetData();
    const uint8_t* GetData() const;
    size_t         GetDataSize() const;

private:
    uint8_t* m_Data;
    size_t   m_Size;
    size_t   m_Alignment;
};
}  // namespace Hitagi::Core
