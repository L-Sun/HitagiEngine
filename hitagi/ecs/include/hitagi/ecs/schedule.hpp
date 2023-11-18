#pragma once
#include <hitagi/ecs/world.hpp>
#include <hitagi/utils/concepts.hpp>

#include <fmt/format.h>

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
        requires utils::unique_no_cvref_parameter_types<Func>;

    World& world;

private:
    friend World;

    void Run() {
        for (auto&& task : m_Tasks) {
            task->Run(world);
        }
    }

    std::pmr::vector<std::shared_ptr<ITask>> m_Tasks;
};

template <typename Func>
Schedule& Schedule::Request(std::string_view name, Func&& task, Filter filter)
    requires utils::unique_no_cvref_parameter_types<Func>
{
    std::shared_ptr<ITask> task_info = std::make_shared<Task<Func>>(name, std::forward<Func>(task), std::move(filter));

    m_Tasks.emplace_back(std::move(task_info));

    return *this;
}

template <typename Func>
void Schedule::Task<Func>::Run(World& world) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        using traits                     = utils::function_traits<Func>;
        using typed_component_types      = std::tuple<typename traits::template no_cvref_arg<I>::type...>;
        const auto typed_component_infos = detail::create_component_infos<std::tuple_element_t<I, typed_component_types>...>();

        const auto components_buffers = world.GetEntityManager().GetComponentsBuffers(typed_component_infos, filter);
        const auto num_buffers        = components_buffers.front().size();

        for (std::size_t buffer_index = 0; buffer_index < num_buffers; buffer_index++) {
            const auto num_entities = components_buffers.front()[buffer_index].second;
            for (std::size_t entity_index = 0; entity_index < num_entities; entity_index++) {
                auto component_data = std::array{(components_buffers[I][buffer_index].first + entity_index * typed_component_infos[I].size)...};
                task((*reinterpret_cast<std::tuple_element_t<I, typed_component_types>*>(component_data[I]))...);
            }
        }
    }(std::make_index_sequence<utils::function_traits<Func>::args_size>{});
}

}  // namespace hitagi::ecs