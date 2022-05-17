#include <hitagi/resource/geometry.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::resource {
Geometry::Geometry(std::shared_ptr<Transform> transform)
    : m_Transform(std::move(transform)) {
    if (m_Transform == nullptr) {
        auto logger = spdlog::get("AssetManager");
        if (logger) logger->error("You must set a valid transform to the geometry!");
        throw std::invalid_argument("The transform is nullptr!");
    }
}

void Geometry::AddMesh(Mesh mesh) noexcept {
    m_Meshes.emplace_back(std::move(mesh));
}

void Geometry::SetTransform(std::shared_ptr<Transform> transform) noexcept {
    if (transform)
        m_Transform = std::move(transform);
}

bool Geometry::IsVisiable() const noexcept {
    return m_Visibility;
}
void Geometry::SetVisibility(bool visibility) noexcept {
    m_Visibility = visibility;
}
}  // namespace hitagi::resource