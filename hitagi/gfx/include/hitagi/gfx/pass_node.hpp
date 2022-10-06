#pragma once
#include <hitagi/gfx/resource_node.hpp>
#include <hitagi/gfx/command_context.hpp>

#include <functional>
#include <set>

namespace hitagi::gfx {

class ResourceHelper;
class RenderGraph;

struct PassNode {
    ResourceHandle Read(const ResourceHandle res);
    ResourceHandle Write(RenderGraph& fg, const ResourceHandle res);

    virtual void Execute() = 0;

    std::pmr::string name;
    // index of the resource node in render graph
    std::pmr::set<ResourceHandle> reads;
    std::pmr::set<ResourceHandle> writes;

    CommandType                     type;
    std::shared_ptr<CommandContext> context = nullptr;
};

template <typename Context, typename PassData>
struct PassNodeWithData : public PassNode {
    void Execute() final {
        executor(std::static_pointer_cast<Context>(context).get());
    }

    std::function<void(Context*)> executor;
    PassData                      data = {};
};

template <typename Context>
struct PassNodeWithData<Context, void> : public PassNode {
    void Execute() final {
        executor(std::static_pointer_cast<Context>(context).get());
    }

    std::function<void(Context*)> executor;
};

}  // namespace hitagi::gfx
