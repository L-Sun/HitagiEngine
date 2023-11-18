#include <hitagi/ecs/filter.hpp>

#include <range/v3/algorithm/find.hpp>

namespace hitagi::ecs {

auto Filter::AddComponentToFilter(detail::ComponentInfos& filter, detail::ComponentInfos new_component_infos) noexcept -> Filter& {
    for (auto&& new_component_info : new_component_infos) {
        if (ranges::find(filter, new_component_info) == filter.end()) {
            filter.emplace_back(std::move(new_component_info));
        }
    }

    return *this;
}
}  // namespace hitagi::ecs