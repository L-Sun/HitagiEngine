#include <hitagi/resource/transform.hpp>

using namespace hitagi::math;

namespace hitagi::resource {

Transform::Transform(const std::tuple<math::vec3f, math::quatf, math::vec3f>& trs)
    : m_Scaling(std::get<2>(trs)),
      m_Rotation(std::get<1>(trs)),
      m_Translation(std::get<0>(trs)) {}

mat4f Transform::GetTransform() const noexcept { return m_WorldTransform; }

mat4f Transform::GetLocalTransform() const noexcept {
    return translate(rotate(scale(mat4f(1.0f), m_Scaling), m_Rotation), m_Translation);
}

mat4f Transform::GetParentTransform() const noexcept {
    auto parent = m_Parent.lock();
    return parent ? parent->GetTransform() : mat4f(1.0f);
}

vec3f Transform::GetTranslation() const noexcept { return m_Translation; }

quatf Transform::GetRotation() const noexcept { return m_Rotation; }

vec3f Transform::GetScaling() const noexcept { return m_Scaling; }

void Transform::SetParent(const std::shared_ptr<Transform>& parent) {
    m_Parent = parent;
    if (parent) parent->m_Children.emplace_back(shared_from_this());
    UpdateTransformRecursive();
}

void Transform::Reset() {
    m_WorldTransform = mat4f{1.0f};

    m_Scaling     = vec3f{1.0f, 1.0f, 1.0f};
    m_Rotation    = quatf{0.0f, 0.0f, 0.0f, 1.0f};
    m_Translation = vec3f{0.0f, 0.0f, 0.0f};
}

void Transform::Rotate(const quatf& rotate) {
    m_Rotation = rotate * m_Rotation;
    UpdateTransformRecursive();
}

void Transform::Translate(const vec3f& offset) {
    m_Translation += offset;
    UpdateTransformRecursive();
}

void Transform::Scale(const vec3f& scalr) {
    m_Scaling = m_Scaling * scalr;
    UpdateTransformRecursive();
}

void Transform::UpdateTransformRecursive() {
    if (auto parent = m_Parent.lock(); parent != nullptr) {
        m_WorldTransform = parent->GetTransform() * GetLocalTransform();
    }
    for (const auto& weak_child : m_Children) {
        if (auto child = weak_child.lock()) {
            child->UpdateTransformRecursive();
        }
    }
}

}  // namespace hitagi::resource