#pragma once
#include <cstddef>
#include <cstring>
#include "MemoryManager.hpp"

namespace My {

class Buffer {
public:
    Buffer();
    Buffer(size_t size, const void* srcPtr = nullptr, size_t copySize = 0,
           size_t alignment = 4);
    Buffer(const Buffer& rhs);
    Buffer(Buffer&& rhs);
    Buffer& operator=(const Buffer& rhs);
    Buffer& operator=(Buffer&& rhs);
    ~Buffer();

    uint8_t*       GetData();
    const uint8_t* GetData() const;
    size_t         GetDataSize() const;

private:
    uint8_t* m_pData;
    size_t   m_szSize;
    size_t   m_szAlignment;
};
}  // namespace My
