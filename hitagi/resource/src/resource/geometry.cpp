#include <hitagi/resource/geometry.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::resource {
Geometry::Geometry(std::shared_ptr<Transform> _transform)
    : transform(std::move(_transform)) {
    if (transform == nullptr) {
        auto logger = spdlog::get("AssetManager");
        if (logger) logger->error("You must set a valid transform to the geometry!");
        throw std::invalid_argument("The transform is nullptr!");
    }
}

}  // namespace hitagi::resource