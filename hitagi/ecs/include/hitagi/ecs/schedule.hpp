#pragma once
#include <hitagi/ecs/world.hpp>
#include <hitagi/utils/types.hpp>

#include <fmt/format.h>

#include <bitset>
#include <type_traits>
#include <vector>

namespace hitagi::ecs {

class Schedule {
    struct TaskBase {
        TaskBase(std::string_view name, detail::ComponentIdList component_list, Filter filter)
            : name(name), component_list(std::move(component_list)), filter(std::move(filter)) {}

        virtual void Run(World&) = 0;

        std::pmr::string        name;
        detail::ComponentIdList component_list;
        Filter                  filter;
    };

    template <typename Func>
    struct Task : public TaskBase {
        Task(std::string_view name, detail::ComponentIdList component_list, Filter filter, Func&& task)
            : TaskBase(name, std::move(component_list), std::move(filter)), task(std::move(task)) {}

        void Run(World& world) final;
        Func task;
    };

public:
    Schedule(World& world) : world(world) {}

    // Do task on the entities that contains components indicated at parameters.
    template <typename Func>
    Schedule& Request(
        std::string_view     name,
        Func&&               task,
        DynamicComponentList dynamic_components = {},
        Filter               filter             = {});

    void SetOrder(std::string_view first_task, std::string_view second_task);

    World& world;

private:
    friend World;

    using ReadBeforeWriteParameters = std::pmr::set<utils::TypeID>;
    using WriteParameters           = std::pmr::set<utils::TypeID>;
    using ReadAfterWriteParameters  = std::pmr::set<utils::TypeID>;
    using ParameterSets             = std::tuple<ReadBeforeWriteParameters, WriteParameters, ReadAfterWriteParameters>;

    template <typename T>
    static void FillParameterSets(ParameterSets& parameter_sets, std::string_view dynamic_component);

    void Request(std::shared_ptr<TaskBase> task, const ParameterSets& parameter_sets);

    void Run(tf::Executor& executor);

    bool CheckValid(const std::pmr::unordered_map<std::size_t, std::pmr::unordered_set<std::size_t>>& graph);

    std::pmr::vector<std::shared_ptr<TaskBase>>                           m_Tasks;
    std::pmr::unordered_map<std::pmr::string, std::size_t>                m_TaskNameToIndex;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_ReadBeforeWriteSet;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_WriteSet;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_ReadAfterWriteSet;

    std::pmr::unordered_map<std::pmr::string, std::pmr::string> m_CustomOrder;
};

namespace detail {
template <typename T>
concept ComponentValueType = Component<T>;

template <typename T>
concept ComponentConstValueType = Component<std::remove_cv_t<T>> && std::is_const_v<T>;

template <typename T>
concept ComponentReferenceType = Component<std::decay_t<T>> && utils::is_no_const_reference_v<T>;

template <typename T>
concept ComponentConstReferenceType = Component<std::remove_cvref_t<T>> && utils::is_const_reference_v<T>;

template <typename T>
concept DynamicComponentPointerType = std::same_as<T, std::byte*>;

template <typename T>
concept DynamicComponentConstPointerType = std::same_as<T, const std::byte*>;
}  // namespace detail

template <typename T>
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

    // make sure all parameters are valid
    []<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((detail::ValidParameter<typename traits::template arg_t<I>> && ...), "Invalid parameter type");
    }(std::make_index_sequence<traits::args_size>{});

    // make sure all dynamic component after component
    std::bitset<traits::args_size> dynamic_component_flags;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((!(utils::remove_const_pointer_same<detail::decay_parameter_t<typename traits::template arg_t<I - 1>>, std::byte*> &&
                         Component<detail::decay_parameter_t<typename traits::template arg_t<I>>>)&&...),
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
void Schedule::Task<Func>::Run(World& world) {
    using traits = utils::function_traits<Func>;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
        const auto components_buffers = world.GetEntityManager().GetComponentsBuffers(component_list, filter);
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