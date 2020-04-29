#pragma once
#include <fmt/format.h>

#include "SceneObject.hpp"

namespace Hitagi::Resource {

class BaseSceneNode {
protected:
    std::string                                      m_Name;
    std::list<std::shared_ptr<BaseSceneNode>>        m_Chlidren;
    std::list<std::shared_ptr<SceneObjectTransform>> m_Transforms;
    mat4f                                            m_RuntimeTransform = mat4f(1.0f);
    bool                                             m_Dirty            = true;

    virtual void dump(std::ostream& out, unsigned indent) const {}

public:
    BaseSceneNode() {}
    BaseSceneNode(std::string_view name) { m_Name = name; }

    virtual ~BaseSceneNode() {}

    const std::string& GetName() const { return m_Name; }

    void AppendChild(std::shared_ptr<BaseSceneNode>&& sub_node) { m_Chlidren.push_back(std::move(sub_node)); }
    void AppendTransform(std::shared_ptr<SceneObjectTransform>&& transform) {
        m_Transforms.push_back(std::move(transform));
    }

    mat4f GetCalculatedTransform() const {
        mat4f result(1.0f);

        for (auto trans : m_Transforms) {
            result = static_cast<mat4f>(*trans) * result;
        }
        result = m_RuntimeTransform * result;
        return result;
    }
    // Get is the node updated
    bool Dirty() const { return m_Dirty; }
    void ClearDirty() { m_Dirty = false; }

    void ApplyTransform(const mat4f& trans) {
        m_RuntimeTransform = trans * m_RuntimeTransform;
        m_Dirty            = true;
    }

    void Reset(bool recursive = false) {
        m_RuntimeTransform = mat4f(1.0f);
        if (recursive)
            for (auto&& child : m_Chlidren)
                if (child) child->Reset(recursive);
    }

    friend std::ostream& operator<<(std::ostream& out, const BaseSceneNode& node) {
        static unsigned indent = 0;
        out << fmt::format(
            "{0:{1}}{2:-^30}\n"
            "{0:{1}}{3:<15}{4:>15}\n",
            "", indent, "Scene Node", "Name:", node.m_Name);

        node.dump(out, indent);
        out << fmt::format("{0:{1}}{2}\n", "", indent, "Children:");
        indent += 4;
        for (const std::shared_ptr<BaseSceneNode>& sub_node : node.m_Chlidren) {
            out << *sub_node << std::endl;
        }

        indent -= 4;
        return out << fmt::format("{0:{1}}{2:-^30}", "", indent, "End");
    }
};

template <typename T>
class SceneNode : public BaseSceneNode {
protected:
    std::string m_SceneObjectRef;

    virtual void dump(std::ostream& out, unsigned indent) const {
        if (!m_SceneObjectRef.empty()) {
            out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Obj Ref:", m_SceneObjectRef);
        }
    }

public:
    using BaseSceneNode::BaseSceneNode;
    SceneNode() = default;
    void               AddSceneObjectRef(std::string_view key) { m_SceneObjectRef = key; }
    const std::string& GetSceneObjectRef() { return m_SceneObjectRef; }
};

typedef BaseSceneNode SceneEmptyNode;
class SceneGeometryNode : public SceneNode<SceneObjectGeometry> {
protected:
    bool                  m_Visible    = true;
    bool                  m_Shadow     = true;
    bool                  m_MotionBlur = false;
    std::shared_ptr<void> m_RigidBody  = nullptr;

    virtual void dump(std::ostream& out, unsigned indent) const {
        SceneNode::dump(out, indent);
        out << fmt::format(
            "{0:{1}}{2:<15}{3:>15}\n"
            "{0:{1}}{4:<15}{5:>15}\n"
            "{0:{1}}{6:<15}{7:>15}\n",
            "", indent, "Visible:", m_Visible, "Shadow:", m_Shadow, "Motion Blur:", m_MotionBlur);
    }

public:
    using SceneNode::SceneNode;
    void       SetVisibility(bool visible) { m_Visible = visible; }
    void       SetIfCastShadow(bool shadow) { m_Shadow = shadow; }
    void       SetIfMotionBlur(bool motionBlur) { m_MotionBlur = motionBlur; }
    const bool Visible() { return m_Visible; }
    const bool CastShadow() { return m_Shadow; }
    const bool MotionBlur() { return m_MotionBlur; }

    using SceneNode::AddSceneObjectRef;
    void LinkRigidBody(std::shared_ptr<void> rigidBody) { m_RigidBody = rigidBody; }
    void UnlinkRigidBody() { m_RigidBody = nullptr; }

    std::shared_ptr<void> RigidBody() { return m_RigidBody; }
};

class SceneLightNode : public SceneNode<SceneObjectLight> {
protected:
    bool m_Shadow;

public:
    using SceneNode::SceneNode;
    void       SetIfCastShadow(bool shaodw) { m_Shadow = shaodw; }
    const bool CastShadow() { return m_Shadow; }
};

class SceneCameraNode : public SceneNode<SceneObjectCamera> {
public:
    SceneCameraNode(std::string_view name, const vec3f& position = vec3f(0.0f), const vec3f& up = vec3f(0, 0, 1),
                    const vec3f& lookAt = vec3f(0, -1, 0))
        : SceneNode(name),
          m_Position(position),
          m_LookAt(normalize(lookAt)),
          m_Right(normalize(cross(lookAt, up))),
          m_Up(normalize(cross(m_Right, m_LookAt))) {}

    mat4f GetViewMatrix() const { return lookAt(m_Position, m_LookAt, m_Up) * inverse(GetCalculatedTransform()); }

    vec3f GetCameraPosition() const { return (GetCalculatedTransform() * vec4f(m_Position, 1)).xyz; }
    vec3f GetCameraUp() const { return (GetCalculatedTransform() * vec4f(m_Up, 0)).xyz; }
    vec3f GetCameraLookAt() const { return (GetCalculatedTransform() * vec4f(m_LookAt, 0)).xyz; }
    vec3f GetCameraRight() const { return (GetCalculatedTransform() * vec4f(m_Right, 0)).xyz; }

private:
    vec3f m_Position;
    vec3f m_LookAt;
    vec3f m_Right;
    vec3f m_Up;
};

}  // namespace Hitagi::Resource
