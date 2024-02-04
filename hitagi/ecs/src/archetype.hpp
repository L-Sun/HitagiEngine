#pragma once
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/component.hpp>
#include <hitagi/core/buffer.hpp>
#include <hitagi/utils/utils.hpp>

namespace hitagi::ecs {
class Archetype {
public:
    Archetype(detail::ComponentInfoSet component_infos);
    Archetype(const Archetype&)            = delete;
    Archetype(Archetype&&) noexcept        = default;
    Archetype& operator=(const Archetype&) = delete;
    Archetype& operator=(Archetype&&)      = default;
    ~Archetype();

    const auto& GetComponentInfoSet() const noexcept { return m_ComponentInfoSet; }

    void CreateEntities(const std::pmr::vector<Entity>& entities) noexcept;
    void DeleteEntity(Entity entity) noexcept;
    auto GetComponentBuffers(utils::TypeID component_id) const noexcept -> std::pmr::vector<std::pair<std::byte*, std::size_t>>;
    auto GetComponentData(Entity entity, utils::TypeID component_id) const noexcept -> std::byte*;
    auto GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo&;
    auto NumEntities() const noexcept -> std::size_t;
    bool HasComponent(utils::TypeID component) const noexcept;

private:
    struct ChunkInfo {
        constexpr static auto chunk_size = 2_kB;
        constexpr static auto align_size = 64;

        std::size_t                                         num_entities_per_chunk;
        std::pmr::unordered_map<ComponentInfo, std::size_t> component_offsets;
    };

    struct Chunk {
        Chunk();
        Chunk(const Chunk&)            = delete;
        Chunk(Chunk&&)                 = default;
        Chunk& operator=(const Chunk&) = delete;
        Chunk& operator=(Chunk&&)      = default;

        std::size_t  num_entity_in_chunk = 0;
        core::Buffer data;
    };

    auto GetComponentOffset(utils::TypeID component_id) const noexcept -> std::size_t;
    auto GetOrCreateChunk() noexcept -> Chunk&;
    auto GetLastEntity() const -> Entity;
    void SwapEntityData(Entity lhs, Entity rhs) noexcept;

    detail::ComponentInfoSet m_ComponentInfoSet;
    ChunkInfo                m_ChunkInfo;
    std::pmr::vector<Chunk>  m_Chunks;

    // chunk index, index in chunk
    std::pmr::unordered_map<Entity, std::pair<std::size_t, std::size_t>> m_EntityMap;
};
}  // namespace hitagi::ecs