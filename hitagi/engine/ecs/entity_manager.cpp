export module ecs:entity_manager;
import interop.spdlog;

import std;
import utils;

import :component;
import :entity_storage;
import :entity;

export namespace hitagi::ecs {

class World;

class EntityManager {
public:
    void RegisterDynamicComponent(ComponentInfo component) { m_Storage.RegisterDynamicComponent(std::move(component)); }
    auto GetDynamicComponentInfo(std::string_view name) const -> const ComponentInfo& { return m_Storage.GetDynamicComponentInfo(name); }
    bool Has(Entity entity) const noexcept { return entity.m_Storage == &m_Storage && m_Storage.Has(entity.GetId()); }

    [[nodiscard]] auto Create() -> Entity { return CreateMany(1).front(); }
    template <typename... Components>
        requires(sizeof...(Components) > 0 && ((Component<std::remove_cvref_t<Components>> && utils::not_same_as<std::remove_cvref_t<Components>, Entity>) && ...))
    [[nodiscard]] auto Create(Components&&... components) -> Entity {
        auto entity = Create();
        try {
            (entity.Emplace<std::remove_cvref_t<Components>>(std::forward<Components>(components)), ...);
        } catch (...) {
            Destroy(entity);
            throw;
        }
        return entity;
    }

    void Destroy(Entity& entity) {
        if (!Has(entity)) throw std::out_of_range("Entity does not belong to this manager");
        m_Storage.Destroy(entity.GetId());
        entity = {};
    }

    // MSVC 19.51 miscompiles imported CreateMany(n) calls with a default argument
    // after Create() instantiates CreateMany<> in this module: neither n nor the
    // default empty set is passed. Forward explicitly instead of using a default.
    template <Component... Components>
        requires((std::default_initializable<Components> && utils::not_same_as<Components, Entity>) && ...)
    [[nodiscard]] auto CreateMany(std::size_t num) -> std::pmr::vector<Entity> {
        return CreateMany<Components...>(num, {});
    }

    template <Component... Components>
        requires((std::default_initializable<Components> && utils::not_same_as<Components, Entity>) && ...)
    [[nodiscard]] auto CreateMany(std::size_t num, const std::pmr::set<std::string_view>& dynamic_components) -> std::pmr::vector<Entity>;

    auto NumEntities() const noexcept { return m_Storage.NumEntities(); }

    // Read-only snapshot for serialization/inspection, without running systems.
    // The host must serialize this call with structural mutation.
    template <Component... Components>
    auto GetEntities() const -> std::pmr::vector<Entity> {
        std::pmr::vector<Entity> entities;
        entities.reserve(NumEntities());
        for (auto id : m_Storage.GetEntityIds()) {
            auto entity = m_Storage.GetComponent<Entity>(id);
            if ((entity.Has<Components>() && ...)) entities.emplace_back(entity);
        }
        return entities;
    }

private:
    friend World;
    explicit EntityManager(std::shared_ptr<spdlog::logger> logger) : m_Storage(std::move(logger)) {
        m_Storage.RegisterComponent<Entity>();
    }

    detail::EntityStorage m_Storage;
};

template <Component... Components>
    requires((std::default_initializable<Components> && utils::not_same_as<Components, Entity>) && ...)
auto EntityManager::CreateMany(std::size_t num, const std::pmr::set<std::string_view>& dynamic_components) -> std::pmr::vector<Entity> {
    (m_Storage.RegisterComponent<Components>(), ...);
    detail::ComponentIdList components{utils::TypeID::Create<Entity>(), utils::TypeID::Create<Components>()...};
    for (auto name : dynamic_components) {
        components.emplace_back(m_Storage.GetDynamicComponentInfo(name).type_id);
    }
    std::pmr::vector<Entity> entities;
    entities.reserve(num);
    const entity_id_t first = m_Storage.CreateMany(num, std::move(components));
    for (std::size_t index = 0; index < num; ++index) {
        const entity_id_t id = first + index;
        Entity            entity(&m_Storage, id);
        m_Storage.GetComponent<Entity>(id) = entity;
        entities.emplace_back(entity);
    }
    return entities;
}

}  // namespace hitagi::ecs
