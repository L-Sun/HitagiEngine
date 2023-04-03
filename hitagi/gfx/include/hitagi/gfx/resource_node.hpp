#pragma once
#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/bindless.hpp>

namespace hitagi::gfx {
struct PassNode;

struct ResourceHandle {
    std::uint64_t id = 0;

    constexpr static auto InvalidHandle() { return ResourceHandle{.id = 0}; }

    constexpr bool operator!() const { return id == 0; }
    constexpr auto operator<=>(const ResourceHandle&) const = default;
    constexpr auto operator!=(const ResourceHandle& rhs) const { return id != rhs.id; }
};

struct ResourceNode {
    ResourceNode(std::string_view name, std::size_t res_idx)
        : name(name), res_idx(res_idx) {}

    std::pmr::string                 name;
    std::size_t                      res_idx;
    PassNode*                        writer  = nullptr;
    std::uint32_t                    version = 0;
    std::pmr::vector<BindlessHandle> bindless_handles;
};

}  // namespace hitagi::gfx