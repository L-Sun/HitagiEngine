#pragma once
#include "Geometry.hpp"
#include "Camera.hpp"
#include "Light.hpp"
#include "Bone.hpp"

#include <Hitagi/Math/Transform.hpp>

#include <fmt/format.h>

#include <algorithm>

namespace hitagi::asset {

class SceneNode {
public:
    SceneNode() = default;
    SceneNode(std::string_view name) : m_Name(name) {}

    virtual ~SceneNode() = default;

    const std::string& GetName() const { return m_Name; }

    inline auto GetParent() const noexcept { return m_Parent; }
    inline void SetParent(std::weak_ptr<SceneNode> parent) { m_Parent = parent; }

    inline const auto& GetChildren() const noexcept { return m_Children; }
    inline void        AppendChild(std::shared_ptr<SceneNode> child) { m_Children.push_back(std::move(child)); }

    inline auto GetPosition(bool local_space = true) const { return local_space ? m_Translation : get_translation(m_RuntimeTransform); }
    inline auto GetOrientation() const { return quaternion_to_euler(m_Rotation); }
    inline auto GetScaling() const noexcept { return m_Scaling; }
    inline auto GetVelocity() const noexcept { return m_Velocity; }

    math::mat4f GetCalculatedTransformation();

    void SetTRS(const math::vec3f& translation, const math::quatf& routation, const math::vec3f& scaling);
    // Apply a transformation to the node in its parent's space
    void        ApplyTransform(const math::mat4f& mat);
    void        Translate(const math::vec3f& translate);
    void        Rotate(const math::vec3f& eular);
    void        Scale(const math::vec3f& value);
    inline void SetVelocity(math::vec3f velocity) noexcept { m_Velocity = velocity; }

    friend std::ostream& operator<<(std::ostream& out, const SceneNode& node);

protected:
    virtual void Dump(std::ostream& out, unsigned indent) const {}
    void         SetTransformDirty();

    std::string                           m_Name;
    std::list<std::shared_ptr<SceneNode>> m_Children;
    std::weak_ptr<SceneNode>              m_Parent;

    bool        m_TransformDirty = false;
    math::vec3f m_Translation{0.0f, 0.0f, 0.0f};
    math::quatf m_Rotation{0.0f, 0.0f, 0.0f, 1.0f};
    math::vec3f m_Scaling{1.0f, 1.0f, 1.0f};
    math::vec3f m_Velocity{0.0f, 0.0f, 0.0f};

    math::mat4f m_RuntimeTransform = math::mat4f(1.0f);
};

template <typename T>
class SceneNodeWithRef : public SceneNode {
public:
    using SceneNode::SceneNode;
    SceneNodeWithRef() = default;
    void             SetSceneObjectRef(std::weak_ptr<T> ref) { m_SceneObjectRef = ref; }
    std::weak_ptr<T> GetSceneObjectRef() { return m_SceneObjectRef; }

protected:
    std::weak_ptr<T> m_SceneObjectRef;

    void Dump(std::ostream& out, unsigned indent) const override {
        if (auto obj = m_SceneObjectRef.lock()) {
            out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Obj Ref:", obj->GetGuid().str());
        }
    }
};

using SceneEmptyNode = SceneNode;

class GeometryNode : public SceneNodeWithRef<Geometry> {
public:
    using SceneNodeWithRef::SceneNodeWithRef;

    inline void SetVisibility(bool visible) noexcept { m_Visible = visible; }
    inline bool Visible() const noexcept { return m_Visible; }

private:
    bool m_Visible = true;

    void Dump(std::ostream& out, unsigned indent) const override {
        SceneNodeWithRef::Dump(out, indent);
        out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Visible:", m_Visible);
    }
};

class LightNode : public SceneNodeWithRef<Light> {
public:
    using SceneNodeWithRef::SceneNodeWithRef;
};

class CameraNode : public SceneNodeWithRef<Camera> {
public:
    CameraNode(std::string_view name, const math::vec3f& position = math::vec3f(0.0f), const math::vec3f& up = math::vec3f(0, 0, 1),
               const math::vec3f& look_at = math::vec3f(0, -1, 0))
        : SceneNodeWithRef(name),
          m_Position(position),
          m_LookAt(normalize(look_at)),
          m_Right(normalize(cross(look_at, up))),
          m_Up(normalize(cross(m_Right, m_LookAt))) {}

    math::mat4f GetViewMatrix() { return look_at(m_Position, m_LookAt, m_Up) * inverse(GetCalculatedTransformation()); }

    // TODO
    inline auto GetCameraPosition() { return (GetCalculatedTransformation() * math::vec4f(m_Position, 1)).xyz; }
    inline auto GetCameraUp() { return (GetCalculatedTransformation() * math::vec4f(m_Up, 0)).xyz; }
    inline auto GetCameraLookAt() { return (GetCalculatedTransformation() * math::vec4f(m_LookAt, 0)).xyz; }
    inline auto GetCameraRight() { return (GetCalculatedTransformation() * math::vec4f(m_Right, 0)).xyz; }

private:
    math::vec3f m_Position;
    math::vec3f m_LookAt;
    math::vec3f m_Right;
    math::vec3f m_Up;
};

class BoneNode : public SceneNodeWithRef<Bone> {
public:
    using SceneNodeWithRef::SceneNodeWithRef;

private:
    bool m_Visibility = false;
};

inline auto flatent_scene_tree(std::shared_ptr<SceneNode> node) -> std::vector<std::shared_ptr<SceneNode>> {
    std::vector<std::shared_ptr<SceneNode>> result;
    result.emplace_back(node);
    for (auto&& child : node->GetChildren()) {
        auto sub_result = flatent_scene_tree(child);
        result.insert(result.end(), std::make_move_iterator(sub_result.begin()), std::make_move_iterator(sub_result.end()));
    }
    return result;
}

}  // namespace hitagi::asset
