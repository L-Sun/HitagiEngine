export module gfx.base:gpu_resource;
import std;
import utils;
import math;
import core;
import :types;
import :sync;
import :bindless;
import :utils;

export namespace hitagi::gfx {

class Device;
class CommandQueue;
class CommandQueues;
class ShaderCompiler;
class Fence;
struct GPUBufferBarrier;
struct TextureBarrier;

constexpr auto UNKOWN_NAME = "Unkown";

class Resource {
public:
    Resource(const Resource&)                = delete;
    Resource(Resource&&) noexcept            = default;
    Resource& operator=(const Resource&)     = delete;
    Resource& operator=(Resource&&) noexcept = delete;
    virtual ~Resource()                      = default;

    virtual auto GetName() const noexcept -> std::string_view = 0;
    virtual auto GetType() const noexcept -> ResourceType     = 0;

protected:
    Resource() = default;
};

template <typename T>
concept ResourceDesc = requires(T t) {
    { T::name } -> std::convertible_to<std::string_view>;
};

template <ResourceDesc _Desc>
class ResourceWithDesc : public Resource {
public:
    using Desc = _Desc;

    auto         GetName() const noexcept -> std::string_view final { return m_Desc.name; };
    auto         GetType() const noexcept -> ResourceType final;
    inline auto& GetDesc() const noexcept { return m_Desc; }

protected:
    ResourceWithDesc(_Desc desc) : m_Desc(std::move(desc)) {}

    _Desc m_Desc;
};

struct GPUBufferDesc {
    std::pmr::string    name = UNKOWN_NAME;
    std::uint64_t       size = 0;
    GPUBufferUsageFlags usages{};

    inline constexpr bool operator==(const GPUBufferDesc&) const noexcept;
};

class GPUBuffer : public ResourceWithDesc<GPUBufferDesc> {
public:
    static auto Create(Device& device, GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer>;

    using StorageViewRequirements = gfx::StorageViewRequirements;

    // Constraints on storage bindings, not on individual elements or GPU allocations.
    static auto GetStorageViewRequirements(const Device& device) noexcept -> StorageViewRequirements;

    inline auto Size() const noexcept -> std::uint64_t { return m_Desc.size; }

    virtual auto GetAllocationSize() const noexcept -> std::uint64_t = 0;

    [[nodiscard]] virtual auto Map() -> std::byte* = 0;
    virtual void               UnMap()             = 0;

    [[nodiscard]] auto Transition(BarrierAccess access, PipelineStage stage = PipelineStage::All) -> GPUBufferBarrier;

protected:
    using ResourceWithDesc::ResourceWithDesc;

    BarrierAccess m_CurrentAccess = BarrierAccess::None;
    PipelineStage m_CurrentStage  = PipelineStage::None;
};

struct GPUBufferViewDesc {
    std::pmr::string           name = UNKOWN_NAME;
    std::shared_ptr<GPUBuffer> buffer;
    GPUBufferViewType          type           = GPUBufferViewType::StorageRead;
    std::uint64_t              offset         = 0;
    std::uint64_t              element_size   = 1;
    std::uint64_t              element_count  = 1;
    std::uint64_t              element_stride = 0;  // Zero means tightly packed elements.

    inline bool operator==(const GPUBufferViewDesc&) const noexcept;
};

class GPUBufferView : public ResourceWithDesc<GPUBufferViewDesc> {
public:
    static auto Create(Device& device, BindlessUtils& bindings, GPUBufferViewDesc desc) -> std::shared_ptr<GPUBufferView>;

    ~GPUBufferView() override;

    inline auto GetBindlessHandle() const noexcept -> BindlessHandle { return m_BindlessHandle; }

    inline auto Size() const noexcept -> std::uint64_t { return m_ByteSize; }

    template <typename T>
        requires(!std::is_reference_v<T>)
    class MappedSpan : public utils::StridedSpan<T> {
    public:
        MappedSpan(const MappedSpan&)                = delete;
        MappedSpan(MappedSpan&&) noexcept            = delete;
        MappedSpan& operator=(const MappedSpan&)     = delete;
        MappedSpan& operator=(MappedSpan&&) noexcept = delete;
        ~MappedSpan();

    private:
        friend class GPUBufferView;
        explicit MappedSpan(GPUBufferView& view);

        GPUBuffer& m_Buffer;
    };

    template <typename T>
    [[nodiscard]] inline auto GetMappedSpan() -> MappedSpan<T> { return MappedSpan<T>(*this); }
    template <typename T>
    [[nodiscard]] inline auto GetMappedSpan() const -> MappedSpan<const T> { return MappedSpan<const T>(const_cast<GPUBufferView&>(*this)); }

protected:
    GPUBufferView(BindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc);

    bool RequiresBindlessHandle();

    [[nodiscard]] auto Map() -> std::byte* { return m_Desc.buffer->Map() + m_Desc.offset; }
    void               UnMap() { m_Desc.buffer->UnMap(); }

    std::uint64_t  m_ByteSize = 0;
    BindlessUtils& m_BindlessUtils;
    BindlessHandle m_BindlessHandle{};
};

struct TextureDesc {
    std::pmr::string          name        = UNKOWN_NAME;
    std::uint32_t             width       = 1;
    std::uint32_t             height      = 1;
    std::uint16_t             depth       = 1;
    std::uint16_t             array_size  = 1;
    Format                    format      = Format::UNKNOWN;
    std::uint16_t             mip_levels  = 1;
    std::optional<ClearValue> clear_value = std::nullopt;
    TextureUsageFlags         usages      = TextureUsageFlags::SRV;

    inline constexpr bool operator==(const TextureDesc&) const noexcept;
};

class Texture : public ResourceWithDesc<TextureDesc> {
public:
    static auto Create(Device& device, CommandQueues& queues, BindlessUtils& bindings, TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture>;

    virtual auto GetAllocationSize() const noexcept -> std::uint64_t = 0;

    [[nodiscard]] auto Transition(BarrierAccess access, TextureLayout layout, PipelineStage stage = PipelineStage::All) -> TextureBarrier;

    inline auto GetCurrentLayout() const noexcept -> TextureLayout { return m_CurrentLayout; }

protected:
    using ResourceWithDesc::ResourceWithDesc;

    BarrierAccess m_CurrentAccess = BarrierAccess::None;
    PipelineStage m_CurrentStage  = PipelineStage::All;
    TextureLayout m_CurrentLayout = TextureLayout::Unkown;
};

struct TextureViewDesc {
    std::pmr::string         name = UNKOWN_NAME;
    std::shared_ptr<Texture> texture;
    TextureViewType          type             = TextureViewType::ShaderRead;
    std::uint32_t            base_mip_level   = 0;
    std::uint32_t            mip_levels       = 0;
    std::uint32_t            base_array_layer = 0;
    std::uint32_t            layer_count      = 0;

    inline bool operator==(const TextureViewDesc&) const noexcept;
};

class TextureView : public ResourceWithDesc<TextureViewDesc> {
public:
    static auto Create(Device& device, BindlessUtils& bindings, TextureViewDesc desc) -> std::shared_ptr<TextureView>;

    ~TextureView() override;

    inline auto GetBindlessHandle() const noexcept -> BindlessHandle { return m_BindlessHandle; }

protected:
    TextureView(BindlessUtils& bindings, TextureViewDesc desc)
        : ResourceWithDesc(std::move(desc)), m_BindlessUtils(bindings) {}

    bool RequiresBindlessHandle();

    BindlessUtils& m_BindlessUtils;
    BindlessHandle m_BindlessHandle{};
};

struct SamplerDesc {
    std::pmr::string name           = UNKOWN_NAME;
    AddressMode      address_u      = AddressMode::Clamp;
    AddressMode      address_v      = AddressMode::Clamp;
    AddressMode      address_w      = AddressMode::Clamp;
    FilterMode       mag_filter     = FilterMode::Point;
    FilterMode       min_filter     = FilterMode::Point;
    FilterMode       mipmap_filter  = FilterMode::Point;
    float            min_lod        = 0;
    float            max_lod        = 32;
    float            max_anisotropy = 1;
    CompareOp        compare_op     = CompareOp::Never;

    inline constexpr bool operator==(const SamplerDesc&) const noexcept;
};
class Sampler : public ResourceWithDesc<SamplerDesc> {
public:
    static auto Create(Device& device, BindlessUtils& bindings, SamplerDesc desc) -> std::shared_ptr<Sampler>;

    ~Sampler() override;

    inline auto GetBindlessHandle() const noexcept -> BindlessHandle { return m_BindlessHandle; }

protected:
    Sampler(BindlessUtils& bindings, SamplerDesc desc)
        : ResourceWithDesc(std::move(desc)), m_BindlessUtils(bindings) {}

    bool RequiresBindlessHandle();

    BindlessUtils& m_BindlessUtils;
    BindlessHandle m_BindlessHandle{};
};

struct SwapChainDesc {
    std::pmr::string name = UNKOWN_NAME;
    utils::Window    window;
    ClearColor       clear_color  = math::Color(0, 0, 0, 1);
    std::uint32_t    sample_count = 1;
    bool             vsync        = false;
};
class SwapChain : public ResourceWithDesc<SwapChainDesc> {
public:
    static auto Create(Device& device, CommandQueue& queue, SwapChainDesc desc) -> std::shared_ptr<SwapChain>;

    virtual auto AcquireTextureForRendering() -> utils::optional_ref<Texture> = 0;
    virtual auto GetWidth() const noexcept -> std::uint32_t                   = 0;
    virtual auto GetHeight() const noexcept -> std::uint32_t                  = 0;
    virtual auto GetFormat() const noexcept -> Format                         = 0;
    virtual void Present()                                                    = 0;
    virtual void Resize()                                                     = 0;

protected:
    using ResourceWithDesc::ResourceWithDesc;
};

struct ShaderDesc {
    std::pmr::string      name;
    ShaderType            type;
    std::pmr::string      entry = "main";
    std::pmr::string      source_code;
    std::filesystem::path path;
};

class Shader : public ResourceWithDesc<ShaderDesc> {
public:
    static auto Create(Device& device, const ShaderCompiler& compiler, ShaderDesc desc) -> std::shared_ptr<Shader>;

    virtual auto GetDXILData() const noexcept -> std::span<const std::byte> { return {}; }
    virtual auto GetSPIRVData() const noexcept -> std::span<const std::byte> { return {}; }

protected:
    using ResourceWithDesc::ResourceWithDesc;
};

struct RenderPipelineDesc {
    std::pmr::string name = UNKOWN_NAME;

    AssemblyState      assembly_state       = {};
    VertexLayout       vertex_input_layout  = {};
    RasterizationState rasterization_state  = {};
    DepthStencilState  depth_stencil_state  = {};
    BlendState         blend_state          = {};
    Format             render_format        = Format::R8G8B8A8_UNORM;
    Format             depth_stencil_format = Format::UNKNOWN;
};
class RenderPipeline : public ResourceWithDesc<RenderPipelineDesc> {
public:
    static auto Create(Device& device, BindlessUtils& bindings, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) -> std::shared_ptr<RenderPipeline>;

protected:
    using ResourceWithDesc::ResourceWithDesc;
};

struct ComputePipelineDesc {
    std::pmr::string name = UNKOWN_NAME;
};
class ComputePipeline : public ResourceWithDesc<ComputePipelineDesc> {
public:
    static auto Create(Device& device, BindlessUtils& bindings, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs) -> std::shared_ptr<ComputePipeline>;

protected:
    using ResourceWithDesc::ResourceWithDesc;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

template <ResourceDesc Desc>
auto ResourceWithDesc<Desc>::GetType() const noexcept -> ResourceType {
    if constexpr (std::is_same_v<Desc, GPUBufferDesc>) {
        return ResourceType::GPUBuffer;
    } else if constexpr (std::is_same_v<Desc, GPUBufferViewDesc>) {
        return ResourceType::GPUBufferView;
    } else if constexpr (std::is_same_v<Desc, TextureDesc>) {
        return ResourceType::Texture;
    } else if constexpr (std::is_same_v<Desc, TextureViewDesc>) {
        return ResourceType::TextureView;
    } else if constexpr (std::is_same_v<Desc, SamplerDesc>) {
        return ResourceType::Sampler;
    } else if constexpr (std::is_same_v<Desc, SwapChainDesc>) {
        return ResourceType::SwapChain;
    } else if constexpr (std::is_same_v<Desc, ShaderDesc>) {
        return ResourceType::Shader;
    } else if constexpr (std::is_same_v<Desc, RenderPipelineDesc>) {
        return ResourceType::RenderPipeline;
    } else if constexpr (std::is_same_v<Desc, ComputePipelineDesc>) {
        return ResourceType::ComputePipeline;
    } else {
        []<bool flag = false>() { static_assert(flag, "Unknown resource type"); }();
    }
}

inline constexpr bool GPUBufferDesc::operator==(const GPUBufferDesc& rhs) const noexcept {
    return name == rhs.name &&
           size == rhs.size &&
           usages == rhs.usages;
}

inline bool GPUBufferViewDesc::operator==(const GPUBufferViewDesc& rhs) const noexcept {
    return name == rhs.name &&
           buffer.get() == rhs.buffer.get() &&
           type == rhs.type &&
           offset == rhs.offset &&
           element_size == rhs.element_size &&
           element_count == rhs.element_count &&
           element_stride == rhs.element_stride;
}

inline constexpr bool TextureDesc::operator==(const TextureDesc& rhs) const noexcept {
    bool result =
        name == rhs.name &&
        width == rhs.width &&
        height == rhs.height &&
        depth == rhs.depth &&
        array_size == rhs.array_size &&
        format == rhs.format &&
        mip_levels == rhs.mip_levels &&
        clear_value.has_value() == rhs.clear_value.has_value() &&
        usages == rhs.usages;
    if (clear_value.has_value() && rhs.clear_value.has_value()) {
        if (std::holds_alternative<ClearColor>(clear_value.value()) && std::holds_alternative<ClearColor>(rhs.clear_value.value())) {
            result = result && (std::get<ClearColor>(clear_value.value()) == std::get<ClearColor>(rhs.clear_value.value()));
        }
        if (std::holds_alternative<ClearDepthStencil>(clear_value.value()) && std::holds_alternative<ClearDepthStencil>(rhs.clear_value.value())) {
            result = result &&
                     (std::get<ClearDepthStencil>(clear_value.value()).depth == std::get<ClearDepthStencil>(rhs.clear_value.value()).depth) &&
                     (std::get<ClearDepthStencil>(clear_value.value()).stencil == std::get<ClearDepthStencil>(rhs.clear_value.value()).stencil);
        }
    }

    return result;
}

inline bool TextureViewDesc::operator==(const TextureViewDesc& rhs) const noexcept {
    return name == rhs.name &&
           texture.get() == rhs.texture.get() &&
           type == rhs.type &&
           base_mip_level == rhs.base_mip_level &&
           mip_levels == rhs.mip_levels &&
           base_array_layer == rhs.base_array_layer &&
           layer_count == rhs.layer_count;
}

inline constexpr bool SamplerDesc::operator==(const SamplerDesc& rhs) const noexcept {
    return name == rhs.name &&
           address_u == rhs.address_u &&
           address_v == rhs.address_v &&
           address_w == rhs.address_w &&
           mag_filter == rhs.mag_filter &&
           min_filter == rhs.min_filter &&
           mipmap_filter == rhs.mipmap_filter &&
           min_lod == rhs.min_lod &&
           max_lod == rhs.max_lod &&
           max_anisotropy == rhs.max_anisotropy &&
           compare_op == rhs.compare_op;
}

}  // namespace hitagi::gfx

export namespace std {
template <>
struct hash<hitagi::gfx::GPUBufferDesc> {
    constexpr std::size_t operator()(const hitagi::gfx::GPUBufferDesc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(desc.size),
            hitagi::utils::hash(desc.usages),
        });
    }
};

template <>
struct hash<hitagi::gfx::GPUBufferViewDesc> {
    std::size_t operator()(const hitagi::gfx::GPUBufferViewDesc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(reinterpret_cast<std::uintptr_t>(desc.buffer.get())),
            hitagi::utils::hash(desc.type),
            hitagi::utils::hash(desc.offset),
            hitagi::utils::hash(desc.element_size),
            hitagi::utils::hash(desc.element_count),
            hitagi::utils::hash(desc.element_stride),
        });
    }
};

template <>
struct hash<hitagi::gfx::TextureViewDesc> {
    std::size_t operator()(const hitagi::gfx::TextureViewDesc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(reinterpret_cast<std::uintptr_t>(desc.texture.get())),
            hitagi::utils::hash(desc.type),
            hitagi::utils::hash(desc.base_mip_level),
            hitagi::utils::hash(desc.mip_levels),
            hitagi::utils::hash(desc.base_array_layer),
            hitagi::utils::hash(desc.layer_count),
        });
    }
};

template <>
struct hash<hitagi::gfx::TextureDesc> {
    constexpr std::size_t operator()(const hitagi::gfx::TextureDesc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(desc.width),
            hitagi::utils::hash(desc.height),
            hitagi::utils::hash(desc.depth),
            hitagi::utils::hash(desc.array_size),
            hitagi::utils::hash(desc.format),
            hitagi::utils::hash(desc.mip_levels),
            hitagi::utils::hash(desc.usages),
        });
    }
};

template <>
struct hash<hitagi::gfx::SamplerDesc> {
    constexpr std::size_t operator()(const hitagi::gfx::SamplerDesc& desc) const noexcept {
        return hitagi::utils::combine_hash(std::array{
            hitagi::utils::hash(desc.name),
            hitagi::utils::hash(desc.address_u),
            hitagi::utils::hash(desc.address_v),
            hitagi::utils::hash(desc.address_w),
            hitagi::utils::hash(desc.mag_filter),
            hitagi::utils::hash(desc.min_filter),
            hitagi::utils::hash(desc.mipmap_filter),
            hitagi::utils::hash(desc.min_lod),
            hitagi::utils::hash(desc.max_lod),
            hitagi::utils::hash(desc.max_anisotropy),
            hitagi::utils::hash(desc.compare_op),
        });
    }
};
}  // namespace std

namespace hitagi::gfx {

template <typename T>
    requires(!std::is_reference_v<T>)
GPUBufferView::MappedSpan<T>::MappedSpan(GPUBufferView& view)
    : utils::StridedSpan<T>(nullptr, view.m_Desc.element_count, view.m_Desc.element_stride), m_Buffer(*view.m_Desc.buffer) {
    using ValueType = std::remove_const_t<T>;

    if (view.m_Desc.element_size != sizeof(ValueType)) {
        throw std::invalid_argument(std::format(
            "GPUBufferView {} element size({}) does not match mapped type size({})",
            view.GetName(),
            view.m_Desc.element_size,
            sizeof(ValueType)));
    }

    const auto& buffer_desc = m_Buffer.GetDesc();
    if (!utils::has_flag(buffer_desc.usages, GPUBufferUsageFlags::MapRead) &&
        !utils::has_flag(buffer_desc.usages, GPUBufferUsageFlags::MapWrite)) {
        throw std::invalid_argument(std::format("GPUBuffer {} is not mappable", view.GetName()));
    }
    if (!std::is_const_v<T> && !utils::has_flag(buffer_desc.usages, GPUBufferUsageFlags::MapWrite)) {
        throw std::invalid_argument(std::format("GPUBuffer {} is not writable on host", view.GetName()));
    }

    if (view.m_Desc.element_stride % alignof(ValueType) != 0 && view.m_Desc.element_count > 1) {
        throw std::invalid_argument("Mapped buffer element stride does not satisfy the C++ type alignment");
    }
    this->m_Data = view.Map();
    if (reinterpret_cast<std::uintptr_t>(this->m_Data) % alignof(ValueType) != 0) {
        view.UnMap();
        throw std::invalid_argument("Mapped buffer address does not satisfy the C++ type alignment");
    }
}

template <typename T>
    requires(!std::is_reference_v<T>)
GPUBufferView::MappedSpan<T>::~MappedSpan() {
    if (this->m_Data) {
        m_Buffer.UnMap();
    }
}

}  // namespace hitagi::gfx

namespace hitagi::gfx {

GPUBufferView::GPUBufferView(BindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc)
    : ResourceWithDesc(std::move(desc)), m_BindlessUtils(bindings) {
    const auto fail = [&](std::string_view reason) {
        throw std::invalid_argument(std::format("Invalid GPU buffer view({}): {}", GetName(), reason));
    };
    if (!m_Desc.buffer) fail("buffer is nullptr");
    if (m_Desc.element_size == 0) fail("element size must be larger than zero");
    if (m_Desc.element_stride == 0) m_Desc.element_stride = m_Desc.element_size;
    if (m_Desc.element_stride < m_Desc.element_size) fail("element stride is smaller than element size");

    const auto required_usage = m_Desc.type == GPUBufferViewType::StorageRead
                                    ? GPUBufferUsageFlags::StorageRead
                                    : GPUBufferUsageFlags::StorageWrite;
    const auto storage        = utils::has_flag(m_Desc.buffer->GetDesc().usages, required_usage);
    // Non-storage views also support typed mapping of vertex/index/copy buffers.
    const auto requirements = storage ? storage_requirements
                                      : GPUBuffer::StorageViewRequirements{.offset_alignment = 1, .size_alignment = 1};
    if (m_Desc.offset % requirements.offset_alignment != 0) fail("binding offset is not aligned");
    if (m_Desc.offset >= m_Desc.buffer->Size()) fail("offset is outside the buffer");

    const auto available = m_Desc.buffer->Size() - m_Desc.offset;
    const auto usable    = available - available % requirements.size_alignment;
    if (m_Desc.element_size > usable) fail("element exceeds the available binding range");
    const auto max_count = 1 + (usable - m_Desc.element_size) / m_Desc.element_stride;
    if (m_Desc.element_count == 0) m_Desc.element_count = max_count;
    if (m_Desc.element_count > max_count) fail("element range exceeds the buffer");

    // The preceding division check also prevents multiplication/addition overflow.
    const auto payload_size = (m_Desc.element_count - 1) * m_Desc.element_stride + m_Desc.element_size;
    m_ByteSize              = utils::align(payload_size, requirements.size_alignment);
    // Only the binding's tail is rounded; elements are never moved or padded implicitly.
}

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

namespace hitagi::gfx {
Sampler::~Sampler() {
    if (m_BindlessHandle) {
        m_BindlessUtils.DiscardBindlessHandle(m_BindlessHandle);
        m_BindlessHandle = {};
    }
}

bool Sampler::RequiresBindlessHandle() {
    if (m_BindlessHandle) {
        throw std::logic_error(std::format("Sampler({}) already has bindless handle", GetName()));
    }
    return true;
}

GPUBufferView::~GPUBufferView() {
    if (m_BindlessHandle) {
        m_BindlessUtils.DiscardBindlessHandle(m_BindlessHandle);
        m_BindlessHandle = {};
    }
}

bool GPUBufferView::RequiresBindlessHandle() {
    if (m_BindlessHandle) {
        throw std::logic_error(std::format("GPUBufferView({}) already has bindless handle", GetName()));
    }
    const auto usage          = m_Desc.buffer->GetDesc().usages;
    const auto required_usage = m_Desc.type == GPUBufferViewType::StorageWrite
                                    ? GPUBufferUsageFlags::StorageWrite
                                    : GPUBufferUsageFlags::StorageRead;
    if (!utils::has_flag(usage, required_usage)) return false;
    return true;
}

TextureView::~TextureView() {
    if (m_BindlessHandle) {
        m_BindlessUtils.DiscardBindlessHandle(m_BindlessHandle);
        m_BindlessHandle = {};
    }
}

bool TextureView::RequiresBindlessHandle() {
    if (m_BindlessHandle) {
        throw std::logic_error(std::format("TextureView({}) already has bindless handle", GetName()));
    }
    const auto usage = m_Desc.texture->GetDesc().usages;
    switch (m_Desc.type) {
        case TextureViewType::ShaderRead:
            if (!utils::has_flag(usage, TextureUsageFlags::SRV)) {
                throw std::invalid_argument(std::format(
                    "TextureView({}) requires texture({}) usage {} for {} view, actual usages are {}",
                    GetName(),
                    m_Desc.texture->GetName(),
                    TextureUsageFlags::SRV,
                    m_Desc.type,
                    usage));
            }
            break;
        case TextureViewType::ShaderWrite:
            if (!utils::has_flag(usage, TextureUsageFlags::UAV)) {
                throw std::invalid_argument(std::format(
                    "TextureView({}) requires texture({}) usage {} for {} view, actual usages are {}",
                    GetName(),
                    m_Desc.texture->GetName(),
                    TextureUsageFlags::UAV,
                    m_Desc.type,
                    usage));
            }
            break;
        case TextureViewType::RenderTarget:
        case TextureViewType::DepthStencil:
            return false;
    }
    return true;
}

}  // namespace hitagi::gfx
