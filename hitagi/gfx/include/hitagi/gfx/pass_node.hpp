#pragma once
#include <hitagi/gfx/resource_node.hpp>
#include <hitagi/gfx/command_context.hpp>

#include <functional>
#include <set>

namespace hitagi::gfx {

class ResourceHelper;
class RenderGraph;

struct PassNode {
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
    PassNodeWithData() {
        if constexpr (std::is_same_v<Context, GraphicsCommandContext>) {
            type = CommandType::Graphics;
        }
        if constexpr (std::is_same_v<Context, ComputeCommandContext>) {
            type = CommandType::Compute;
        }
        if constexpr (std::is_same_v<Context, CopyCommandContext>) {
            type = CommandType::Copy;
        }
    }
    void Execute() final {
        executor(std::static_pointer_cast<Context>(context).get());
    }

    std::function<void(Context*)> executor;
    PassData                      data = {};
};

}  // namespace hitagi::gfx
