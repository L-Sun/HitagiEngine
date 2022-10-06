#pragma once
#include <hitagi/gfx/gpu_resource.hpp>

namespace hitagi::gfx {
struct PassNode;

using ResourceHandle = std::size_t;

struct ResourceNode {
    ResourceNode(std::string_view name, std::size_t res_idx)
        : name(name), res_idx(res_idx) {}

    std::pmr::string name;
    std::size_t      res_idx;
    PassNode*        writer  = nullptr;
    std::uint32_t    version = 0;
};

}  // namespace hitagi::gfx