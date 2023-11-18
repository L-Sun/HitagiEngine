#include "archetype.hpp"

#include <range/v3/numeric/accumulate.hpp>

#include <algorithm>

namespace hitagi::ecs {

Archetype::Archetype(detail::ComponentInfos component_infos)
    : m_ComponentInfos(std::move(component_infos)) {
    std::sort(m_ComponentInfos.begin(), m_ComponentInfos.end());
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
                                   ranges::accumulate(m_ComponentInfos, std::size_t{0}, [](std::size_t acc, const auto& info) { return acc + info.size; });

        const auto calculate_offset = [this](std::size_t num_entities) {
            std::size_t current_offset = 0;
            for (const auto& component_info : m_ComponentInfos) {
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
    // TODO
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

    for (const auto& component_info : m_ComponentInfos) {
        for (auto entity : entities) {
            if (component_info.constructor)
                component_info.constructor(GetComponentData(component_info, entity));
        }
    }

    // init Entity component
    const auto entity_component_info = detail::create_component_infos<Entity>().front();
    for (auto entity : entities) {
        *reinterpret_cast<Entity*>(GetComponentData(entity_component_info, entity)) = entity;
    }
}

void Archetype::DeleteEntity(Entity entity) noexcept {
    SwapEntityData(entity, GetLastEntity());

    for (const auto& component_info : m_ComponentInfos) {
        if (component_info.destructor)
            component_info.destructor(GetComponentData(component_info, entity));
    }
    m_EntityMap.erase(entity);
    if (m_Chunks.back().num_entity_in_chunk == 0) {
        m_Chunks.pop_back();
    }
}

auto Archetype::GetComponentBuffers(const detail::ComponentInfo& component_info) const noexcept -> std::pmr::vector<std::pair<std::byte*, std::size_t>> {
    if (!m_ChunkInfo.component_offsets.contains(component_info))
        return {};

    std::pmr::vector<std::pair<std::byte*, std::size_t>> result;
    result.reserve(m_Chunks.size());
    for (auto& chunk : m_Chunks) {
        result.emplace_back(const_cast<std::byte*>(chunk.data.GetData()) + m_ChunkInfo.component_offsets.at(component_info), chunk.num_entity_in_chunk);
    }
    return result;
}

auto Archetype::GetComponentData(const detail::ComponentInfo& component_info, Entity entity) const noexcept -> std::byte* {
    if (!m_EntityMap.contains(entity) || !m_ChunkInfo.component_offsets.contains(component_info))
        return nullptr;

    const auto [chunk_index, index_in_chunk] = m_EntityMap.at(entity);
    return const_cast<std::byte*>(m_Chunks[chunk_index].data.GetData()) + m_ChunkInfo.component_offsets.at(component_info) + index_in_chunk * component_info.size;
}

auto Archetype::NumEntities() const noexcept -> std::size_t {
    return m_EntityMap.size();
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

    const auto  entity_component_info = detail::create_component_infos<Entity>().front();
    const auto& chunk                 = m_Chunks.back();

    auto entity_ptr = chunk.data.GetData() + m_ChunkInfo.component_offsets.at(entity_component_info) + (chunk.num_entity_in_chunk - 1) * entity_component_info.size;

    return *reinterpret_cast<const Entity*>(entity_ptr);
}

void Archetype::SwapEntityData(Entity lhs, Entity rhs) noexcept {
    if (lhs == rhs) return;
    for (const auto& component_info : m_ComponentInfos) {
        auto lhs_ptr = GetComponentData(component_info, lhs);
        auto rhs_ptr = GetComponentData(component_info, rhs);
        std::swap_ranges(lhs_ptr, lhs_ptr + component_info.size, rhs_ptr);
    }
    std::swap(m_EntityMap[lhs], m_EntityMap[rhs]);
}

Archetype::Chunk::Chunk(const ChunkInfo& chunk_info)
    : data(ChunkInfo::chunk_size, nullptr, ChunkInfo::align_size) {
}

}  // namespace hitagi::ecs