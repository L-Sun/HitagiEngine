#pragma once
#include <hitagi/ecs/common_types.hpp>
#include <hitagi/ecs/component.hpp>
#include <hitagi/core/buffer.hpp>
#include <hitagi/utils/utils.hpp>
#include <hitagi/utils/soa.hpp>

namespace hitagi::ecs {

class Archetype {
public:
    Archetype(detail::ComponentInfoSet component_infos);
    ~Archetype();

    const auto& GetComponentInfoSet() const noexcept { return m_ComponentInfoSet; }

    // create a entity in this archetype without any initialization
    void AllocateFor(entity_id_t entity) noexcept;

    // destroy a entity in this archetype without any destruction
    void DeallocateFor(entity_id_t entity) noexcept;

    // Template member functions
    template <Component T, typename... Args>
    auto ConstructComponent(entity_id_t entity, Args&&... args) -> T&;

    template <Component T>
    void DestructComponent(entity_id_t entity);

    template <Component T>
    auto GetComponent(entity_id_t entity) noexcept -> T&;

    bool HasComponent(utils::TypeID component) const noexcept;

    // Raw pointer member functions
    void DefaultConstructComponent(utils::TypeID component_id, entity_id_t entity);
    void CopyConstructComponent(utils::TypeID component_id, entity_id_t entity, const std::byte* src);
    void MoveConstructComponent(utils::TypeID component_id, entity_id_t entity, std::byte* src);
    void DestructComponent(utils::TypeID component_id, entity_id_t entity) noexcept;
    void DestructAllComponents(entity_id_t entity) noexcept;

    auto GetComponentData(utils::TypeID component_id, entity_id_t entity) noexcept -> std::byte*;
    auto GetAllComponentData(entity_id_t entity) noexcept -> std::unordered_map<utils::TypeID, std::byte*>;

    auto GetComponentBuffers(utils::TypeID component_id) const noexcept -> std::pmr::vector<std::pair<std::byte*, std::size_t>>;

private:
    constexpr static auto sm_chunk_size = 2_kB;
    constexpr static auto sm_align_size = 64;

    struct ChunkInfo {
        std::size_t                                         num_entities_per_chunk;
        std::pmr::unordered_map<utils::TypeID, std::size_t> component_offsets;
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

    auto GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo&;
    auto GetComponentOffset(utils::TypeID component_id) const noexcept -> std::size_t;
    auto GetOrCreateChunk() noexcept -> Chunk&;
    auto GetLastEntity() const -> entity_id_t;

    detail::ComponentInfoSet m_ComponentInfoSet;
    ChunkInfo                m_ChunkInfo;
    std::pmr::vector<Chunk>  m_Chunks;

    std::pmr::unordered_map<entity_id_t, std::pair<std::size_t, std::size_t>> m_EntityMap;
};

template <Component T, typename... Args>
auto Archetype::ConstructComponent(entity_id_t entity, Args&&... args) -> T& {
    return *std::construct_at<T>(&GetComponent<T>(entity), std::forward<Args>(args)...);
}

template <Component T>
void Archetype::DestructComponent(entity_id_t entity) {
    std::destroy_at<T>(&GetComponent<T>(entity));
}

template <Component T>
auto Archetype::GetComponent(entity_id_t entity) noexcept -> T& {
    return *reinterpret_cast<T*>(GetComponentData(utils::TypeID::Create<T>(), entity));
}

}  // namespace hitagi::ecs
