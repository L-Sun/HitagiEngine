#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/sync.hpp>

namespace hitagi::gfx {

auto GPUBuffer::Transition(BarrierAccess access, PipelineStage stage) -> GPUBufferBarrier {
    GPUBufferBarrier result{
        .src_access = m_CurrentAccess,
        .dst_access = access,
        .src_stage  = m_CurrentStage,
        .dst_stage  = stage,
        .buffer     = *this,
    };

    m_CurrentAccess = access;
    m_CurrentStage  = stage;

    return result;
}

auto Texture::Transition(BarrierAccess access, TextureLayout layout, PipelineStage stage) -> TextureBarrier {
    TextureBarrier result{
        .src_access = m_CurrentAccess,
        .dst_access = access,
        .src_stage  = m_CurrentStage,
        .dst_stage  = stage,
        .src_layout = m_CurrentLayout,
        .dst_layout = layout,
        .texture    = *this,
    };

    m_CurrentAccess = access;
    m_CurrentStage  = stage;
    m_CurrentLayout = layout;

    return result;
}

}  // namespace hitagi::gfx