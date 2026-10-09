export module ecs:archetype;
import std;
import utils;
import core;

import :component;

export namespace hitagi::ecs {

class Archetype {
public:
    struct Column {
        const ComponentInfo* info;
        std::size_t          offset;
    };
    struct ComponentData {
        std::byte*  data;
        std::size_t stride;
        auto        operator[](std::size_t row) const noexcept -> std::byte* { return data + row * stride; }
    };

    // Descriptions belong to EntityStorage and must outlive this archetype.
    explicit Archetype(std::span<const ComponentInfo* const> components);
    ~Archetype();
    Archetype(const Archetype&)            = delete;
    Archetype& operator=(const Archetype&) = delete;

    const auto& GetColumns() const noexcept { return m_Columns; }
    bool        HasComponent(utils::TypeID component) const noexcept { return FindColumn(component) != nullptr; }
    void        AllocateFor(entity_id_t entity);
    // The caller has destroyed the erased row. Relocation/destruction must not throw.
    void DeallocateFor(entity_id_t entity) noexcept;
    void DefaultConstructComponent(utils::TypeID component, entity_id_t entity);
    void MoveConstructComponent(utils::TypeID component, entity_id_t entity, std::byte* source) noexcept;
    void DestructComponent(utils::TypeID component, entity_id_t entity) noexcept;
    void DestructAllComponents(entity_id_t entity) noexcept;
    auto GetComponentData(utils::TypeID component, entity_id_t entity) const -> std::byte*;

    template <Component T>
    auto GetComponent(entity_id_t entity) const -> T& {
        return *reinterpret_cast<T*>(GetComponentData(utils::TypeID::Create<T>(), entity));
    }

    template <std::size_t N, typename Func>
    void ForEachChunk(std::span<const utils::TypeID> components, Func&& visit) const {
        std::array<const Column*, N> columns;
        for (std::size_t index = 0; index < N; ++index) columns[index] = FindColumn(components[index]);
        std::array<ComponentData, N> data;
        for (const auto& chunk : m_Chunks) {
            for (std::size_t index = 0; index < N; ++index) {
                data[index] = {const_cast<std::byte*>(chunk.data.GetData()) + columns[index]->offset, columns[index]->info->size};
            }
            visit(data, chunk.count);
        }
    }

private:
    struct Chunk {
        Chunk(std::size_t size, std::size_t alignment) : data(size, nullptr, alignment) {}
        core::Buffer data;
        std::size_t  count = 0;
    };

    auto        FindColumn(utils::TypeID component) const noexcept -> const Column*;
    auto        Data(const Column& column, std::size_t row) const noexcept -> std::byte*;
    static void Relocate(const ComponentInfo& info, std::byte* target, std::byte* source) noexcept;

    std::pmr::vector<Column>                          m_Columns;
    std::size_t                                       m_Capacity  = 0;
    std::size_t                                       m_ChunkSize = 2_kB;
    std::size_t                                       m_Alignment = 64;
    std::pmr::vector<Chunk>                           m_Chunks;
    std::pmr::vector<entity_id_t>                     m_Entities;
    std::pmr::unordered_map<entity_id_t, std::size_t> m_EntityRows;
};

}  // namespace hitagi::ecs

namespace hitagi::ecs {

Archetype::Archetype(std::span<const ComponentInfo* const> components) {
    if (components.empty()) throw std::invalid_argument("An archetype must contain at least one component");
    std::size_t row_size = 0;
    for (const auto* info : components) {
        if (info->size == 0 || !std::has_single_bit(info->alignment) || info->size % info->alignment != 0)
            throw std::invalid_argument("Invalid component size or alignment");
        if (info->size > std::numeric_limits<std::size_t>::max() - row_size)
            throw std::length_error("Archetype row is too large");
        row_size += info->size;
        m_Alignment = std::max(m_Alignment, info->alignment);
        m_Columns.push_back({info, 0});
    }
    std::ranges::sort(m_Columns, {}, [](const Column& column) { return column.info->type_id; });
    m_Capacity  = std::max(std::size_t{1}, m_ChunkSize / row_size);
    auto layout = [&] {
        std::size_t end = 0;
        for (auto& column : m_Columns) {
            const auto alignment = std::max(std::size_t{64}, column.info->alignment);
            if (end > std::numeric_limits<std::size_t>::max() - (alignment - 1))
                throw std::length_error("Archetype layout is too large");
            column.offset    = utils::align(end, alignment);
            const auto bytes = m_Capacity * column.info->size;
            if (bytes > std::numeric_limits<std::size_t>::max() - column.offset)
                throw std::length_error("Archetype layout is too large");
            end = column.offset + bytes;
        }
        return end;
    };
    auto bytes = layout();
    while (bytes > m_ChunkSize && m_Capacity > 1) {
        --m_Capacity;
        bytes = layout();
    }
    // If one row exceeds the target size, use a larger single-row chunk.
    m_ChunkSize = std::max(m_ChunkSize, bytes);
}

Archetype::~Archetype() {
    for (const auto entity : m_Entities) DestructAllComponents(entity);
}

auto Archetype::FindColumn(utils::TypeID component) const noexcept -> const Column* {
    const auto column = std::ranges::lower_bound(m_Columns, component, {}, [](const Column& value) { return value.info->type_id; });
    return column != m_Columns.end() && column->info->type_id == component ? &*column : nullptr;
}

auto Archetype::Data(const Column& column, std::size_t row) const noexcept -> std::byte* {
    return const_cast<std::byte*>(m_Chunks[row / m_Capacity].data.GetData()) + column.offset + (row % m_Capacity) * column.info->size;
}

void Archetype::AllocateFor(entity_id_t entity) {
    const bool new_chunk = m_Entities.size() % m_Capacity == 0;
    if (new_chunk) m_Chunks.emplace_back(m_ChunkSize, m_Alignment);
    try {
        m_Entities.push_back(entity);
        try {
            m_EntityRows.emplace(entity, m_Entities.size() - 1);
        } catch (...) {
            m_Entities.pop_back();
            throw;
        }
    } catch (...) {
        if (new_chunk) m_Chunks.pop_back();
        throw;
    }
    ++m_Chunks.back().count;
}

void Archetype::Relocate(const ComponentInfo& info, std::byte* target, std::byte* source) noexcept {
    if (info.move_constructor)
        info.move_constructor(target, source);
    else if (info.copy_constructor)
        info.copy_constructor(target, source);
    else
        std::memcpy(target, source, info.size);  // Dynamic raw-byte component.
}

void Archetype::DeallocateFor(entity_id_t entity) noexcept {
    const auto row  = m_EntityRows.at(entity);
    const auto last = m_Entities.size() - 1;
    if (row != last) {
        for (const auto& column : m_Columns) {
            Relocate(*column.info, Data(column, row), Data(column, last));
            if (column.info->destructor) column.info->destructor(Data(column, last));
        }
        m_Entities[row]                  = m_Entities.back();
        m_EntityRows.at(m_Entities[row]) = row;
    }
    m_Entities.pop_back();
    m_EntityRows.erase(entity);
    if (--m_Chunks.back().count == 0) m_Chunks.pop_back();
}

auto Archetype::GetComponentData(utils::TypeID component, entity_id_t entity) const -> std::byte* {
    const auto* column = FindColumn(component);
    if (!column) throw std::invalid_argument("Entity does not have the requested component");
    return Data(*column, m_EntityRows.at(entity));
}

void Archetype::DefaultConstructComponent(utils::TypeID component, entity_id_t entity) {
    const auto& info = *FindColumn(component)->info;
    auto*       data = GetComponentData(component, entity);
    if (info.default_constructor)
        info.default_constructor(data);
    else
        std::memset(data, 0, info.size);
}

void Archetype::MoveConstructComponent(utils::TypeID component, entity_id_t entity, std::byte* source) noexcept {
    Relocate(*FindColumn(component)->info, GetComponentData(component, entity), source);
}

void Archetype::DestructComponent(utils::TypeID component, entity_id_t entity) noexcept {
    const auto& info = *FindColumn(component)->info;
    if (info.destructor) info.destructor(GetComponentData(component, entity));
}

void Archetype::DestructAllComponents(entity_id_t entity) noexcept {
    const auto row = m_EntityRows.at(entity);
    for (const auto& column : m_Columns) {
        if (column.info->destructor) column.info->destructor(Data(column, row));
    }
}

}  // namespace hitagi::ecs
