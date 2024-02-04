#include <hitagi/ecs/component.hpp>

#include <fmt/format.h>

namespace hitagi::asset {
struct MetaInfo {
    std::pmr::string name;
};
static_assert(ecs::Component<MetaInfo>);

}  // namespace hitagi::asset