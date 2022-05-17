#pragma once
#include <hitagi/math/transform.hpp>

#include <memory_resource>

namespace hitagi::resource {
class Transform : std::enable_shared_from_this<Transform> {
public:
    explicit Transform();

    Transform(const std::tuple<math::vec3f, math::quatf, math::vec3f>& trs);

    math::mat4f GetTransform() const noexcept;
    math::mat4f GetLocalTransform() const noexcept;
    math::mat4f GetParentTransform() const noexcept;
    math::vec3f GetTranslation() const noexcept;
    math::quatf GetRotation() const noexcept;
    math::vec3f GetScaling() const noexcept;

    void SetParent(const std::shared_ptr<Transform>& parent);

    void Reset();
    void Rotate(const math::quatf& rotate);
    void Translate(const math::vec3f& offset);
    void Scale(const math::vec3f& scalr);

private:
    void UpdateTransformRecursive();

    std::weak_ptr<Transform>                   m_Parent;
    std::pmr::vector<std::weak_ptr<Transform>> m_Children;

    math::mat4f m_WorldTransform = math::mat4f{1.0f};

    math::vec3f m_Scaling     = math::vec3f{1.0f, 1.0f, 1.0f};
    math::quatf m_Rotation    = math::quatf{0.0f, 0.0f, 0.0f, 1.0f};
    math::vec3f m_Translation = math::vec3f{0.0f, 0.0f, 0.0f};
};
}  // namespace hitagi::resource