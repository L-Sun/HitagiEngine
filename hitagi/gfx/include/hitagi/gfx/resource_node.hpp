#pragma once
#include <hitagi/gfx/bindless.hpp>

namespace hitagi::gfx {
class PassNodeBase;

struct ResourceHandle {
    std::uint32_t id = std::numeric_limits<std::uint32_t>::max();

    constexpr static auto InvalidHandle() noexcept { return ResourceHandle{}; }

    inline operator bool() const noexcept {
        return id != std::numeric_limits<std::uint32_t>::max();
    }
    constexpr auto operator<=>(const ResourceHandle&) const noexcept = default;
    constexpr auto operator!=(const ResourceHandle& rhs) const noexcept { return id != rhs.id; }
};

class ResourceNode {
public:
    ResourceNode(std::string_view name, std::size_t resource_index);

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto GetVersion() const noexcept { return m_Version; }

    auto Write(PassNodeBase& writer_pass) const noexcept -> ResourceNode;

private:
    std::pmr::string m_Name;
    std::uint32_t    m_ResourceIndex;
    PassNodeBase*    m_Writer  = nullptr;
    std::uint32_t    m_Version = 0;
};
}  // namespace hitagi::gfx