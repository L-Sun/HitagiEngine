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

class Schedule {
    struct TaskBase {
        TaskBase(std::string_view name, detail::ComponentIdList component_list, Filter filter)
            : name(name), component_list(std::move(component_list)), filter(std::move(filter)) {}

        virtual void Run(detail::EntityStorage&) = 0;

        std::pmr::string        name;
        detail::ComponentIdList component_list;
        Filter                  filter;
    };

    template <typename Func>
    struct Task : public TaskBase {
        Task(std::string_view name, detail::ComponentIdList component_list, Filter filter, Func&& task)
            : TaskBase(name, std::move(component_list), std::move(filter)), task(std::move(task)) {}

        void Run(detail::EntityStorage& storage) final;
        Func task;
    };

public:
    template <typename Func>
    Schedule& Request(
        std::string_view     name,
        Func&&               task,
        DynamicComponentList dynamic_components = {},
        Filter               filter             = {});

    void SetOrder(std::string_view first_task, std::string_view second_task);

private:
    friend class World;

    Schedule(detail::EntityStorage& storage, std::shared_ptr<spdlog::logger> logger, std::string_view name)
        : m_Storage(storage), m_Logger(std::move(logger)), m_Name(name) {}

    detail::EntityStorage&          m_Storage;
    std::shared_ptr<spdlog::logger> m_Logger;
    std::pmr::string                m_Name;

    using ReadBeforeWriteParameters = std::pmr::set<utils::TypeID>;
    using WriteParameters           = std::pmr::set<utils::TypeID>;
    using ReadAfterWriteParameters  = std::pmr::set<utils::TypeID>;
    using ParameterSets             = std::tuple<ReadBeforeWriteParameters, WriteParameters, ReadAfterWriteParameters>;

    template <typename T>
    static void FillParameterSets(ParameterSets& parameter_sets, std::string_view dynamic_component);

    void Request(std::shared_ptr<TaskBase>&& task, const ParameterSets& parameter_sets);

    void Run(core::JobSystem& job_system);
    void RunSerial();
    void BuildTaskflow();

    bool CheckValid(const std::pmr::unordered_map<std::size_t, std::pmr::unordered_set<std::size_t>>& graph);

    std::pmr::vector<std::shared_ptr<TaskBase>>                           m_Tasks;
    std::pmr::unordered_map<std::pmr::string, std::size_t>                m_TaskNameToIndex;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_ReadBeforeWriteSet;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_WriteSet;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_ReadAfterWriteSet;

    std::pmr::unordered_map<std::pmr::string, std::pmr::string> m_CustomOrder;

    interop::tf::Taskflow               m_Taskflow;
    std::pmr::vector<interop::tf::Task> m_TaskflowTasks;
    std::pmr::vector<std::size_t>       m_SerialOrder;  // topological order; empty if the graph is invalid
    bool                                m_TaskflowDirty = true;
    // Executor of the in-flight parallel run; tasks read it to name worker threads.
    core::JobSystem* m_RunningJobSystem = nullptr;
};

}  // namespace hitagi::ecs

namespace hitagi::ecs {

export template <typename T>
    requires detail::ComponentValueType<T> ||
             detail::ComponentConstValueType<T> ||
             detail::ComponentConstReferenceType<T> ||
             detail::DynamicComponentConstPointerType<T>
struct LastFrame {
    using type = T;

    inline operator T() const noexcept { return value; }

    T value;
};

namespace detail {

template <typename T>
concept HasLastFrameTag = std::same_as<T, LastFrame<typename T::type>>;

template <typename T>
concept ReadBeforWriteParameter = HasLastFrameTag<T> &&
                                  (ComponentValueType<typename T::type> ||
                                   ComponentConstReferenceType<typename T::type> ||
                                   DynamicComponentConstPointerType<typename T::type>);

template <typename T>
concept WriteParameter =
    !HasLastFrameTag<std::decay_t<T>> &&
    !std::same_as<T, Entity&> &&
    (ComponentReferenceType<T> || DynamicComponentPointerType<T>);

template <typename T>
concept ReadAfterWriteParameter =
    !HasLastFrameTag<std::decay_t<T>> && (ComponentValueType<T> ||
                                          ComponentConstValueType<T> ||
                                          ComponentConstReferenceType<T> ||
                                          DynamicComponentConstPointerType<T>);

template <typename T>
concept ValidParameter = ReadBeforWriteParameter<T> || WriteParameter<T> || ReadAfterWriteParameter<T>;

template <typename T>
using remove_last_frame_tag_t = std::conditional_t<HasLastFrameTag<T>, T, utils::delay_type<T>>::type;

template <typename T>
using decay_parameter_t = std::remove_cvref_t<remove_last_frame_tag_t<T>>;

struct Parameter {
    Parameter(std::byte* data) : data(data) {}

    std::byte* data;

    template <typename T>
    inline operator LastFrame<T>() const noexcept {
        if constexpr (DynamicComponentPointerType<T> || DynamicComponentConstPointerType<T>) {
            return {data};
        } else {
            return {*reinterpret_cast<T*>(data)};
        }
    };

    template <typename T>
        requires(!DynamicComponentPointerType<T> && !DynamicComponentConstPointerType<T>)
    inline operator T&() const noexcept {
        return *reinterpret_cast<T*>(data);
    };

    template <typename T>
        requires(DynamicComponentPointerType<T> || DynamicComponentConstPointerType<T>)
    inline operator T() const noexcept {
        return data;
    };
};

}  // namespace detail

template <typename Func>
Schedule& Schedule::Request(std::string_view name, Func&& task, DynamicComponentList dynamic_components, Filter filter) {
    using traits = utils::function_traits<Func>;

    static_assert(traits::args_size > 0, "Task must request at least one component");

    []<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((detail::ValidParameter<typename traits::template arg_t<I>> && ...), "Invalid parameter type");
    }(std::make_index_sequence<traits::args_size>{});

    std::bitset<traits::args_size> dynamic_component_flags;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((!(utils::remove_const_pointer_same<detail::decay_parameter_t<typename traits::template arg_t<I - 1>>, std::byte*> &&
                         Component<detail::decay_parameter_t<typename traits::template arg_t<I>>>) &&
                       ...),
                      "dynamic parameter pointers must be after component types");
        ((dynamic_component_flags[I - 1] = utils::remove_const_pointer_same<detail::decay_parameter_t<typename traits::template arg_t<I - 1>>, std::byte*>), ...);
    }(utils::make_index_sequence_from_to<1, traits::args_size>{});

    constexpr std::size_t first_dynamic_component_index = [&]<std::size_t... I>(std::index_sequence<I...>) {
        return utils::first_index_of_v<std::byte*, utils::remove_const_pointer_t<detail::decay_parameter_t<typename traits::template arg_t<I>>>...>;
    }(std::make_index_sequence<traits::args_size>{});

    ParameterSets parameter_sets;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (FillParameterSets<typename traits::template arg_t<I>>(
             parameter_sets,
             I < first_dynamic_component_index ? "" : dynamic_components[I - first_dynamic_component_index]),
         ...);
    }(std::make_index_sequence<traits::args_size>{});

    auto component_id_list = [&]<std::size_t... I>(std::index_sequence<I...>) {
        return detail::create_component_id_list<detail::decay_parameter_t<typename traits::template arg_t<I>>...>(dynamic_components);
    }(std::make_index_sequence<first_dynamic_component_index>{});

    Request(std::make_shared<Task<Func>>(name, std::move(component_id_list), std::move(filter), std::forward<Func>(task)), std::move(parameter_sets));

    return *this;
}

template <typename T>
void Schedule::FillParameterSets(ParameterSets& parameter_sets, std::string_view dynamic_component) {
    using ComponentType = detail::decay_parameter_t<T>;

    if constexpr (detail::ReadBeforWriteParameter<T>) {
        if constexpr (utils::remove_const_pointer_same<ComponentType, std::byte*>) {
            std::get<0>(parameter_sets).emplace(utils::TypeID{dynamic_component});
        } else {
            std::get<0>(parameter_sets).emplace(utils::TypeID::Create<ComponentType>());
        }
    } else if constexpr (detail::WriteParameter<T>) {
        if constexpr (utils::remove_const_pointer_same<ComponentType, std::byte*>) {
            std::get<1>(parameter_sets).emplace(utils::TypeID{dynamic_component});
        } else {
            std::get<1>(parameter_sets).emplace(utils::TypeID::Create<ComponentType>());
        }
    } else if constexpr (detail::ReadAfterWriteParameter<T>) {
        if constexpr (utils::remove_const_pointer_same<ComponentType, std::byte*>) {
            std::get<2>(parameter_sets).emplace(utils::TypeID{dynamic_component});
        } else {
            std::get<2>(parameter_sets).emplace(utils::TypeID::Create<ComponentType>());
        }
    }
}

template <typename Func>
void Schedule::Task<Func>::Run(detail::EntityStorage& storage) {
    using traits = utils::function_traits<Func>;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
        const auto components_buffers = storage.GetComponentsBuffers(component_list, filter);
        if (components_buffers.empty()) return;

        const auto num_buffers = components_buffers.front().size();

        for (std::size_t buffer_index = 0; buffer_index < num_buffers; buffer_index++) {
            const auto num_entities = components_buffers.front()[buffer_index].num_entities;
            for (std::size_t entity_index = 0; entity_index < num_entities; entity_index++) {
                task(detail::Parameter(components_buffers[I][buffer_index][entity_index])...);
            }
        }
    }(std::make_index_sequence<traits::args_size>{});
}

}  // namespace hitagi::ecs

namespace hitagi::ecs {
void Schedule::Request(std::shared_ptr<TaskBase>&& task, const ParameterSets& parameter_sets) {
    if (m_TaskNameToIndex.contains(task->name)) {
        m_Logger->warn("Task {} already exists", task->name);
        return;
    }

    m_TaskNameToIndex[task->name] = m_Tasks.size();
    m_Tasks.emplace_back(std::move(task));

    const auto& [read_before_write, write, read_after_write] = parameter_sets;
    for (auto parameter : read_before_write) {
        m_ReadBeforeWriteSet[parameter].emplace_back(m_Tasks.size() - 1);
    }
    for (auto parameter : write) {
        m_WriteSet[parameter].emplace_back(m_Tasks.size() - 1);
    }
    for (auto parameter : read_after_write) {
        m_ReadAfterWriteSet[parameter].emplace_back(m_Tasks.size() - 1);
    }

    m_TaskflowDirty = true;
}

void Schedule::SetOrder(std::string_view first_task, std::string_view second_task) {
    m_CustomOrder.emplace(first_task, second_task);
    m_TaskflowDirty = true;
}

void Schedule::Run(core::JobSystem& job_system) {
    if (m_TaskflowDirty) {
        BuildTaskflow();
    }

    ZoneScopedN("ECSFrame");
    m_RunningJobSystem = &job_system;
    job_system.RunTaskflow(m_Taskflow);
    m_RunningJobSystem = nullptr;
}

void Schedule::RunSerial() {
    if (m_TaskflowDirty) {
        BuildTaskflow();
    }

    ZoneScopedN("ECSFrame");
    for (const auto task_index : m_SerialOrder) {
        const auto& task = m_Tasks[task_index];
        ZoneScoped;
        ZoneName(task->name.data(), task->name.size());
        task->Run(m_Storage);
    }
}

void Schedule::BuildTaskflow() {
    m_Taskflow.clear();
    m_TaskflowTasks.clear();
    m_SerialOrder.clear();
    m_TaskflowTasks.reserve(m_Tasks.size());

    // adjacency list
    std::pmr::unordered_map<std::size_t, std::pmr::unordered_set<std::size_t>> direct_graph;
    for (auto i = 0; i < m_Tasks.size(); ++i)
        direct_graph[i] = {};

    for (const auto& task : m_Tasks) {
        m_TaskflowTasks.emplace_back(m_Taskflow.emplace([this, task]() {
                                                   thread_local bool tracy_thread_named = false;
                                                   if (!tracy_thread_named) {
                                                       if (const auto worker_id = m_RunningJobSystem->GetCurrentWorkerId(); worker_id >= 0) {
                                                           const auto thread_name = std::format("Hitagi/ECS/{}/Worker-{}", m_Name, worker_id);
#ifdef TRACY_ENABLE
                                                           tracy::SetThreadName(thread_name.c_str());
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

    for (const auto& [component, task_indices] : m_ReadBeforeWriteSet) {
        for (const auto task_index : task_indices) {
            for (const auto write_task_index : m_WriteSet[component]) {
                m_TaskflowTasks[task_index].precede(m_TaskflowTasks[write_task_index]);
                direct_graph[task_index].emplace(write_task_index);
            }
        }
        for (const auto& task_index : task_indices) {
            for (const auto read_after_write_task_index : m_ReadAfterWriteSet[component]) {
                m_TaskflowTasks[task_index].precede(m_TaskflowTasks[read_after_write_task_index]);
                direct_graph[task_index].emplace(read_after_write_task_index);
            }
        }
    }

    for (const auto& [component, task_indices] : m_WriteSet) {
        // Keep adjacent-pair traversal indexed: zip_view in this interface BMI
        // crashes clang-cl 23.1.2 / MSVC STL 14.51 in consumer std::apply mangling.
        for (std::size_t index = 1; index < task_indices.size(); ++index) {
            const auto task_index      = task_indices[index - 1];
            const auto next_task_index = task_indices[index];
            m_TaskflowTasks[task_index].precede(m_TaskflowTasks[next_task_index]);
            direct_graph[task_index].emplace(next_task_index);
        }

        for (const auto task_index : task_indices) {
            for (const auto read_after_write_task_index : m_ReadAfterWriteSet[component]) {
                m_TaskflowTasks[task_index].precede(m_TaskflowTasks[read_after_write_task_index]);
                direct_graph[task_index].emplace(read_after_write_task_index);
            }
        }
    }

    for (const auto& [first_task_name, second_task_name] : m_CustomOrder) {
        if (!m_TaskNameToIndex.contains(first_task_name)) {
            m_Logger->warn(
                "Fail to create custom order: {} -> {}, because {} does not exist",
                first_task_name, second_task_name, first_task_name);
            continue;
        }
        if (!m_TaskNameToIndex.contains(second_task_name)) {
            m_Logger->warn(
                "Fail to create custom order: {} -> {}, because {} does not exist",
                first_task_name, second_task_name, second_task_name);
            continue;
        }
        const auto first_task_index  = m_TaskNameToIndex[first_task_name];
        const auto second_task_index = m_TaskNameToIndex[second_task_name];
        m_TaskflowTasks[first_task_index].precede(m_TaskflowTasks[second_task_index]);
        direct_graph[first_task_index].emplace(second_task_index);
    }

    if (!CheckValid(direct_graph)) {
        return;
    }

    m_TaskflowDirty = false;
}

bool Schedule::CheckValid(const std::pmr::unordered_map<std::size_t, std::pmr::unordered_set<std::size_t>>& graph) {
    std::pmr::unordered_map<std::size_t, std::size_t> in_degree;
    for (const auto& [task_id, successor_task_ids] : graph) {
        if (!in_degree.contains(task_id)) in_degree[task_id] = 0;
        for (const auto& successor_task_id : successor_task_ids) {
            in_degree[successor_task_id] += 1;
        }
    }

    std::pmr::vector<std::size_t> zero_in_degree_nodes;
    for (const auto& [node, degree] : in_degree) {
        if (degree == 0) {
            zero_in_degree_nodes.emplace_back(node);
        }
    }

    std::pmr::vector<std::size_t> sorted_nodes;
    while (!zero_in_degree_nodes.empty()) {
        auto node = zero_in_degree_nodes.back();
        zero_in_degree_nodes.pop_back();
        sorted_nodes.emplace_back(node);
        for (const auto& edge : graph.at(node)) {
            in_degree[edge] -= 1;
            if (in_degree[edge] == 0) {
                zero_in_degree_nodes.emplace_back(edge);
            }
        }
    }

    if (sorted_nodes.size() != graph.size()) {
        std::pmr::string dot;

        dot += "digraph {\n";
        for (const auto task_id : graph | std::ranges::views::keys) {
            dot += std::format(R"(  {} [label="{}"])", task_id, m_Tasks[task_id]->name);
        }

        for (const auto& [task_id, successor_task_ids] : graph) {
            for (const auto& successor_task_id : successor_task_ids) {
                dot += std::format("  {} -> {}", task_id, successor_task_id);
                if (in_degree[task_id] != 0 && in_degree[successor_task_id] != 0)
                    dot += " [color=red]";
                dot += ";\n";
            }
        }

        dot += "}\n";

        m_Logger->error("Detect cycle in task graph:");
        m_Logger->error("{}", dot);
        return false;
    }
    m_SerialOrder.assign(sorted_nodes.begin(), sorted_nodes.end());
    return true;
}

}  // namespace hitagi::ecs
