module;
#include <taskflow/taskflow.hpp>
#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>

module ecs;
import std;
namespace hitagi::ecs {
void Schedule::Request(std::shared_ptr<TaskBase>&& task, const ParameterSets& parameter_sets) {
    if (m_TaskNameToIndex.contains(task->name)) {
        world.GetLogger()->warn("Task {} already exists", task->name);
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

void Schedule::Run(tf::Executor& executor) {
    if (m_TaskflowDirty) {
        BuildTaskflow(executor);
    }

    ZoneScopedN("ECSFrame");
    executor.run(m_Taskflow).wait();
}

void Schedule::BuildTaskflow(tf::Executor& executor) {
    m_Taskflow.clear();
    m_TaskflowTasks.clear();
    m_TaskflowTasks.reserve(m_Tasks.size());

    // adjacency list
    std::pmr::unordered_map<std::size_t, std::pmr::unordered_set<std::size_t>> direct_graph;
    for (auto i = 0; i < m_Tasks.size(); ++i)
        direct_graph[i] = {};

    for (const auto& task : m_Tasks) {
        m_TaskflowTasks.emplace_back(m_Taskflow.emplace([this, &executor, task]() {
            thread_local bool tracy_thread_named = false;
            if (!tracy_thread_named) {
                if (const auto worker_id = executor.this_worker_id(); worker_id >= 0) {
                    const auto thread_name = std::format("Hitagi/ECS/{}/Worker-{}", world.GetName(), worker_id);
                    tracy::SetThreadName(thread_name.c_str());
                    tracy_thread_named = true;
                }
            }

            ZoneScoped;
            ZoneName(task->name.data(), task->name.size());
            task->Run(world);
        }).name(task->name.data()));
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
        for (const auto [task_index, next_task_index] : std::ranges::views::zip(task_indices, task_indices | std::ranges::views::drop(1))) {
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
            world.GetLogger()->warn(
                "Fail to create custom order: {} -> {}, because {} does not exist",
                first_task_name, second_task_name, first_task_name);
            continue;
        }
        if (!m_TaskNameToIndex.contains(second_task_name)) {
            world.GetLogger()->warn(
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

        world.GetLogger()->error("Detect cycle in task graph:");
        world.GetLogger()->error("{}", dot);
        return false;
    }
    return true;
}

}  // namespace hitagi::ecs
