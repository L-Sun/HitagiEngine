export module ecs:query;
import std;
import core;
import utils;
import :component;
import :entity;
import :entity_storage;
import :filter;

namespace hitagi::ecs::detail {
enum class AccessMode { Read,
                        Write };
template <typename T>
struct ParameterTraits {
    using component_type                 = std::remove_cvref_t<T>;
    static constexpr bool       dynamic  = std::same_as<T, std::byte*> || std::same_as<T, const std::byte*>;
    static constexpr bool       valid    = !std::is_volatile_v<std::remove_reference_t<T>> && !std::is_rvalue_reference_v<T> &&
                                           (dynamic || (Component<component_type> && !std::same_as<T, Entity&>));
    static constexpr bool       writable = dynamic ? std::same_as<T, std::byte*> : std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>;
    static constexpr AccessMode access   = writable ? AccessMode::Write : AccessMode::Read;
    static decltype(auto)       Read(std::byte* data) {
        if constexpr (dynamic)
            return static_cast<T>(data);
        else
            return static_cast<T>(*reinterpret_cast<component_type*>(data));
    }
};
struct ParameterAccess {
    utils::TypeID component;
    AccessMode    mode;
};
}  // namespace hitagi::ecs::detail

export namespace hitagi::ecs {
class Schedule;
// A borrowed query is valid only during its scheduled callback. The callback runs
// once, even for an empty query; the consumer controls how often it traverses it.
template <typename... Parameters>
class Query {
    static_assert(sizeof...(Parameters) > 0);
    static_assert((detail::ParameterTraits<Parameters>::valid && ...), "Invalid Query component access");

public:
    Query(const Query&) = delete;
    Query(Query&&)      = default;
    template <typename Func>
    void Each(Func&& visit) const {
        m_Storage.ForEachChunk<sizeof...(Parameters)>(m_Components, m_Filter, [&](const auto& columns, std::size_t count) {
            for (std::size_t row = 0; row < count; ++row) {
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    std::invoke(visit, detail::ParameterTraits<Parameters>::Read(columns[I][row])...);
                }(std::index_sequence_for<Parameters...>{});
            }
        });
    }
    // The visitor may execute concurrently. Captured state and indirect entity
    // accesses must be safe; all batches join before this call returns.
    template <typename Func>
    void ParallelEach(Func&& visit, std::size_t batch_size = 256) const {
        if (batch_size == 0) throw std::invalid_argument("Query batch size must be positive");
        if (m_Jobs.GetWorkerCount() == 0) {
            Each(std::forward<Func>(visit));
            return;
        }
        std::pmr::vector<core::JobGraph::Task> tasks;
        m_Storage.ForEachChunk<sizeof...(Parameters)>(m_Components, m_Filter, [&](const auto& columns, std::size_t count) {
            for (std::size_t begin = 0; begin < count;) {
                const auto end = begin + std::min(batch_size, count - begin);
                tasks.push_back({"Query batch", [columns, begin, end, &visit] {
                                     for (std::size_t row = begin; row < end; ++row)
                                         [&]<std::size_t... I>(std::index_sequence<I...>) {
                                             std::invoke(visit, detail::ParameterTraits<Parameters>::Read(columns[I][row])...);
                                         }(std::index_sequence_for<Parameters...>{});
                                 }});
                begin = end;
            }
        });
        std::pmr::vector<std::pmr::vector<std::size_t>> edges(tasks.size());
        core::JobGraph                                  graph(std::move(tasks), edges);
        m_Jobs.Run(graph);
    }

private:
    friend class Schedule;
    Query(detail::EntityStorage& storage, std::span<const utils::TypeID> components, const Filter& filter, core::JobSystem& jobs)
        : m_Storage(storage), m_Components(components), m_Filter(filter), m_Jobs(jobs) {}
    static auto Describe(const DynamicComponentList& dynamic_components) -> std::pmr::vector<detail::ParameterAccess> {
        constexpr auto dynamic_count = (std::size_t(detail::ParameterTraits<Parameters>::dynamic) + ...);
        if (dynamic_components.size() != dynamic_count)
            throw std::invalid_argument("Dynamic names must match Query dynamic accesses");
        std::pmr::vector<detail::ParameterAccess> accesses;
        std::size_t                               index  = 0;
        auto                                      append = [&]<typename T>() {
            using P = detail::ParameterTraits<T>;
            utils::TypeID id;
            if constexpr (P::dynamic)
                id = utils::TypeID(dynamic_components[index++]);
            else
                id = utils::TypeID::Create<typename P::component_type>();
            for (const auto& other : accesses)
                if (other.component == id && other.mode != P::access)
                    throw std::invalid_argument("Query requests conflicting aliases of one component");
            accesses.push_back({id, P::access});
        };
        (append.template operator()<Parameters>(), ...);
        return accesses;
    }
    detail::EntityStorage&         m_Storage;
    std::span<const utils::TypeID> m_Components;
    const Filter&                  m_Filter;
    core::JobSystem&               m_Jobs;
};
}  // namespace hitagi::ecs
namespace hitagi::ecs::detail {
template <typename T>
inline constexpr bool is_query = false;
template <typename... T>
inline constexpr bool is_query<Query<T...>> = true;
}  // namespace hitagi::ecs::detail
