module;
#include "interop/tracy_macros.hpp"

export module ecs:schedule;
import interop.spdlog;
import interop.tracy;

import std;
import utils;
import core;

import :entity;
import :entity_storage;
import :filter;
import :component;
import :query;

export namespace hitagi::ecs {

// A label retains enum identity, not just its underlying value or a type hash.
// Equal-valued aliases within the same enum intentionally identify one group.
class Group {
public:
    template <typename Enum>
        requires std::is_enum_v<Enum>
    Group(Enum value) noexcept
        : m_Type(typeid(Enum)), m_Value(static_cast<std::uint64_t>(std::to_underlying(value))) {
        static_assert(sizeof(std::underlying_type_t<Enum>) <= sizeof(std::uint64_t));
    }
    bool operator==(const Group&) const noexcept = default;

private:
    friend class Schedule;
    std::type_index m_Type;
    std::uint64_t   m_Value;
};

class Schedule {
    struct TaskBase {
        TaskBase(std::string_view name, detail::ComponentIdList components, Filter filter)
            : name(name), components(std::move(components)), filter(std::move(filter)) {}
        virtual ~TaskBase()                                        = default;
        virtual void Run(detail::EntityStorage&, core::JobSystem&) = 0;

        std::optional<std::size_t> group;
        std::pmr::string           name;
        detail::ComponentIdList    components;
        Filter                     filter;
    };

    template <typename Func>
    struct Task : TaskBase {
        Task(std::string_view name, detail::ComponentIdList components, Filter filter, Func task)
            : TaskBase(name, std::move(components), std::move(filter)), task(std::move(task)) {}
        void Run(detail::EntityStorage& storage, core::JobSystem& jobs) final;
        Func task;
    };

public:
    ~Schedule() = default;
    // A task with no component parameters executes once, even in an empty World.
    template <typename Func>
        requires std::invocable<Func&>
    Schedule& Request(std::string_view name, Func&& task, std::optional<Group> group = {}) {
        using Function = std::decay_t<Func>;
        static_assert(utils::function_traits<Function>::args_size == 0, "Once tasks must have no parameters");
        auto bound = std::make_shared<Task<Function>>(name, detail::ComponentIdList{}, Filter{}, std::forward<Func>(task));
        Request(std::move(bound), {}, group);
        return *this;
    }

    // Seconds supplied by the host for the current update, including cached schedules.
    auto GetDeltaSeconds() const noexcept -> double { return m_DeltaSeconds; }
    template <typename Func>
        requires(!std::invocable<Func&>)
    Schedule& Request(std::string_view name, Func&& task, DynamicComponentList dynamic_components = {}, Filter filter = {}, std::optional<Group> group = {});
    template <typename Func>
        requires(!std::invocable<Func&>)
    Schedule& Request(std::string_view name, Func&& task, Group group) {
        return Request(name, std::forward<Func>(task), {}, {}, group);
    }
    void SetOrder(std::string_view first_task, std::string_view second_task);
    // Every task in the first group finishes before any task in the second.
    // Empty groups still preserve transitive order and participate in cycle checks.
    void SetOrder(Group first_group, Group second_group);

private:
    friend class World;
    double m_DeltaSeconds = 0.0;
    Schedule(detail::EntityStorage& storage, std::shared_ptr<spdlog::logger> logger, std::string_view name)
        : m_Storage(storage), m_Logger(std::move(logger)), m_Name(name) {}

    void Request(std::shared_ptr<TaskBase> task, std::span<const detail::ParameterAccess> parameters, std::optional<Group> group);
    auto FindOrAddGroup(Group group) -> std::size_t;
    void Run(core::JobSystem& job_system);
    void BuildGraph();

    struct ComponentAccess {
        std::pmr::vector<std::size_t> read;
        std::pmr::vector<std::size_t> write;
    };

    detail::EntityStorage&          m_Storage;
    std::shared_ptr<spdlog::logger> m_Logger;
    std::pmr::string                m_Name;
    // Destroy task captures before the instances they may borrow.
    std::vector<std::shared_ptr<void>>                              m_SystemStates;
    std::pmr::vector<std::shared_ptr<TaskBase>>                     m_Tasks;
    std::pmr::unordered_map<std::pmr::string, std::size_t>          m_TaskNameToIndex;
    std::pmr::unordered_map<utils::TypeID, ComponentAccess>         m_Accesses;
    std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> m_CustomOrder;

    std::vector<Group>                               m_Groups;
    std::vector<std::pair<std::size_t, std::size_t>> m_GroupOrder;
    std::unique_ptr<core::JobGraph>                  m_Graph;
    bool                                             m_GraphDirty       = true;
    core::JobSystem*                                 m_RunningJobSystem = nullptr;
};

template <typename Func>
    requires(!std::invocable<Func&>)
Schedule& Schedule::Request(std::string_view name, Func&& task, DynamicComponentList dynamic_components, Filter filter, std::optional<Group> group) {
    using Function = std::decay_t<Func>;
    using traits   = utils::function_traits<Function>;
    static_assert(traits::args_size > 0, "Task must request at least one component");

    static_assert(traits::args_size == 1, "A system callback takes one Query (or no arguments)");
    using Q = typename traits::template arg_t<0>;
    static_assert(detail::is_query<Q>, "System parameters must be Query values");
    auto                    parameters = Q::Describe(dynamic_components);
    detail::ComponentIdList components;
    for (const auto& access : parameters) components.push_back(access.component);
    auto bound = std::make_shared<Task<Function>>(name, std::move(components), std::move(filter), std::forward<Func>(task));
    Request(std::move(bound), parameters, group);
    return *this;
}

template <typename Func>
void Schedule::Task<Func>::Run(detail::EntityStorage& storage, core::JobSystem& jobs) {
    using traits = utils::function_traits<Func>;
    if constexpr (traits::args_size == 0) {
        std::invoke(task);
    } else {
        using Q = typename traits::template arg_t<0>;
        std::invoke(task, Q(storage, components, filter, jobs));
    }
}

}  // namespace hitagi::ecs

namespace hitagi::ecs {

void Schedule::Request(std::shared_ptr<TaskBase> task, std::span<const detail::ParameterAccess> parameters, std::optional<Group> group) {
    if (m_TaskNameToIndex.contains(task->name)) {
        throw std::invalid_argument(std::format("Duplicate ECS task: {}", task->name));
    }
    if (group) task->group = FindOrAddGroup(*group);
    const auto index = m_Tasks.size();
    m_TaskNameToIndex.emplace(task->name, index);
    m_Tasks.emplace_back(std::move(task));
    for (const auto& parameter : parameters) {
        auto& access = m_Accesses[parameter.component];
        auto  append = [index](auto& tasks) {
            if (tasks.empty() || tasks.back() != index) tasks.emplace_back(index);
        };
        switch (parameter.mode) {
            case detail::AccessMode::Read:
                append(access.read);
                break;
            case detail::AccessMode::Write:
                append(access.write);
                break;
        }
    }
    m_GraphDirty = true;
}

void Schedule::SetOrder(std::string_view first_task, std::string_view second_task) {
    m_CustomOrder.emplace_back(first_task, second_task);
    m_GraphDirty = true;
}

auto Schedule::FindOrAddGroup(Group group) -> std::size_t {
    const auto found = std::ranges::find(m_Groups, group);
    if (found != m_Groups.end()) {
        return static_cast<std::size_t>(found - m_Groups.begin());
    }
    m_Groups.push_back(group);
    return m_Groups.size() - 1;
}

void Schedule::SetOrder(Group first_group, Group second_group) {
    const auto first  = FindOrAddGroup(first_group);
    const auto second = FindOrAddGroup(second_group);
    m_GroupOrder.emplace_back(first, second);
    m_GraphDirty = true;
}

void Schedule::Run(core::JobSystem& job_system) {
    if (m_GraphDirty) BuildGraph();
    ZoneScopedN("ECSFrame");
    m_Storage.BeginExecution();
    m_RunningJobSystem = &job_system;
    try {
        job_system.Run(*m_Graph);
    } catch (...) {
        m_RunningJobSystem = nullptr;
        m_Storage.EndExecution();
        throw;
    }
    m_RunningJobSystem = nullptr;
    m_Storage.EndExecution();
}

void Schedule::BuildGraph() {
    // Entry/exit nodes express group barriers without all-to-all task edges.
    const auto                                      task_count = m_Tasks.size();
    const auto                                      entry      = [task_count](std::size_t group) { return task_count + 2 * group; };
    const auto                                      exit       = [&](std::size_t group) { return entry(group) + 1; };
    std::pmr::vector<std::pmr::vector<std::size_t>> graph(task_count + 2 * m_Groups.size());

    for (std::size_t group = 0; group < m_Groups.size(); ++group)
        graph[entry(group)].emplace_back(exit(group));

    for (std::size_t task = 0; task < task_count; ++task) {
        if (const auto group = m_Tasks[task]->group) {
            graph[entry(*group)].emplace_back(task);
            graph[task].emplace_back(exit(*group));
        }
    }
    for (const auto& [first, second] : m_GroupOrder)
        graph[exit(first)].emplace_back(entry(second));
    for (const auto& [first, second] : m_CustomOrder) {
        const auto from = m_TaskNameToIndex.find(first);
        const auto to   = m_TaskNameToIndex.find(second);
        if (from == m_TaskNameToIndex.end() || to == m_TaskNameToIndex.end()) {
            throw std::invalid_argument(std::format("Cannot order {} -> {}: task does not exist", first, second));
        }
        graph[from->second].emplace_back(to->second);
    }

    std::pmr::vector<std::size_t> in_degree(graph.size(), 0);
    for (auto& successors : graph) {
        std::ranges::sort(successors);
        successors.erase(std::unique(successors.begin(), successors.end()), successors.end());
        for (const auto next : successors) ++in_degree[next];
    }
    std::pmr::vector<std::size_t> ready;
    for (std::size_t index = 0; index < graph.size(); ++index) {
        if (in_degree[index] == 0) ready.emplace_back(index);
    }
    std::pmr::vector<std::size_t> sorted;
    while (!ready.empty()) {
        const auto current = ready.back();
        ready.pop_back();
        sorted.emplace_back(current);
        for (const auto next : graph[current]) {
            if (--in_degree[next] == 0) ready.emplace_back(next);
        }
    }
    const auto node_name = [&](std::size_t index) -> std::string {
        if (index < task_count) return std::string(m_Tasks[index]->name);
        const auto& group = m_Groups[(index - task_count) / 2];
        return std::format(
            "Group({}:{})/{}",
            group.m_Type.name(),
            group.m_Value,
            (index - task_count) % 2 == 0 ? "begin" : "end");
    };
    if (sorted.size() != graph.size()) {
        std::pmr::string dot("digraph {\n");
        for (std::size_t index = 0; index < graph.size(); ++index) {
            dot += std::format("  {} [label=\"{}\"];\n", index, node_name(index));
            for (const auto next : graph[index]) dot += std::format("  {} -> {};\n", index, next);
        }
        dot += "}\n";
        m_Logger->error("Cycle in task graph:\n{}", dot);
        throw std::invalid_argument("Cycle in ECS task graph");
    }

    // Access conflicts never invent a business order. Every conflicting pair
    // must be connected by explicit task/group dependencies (possibly transitively).
    const auto reaches = [&](std::size_t first, std::size_t second) {
        std::pmr::vector<bool>        visited(graph.size(), false);
        std::pmr::vector<std::size_t> pending{first};
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            if (current == second) return true;
            if (visited[current]) continue;
            visited[current] = true;
            for (const auto next : graph[current]) pending.push_back(next);
        }
        return false;
    };
    const auto require_order = [&](std::size_t first, std::size_t second) {
        if (first == second || reaches(first, second) || reaches(second, first)) return;
        throw std::invalid_argument(std::format("Unordered component conflict: {} and {}; declare SetOrder", node_name(first), node_name(second)));
    };
    for (const auto& [component, access] : m_Accesses) {
        for (const auto writer : access.write) {
            for (const auto reader : access.read) require_order(writer, reader);
            for (const auto other : access.write) require_order(writer, other);
        }
    }

    std::pmr::vector<core::JobGraph::Task> tasks;
    tasks.reserve(graph.size());
    for (const auto& task : m_Tasks) {
        const auto execution = [this, task] {
            thread_local bool tracy_thread_named = false;
            if (!tracy_thread_named) {
                if (const auto worker = m_RunningJobSystem->GetCurrentWorkerId(); worker >= 0) {
#ifdef TRACY_ENABLE
                    const auto name = std::format("Hitagi/ECS/{}/Worker-{}", m_Name, worker);
                    tracy::SetThreadName(name.c_str());
#endif
                    tracy_thread_named = true;
                }
            }
            ZoneScoped;
            ZoneName(task->name.data(), task->name.size());
            task->Run(m_Storage, *m_RunningJobSystem);
        };
        tasks.push_back({std::pmr::string(task->name), std::move(execution)});
    }
    for (std::size_t index = task_count; index < graph.size(); ++index)
        tasks.push_back({std::pmr::string(node_name(index)), [] {}});
    std::pmr::vector<std::pmr::vector<std::size_t>> edges;
    for (const auto& next : graph) edges.emplace_back(next.begin(), next.end());
    m_Graph      = std::make_unique<core::JobGraph>(std::move(tasks), edges);
    m_GraphDirty = false;
}

}  // namespace hitagi::ecs
