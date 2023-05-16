#pragma once
#include <hitagi/gfx/gpu_resource.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <memory_resource>
#include <memory>

namespace hitagi::gfx {
class Device;

class Fence {
public:
    Fence(const Fence&)            = delete;
    Fence(Fence&&)                 = default;
    Fence& operator=(const Fence&) = delete;
    Fence& operator=(Fence&&)      = delete;
    virtual ~Fence()               = default;

    virtual void Signal(std::uint64_t value)                                                                     = 0;
    virtual bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) = 0;
    virtual auto GetCurrentValue() -> std::uint64_t                                                              = 0;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

protected:
    Fence(Device& device, std::string_view name = "") : m_Device(device), m_Name(name) {}

    Device&          m_Device;
    std::pmr::string m_Name;
};

enum struct BarrierAccess : std::uint32_t {
    Unkown            = 0x0,
    CopySrc           = 0x1,
    CopyDst           = (CopySrc << 1),
    Vertex            = (CopyDst << 1),
    Index             = (Vertex << 1),
    Constant          = (Index << 1),
    ShaderRead        = (Constant << 1),
    ShaderWrite       = (ShaderRead << 1),
    DepthStencilRead  = (ShaderWrite << 1),
    DepthStencilWrite = (DepthStencilRead << 1),
    RenderTarget      = (DepthStencilWrite << 1),
    Present           = (RenderTarget << 1),
};

enum struct PipelineStage : std::uint32_t {
    None          = 0x0,
    All           = 0x1,
    VertexInput   = (All << 1),
    VertexShader  = (VertexInput << 1),
    PixelShader   = (VertexShader << 1),
    DepthStencil  = (PixelShader << 1),
    Render        = (DepthStencil << 1),
    AllGraphics   = (Render << 1),
    ComputeShader = (AllGraphics << 1),
    Copy          = (ComputeShader << 1),
    Resolve       = (Copy << 1),
};

enum struct TextureLayout : std::uint16_t {
    Unkown            = 0x0,
    Common            = 0x1,
    CopySrc           = (Common << 1),
    CopyDst           = (CopySrc << 1),
    ShaderRead        = (CopyDst << 1),
    ShaderWrite       = (ShaderRead << 1),
    DepthStencilRead  = (ShaderWrite << 1),
    DepthStencilWrite = (DepthStencilRead << 1),
    RenderTarget      = (DepthStencilWrite << 1),
    ResolveSrc        = (RenderTarget << 1),
    ResolveDst        = (ResolveSrc << 1),
    Present           = (ResolveDst << 1),
};

struct GlobalBarrier {
    BarrierAccess src_access;
    BarrierAccess dst_access;
    PipelineStage src_stage = PipelineStage::All;
    PipelineStage dst_stage = PipelineStage::All;
};

struct GPUBufferBarrier {
    BarrierAccess src_access;
    BarrierAccess dst_access;
    PipelineStage src_stage = PipelineStage::All;
    PipelineStage dst_stage = PipelineStage::All;

    GPUBuffer& buffer;
};

struct TextureBarrier {
    BarrierAccess src_access;
    BarrierAccess dst_access;
    PipelineStage src_stage = PipelineStage::All;
    PipelineStage dst_stage = PipelineStage::All;

    TextureLayout src_layout;
    TextureLayout dst_layout;

    Texture& texture;
};

struct FenceSignalInfo {
    Fence&        fence;
    std::uint64_t value;
};

struct FenceWaitInfo {
    Fence&        fence;
    std::uint64_t value;
    PipelineStage stage = PipelineStage::All;
};

}  // namespace hitagi::gfx

template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::BarrierAccess> {
    static constexpr bool is_flags = true;
};
template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::PipelineStage> {
    static constexpr bool is_flags = true;
};
