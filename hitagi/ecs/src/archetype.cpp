#include "archetype.hpp"

#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/map.hpp>

#include <algorithm>

namespace hitagi::ecs {

Archetype::Archetype(detail::ComponentInfoSet component_infos)
    : m_ComponentInfoSet(std::move(component_infos)) {
    // calculate chunk info
    {
        // a `ChunkInfo::chunk_size` bytes buffer contain
        // header(ChunkInfo::align_size),
        // c_1_1, c_1_2, ..., c_1_n, padding_1,
        // c_2_1, c_2_2, ..., c_2_n, padding_2,
        // ...
        // c_m_1, c_m_2, ..., c_m_n, padding_m,
        // where c_i_j is the i-th component of j-th entity, and all element are contiguous, also each line align to `ChunkInfo::align_size`

        // calculate num_entities without alignment
        std::size_t num_entities = ChunkInfo::chunk_size /
                                   ranges::accumulate(m_ComponentInfoSet, std::size_t{0}, [](std::size_t acc, const auto& info) { return acc + info.size; });

        const auto calculate_offset = [this](std::size_t num_entities) {
            std::size_t current_offset = 0;
            for (const auto& component_info : m_ComponentInfoSet) {
                m_ChunkInfo.component_offsets[component_info] = current_offset;
                current_offset += utils::align(num_entities * component_info.size, ChunkInfo::align_size);
            }
            return current_offset;
        };

        while (calculate_offset(num_entities) > ChunkInfo::chunk_size) {
            num_entities--;
        }
        m_ChunkInfo.num_entities_per_chunk = num_entities;
    }
}

Archetype::~Archetype() {
    for (auto& chunk : m_Chunks) {
        for (const auto& component_info : m_ComponentInfoSet) {
            const auto component_data = chunk.data.GetData() + m_ChunkInfo.component_offsets.at(component_info);
            for (std::size_t i = 0; i < chunk.num_entity_in_chunk; i++) {
                if (component_info.destructor)
                    component_info.destructor(component_data + i * component_info.size);
            }
        }
    }
}

void Archetype::CreateEntities(const std::pmr::vector<Entity>& entities) noexcept {
    std::size_t entity_index = 0;

    while (entity_index < entities.size()) {
        auto&      chunk                  = GetOrCreateChunk();
        const auto num_entities_to_create = std::min(entities.size() - entity_index, m_ChunkInfo.num_entities_per_chunk - chunk.num_entity_in_chunk);

        const auto chunk_index            = m_Chunks.size() - 1;
        const auto entity_offset_in_chunk = chunk.num_entity_in_chunk;

        for (std::size_t i = 0; i < num_entities_to_create; i++) {
            m_EntityMap.emplace(entities[entity_index + i], std::pair{chunk_index, entity_offset_in_chunk + i});
        }

        entity_index += num_entities_to_create;
        chunk.num_entity_in_chunk += num_entities_to_create;
    }

    for (const auto& component_info : m_ComponentInfoSet) {
        for (auto entity : entities) {
            if (component_info.constructor)
                component_info.constructor(GetComponentData(entity, component_info.type_id));
        }
    }

    // init Entity component
    const auto entity_component_id = utils::TypeID::Create<Entity>();
    for (auto entity : entities) {
        *reinterpret_cast<Entity*>(GetComponentData(entity, entity_component_id)) = entity;
    }
}

void Archetype::DeleteEntity(Entity entity) noexcept {
    SwapEntityData(entity, GetLastEntity());

    for (const auto& component_info : m_ComponentInfoSet) {
        if (component_info.destructor)
            component_info.destructor(GetComponentData(entity, component_info.type_id));
    }
    m_EntityMap.erase(entity);
    if (m_Chunks.back().num_entity_in_chunk == 0) {
        m_Chunks.pop_back();
    }
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

auto Archetype::GetComponentData(Entity entity, utils::TypeID component_id) const noexcept -> std::byte* {
    if (!m_EntityMap.contains(entity))
        return nullptr;

    const auto component_info   = GetComponentInfo(component_id);
    const auto component_offset = GetComponentOffset(component_id);

    const auto [chunk_index, index_in_chunk] = m_EntityMap.at(entity);
    return const_cast<std::byte*>(m_Chunks[chunk_index].data.GetData()) + component_offset + index_in_chunk * component_info.size;
}

auto Archetype::NumEntities() const noexcept -> std::size_t {
    return m_EntityMap.size();
}

bool Archetype::HasComponent(utils::TypeID component) const noexcept {
    return ranges::find_if(m_ComponentInfoSet, [component](const auto& info) { return info.type_id == component; }) != m_ComponentInfoSet.end();
}

auto Archetype::GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo& {
    return *ranges::find_if(m_ComponentInfoSet, [component_id](const auto& info) { return info.type_id == component_id; });
}

auto Archetype::GetComponentOffset(utils::TypeID component_id) const noexcept -> std::size_t {
    return m_ChunkInfo.component_offsets.at(GetComponentInfo(component_id));
}

auto Archetype::GetOrCreateChunk() noexcept -> Chunk& {
    if (m_Chunks.empty() || m_ChunkInfo.num_entities_per_chunk == m_Chunks.back().num_entity_in_chunk) {
        m_Chunks.emplace_back(m_ChunkInfo);
    }
    return m_Chunks.back();
}

auto Archetype::GetLastEntity() const -> Entity {
    if (m_Chunks.empty()) {
        throw std::out_of_range("No entity in this archetype");
    }

    const auto  entity_component_info = detail::create_static_component_info<Entity>();
    const auto& chunk                 = m_Chunks.back();

    auto entity_ptr = chunk.data.GetData() + m_ChunkInfo.component_offsets.at(entity_component_info) + (chunk.num_entity_in_chunk - 1) * entity_component_info.size;

    return *reinterpret_cast<const Entity*>(entity_ptr);
}

void Archetype::SwapEntityData(Entity lhs, Entity rhs) noexcept {
    if (lhs == rhs) return;
    for (const auto& component_info : m_ComponentInfoSet) {
        auto lhs_ptr = GetComponentData(lhs, component_info.type_id);
        auto rhs_ptr = GetComponentData(rhs, component_info.type_id);
        std::swap_ranges(lhs_ptr, lhs_ptr + component_info.size, rhs_ptr);
    }
    std::swap(m_EntityMap[lhs], m_EntityMap[rhs]);
}

Archetype::Chunk::Chunk(const ChunkInfo& chunk_info)
    : data(ChunkInfo::chunk_size, nullptr, ChunkInfo::align_size) {
}

}  // namespace hitagi::ecs