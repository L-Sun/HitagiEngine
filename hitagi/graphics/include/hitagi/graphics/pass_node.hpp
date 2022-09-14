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

    template <typename PassData>
    PassData& GetData() { return *reinterpret_cast<PassData*>(data.GetData()); }

    std::pmr::string name;
    core::Buffer     data;

    // index of the resource node in render graph
    std::pmr::set<ResourceHandle> reads;
    std::pmr::set<ResourceHandle> writes;

    std::function<void(IGraphicsCommandContext*)> executor;
};

}  // namespace hitagi::gfx
