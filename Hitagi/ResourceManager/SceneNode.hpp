#pragma once
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <list>

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

    const std::shared_ptr<mat4f> GetCalculatedTransform() const {
        std::shared_ptr<mat4f> result(new mat4f(1.0f));

        for (auto trans : m_Transforms) {
            *result *= static_cast<mat4f>(*trans);
        }
        *result *= m_RuntimeTransform;
        return result;
    }
    // Get is the node updated
    bool Dirty() const { return m_Dirty; }
    void ClearDirty() { m_Dirty = false; }

    void RotateBy(const float& x, const float& y, const float& z) {
        m_RuntimeTransform *= rotate(mat4f(1.0f), x, y, z);
        m_Dirty = true;
    }
    void Move(const float& x, const float& y, const float& z) {
        m_RuntimeTransform *= translate(mat4f(1.0f), vec3f(x, y, z));
        m_Dirty = true;
    }

    void Reset() { m_RuntimeTransform = mat4f(1.0f); }

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
    bool                  m_Visible;
    bool                  m_Shadow;
    bool                  m_MotionBlur;
    std::shared_ptr<void> m_RigidBody = nullptr;

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
protected:
    vec3f m_Target;

public:
    using SceneNode::SceneNode;
    void         SetTarget(vec3f& target) { m_Target = target; }
    const vec3f& GetTarget() { return m_Target; }
};

}  // namespace Hitagi::Resource
