#include <iostream>
#include <string>
#include <vector>
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm.hpp"
#include "gtx/string_cast.hpp"

#include "guid.hpp"
#include "Image.hpp"
#include "portable.hpp"

namespace My {
namespace details {
constexpr int32_t i32(const char* s, int32_t v) {
    return *s ? i32(s + 1, v * 256 + *s) : v;
}
}  // namespace details
constexpr int32_t operator"" _i32(const char* s, size_t) {
    return details::i32(s, 0);
}

enum SceneObjectType {
    SCENE_OBJECT_TYPE_MESH          = "MESH"_i32,
    SCENE_OBJECT_TYPE_MATRIAL       = "MATL"_i32,
    SCENE_OBJECT_TYPE_TEXTURE       = "TXTU"_i32,
    SCENE_OBJECT_TYPE_LIGHT         = "LGHT"_i32,
    SCENE_OBJECT_TYPE_CAMERA        = "CAMR"_i32,
    SCENE_OBJECT_TYPE_ANIMATOR      = "ANIM"_i32,
    SCENE_OBJECT_TYPE_CLIP          = "CLIP"_i32,
    SCENE_OBJECT_TYPE_GEOMETRY      = "GEOM"_i32,
    SCENE_OBJECT_TYPE_INDEX_ARRAY   = "VARR"_i32,
    SCENE_OBJECT_TYPE_VERETEX_ARRAY = "VARR"_i32,
};

std::ostream& operator<<(std::ostream& out, SceneObjectType type) {
    int32_t n = static_cast<int32_t>(type);
    // little endian, read bit from end;
    n       = endian_net_unsigned_int<int32_t>(n);
    char* c = reinterpret_cast<char*>(&n);

    for (int i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }

    return out;
}

using namespace xg;

class BaseSceneObject {
protected:
    Guid            m_Guid;
    SceneObjectType m_Type;
    BaseSceneObject(SceneObjectType type) : m_Type(type) { m_Guid = newGuid(); }
    BaseSceneObject(Guid& guid, SceneObjectType type)
        : m_Guid(guid), m_Type(type) {}
    BaseSceneObject(Guid&& guid, SceneObjectType type)
        : m_Guid(std::move(guid)), m_Type(type) {}
    BaseSceneObject(BaseSceneObject&& obj)
        : m_Guid(std::move(obj.m_Guid)), m_Type(obj.m_Type) {}
    BaseSceneObject& operator=(BaseSceneObject&& obj) {
        this->m_Guid = std::move(obj.m_Guid);
        this->m_Type = obj.m_Type;
        return *this;
    }

private:
    BaseSceneObject()                     = delete;
    BaseSceneObject(BaseSceneObject& obj) = delete;
    BaseSceneObject& operator=(BaseSceneObject& obj) = delete;

public:
    const Guid&           GetGuid() const { return m_Guid; }
    const SceneObjectType GetType() const { return m_Type; }

    friend std::ostream& operator<<(std::ostream&          out,
                                    const BaseSceneObject& obj) {
        out << "SceneObject" << std::endl;
        out << "-----------" << std::endl;
        out << "GUID: " << obj.m_Guid << std::endl;
        out << "Type: " << obj.m_Type << std::endl;
        return out;
    }
};

enum VertexDataType {
    VERTEX_DATA_TYPE_FLOAT1  = "FLT1"_i32,
    VERTEX_DATA_TYPE_FLOAT2  = "FLT2"_i32,
    VERTEX_DATA_TYPE_FLOAT3  = "FLT3"_i32,
    VERTEX_DATA_TYPE_FLOAT4  = "FLT4"_i32,
    VERTEX_DATA_TYPE_DOUBLE1 = "DUB1"_i32,
    VERTEX_DATA_TYPE_DOUBLE2 = "DUB2"_i32,
    VERTEX_DATA_TYPE_DOUBLE3 = "DUB3"_i32,
    VERTEX_DATA_TYPE_DOUBLE4 = "DUB4"_i32,
};

class SceneObjectVertexArray : public BaseSceneObject {
protected:
    std::string    m_Attribute;
    uint32_t       m_MorphTargetIndex;
    VertexDataType m_DataType;
    void*          m_pDataFloat;
    size_t         m_szData;

public:
    SceneObjectVertexArray(const char* attr, void* data, size_t data_size,
                           VertexDataType data_type, uint32_t morph_index = 0)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_VERETEX_ARRAY),
          m_Attribute(attr),
          m_MorphTargetIndex(morph_index),
          m_DataType(data_type),
          m_pDataFloat(data),
          m_szData(data_size){};
};

enum IndexDataType {
    INDEX_DATA_TYPE_INT16 = "_I16"_i32,
    INDEX_DATA_TYPE_INT32 = "_I32"_i32
};

class SceneObjectIndexArray : public BaseSceneObject {
protected:
    uint32_t      m_MaterialIndex;
    size_t        m_RestartIndex;
    IndexDataType m_DataType;
    void*         m_pData;
    size_t        m_szData;

public:
    SceneObjectIndexArray(
        uint32_t      material_index,
        IndexDataType data_type     = IndexDataType::INDEX_DATA_TYPE_INT16,
        uint32_t      restart_index = 0)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_INDEX_ARRAY),
          m_MaterialIndex(material_index),
          m_RestartIndex(restart_index),
          m_DataType(data_type) {}
};

class SceneObjectMesh : public BaseSceneObject {
protected:
    std::vector<SceneObjectIndexArray>  m_IndexArray;
    std::vector<SceneObjectVertexArray> m_VertexArray;

    bool m_bVisible;
    bool m_bShadow;
    bool m_bMotionBlur;

public:
    SceneObjectMesh(bool visible = true, bool shadow = true,
                    bool motion_blur = true)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_MESH),
          m_bVisible(visible),
          m_bShadow(shadow),
          m_bMotionBlur(motion_blur) {}

    void AddIndexArray(SceneObjectIndexArray&& array) {
        m_IndexArray.push_back(std::move(array));
    }
    void AddVertexArray(SceneObjectVertexArray&& array) {
        m_VertexArray.push_back(std::move(array));
    }
    friend std::ostream& operator<<(std::ostream&          out,
                                    const SceneObjectMesh& obj) {
        out << static_cast<const BaseSceneObject&>(obj) << std::endl;
        out << "Visible: " << obj.m_bVisible << std::endl;
        out << "Shadow: " << obj.m_bShadow << std::endl;
        out << "Motion Blur: " << obj.m_bMotionBlur << std::endl;

        return out;
    }
};

template <typename T>
struct ParameterMap {
    bool bUsingSingleValue;

    union {
        T                      Value;
        std::shared_ptr<Image> Map;
    };

    ParameterMap(T value) : bUsingSingleValue(true), Value(value) {}

    ParameterMap(const ParameterMap& rhs) {
        bUsingSingleValue = rhs.bUsingSingleValue;

        if (bUsingSingleValue)
            Value = rhs.Value;
        else
            Map = rhs.Map;
    }

    ParameterMap(ParameterMap&& rhs) {
        bUsingSingleValue = rhs.bUsingSingleValue;
        if (bUsingSingleValue)
            Value = rhs.Value;
        else {
            Map = std::move(rhs.Map);
            rhs.Map.reset();
        }
    }

    ~ParameterMap() {
        if (!bUsingSingleValue) {
            Map.reset();
        }
    }
    friend std::ostream& operator<<(std::ostream&       out,
                                    const ParameterMap& obj) {
        if (obj.bUsingSingleValue) {
            out << "Parameter Type: Single Value" << std::endl;
            out << "Parameter Value: " << glm::to_string(obj.Value)
                << std::endl;
        } else {
            out << "Parameter Type: Map" << std::endl;
        }

        return out;
    }
};

typedef ParameterMap<glm::vec4> Color;
typedef ParameterMap<glm::vec3> Normal;
typedef ParameterMap<float>     Parameter;

class SceneObjectMaterial : public BaseSceneObject {
protected:
    Color     m_BaseColor;
    Parameter m_Metallic;
    Parameter m_Roughness;
    Normal    m_Normal;
    Parameter m_Specular;
    Parameter m_AmbientOcclusion;

public:
    SceneObjectMaterial(Color&&     base_color = glm::vec4(1.0f),
                        Parameter&& metallic   = 0.0f,
                        Parameter&& roughness  = 0.0f,
                        Normal&&    normal     = glm::vec3(0.0f, 0.0f, 1.0f),
                        Parameter&& specular = 0.0f, Parameter&& ao = 0.0f)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_MATRIAL),
          m_BaseColor(base_color),
          m_Metallic(metallic),
          m_Roughness(roughness),
          m_Normal(normal),
          m_Specular(specular),
          m_AmbientOcclusion(ao) {}

    friend std::ostream& operator<<(std::ostream&              out,
                                    const SceneObjectMaterial& obj) {
        out << static_cast<const BaseSceneObject&>(obj) << std::endl;
        out << "Albedo: " << obj.m_BaseColor << std::endl;
        out << "Metallic: " << obj.m_Metallic << std::endl;
        out << "Roughness: " << obj.m_Roughness << std::endl;
        out << "Normal: " << obj.m_Normal << std::endl;
        out << "Specular: " << obj.m_Specular << std::endl;
        out << "Ambient Occlusion: " << obj.m_AmbientOcclusion << std::endl;
        return out;
    }
};

class SceneObjectGeometry : public BaseSceneObject {
protected:
    std::vector<SceneObjectMesh> m_Mesh;

public:
    void AddMesh(SceneObjectMesh&& mesh) { m_Mesh.push_back(std::move(mesh)); }
    SceneObjectGeometry()
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_GEOMETRY) {}
};

typedef float (*AttenFunc)(float /* Intensity */, float /* distance */);

float DefaultAttenFunc(float intensity, float distance) {
    return intensity / (1 + distance);
}

class SceneObjectLight : public BaseSceneObject {
protected:
    Color     m_LightColor;
    float     m_fIntensity;
    AttenFunc m_LightAttenuation;
    float     m_fNearClipDistance;
    float     m_fFarClipDistance;
    bool      m_bCastShadows;

protected:
    // can only be used as base class of delivered lighting objects
    SceneObjectLight(Color&& color = glm::vec4(1.0f), float intensity = 10.0f,
                     AttenFunc atten_func = DefaultAttenFunc,
                     float near_clip = 1.0f, float far_clip = 100.0f,
                     bool cast_shadows = false)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_LIGHT),
          m_LightColor(std::move(color)),
          m_fIntensity(intensity),
          m_LightAttenuation(atten_func),
          m_fNearClipDistance(near_clip),
          m_fFarClipDistance(far_clip),
          m_bCastShadows(cast_shadows) {}

    friend std::ostream& operator<<(std::ostream&           out,
                                    const SceneObjectLight& obj) {
        out << static_cast<const BaseSceneObject&>(obj) << std::endl;
        out << "Color: " << obj.m_LightColor << std::endl;
        out << "Intensity: " << obj.m_fIntensity << std::endl;
        out << "Near Clip Distance: " << obj.m_fNearClipDistance << std::endl;
        out << "Far Clip Distance: " << obj.m_fFarClipDistance << std::endl;
        out << "Cast Shadows: " << obj.m_bCastShadows << std::endl;

        return out;
    }
};

class SceneObjectOmniLight : public SceneObjectLight {
public:
    using SceneObjectLight::SceneObjectLight;
    friend std::ostream& operator<<(std::ostream&               out,
                                    const SceneObjectOmniLight& obj) {
        out << static_cast<const SceneObjectLight&>(obj) << std::endl;
        out << "Light Type: Omni" << std::endl;
        return out;
    }
};

class SceneObjectSpotLight : public SceneObjectLight {
protected:
    float m_fConeAngle;
    float m_fPenumbraAngle;

public:
    SceneObjectSpotLight(Color&&   color      = glm::vec4(1.0f),
                         float     intensity  = 10.0f,
                         AttenFunc atten_func = DefaultAttenFunc,
                         float near_clip = 1.0f, float far_clip = 100.0f,
                         bool  cast_shadows   = false,
                         float cone_angle     = glm::pi<float>() / 4.0f,
                         float penumbra_angle = glm::pi<float>() / 3.0f)
        : SceneObjectLight(std::move(color), intensity, atten_func, near_clip,
                           far_clip, cast_shadows),
          m_fConeAngle(cone_angle),
          m_fPenumbraAngle(penumbra_angle) {}

    friend std::ostream& operator<<(std::ostream&               out,
                                    const SceneObjectSpotLight& obj) {
        out << static_cast<const SceneObjectLight&>(obj) << std::endl;
        out << "Light Type: Spot" << std::endl;
        out << "Cone Angle" << obj.m_fConeAngle << std::endl;
        out << "Penumbra Angle" << obj.m_fPenumbraAngle << std::endl;
        return out;
    }
};

class SceneObjectCamera : public BaseSceneObject {
protected:
    float m_fAspect;
    float m_fNearClipDistance;
    float m_fFarClipDistance;

    // can only be used as base class
    SceneObjectCamera(float aspect = 16.0f / 9.0f, float near_clip = 1.0f,
                      float far_clip = 100.0f)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_CAMERA),
          m_fAspect(aspect),
          m_fNearClipDistance(near_clip),
          m_fFarClipDistance(far_clip) {}

    friend std::ostream& operator<<(std::ostream&            out,
                                    const SceneObjectCamera& obj) {
        out << static_cast<const BaseSceneObject&>(obj) << std::endl;
        out << "Aspcet: " << obj.m_fAspect << std::endl;
        out << "Near Clip Distance: " << obj.m_fNearClipDistance << std::endl;
        out << "Far Clip Distance: " << obj.m_fFarClipDistance << std::endl;
        return out;
    }
};

class SceneObjectOrthogonalCamera : public SceneObjectCamera {
public:
    using SceneObjectCamera::SceneObjectCamera;
    friend std::ostream& operator<<(std::ostream&                      out,
                                    const SceneObjectOrthogonalCamera& obj) {
        out << static_cast<const SceneObjectCamera&>(obj) << std::endl;
        out << "Camera Type: Orthogonal" << std::endl;
        return out;
    }
};

class SceneObjectPerspectiveCamera : public SceneObjectCamera {
protected:
    float m_fFov;

public:
    SceneObjectPerspectiveCamera(float aspect    = 16.0f / 9.0f,
                                 float near_clip = 1.0f,
                                 float far_clip  = 100.0f,
                                 float fov       = glm::pi<float>() / 2.0)
        : SceneObjectCamera(aspect, near_clip, far_clip), m_fFov(fov) {}
    friend std::ostream& operator<<(std::ostream&                       out,
                                    const SceneObjectPerspectiveCamera& obj) {
        out << static_cast<const SceneObjectCamera&>(obj) << std::endl;
        out << "Camera Type: Perspective" << std::endl;
        out << "FOV" << obj.m_fFov << std::endl;
        return out;
    }
};

}  // namespace My
