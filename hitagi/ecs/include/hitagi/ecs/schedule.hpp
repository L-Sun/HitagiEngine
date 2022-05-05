#pragma once
#include <hitagi/ecs/world.hpp>
#include <hitagi/utils/concepts.hpp>

#include <typeindex>
#include <vector>
#include <functional>

namespace hitagi::ecs {

class Schedule {
    struct ITask {
        virtual void Run(World&) = 0;
    };

    template <typename Func>
    struct Task : public ITask {
        using allocator_type = std::pmr::polymorphic_allocator<>;

        Task(
            std::string_view      name,
            Func&&                task,
            const allocator_type& alloc = {})
            : task_name(name, alloc), task(std::move(task)) {}

        Task(Task&& other, const allocator_type& alloc)
            : task_name(other.task_name, alloc), task(std::move(other.task)) {}

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        Task(Task&&) noexcept        = default;
        Task& operator=(Task&&) noexcept = default;

        void Run(World& world) final;

        std::pmr::string task_name;
        Func             task;
    };

public:
    Schedule(World& world) : m_World(world) {}

    // Do task on the entities that contains components indicated at parameters.
    template <typename Func>
    requires utils::unique_parameter_types<Func>
        Schedule& Register(std::string_view name, Func&& task);

    void Run() {
        for (auto&& task : m_Tasks) {
            task->Run(m_World);
        }
    }

private:
    World&                                   m_World;
    std::pmr::vector<std::shared_ptr<ITask>> m_Tasks;
};

template <typename Func>
requires utils::unique_parameter_types<Func>
    Schedule& Schedule::Register(std::string_view name, Func&& task) {
    auto                   alloc     = std::pmr::polymorphic_allocator<Task<Func>>(std::pmr::get_default_resource());
    std::shared_ptr<ITask> task_info = std::allocate_shared<Task<Func>>(alloc, name, std::forward<Func>(task));

    m_Tasks.emplace_back(std::move(task_info));

    return *this;
}

template <typename Func>
void Schedule::Task<Func>::Run(World& world) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        using traits = utils::function_traits<Func>;

        for (const std::shared_ptr<IArchetype>& archetype : world.GetArchetypes<typename traits::template arg<I>::type...>()) {
            auto num_entities     = archetype->NumEntities();
            auto components_array = std::make_tuple(archetype->GetComponentArray<typename traits::template arg<I>::type>()...);

            for (std::size_t index = 0; index < num_entities; index++) {
                task(std::get<I>(components_array)[index]...);
            }
        };
    }
    (std::make_index_sequence<utils::function_traits<Func>::args_size>{});
}

}  // namespace hitagi::ecs