export module gfx.metal:bindless;
import interop.metal;
import interop.spdlog;

import std;
import gfx.base;
import :types;
import :utils;
import :configs;

export namespace hitagi::gfx {

// An immutable table version owns everything reachable through its GPU addresses.
struct MetalBindlessSnapshot {
    ns::SharedPtr<mtl::Buffer>                         buffer;
    std::pmr::vector<ns::SharedPtr<mtl::Resource>>     resources;
    std::pmr::vector<ns::SharedPtr<mtl::SamplerState>> samplers;
};

class MetalBindlessUtils final : public BindlessUtils {
public:
    MetalBindlessUtils(const ns::SharedPtr<mtl::Device>& device, std::shared_ptr<spdlog::logger> logger, std::string_view name);

    auto        CreateBindlessHandle(mtl::Buffer* buffer, std::uint64_t offset, bool writable) -> BindlessHandle;
    auto        CreateBindlessHandle(mtl::Texture* texture, bool writable) -> BindlessHandle;
    auto        CreateBindlessHandle(mtl::SamplerState* sampler) -> BindlessHandle;
    void        DiscardBindlessHandle(BindlessHandle handle) final;
    inline auto GetNativeDevice() const noexcept { return &m_Device; }

    auto GetSnapshot() -> std::shared_ptr<const MetalBindlessSnapshot>;

private:
    auto AllocateHandle(BindlessHandleType type, bool writable) -> BindlessHandle;
    struct ArgumentBufferLayout {
        std::array<std::uint64_t, metal_resource_capacity> buffers{};
        std::array<std::uint64_t, metal_resource_capacity> textures1d{};
        std::array<std::uint64_t, metal_resource_capacity> textures{};
        std::array<std::uint64_t, metal_resource_capacity> uint_textures{};
        std::array<std::uint64_t, metal_resource_capacity> sint_textures{};
        std::array<std::uint64_t, metal_resource_capacity> storage_textures1d{};
        std::array<std::uint64_t, metal_resource_capacity> storage_textures{};
        std::array<std::uint64_t, metal_resource_capacity> storage_uint_textures{};
        std::array<std::uint64_t, metal_resource_capacity> storage_sint_textures{};
        std::array<std::uint64_t, metal_sampler_capacity>  samplers{};
    } m_ArgumentBuffer;
    static_assert(sizeof(ArgumentBufferLayout) == (metal_sampler_binding + metal_sampler_capacity) * sizeof(std::uint64_t));
    const ns::SharedPtr<mtl::Device>&              m_Device;
    std::shared_ptr<spdlog::logger>                m_Logger;
    std::array<std::pmr::vector<std::uint32_t>, 3> m_FreeIndices;
    std::array<std::uint32_t, 3>                   m_NextIndex{};
    std::array<std::pmr::vector<std::uint32_t>, 3> m_Versions{
        std::pmr::vector<std::uint32_t>(metal_resource_capacity),
        std::pmr::vector<std::uint32_t>(metal_resource_capacity),
        std::pmr::vector<std::uint32_t>(metal_sampler_capacity)};
    std::pmr::map<std::pair<BindlessHandleType, std::uint32_t>, ns::SharedPtr<mtl::Resource>> m_Resources;
    std::pmr::map<std::uint32_t, ns::SharedPtr<mtl::SamplerState>>                            m_Samplers;
    std::mutex                                                                                m_Mutex;
    std::shared_ptr<const MetalBindlessSnapshot>                                              m_Snapshot;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

MetalBindlessUtils::MetalBindlessUtils(const ns::SharedPtr<mtl::Device>& device, std::shared_ptr<spdlog::logger> logger, std::string_view name) : BindlessUtils(name), m_Device(device), m_Logger(std::move(logger)) {}

auto MetalBindlessUtils::AllocateHandle(BindlessHandleType type, bool writable) -> BindlessHandle {
    const auto    type_index   = static_cast<std::size_t>(type);
    auto&         free_indices = m_FreeIndices[type_index];
    auto&         next_index   = m_NextIndex[type_index];
    std::uint32_t index;
    if (free_indices.empty()) {
        check_metal(m_Logger, next_index < m_Versions[type_index].size(), "Metal bindless table exhausted");
        index = next_index++;
    } else {
        index = free_indices.back();
        free_indices.pop_back();
    }
    return {.index = index, .type = type, .writable = writable, .version = m_Versions[type_index][index]};
}

auto MetalBindlessUtils::CreateBindlessHandle(mtl::Buffer* buffer, std::uint64_t offset, bool writable) -> BindlessHandle {
    std::scoped_lock lock(m_Mutex);
    auto             handle                  = AllocateHandle(BindlessHandleType::Buffer, writable);
    m_ArgumentBuffer.buffers[handle.index]   = buffer->gpuAddress() + offset;
    m_Resources[{handle.type, handle.index}] = ns::RetainPtr(static_cast<mtl::Resource*>(buffer));
    m_Snapshot.reset();
    return handle;
}

auto MetalBindlessUtils::CreateBindlessHandle(mtl::Texture* texture, bool writable) -> BindlessHandle {
    std::scoped_lock lock(m_Mutex);
    auto             handle                              = AllocateHandle(BindlessHandleType::Texture, writable);
    auto             id                                  = texture->gpuResourceID()._impl;
    m_ArgumentBuffer.textures1d[handle.index]            = id;
    m_ArgumentBuffer.textures[handle.index]              = id;
    m_ArgumentBuffer.uint_textures[handle.index]         = id;
    m_ArgumentBuffer.sint_textures[handle.index]         = id;
    m_ArgumentBuffer.storage_textures1d[handle.index]    = id;
    m_ArgumentBuffer.storage_textures[handle.index]      = id;
    m_ArgumentBuffer.storage_uint_textures[handle.index] = id;
    m_ArgumentBuffer.storage_sint_textures[handle.index] = id;
    m_Resources[{handle.type, handle.index}]             = ns::RetainPtr(static_cast<mtl::Resource*>(texture));
    m_Snapshot.reset();
    return handle;
}

auto MetalBindlessUtils::CreateBindlessHandle(mtl::SamplerState* sampler) -> BindlessHandle {
    std::scoped_lock lock(m_Mutex);
    auto             handle                 = AllocateHandle(BindlessHandleType::Sampler, false);
    m_ArgumentBuffer.samplers[handle.index] = sampler->gpuResourceID()._impl;
    m_Samplers[handle.index]                = ns::RetainPtr(sampler);
    m_Snapshot.reset();
    return handle;
}

void MetalBindlessUtils::DiscardBindlessHandle(BindlessHandle handle) {
    std::scoped_lock lock(m_Mutex);
    auto             kind  = static_cast<std::size_t>(handle.type);
    const auto       limit = handle.type == BindlessHandleType::Sampler ? metal_sampler_capacity : metal_resource_capacity;
    if (kind >= m_Versions.size() || handle.index >= limit || m_Versions[kind][handle.index] != handle.version) return;
    if (handle.type == BindlessHandleType::Sampler ? !m_Samplers.contains(handle.index) : !m_Resources.contains({handle.type, handle.index})) return;
    m_Snapshot.reset();
    ++m_Versions[kind][handle.index];
    switch (handle.type) {
        case BindlessHandleType::Buffer:
            m_ArgumentBuffer.buffers[handle.index] = 0;
            break;
        case BindlessHandleType::Texture:
            m_ArgumentBuffer.textures1d[handle.index] = m_ArgumentBuffer.textures[handle.index] = m_ArgumentBuffer.uint_textures[handle.index] = m_ArgumentBuffer.sint_textures[handle.index] = 0;
            m_ArgumentBuffer.storage_textures1d[handle.index] = m_ArgumentBuffer.storage_textures[handle.index] = 0;
            m_ArgumentBuffer.storage_uint_textures[handle.index] = m_ArgumentBuffer.storage_sint_textures[handle.index] = 0;
            break;
        case BindlessHandleType::Sampler:
            m_ArgumentBuffer.samplers[handle.index] = 0;
            m_Samplers.erase(handle.index);
            break;
        default:
            return;
    }
    m_Resources.erase({handle.type, handle.index});
    m_FreeIndices[static_cast<std::size_t>(handle.type)].push_back(handle.index);
}

auto MetalBindlessUtils::GetSnapshot() -> std::shared_ptr<const MetalBindlessSnapshot> {
    std::scoped_lock lock(m_Mutex);
    if (m_Snapshot) return m_Snapshot;
    auto snapshot    = std::allocate_shared<MetalBindlessSnapshot>(std::pmr::polymorphic_allocator<MetalBindlessSnapshot>{});
    snapshot->buffer = ns::TransferPtr(m_Device->newBuffer(&m_ArgumentBuffer, sizeof(m_ArgumentBuffer), mtl::ResourceStorageModeShared));
    check_metal(m_Logger, bool(snapshot->buffer), "Create Metal bindless snapshot failed");
    snapshot->resources.reserve(m_Resources.size());
    for (const auto& [key, resource] : m_Resources) snapshot->resources.push_back(resource);
    snapshot->samplers.reserve(m_Samplers.size());
    for (const auto& [index, sampler] : m_Samplers) snapshot->samplers.push_back(sampler);
    m_Snapshot = std::move(snapshot);
    return m_Snapshot;
}

}  // namespace hitagi::gfx
