module;
#include "interop/tracy_macros.hpp"

export module ecs:schedule;
import interop.taskflow;
import interop.spdlog;
import interop.tracy;

import std;
import utils;
import core;

import :entity;
import :entity_storage;
import :filter;
import :component;

export namespace hitagi::ecs {

template <typename T>
    requires detail::ComponentValueType<T> ||
             detail::ComponentConstValueType<T> ||
             detail::ComponentConstReferenceType<T> ||
             detail::DynamicComponentConstPointerType<T>
struct LastFrame {
    using type = T;
      operator T() const noexcept { return value; }
    T value;
};

}  // namespace hitagi::ecs

namespace hitagi::ecs::detail {

enum class AccessMode { ReadBeforeWrite,
                        Write,
                        ReadAfterWrite };

template <typename T>
struct ParameterType {
    using type                       = T;
    static constexpr bool last_frame = false;
};
template <typename T>
struct ParameterType<LastFrame<T>> {
    using type                       = T;
    static constexpr bool last_frame = true;
};

// One interpretation of a task parameter, shared by validation, ordering and invocation.
template <typename T>
struct ParameterTraits {
    using wrapped                 = ParameterType<std::remove_cvref_t<T>>;
    using value_type              = std::conditional_t<wrapped::last_frame, typename wrapped::type, T>;
    using component_type          = std::remove_cvref_t<value_type>;
    static constexpr bool dynamic = std::same_as<component_type, std::byte*> || std::same_as<component_type, const std::byte*>;
    static constexpr bool valid =
        !std::is_volatile_v<std::remove_reference_t<value_type>> &&
        !std::is_rvalue_reference_v<value_type> &&
        (!wrapped::last_frame || !std::is_reference_v<T>) &&
        ((dynamic && !std::is_reference_v<value_type>) ||
         (Component<component_type> && !std::same_as<value_type, Entity&>));
    static constexpr bool writable =
        dynamic ? std::same_as<component_type, std::byte*> : std::is_lvalue_reference_v<value_type> && !std::is_const_v<std::remove_reference_t<value_type>>;
    static constexpr AccessMode access = wrapped::last_frame ? AccessMode::ReadBeforeWrite : writable ? AccessMode::Write
                                                                                                      : AccessMode::ReadAfterWrite;

    static decltype(auto) Read(std::byte* data) {
        if constexpr (wrapped::last_frame) {
            if constexpr (dynamic)
                return LastFrame<value_type>{data};
            else
                return LastFrame<value_type>{*reinterpret_cast<component_type*>(data)};
        } else if constexpr (dynamic) {
            return static_cast<component_type>(data);
        } else {
            return *reinterpret_cast<std::remove_reference_t<T>*>(data);
        }
    }
};

struct ParameterAccess {
    utils::TypeID component;
    AccessMode    mode;
};

}  // namespace hitagi::ecs::detail

export namespace hitagi::ecs {

class Schedule {
    struct TaskBase {
        TaskBase(std::string_view name, detail::ComponentIdList components, Filter filter)
            : name(name), components(std::move(components)), filter(std::move(filter)) {}
        virtual void            Run(detail::EntityStorage&) = 0;
        std::pmr::string        name;
        detail::ComponentIdList components;
        Filter                  filter;
    };

    template <typename Func>
    struct Task : TaskBase {
        Task(std::string_view name, detail::ComponentIdList components, Filter filter, Func task)
            : TaskBase(name, std::move(components), std::move(filter)), task(std::move(task)) {}
        void Run(detail::EntityStorage& storage) final;
        Func task;
    };

public:
    template <typename Func>
    Schedule& Request(std::string_view name, Func&& task, DynamicComponentList dynamic_components = {}, Filter filter = {});
    void      SetOrder(std::string_view first_task, std::string_view second_task);

private:
    friend class World;
    Schedule(detail::EntityStorage& storage, std::shared_ptr<spdlog::logger> logger, std::string_view name)
        : m_Storage(storage), m_Logger(std::move(logger)), m_Name(name) {}

    void Request(std::shared_ptr<TaskBase> task, std::span<const detail::ParameterAccess> parameters);
    void Run(core::JobSystem& job_system);
    void RunSerial();
    void BuildTaskflow();

    struct ComponentAccess {
        std::pmr::vector<std::size_t> read_before_write;
        std::pmr::vector<std::size_t> write;
        std::pmr::vector<std::size_t> read_after_write;
    };

    detail::EntityStorage&                                          m_Storage;
    std::shared_ptr<spdlog::logger>                                 m_Logger;
    std::pmr::string                                                m_Name;
    std::pmr::vector<std::shared_ptr<TaskBase>>                     m_Tasks;
    std::pmr::unordered_map<std::pmr::string, std::size_t>          m_TaskNameToIndex;
    std::pmr::unordered_map<utils::TypeID, ComponentAccess>         m_Accesses;
    std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> m_CustomOrder;

    interop::tf::Taskflow         m_Taskflow;
    std::pmr::vector<std::size_t> m_SerialOrder;
    bool                          m_TaskflowDirty    = true;
    core::JobSystem*              m_RunningJobSystem = nullptr;
};

template <typename Func>
Schedule& Schedule::Request(std::string_view name, Func&& task, DynamicComponentList dynamic_components, Filter filter) {
    using Function = std::decay_t<Func>;
    using traits   = utils::function_traits<Function>;
    static_assert(traits::args_size > 0, "Task must request at least one component");

    [&]<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((detail::ParameterTraits<typename traits::template arg_t<I>>::valid && ...), "Invalid task parameter");
        constexpr std::array dynamic{detail::ParameterTraits<typename traits::template arg_t<I>>::dynamic...};
        static_assert([&] {
            bool seen_dynamic = false;
            for (const bool value : dynamic) {
                if (seen_dynamic && !value) return false;
                seen_dynamic |= value;
            }
            return true;
        }(),
                      "Dynamic parameter pointers must be after static components");
        constexpr auto dynamic_count = std::ranges::count(dynamic, true);
        if (dynamic_components.size() != dynamic_count)
            throw std::invalid_argument("Dynamic component names must match the number of dynamic task parameters");

        std::array<detail::ParameterAccess, traits::args_size> parameters;
        detail::ComponentIdList                                components;
        components.reserve(traits::args_size);
        std::size_t dynamic_index = 0;
        auto        append        = [&]<std::size_t Index>() {
            using parameter = detail::ParameterTraits<typename traits::template arg_t<Index>>;
            utils::TypeID id;
            if constexpr (parameter::dynamic)
                id = utils::TypeID(dynamic_components[dynamic_index++]);
            else
                id = utils::TypeID::Create<typename parameter::component_type>();
            components.push_back(id);
            parameters[Index] = {id, parameter::access};
        };
        (append.template operator()<I>(), ...);
        // Conflicting aliases would otherwise create an implicit self-cycle.
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            for (std::size_t j = 0; j < i; ++j) {
                if (parameters[i].component == parameters[j].component && parameters[i].mode != parameters[j].mode)
                    throw std::invalid_argument("Task requests conflicting access modes for the same component");
            }
        }
        Request(std::make_shared<Task<Function>>(name, std::move(components), std::move(filter), std::forward<Func>(task)), parameters);
    }(std::make_index_sequence<traits::args_size>{});
    return *this;
}

template <typename Func>
void Schedule::Task<Func>::Run(detail::EntityStorage& storage) {
    using traits = utils::function_traits<Func>;
    storage.ForEachChunk<traits::args_size>(components, filter, [&](const auto& columns, std::size_t count) {
        for (std::size_t row = 0; row < count; ++row) {
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                std::invoke(task, detail::ParameterTraits<typename traits::template arg_t<I>>::Read(columns[I][row])...);
            }(std::make_index_sequence<traits::args_size>{});
        }
    });
}

}  // namespace hitagi::ecs

namespace hitagi::ecs {

void Schedule::Request(std::shared_ptr<TaskBase> task, std::span<const detail::ParameterAccess> parameters) {
    if (m_TaskNameToIndex.contains(task->name)) {
        m_Logger->warn("Task {} already exists", task->name);
        return;
    }
    const auto index = m_Tasks.size();
    m_TaskNameToIndex.emplace(task->name, index);
    m_Tasks.push_back(std::move(task));
    for (const auto& parameter : parameters) {
        auto& access = m_Accesses[parameter.component];
        auto  append = [index](auto& tasks) {
            if (tasks.empty() || tasks.back() != index) tasks.push_back(index);
        };
        switch (parameter.mode) {
            case detail::AccessMode::ReadBeforeWrite:
                append(access.read_before_write);
                break;
            case detail::AccessMode::Write:
                append(access.write);
                break;
            case detail::AccessMode::ReadAfterWrite:
                append(access.read_after_write);
                break;
        }
    }
    m_TaskflowDirty = true;
}

void Schedule::SetOrder(std::string_view first_task, std::string_view second_task) {
    m_CustomOrder.emplace_back(first_task, second_task);
    m_TaskflowDirty = true;
}

void Schedule::Run(core::JobSystem& job_system) {
    if (m_TaskflowDirty) BuildTaskflow();
    ZoneScopedN("ECSFrame");
    m_RunningJobSystem = &job_system;
    try {
        job_system.RunTaskflow(m_Taskflow);
    } catch (...) {
        m_RunningJobSystem = nullptr;
        throw;
    }
    m_RunningJobSystem = nullptr;
}

void Schedule::RunSerial() {
    if (m_TaskflowDirty) BuildTaskflow();
    ZoneScopedN("ECSFrame");
    for (const auto index : m_SerialOrder) {
        const auto& task = m_Tasks[index];
        ZoneScoped;
        ZoneName(task->name.data(), task->name.size());
        task->Run(m_Storage);
    }
}

void Schedule::BuildTaskflow() {
    m_Taskflow.clear();
    m_SerialOrder.clear();
    std::pmr::vector<std::pmr::vector<std::size_t>> graph(m_Tasks.size());
    for (const auto& [component, access] : m_Accesses) {
        for (const auto before : access.read_before_write) {
            for (const auto writer : access.write) graph[before].push_back(writer);
            for (const auto after : access.read_after_write) graph[before].push_back(after);
        }
        // zip_view here crashes clang-cl 23.1.2 / MSVC STL 14.51 in importing consumers.
        for (std::size_t index = 1; index < access.write.size(); ++index)
            graph[access.write[index - 1]].push_back(access.write[index]);
        for (const auto writer : access.write) {
            for (const auto after : access.read_after_write) graph[writer].push_back(after);
        }
    }
    for (const auto& [first, second] : m_CustomOrder) {
        const auto from = m_TaskNameToIndex.find(first);
        const auto to   = m_TaskNameToIndex.find(second);
        if (from == m_TaskNameToIndex.end() || to == m_TaskNameToIndex.end()) {
            m_Logger->warn("Cannot order {} -> {}: task does not exist", first, second);
            continue;
        }
        graph[from->second].push_back(to->second);
    }

    std::pmr::vector<std::size_t> in_degree(graph.size(), 0);
    for (auto& successors : graph) {
        std::ranges::sort(successors);
        successors.erase(std::unique(successors.begin(), successors.end()), successors.end());
        for (const auto next : successors) ++in_degree[next];
    }
    std::pmr::vector<std::size_t> ready;
    for (std::size_t index = 0; index < graph.size(); ++index) {
        if (in_degree[index] == 0) ready.push_back(index);
    }
    std::pmr::vector<std::size_t> sorted;
    while (!ready.empty()) {
        const auto current = ready.back();
        ready.pop_back();
        sorted.push_back(current);
        for (const auto next : graph[current]) {
            if (--in_degree[next] == 0) ready.push_back(next);
        }
    }
    if (sorted.size() != graph.size()) {
        std::pmr::string dot("digraph {\n");
        for (std::size_t index = 0; index < graph.size(); ++index) {
            dot += std::format("  {} [label=\"{}\"];\n", index, m_Tasks[index]->name);
            for (const auto next : graph[index]) dot += std::format("  {} -> {};\n", index, next);
        }
        dot += "}\n";
        m_Logger->error("Cycle in task graph:\n{}", dot);
        throw std::invalid_argument("Cycle in ECS task graph");
    }

    // Both execution paths are derived from the same validated, deduplicated graph.
    std::pmr::vector<interop::tf::Task> tasks;
    tasks.reserve(m_Tasks.size());
    for (const auto& task : m_Tasks) {
        tasks.push_back(m_Taskflow.emplace([this, task] {
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
                                      task->Run(m_Storage);
                                  })
                            .name(task->name.data()));
    }
    for (std::size_t index = 0; index < graph.size(); ++index) {
        for (const auto next : graph[index]) tasks[index].precede(tasks[next]);
    }
    m_SerialOrder   = std::move(sorted);
    m_TaskflowDirty = false;
}

}  // namespace hitagi::ecs
