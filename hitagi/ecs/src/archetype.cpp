#include <hitagi/ecs/archetype.hpp>
#include <hitagi/ecs/entity.hpp>

#include <memory>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/map.hpp>

#include <algorithm>

namespace hitagi::ecs {

Archetype::Archetype(detail::ComponentInfoSet component_infos)
    : m_ComponentInfoSet(std::move(component_infos)) {
    // calculate chunk info
    {
        // a `sm_chunk_size` bytes buffer contain
        // header(sm_align_size),
        // c_1_1, c_1_2, ..., c_1_n, padding_1,
        // c_2_1, c_2_2, ..., c_2_n, padding_2,
        // ...
        // c_m_1, c_m_2, ..., c_m_n, padding_m,
        // where c_i_j is the i-th component of j-th entity, and all element are contiguous, also each line align to `sm_align_size`

        // calculate num_entities without alignment
        std::size_t num_entities = sm_chunk_size /
                                   ranges::accumulate(m_ComponentInfoSet, std::size_t{0}, [](std::size_t acc, const auto& info) { return acc + info.size; });

        const auto calculate_offset = [this](std::size_t num_entities) {
            std::size_t current_offset = 0;
            for (const auto& component_info : m_ComponentInfoSet) {
                m_ChunkInfo.component_offsets[component_info.type_id] = current_offset;
                current_offset += utils::align(num_entities * component_info.size, sm_align_size);
            }
            return current_offset;
        };

        while (calculate_offset(num_entities) > sm_chunk_size) {
            num_entities--;
        }
        m_ChunkInfo.num_entities_per_chunk = num_entities;
    }
}

Archetype::~Archetype() {
    for (const auto entity : m_EntityMap | ranges::views::keys) {
        DestructAllComponents(entity);
    }
}

void Archetype::AllocateFor(entity_id_t entity) noexcept {
    auto& chunk = GetOrCreateChunk();
    m_EntityMap.emplace(entity, std::pair{m_Chunks.size() - 1, chunk.num_entity_in_chunk++});
}

void Archetype::DeallocateFor(entity_id_t entity) noexcept {
    const auto last_entity = GetLastEntity();

    if (last_entity != entity) {
        const auto last_entity_components = GetAllComponentData(last_entity);
        for (const auto& [component_id, data] : last_entity_components) {
            MoveConstructComponent(component_id, entity, data);
            DestructComponent(component_id, last_entity);
        }

        std::swap(m_EntityMap.at(entity), m_EntityMap.at(last_entity));
    }
    m_EntityMap.erase(entity);

    m_Chunks.back().num_entity_in_chunk--;

    if (m_Chunks.back().num_entity_in_chunk == 0) {
        m_Chunks.pop_back();
    }
}

bool Archetype::HasComponent(utils::TypeID component) const noexcept {
    return ranges::find_if(m_ComponentInfoSet, [component](const auto& info) { return info.type_id == component; }) != m_ComponentInfoSet.end();
}

void Archetype::DefaultConstructComponent(utils::TypeID component_id, entity_id_t entity) {
    if (const auto& component_info = GetComponentInfo(component_id);
        component_info.default_constructor) {
        component_info.default_constructor(GetComponentData(component_id, entity));
    }
}

void Archetype::CopyConstructComponent(utils::TypeID component_id, entity_id_t entity, const std::byte* src) {
    if (const auto& component_info = GetComponentInfo(component_id);
        component_info.copy_constructor) {
        component_info.copy_constructor(GetComponentData(component_id, entity), src);
    }
}

void Archetype::MoveConstructComponent(utils::TypeID component_id, entity_id_t entity, std::byte* src) {
    if (const auto& component_info = GetComponentInfo(component_id);
        component_info.move_constructor) {
        component_info.move_constructor(GetComponentData(component_id, entity), src);
    }
}

void Archetype::DestructComponent(utils::TypeID component_id, entity_id_t entity) noexcept {
    if (const auto& component_info = GetComponentInfo(component_id);
        component_info.destructor) {
        component_info.destructor(GetComponentData(component_id, entity));
    }
}

void Archetype::DestructAllComponents(entity_id_t entity) noexcept {
    for (const auto& component_info : m_ComponentInfoSet) {
        DestructComponent(component_info.type_id, entity);
    }
}

auto Archetype::GetComponentData(utils::TypeID component_id, entity_id_t entity) noexcept -> std::byte* {
    if (!m_EntityMap.contains(entity))
        return nullptr;

    const auto component_info   = GetComponentInfo(component_id);
    const auto component_offset = GetComponentOffset(component_id);

    const auto [chunk_index, index_in_chunk] = m_EntityMap.at(entity);
    return const_cast<std::byte*>(m_Chunks[chunk_index].data.GetData()) + component_offset + index_in_chunk * component_info.size;
}

auto Archetype::GetAllComponentData(entity_id_t entity) noexcept -> std::unordered_map<utils::TypeID, std::byte*> {
    std::unordered_map<utils::TypeID, std::byte*> result;
    result.reserve(m_ComponentInfoSet.size());
    for (const auto& component_info : m_ComponentInfoSet) {
        result.emplace(component_info.type_id, GetComponentData(component_info.type_id, entity));
    }
    return result;
}

auto Archetype::GetComponentBuffers(utils::TypeID component_id) const noexcept -> std::pmr::vector<std::pair<std::byte*, std::size_t>> {
    const auto component_offset = GetComponentOffset(component_id);

    std::pmr::vector<std::pair<std::byte*, std::size_t>> result;
    result.reserve(m_Chunks.size());
    for (auto& chunk : m_Chunks) {
        result.emplace_back(const_cast<std::byte*>(chunk.data.GetData()) + component_offset, chunk.num_entity_in_chunk);
    }
    return result;
}

auto Archetype::GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo& {
    return *ranges::find_if(m_ComponentInfoSet, [component_id](const auto& info) { return info.type_id == component_id; });
}

auto Archetype::GetComponentOffset(utils::TypeID component_id) const noexcept -> std::size_t {
    return m_ChunkInfo.component_offsets.at(component_id);
}

auto Archetype::GetOrCreateChunk() noexcept -> Chunk& {
    if (m_Chunks.empty() || m_ChunkInfo.num_entities_per_chunk == m_Chunks.back().num_entity_in_chunk) {
        m_Chunks.emplace_back();
    }
    return m_Chunks.back();
}

auto Archetype::GetLastEntity() const -> entity_id_t {
    if (m_Chunks.empty()) {
        throw std::out_of_range("No entity in this archetype");
    }

    const auto  entity_component_info = detail::create_static_component_info<Entity>();
    const auto& chunk                 = m_Chunks.back();

    auto entity_ptr = chunk.data.GetData() + m_ChunkInfo.component_offsets.at(entity_component_info.type_id) + (chunk.num_entity_in_chunk - 1) * entity_component_info.size;

    return reinterpret_cast<const Entity*>(entity_ptr)->GetId();
}

Archetype::Chunk::Chunk() : data(sm_chunk_size, nullptr, sm_align_size) {}

}  // namespace hitagi::ecs