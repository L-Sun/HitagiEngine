#pragma once
#include <hitagi/resource/resource.hpp>

namespace hitagi::gfx {
struct PassNode;

using ResourceHandle = std::size_t;

struct ResourceNode {
    ResourceNode(std::string_view name, resource::Resource* resource)
        : name(name), resource(resource) {
        assert(resource != nullptr);
    }

    std::pmr::string    name;
    resource::Resource* resource = nullptr;
    PassNode*           writer   = nullptr;
    std::uint32_t       version  = 0;
};

}  // namespace hitagi::gfx