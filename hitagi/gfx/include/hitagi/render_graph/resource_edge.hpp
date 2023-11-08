#pragma once
#include <hitagi/render_graph/common_types.hpp>
#include <hitagi/gfx/sync.hpp>
#include <hitagi/gfx/bindless.hpp>

namespace hitagi::rg {

struct GPUBufferEdge {
    bool               write;
    gfx::BarrierAccess access;
    gfx::PipelineStage stage;
    std::size_t        element_offset;
    std::size_t        num_elements;

    std::pmr::vector<gfx::BindlessHandle> bindless_handles;

    bool operator==(const GPUBufferEdge& rhs) const noexcept {
        return write == rhs.write &&
               access == rhs.access &&
               stage == rhs.stage &&
               element_offset == rhs.element_offset &&
               num_elements == rhs.num_elements;
    }
};

struct TextureEdge {
    bool                         write;
    gfx::BarrierAccess           access;
    gfx::PipelineStage           stage;
    gfx::TextureLayout           layout;
    gfx::TextureSubresourceLayer layer;

    gfx::BindlessHandle bindless;

    bool operator==(const TextureEdge& rhs) const noexcept {
        return write == rhs.write &&
               access == rhs.access &&
               layout == rhs.layout &&
               stage == rhs.stage;
    }
};

struct SamplerEdge {
    gfx::BindlessHandle bindless;
};

}  // namespace hitagi::rg