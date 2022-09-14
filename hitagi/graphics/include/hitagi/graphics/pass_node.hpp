#pragma once
#include <hitagi/graphics/resource_node.hpp>
#include <hitagi/graphics/command_context.hpp>

#include <functional>
#include <set>

namespace hitagi::gfx {

class ResourceHelper;
class RenderGraph;

struct PassNode {
    ResourceHandle Read(const ResourceHandle res);
    ResourceHandle Write(RenderGraph& fg, const ResourceHandle res);

    std::pmr::string name;
    // index of the resource node in render graph
    std::pmr::set<ResourceHandle> reads;
    std::pmr::set<ResourceHandle> writes;

    std::function<void(IGraphicsCommandContext*)> executor;
};

template <typename PassData>
struct PassNodeWithData : public PassNode {
    PassData data = {};
};

}  // namespace hitagi::gfx
