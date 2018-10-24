#pragma once
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <list>
#include "SceneObject.hpp"

namespace My {

class BaseSceneNode {
protected:
    std::string                                      m_strName;
    std::list<std::unique_ptr<BaseSceneNode>>        m_Chlidren;
    std::list<std::unique_ptr<SceneObjectTransform>> m_Transforms;

    virtual void dump(std::ostream& out) const {}

public:
    BaseSceneNode() {}
    BaseSceneNode(const char* name) { m_strName = name; }
    BaseSceneNode(std::string& name) { m_strName = name; }
    BaseSceneNode(std::string&& name) { m_strName = std::move(name); }

    virtual ~BaseSceneNode() {}

    void AppendChild(std::unique_ptr<BaseSceneNode>&& sub_node) {
        m_Chlidren.push_back(std::move(sub_node));
    }
    void AppendChild(std::unique_ptr<SceneObjectTransform>&& transform) {
        m_Transforms.push_back(std::move(transform));
    }

    friend std::ostream& operator<<(std::ostream&        out,
                                    const BaseSceneNode& node) {
        static thread_local int32_t indent = 0;
        indent++;
        out << std::string(indent, ' ') << "Scene Node" << std::endl;
        out << std::string(indent, ' ') << "----------" << std::endl;
        out << std::string(indent, ' ') << "Name: " << node.m_strName
            << std::endl;
        node.dump(out);
        out << std::endl;

        for (const std::unique_ptr<BaseSceneNode>& sub_node : node.m_Chlidren) {
            out << *sub_node << std::endl;
        }
        for (const std::unique_ptr<SceneObjectTransform>& sub_node :
             node.m_Transforms) {
            out << *sub_node << std::endl;
        }
        indent--;

        return out;
    }
};

template <typename T>
class SceneNode : public BaseSceneNode {
protected:
    std::shared_ptr<T> m_pSceneObjcet;

    virtual void dump(std::ostream& out) const {
        if (m_pSceneObjcet) out << *m_pSceneObjcet << std::endl;
    }

public:
    using BaseSceneNode::BaseSceneNode;
    SceneNode() = default;
    SceneNode(const std::shared_ptr<T>& object) { m_pSceneObjcet = object; }
    SceneNode(const std::shared_ptr<T>&& object) {
        m_pSceneObjcet = std::move(object);
    }

    void AddSceneObjectRef(const std::shared_ptr<T>& object) {
        m_pSceneObjcet = object;
    }
    void AddSceneObjectRef(const std::shared_ptr<T>&& object) {
        m_pSceneObjcet = std::move(object);
    }
};

typedef BaseSceneNode SceneEmptyNode;
class SceneGeometryNode : public SceneNode<SceneObjectGeometry> {
protected:
    bool m_bVisible;
    bool m_bShadow;
    bool m_bMotionBlur;

    std::vector<std::shared_ptr<SceneObjectMaterial>> m_Materials;

    virtual void dump(std::ostream& out) const {
        SceneNode::dump(out);
        out << "Visible: " << m_bVisible << std::endl;
        out << "Shadow: " << m_bShadow << std::endl;
        out << "Motion Blur: " << m_bMotionBlur << std::endl;
        out << "Material(s): " << std::endl;
        for (auto material : m_Materials) out << *material << std::endl;
    }

public:
    using SceneNode::SceneNode;
    void SetVisibility(bool visible) { m_bVisible = visible; }
    void SetIfCastShadow(bool shadow) { m_bShadow = shadow; }
    void SetIfMotionBlur(bool motion_blur) { m_bMotionBlur = motion_blur; }
    const bool Visible() { return m_bVisible; }
    const bool CastShadow() { return m_bShadow; }
    const bool MotionBlur() { return m_bMotionBlur; }

    using SceneNode::AddSceneObjectRef;
    void AddSceneObjectRef(const std::shared_ptr<SceneObjectMaterial>& object) {
        m_Materials.push_back(object);
    }
};

class SceneLightNode : public SceneNode<SceneObjectLight> {
protected:
    glm::vec3 m_Target;

public:
    using SceneNode::SceneNode;
    void             SetTarget(glm::vec3& target) { m_Target = target; }
    const glm::vec3& GetTarget() { return m_Target; }
};

class SceneCameraNode : public SceneNode<SceneObjectCamera> {
protected:
    glm::vec3 m_Target;

public:
    using SceneNode::SceneNode;
    void             SetTarget(glm::vec3& target) { m_Target = target; }
    const glm::vec3& GetTarget() { return m_Target; }
};

}  // namespace My
