#pragma once
#include "Geometry.hpp"
#include "Camera.hpp"
#include "Light.hpp"

#include <fmt/format.h>

namespace Hitagi::Asset {

class SceneNode {
public:
    SceneNode() = default;
    SceneNode(std::string_view name) : m_Name(name) {}

    virtual ~SceneNode() = default;

    const std::string& GetName() const { return m_Name; }

    inline void SetParent(std::weak_ptr<SceneNode> parent) {
        m_Parent = parent;
    }
    inline void AppendChild(std::shared_ptr<SceneNode> child) {
        m_Children.push_back(std::move(child));
    }
    inline const auto& GetChildren() const noexcept { return m_Children; }

    // Apply a transformation to the node in its parent's space
    void ApplyTransform(const mat4f& mat) {
        auto curr_transform                   = translate(rotate(scale(mat4f(1.0f), m_Scaling), m_Orientation), m_Position);
        auto [translation, rotation, scaling] = decompose(mat * curr_transform);

        m_Orientation = rotation;
        m_Position    = translation;
        m_Scaling     = scaling;

        SetDirty();
    }

    inline const vec3f GetPosition(bool local_space = true) const {
        return local_space ? m_Position : get_translation(m_RuntimeTransform);
    }

    inline const vec3f GetOrientation() const {
        return m_Orientation;
    }

    inline const vec3f GetScaling() const {
        return m_Scaling;
    }

    inline void Translate(const vec3f& translate) noexcept {
        m_Position += translate;
        SetDirty();
    }
    inline void Rotate(const vec3f& angles) noexcept {
        m_Orientation += angles;
        SetDirty();
    }
    inline void Scale(const vec3f value) noexcept {
        m_Scaling = m_Scaling * value;
        SetDirty();
    }

    const mat4f GetCalculatedTransformation() {
        if (m_Dirty) {
            m_RuntimeTransform = translate(rotate(scale(mat4f(1.0f), m_Scaling), m_Orientation), m_Position);
            if (auto parent = m_Parent.lock())
                m_RuntimeTransform = m_RuntimeTransform * parent->GetCalculatedTransformation();
        }
        m_Dirty = false;
        return m_RuntimeTransform;
    }

    friend std::ostream& operator<<(std::ostream& out, const SceneNode& node) {
        static unsigned indent = 0;
        out << fmt::format(
            "{0:{1}}{2:-^30}\n"
            "{0:{1}}{3:<15}{4:>15}\n",
            "", indent, "Scene Node", "Name:", node.m_Name);

        node.Dump(out, indent);
        out << fmt::format("{0:{1}}{2}\n", "", indent, "Children:");
        indent += 4;
        for (const std::shared_ptr<SceneNode>& sub_node : node.m_Children) {
            out << *sub_node << std::endl;
        }

        indent -= 4;
        return out << fmt::format("{0:{1}}{2:-^30}", "", indent, "End");
    }

protected:
    virtual void Dump(std::ostream& out, unsigned indent) const {}
    inline void  SetDirty() {
        m_Dirty = true;
        for (auto&& child : m_Children)
            child->SetDirty();
    }

    std::string                           m_Name;
    std::list<std::shared_ptr<SceneNode>> m_Children;
    std::weak_ptr<SceneNode>              m_Parent;

    vec3f m_Orientation{0.0f, 0.0f, 0.0f};
    vec3f m_Position{0.0f, 0.0f, 0.0f};
    vec3f m_Scaling{1.0f, 1.0f, 1.0f};

    mat4f m_RuntimeTransform = mat4f(1.0f);
    bool  m_Dirty            = true;
};

template <typename T>
class SceneNodeWithRef : public SceneNode {
protected:
    std::weak_ptr<T> m_SceneObjectRef;

    void Dump(std::ostream& out, unsigned indent) const override {
        if (auto obj = m_SceneObjectRef.lock()) {
            out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Obj Ref:", obj->GetGuid());
        }
    }

public:
    using SceneNode::SceneNode;
    SceneNodeWithRef() = default;
    void             SetSceneObjectRef(std::weak_ptr<T> ref) { m_SceneObjectRef = ref; }
    std::weak_ptr<T> GetSceneObjectRef() { return m_SceneObjectRef; }
};

using SceneEmptyNode = SceneNode;

class GeometryNode : public SceneNodeWithRef<Geometry> {
protected:
    bool m_Visible = true;

    void Dump(std::ostream& out, unsigned indent) const override {
        SceneNodeWithRef::Dump(out, indent);
        out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Visible:", m_Visible);
    }

public:
    using SceneNodeWithRef::SceneNodeWithRef;
    using SceneNodeWithRef::SetSceneObjectRef;

    void       SetVisibility(bool visible) { m_Visible = visible; }
    const bool Visible() { return m_Visible; }
};

class LightNode : public SceneNodeWithRef<Light> {
public:
    using SceneNodeWithRef::SceneNodeWithRef;
};

class CameraNode : public SceneNodeWithRef<Camera> {
public:
    CameraNode(std::string_view name, const vec3f& position = vec3f(0.0f), const vec3f& up = vec3f(0, 0, 1),
               const vec3f& look_at = vec3f(0, -1, 0))
        : SceneNodeWithRef(name),
          m_Position(position),
          m_LookAt(normalize(look_at)),
          m_Right(normalize(cross(look_at, up))),
          m_Up(normalize(cross(m_Right, m_LookAt))) {}

    mat4f GetViewMatrix() { return look_at(m_Position, m_LookAt, m_Up) * inverse(GetCalculatedTransformation()); }

    vec3f GetCameraPosition() { return (GetCalculatedTransformation() * vec4f(m_Position, 1)).xyz; }
    vec3f GetCameraUp() { return (GetCalculatedTransformation() * vec4f(m_Up, 0)).xyz; }
    vec3f GetCameraLookAt() { return (GetCalculatedTransformation() * vec4f(m_LookAt, 0)).xyz; }
    vec3f GetCameraRight() { return (GetCalculatedTransformation() * vec4f(m_Right, 0)).xyz; }

private:
    vec3f m_Position;
    vec3f m_LookAt;
    vec3f m_Right;
    vec3f m_Up;
};

class BoneNode : public SceneNode {
public:
    using SceneNode::SceneNode;

private:
    bool m_Visibility = false;
};

}  // namespace Hitagi::Asset
