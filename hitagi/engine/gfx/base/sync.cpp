export module gfx.base:sync;
import std;
import :types;

export namespace hitagi::gfx {

class Device;
class GPUBuffer;
class Texture;

class Fence {
public:
    static auto Create(Device& device, std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Fence>;

    Fence(const Fence&)            = delete;
    Fence(Fence&&)                 = default;
    Fence& operator=(const Fence&) = delete;
    Fence& operator=(Fence&&)      = delete;
    virtual ~Fence()               = default;

    virtual void Signal(std::uint64_t value)                                                                       = 0;
    virtual bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = (std::chrono::milliseconds::max)()) = 0;
    virtual auto GetCurrentValue() -> std::uint64_t                                                                = 0;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

protected:
    Fence(std::string_view name = "") : m_Name(name) {}

    std::pmr::string m_Name;
};

struct GlobalBarrier {
    BarrierAccess src_access = BarrierAccess::None;
    BarrierAccess dst_access = BarrierAccess::None;
    PipelineStage src_stage  = PipelineStage::None;
    PipelineStage dst_stage  = PipelineStage::None;
};

struct GPUBufferBarrier {
    BarrierAccess src_access = BarrierAccess::None;
    BarrierAccess dst_access = BarrierAccess::None;
    PipelineStage src_stage  = PipelineStage::None;
    PipelineStage dst_stage  = PipelineStage::None;

    GPUBuffer& buffer;
};

struct TextureBarrier {
    BarrierAccess src_access = BarrierAccess::None;
    BarrierAccess dst_access = BarrierAccess::None;
    PipelineStage src_stage  = PipelineStage::None;
    PipelineStage dst_stage  = PipelineStage::None;

    TextureLayout src_layout = TextureLayout::Unkown;
    TextureLayout dst_layout = TextureLayout::Unkown;

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
