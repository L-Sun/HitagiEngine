module;

#include <cassert>

export module core:buffer;
import std;

export namespace hitagi::core {

class Buffer {
public:
    Buffer() = default;
    Buffer(std::size_t size, const std::byte* data = nullptr, std::size_t alignment = 4);
    Buffer(std::span<const std::byte> data, std::size_t alignment = 4);

    Buffer(const Buffer& buffer);
    Buffer(Buffer&& buffer) noexcept;

    Buffer& operator=(const Buffer& rhs);
    Buffer& operator=(Buffer&& rhs) noexcept;

    ~Buffer();

    void Resize(std::size_t size, std::size_t alignment = 4);

    inline std::byte*       GetData() noexcept { return m_Data; }
    inline const std::byte* GetData() const noexcept { return m_Data; }
    inline auto             GetDataSize() const noexcept { return m_Size; }
    inline bool             Empty() const noexcept { return m_Data == nullptr || m_Size == 0; }

    template <typename T>
    std::span<const T> Span() const {
        assert(
            m_Size % sizeof(T) == 0 &&
            "Create span from buffer failed,"
            " since the buffer size is not multiple of sizeof(T)");
        return std::span<const T>(reinterpret_cast<const T*>(m_Data), m_Size / sizeof(T));
    }

    template <typename T>
    std::span<T> Span() {
        assert(
            m_Size % sizeof(T) == 0 &&
            "Create span from buffer failed,"
            " since the buffer size is not multiple of sizeof(T)");
        return std::span<T>(reinterpret_cast<T*>(m_Data), m_Size / sizeof(T));
    }

    auto Str() const noexcept {
        return std::string_view{reinterpret_cast<const char*>(m_Data), m_Size};
    }

private:
    std::pmr::polymorphic_allocator<> m_Allocator;
    std::byte*                        m_Data      = nullptr;
    std::size_t                       m_Size      = 0;
    std::size_t                       m_Alignment = alignof(std::uint32_t);
};

}  // namespace hitagi::core

namespace hitagi::core {

Buffer::Buffer(size_t size, const std::byte* data, size_t alignment)
    : m_Allocator(std::pmr::get_default_resource()),
      m_Data(size != 0 ? static_cast<std::byte*>(m_Allocator.allocate_bytes(size, alignment)) : nullptr),
      m_Size(size),
      m_Alignment(alignment)

{
    if (data != nullptr) {
        std::memcpy(m_Data, data, m_Size);
    }
}

Buffer::Buffer(std::span<const std::byte> data, std::size_t alignment)
    : m_Allocator(std::pmr::get_default_resource()),
      m_Data(data.size() != 0 ? static_cast<std::byte*>(m_Allocator.allocate_bytes(data.size(), alignment)) : nullptr),
      m_Size(data.size()),
      m_Alignment(alignment)

{
    if (m_Size != 0) std::memcpy(m_Data, data.data(), m_Size);
}

Buffer::Buffer(const Buffer& other)
    : m_Allocator(other.m_Allocator),
      m_Data(other.m_Size != 0 ? static_cast<std::byte*>(m_Allocator.allocate_bytes(other.m_Size, other.m_Alignment)) : nullptr),
      m_Size(other.m_Size),
      m_Alignment(other.m_Alignment) {
    if (m_Size == 0) return;
    std::memcpy(m_Data, other.m_Data, other.m_Size);
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_Allocator(other.m_Allocator),
      m_Data(other.m_Data),
      m_Size(other.m_Size),
      m_Alignment(other.m_Alignment) {
    other.m_Data = nullptr;
    other.m_Size = 0;
}

Buffer& Buffer::operator=(const Buffer& rhs) {
    if (this != &rhs) {
        if (m_Data) {
            m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
            m_Data = nullptr;
        }

        if (rhs.m_Size != 0) {
            m_Data = static_cast<std::byte*>(m_Allocator.allocate_bytes(rhs.m_Size, rhs.m_Alignment));
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
        }

        m_Size      = rhs.m_Size;
        m_Alignment = rhs.m_Alignment;
    }
    return *this;
}

Buffer& Buffer::operator=(Buffer&& rhs) noexcept {
    if (this != &rhs) {
        // use copy
        if (m_Allocator != rhs.m_Allocator) {
            return this->operator=(std::cref(rhs));
        } else {
            if (m_Data != nullptr) m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
        }
        m_Data      = rhs.m_Data;
        m_Size      = rhs.m_Size;
        m_Alignment = rhs.m_Alignment;

        rhs.m_Data      = nullptr;
        rhs.m_Size      = 0;
        rhs.m_Alignment = 0;
    }
    return *this;
}
Buffer::~Buffer() {
    if (m_Data) m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);

    m_Data      = nullptr;
    m_Size      = 0;
    m_Alignment = 0;
}

void Buffer::Resize(std::size_t size, std::size_t alignment) {
    auto data = static_cast<std::byte*>(m_Allocator.allocate_bytes(size, alignment));
    if (m_Data) {
        std::memcpy(data, m_Data, std::min(size, m_Size));
        m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
    }
    m_Data      = data;
    m_Size      = size;
    m_Alignment = alignment;
}

}  // namespace hitagi::core
