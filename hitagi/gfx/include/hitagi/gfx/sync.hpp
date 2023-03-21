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
    Fence(Device& device, std::string_view name = "") : m_Device(device), m_Name(name) {}
    Fence(const Fence&)            = delete;
    Fence(Fence&&)                 = default;
    Fence& operator=(const Fence&) = delete;
    Fence& operator=(Fence&&)      = delete;
    virtual ~Fence()               = default;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

    virtual void Wait(std::chrono::duration<double> timeout = std::chrono::duration<double>::max()) = 0;

protected:
    Device&          m_Device;
    std::pmr::string m_Name;
};

class Semaphore {
public:
    Semaphore(Device& device, std::string_view name = "") : m_Device(device), m_Name(name) {}
    Semaphore(const Semaphore&)            = delete;
    Semaphore(Semaphore&&)                 = default;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore& operator=(Semaphore&&)      = delete;
    virtual ~Semaphore()                   = default;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

protected:
    Device&          m_Device;
    std::pmr::string m_Name;
};

enum struct BarrierAccess : std::uint32_t {
    Unkown       = 0x0,
    CopySrc      = 0x1,
    CopyDst      = (CopySrc << 1),
    Vertex       = (CopyDst << 1),
    Index        = (Vertex << 1),
    Constant     = (Index << 1),
    ShaderRead   = (Constant << 1),
    ShaderWrite  = (ShaderRead << 1),
    RenderTarget = (ShaderWrite << 1),
    Present      = (RenderTarget << 1),
};

enum struct BarrierStage : std::uint32_t {
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

enum struct BarrierLayout : std::uint16_t {
    Unkown            = 0x0,
    Common            = 0x1,
    CopySrc           = (Common << 1),
    CopyDst           = (CopySrc << 1),
    ShaderRead        = (CopyDst << 1),
    DepthStencilRead  = (ShaderRead << 1),
    DepthStencilWrite = (DepthStencilRead << 1),
    RenderTarget      = (DepthStencilWrite << 1),
    ResolveSrc        = (RenderTarget << 1),
    ResolveDst        = (ResolveSrc << 1),
    Present           = (ResolveDst << 1),
};

struct GlobalBarrier {
    BarrierAccess src_access;
    BarrierAccess dst_access;
    BarrierStage  src_stage;
    BarrierStage  dst_stage;
};

struct GPUBufferBarrier {
    BarrierAccess src_access;
    BarrierAccess dst_access;
    BarrierStage  src_stage;
    BarrierStage  dst_stage;

    GPUBuffer& buffer;
};

struct TextureBarrier {
    BarrierAccess src_access;
    BarrierAccess dst_access;
    BarrierStage  src_stage;
    BarrierStage  dst_stage;

    BarrierLayout src_layout;
    BarrierLayout dst_layout;

    Texture&      texture;
    bool          is_depth_stencil = false;
    std::uint16_t mip_level        = 0;
    std::uint16_t array_layer      = 0;
};

}  // namespace hitagi::gfx

template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::BarrierAccess> {
    static constexpr bool is_flags = true;
};
template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::gfx::BarrierStage> {
    static constexpr bool is_flags = true;
};
