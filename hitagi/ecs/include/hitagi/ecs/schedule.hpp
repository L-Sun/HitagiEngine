#pragma once
#include <hitagi/ecs/world.hpp>
#include <hitagi/utils/concepts.hpp>

#include <vector>

namespace hitagi::ecs {

class Schedule {
    struct ITask {
        virtual void Run(World&) = 0;
    };

    template <typename Func>
    struct Task : public ITask {
        Task(std::string_view name, Func&& task, Filter filter)
            : task_name(name), task(std::move(task)), filter(std::move(filter)) {}

        Task(const Task&)                = delete;
        Task& operator=(const Task&)     = delete;
        Task(Task&&) noexcept            = default;
        Task& operator=(Task&&) noexcept = default;

        void Run(World& world) final;

        std::pmr::string task_name;
        Func             task;
        Filter           filter;
    };

public:
    Schedule(World& world) : world(world) {}

    // Do task on the entities that contains components indicated at parameters.
    template <typename Func>
    Schedule& Request(std::string_view name, Func&& task, Filter filter = {})
        requires utils::unique_parameter_types<Func>;

    // Schedule& RequestOnce(std::string_view name, std::function<void()>&& task);

    void Run() {
        for (auto&& task : m_Tasks) {
            task->Run(world);
        }
    }

    World& world;

private:
    std::pmr::vector<std::shared_ptr<ITask>> m_Tasks;
};

template <typename Func>
Schedule& Schedule::Request(std::string_view name, Func&& task, Filter filter)
    requires utils::unique_parameter_types<Func>
{
    std::shared_ptr<ITask> task_info = std::make_shared<Task<Func>>(name, std::forward<Func>(task), std::move(filter));

    m_Tasks.emplace_back(std::move(task_info));

    return *this;
}

template <typename Func>
void Schedule::Task<Func>::Run(World& world) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        using traits          = utils::function_traits<Func>;
        using component_types = std::tuple<typename traits::template no_cvref_arg<I>::type...>;

        filter.All<std::tuple_element_t<I, component_types>...>();

        for (auto archetype : world.GetEntityManager().GetArchetype(filter)) {
            auto num_entities     = archetype->NumEntities();
            auto components_array = std::make_tuple(archetype->GetComponentArray<std::tuple_element_t<I, component_types>>()...);

            for (std::size_t index = 0; index < num_entities; index++) {
                task(std::get<I>(components_array)[index]...);
            }
        };
    }(std::make_index_sequence<utils::function_traits<Func>::args_size>{});
}

}  // namespace hitagi::ecs