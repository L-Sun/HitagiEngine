#pragma once
#include "HitagiMath.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

#include <crossguid/guid.hpp>

#include <filesystem>

namespace Hitagi::Asset {
enum struct SceneObjectType {
    Mesh,
    Material,
    Texture,
    Light,
    Camera,
    Animator,
    Clip,
    Geometry,
    IndexArray,
    VertexArray,
};

enum struct SceneObjectCollisionType {
    None,
    Sphere,
    BOx,
    Cylinder,
    Capsule,
    Cone,
    MultiSphere,
    ConvexHull,
    ConvexMesh,
    BvhMesh,
    HeightField,
    Plane,
};

enum struct VertexDataType {
    Float1,
    Float2,
    Float3,
    Float4,
    Double1,
    Double2,
    Double3,
    Double4,
};

enum struct IndexDataType {
    Int8,
    Int16,
    Int32,
    Int64,
};

class BaseSceneObject {
protected:
    xg::Guid        m_Guid;
    SceneObjectType m_Type;
    BaseSceneObject(SceneObjectType type);
    BaseSceneObject(const xg::Guid& guid, const SceneObjectType& type);
    BaseSceneObject(xg::Guid&& guid, const SceneObjectType& type);
    BaseSceneObject(BaseSceneObject&& obj);
    BaseSceneObject& operator  =(BaseSceneObject&& obj);
    virtual ~BaseSceneObject() = default;

    BaseSceneObject()                           = default;
    BaseSceneObject(const BaseSceneObject& obj) = default;
    BaseSceneObject& operator=(const BaseSceneObject& obj) = default;

public:
    const xg::Guid&       GetGuid() const { return m_Guid; }
    const SceneObjectType GetType() const { return m_Type; }

    friend std::ostream& operator<<(std::ostream& out, const BaseSceneObject& obj);
};

class SceneObjectTexture : public BaseSceneObject {
protected:
    uint32_t               m_TexCoordIndex = 0;
    std::string            m_Name;
    std::filesystem::path  m_TexturePath;
    std::shared_ptr<Image> m_Image = nullptr;

public:
    SceneObjectTexture() : BaseSceneObject(SceneObjectType::Texture) {}
    SceneObjectTexture(const std::filesystem::path& path)
        : BaseSceneObject(SceneObjectType::Texture), m_Name(path.filename().string()), m_TexturePath(path) {}
    SceneObjectTexture(uint32_t coord_index, std::shared_ptr<Image> image)
        : BaseSceneObject(SceneObjectType::Texture), m_TexCoordIndex(coord_index), m_Image(std::move(image)) {}
    SceneObjectTexture(SceneObjectTexture&)  = default;
    SceneObjectTexture(SceneObjectTexture&&) = default;

    void                   SetName(const std::string& name);
    void                   SetName(std::string&& name);
    void                   LoadTexture();
    const std::string&     GetName() const;
    std::shared_ptr<Image> GetTextureImage() const;
    friend std::ostream&   operator<<(std::ostream& out, const SceneObjectTexture& obj);
};

template <typename T>
struct ParameterValueMap {
    T                                   value;
    std::shared_ptr<SceneObjectTexture> value_map;

    ParameterValueMap() = default;
    ParameterValueMap(const T value) : value(value) {}
    ParameterValueMap(const std::shared_ptr<SceneObjectTexture>& value) : value_map(value) {}
    ParameterValueMap(const ParameterValueMap& rhs) = default;
    ParameterValueMap(ParameterValueMap&& rhs)      = default;
    ParameterValueMap& operator=(const ParameterValueMap& rhs) = default;
    ParameterValueMap& operator=(ParameterValueMap&& rhs) = default;
    ParameterValueMap& operator                           =(const std::shared_ptr<SceneObjectTexture>& rhs) {
        value_map = rhs;
        return *this;
    }
    ~ParameterValueMap() = default;

    friend std::ostream& operator<<(std::ostream& out, const ParameterValueMap& obj) {
        out << obj.value;
        if (obj.value_map) {
            out << fmt::format("\n{:-^30}\n", "Parameter Map") << *(obj.value_map)
                << fmt::format("\n{:-^30}", "Parameter Map End");
        }
        return out;
    }
};

using Color     = ParameterValueMap<vec4f>;
using Normal    = ParameterValueMap<vec3f>;
using Parameter = ParameterValueMap<float>;

class SceneObjectMaterial : public BaseSceneObject {
protected:
    std::string m_Name;
    Color       m_AmbientColor;
    Color       m_DiffuseColor;
    Parameter   m_Metallic;
    Parameter   m_Roughness;
    Normal      m_Normal;
    Color       m_Specular;
    Parameter   m_SpecularPower;
    Parameter   m_AmbientOcclusion;
    Parameter   m_Opacity;
    Color       m_Transparency;
    Color       m_Emission;

public:
    SceneObjectMaterial()
        : BaseSceneObject(SceneObjectType::Material),
          m_Name(""),
          m_AmbientColor(vec4f(0.0f)),
          m_DiffuseColor(vec4f(1.0f)),
          m_Metallic(0.0f),
          m_Roughness(0.0f),
          m_Normal(vec3f(0.0f, 0.0f, 1.0f)),
          m_Specular(vec4f(0.0f)),
          m_SpecularPower(1.0f),
          m_AmbientOcclusion(1.0f),
          m_Opacity(1.0f),
          m_Transparency(vec4f(0.0f)),
          m_Emission(vec4f(0.0f)){};
    SceneObjectMaterial(std::string name) : m_Name(std::move(name)){};

    const std::string&   GetName() const;
    const Color&         GetAmbientColor() const;
    const Color&         GetDiffuseColor() const;
    const Color&         GetSpecularColor() const;
    const Color&         GetEmission() const;
    const Parameter&     GetSpecularPower() const;
    void                 SetName(const std::string& name);
    void                 SetName(std::string&& name);
    void                 SetColor(std::string_view attrib, const vec4f& color);
    void                 SetParam(std::string_view attrib, const float param);
    void                 SetTexture(std::string_view attrib, const std::shared_ptr<SceneObjectTexture>& texture);
    void                 LoadTextures();
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectMaterial& obj);
};

class SceneObjectVertexArray : public BaseSceneObject {
public:
    SceneObjectVertexArray(
        std::string_view attr,
        VertexDataType   data_type,
        Core::Buffer&&   buffer,
        uint32_t         morph_index = 0);

    SceneObjectVertexArray(const SceneObjectVertexArray&) = default;
    SceneObjectVertexArray(SceneObjectVertexArray&&)      = default;
    SceneObjectVertexArray& operator=(const SceneObjectVertexArray&) = default;
    SceneObjectVertexArray& operator=(SceneObjectVertexArray&&) = default;
    ~SceneObjectVertexArray() override                          = default;

    const std::string&   GetAttributeName() const;
    VertexDataType       GetDataType() const;
    size_t               GetDataSize() const;
    const uint8_t*       GetData() const;
    size_t               GetVertexCount() const;
    size_t               GetVertexSize() const;
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectVertexArray& obj);

private:
    std::string    m_Attribute;
    VertexDataType m_DataType;
    size_t         m_VertexCount;
    Core::Buffer   m_Data;

    uint32_t m_MorphTargetIndex;
};

class SceneObjectIndexArray : public BaseSceneObject {
public:
    SceneObjectIndexArray() = default;
    SceneObjectIndexArray(
        const IndexDataType data_type,
        Core::Buffer&&      data,
        const uint32_t      restart_index = 0);

    SceneObjectIndexArray(const SceneObjectIndexArray&) = default;
    SceneObjectIndexArray(SceneObjectIndexArray&&)      = default;
    SceneObjectIndexArray& operator=(const SceneObjectIndexArray&) = default;
    SceneObjectIndexArray& operator=(SceneObjectIndexArray&&) = default;
    ~SceneObjectIndexArray() override                         = default;

    const IndexDataType  GetIndexType() const;
    const uint8_t*       GetData() const;
    size_t               GetIndexCount() const;
    size_t               GetDataSize() const;
    size_t               GetIndexSize() const;
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectIndexArray& obj);

private:
    IndexDataType m_DataType;
    size_t        m_IndexCount = 0;
    Core::Buffer  m_Data;

    size_t m_ResetartIndex = 0;
};

class SceneObjectMesh : public BaseSceneObject {
protected:
    // Different vector elements correspond to different attributes of
    // vertices, such as the first element is coorespond to the position of
    // vertices, the second element is coorrespond to the normal of
    // vertices, etc.
    std::vector<SceneObjectVertexArray> m_VertexArray;
    SceneObjectIndexArray               m_IndexArray;
    std::weak_ptr<SceneObjectMaterial>  m_Material;
    PrimitiveType                       m_PrimitiveType;

    bool m_Visible    = true;
    bool m_Shadow     = false;
    bool m_MotionBlur = false;

public:
    SceneObjectMesh(bool visible = true, bool shadow = true, bool motion_blur = true)
        : BaseSceneObject(SceneObjectType::Mesh) {}
    SceneObjectMesh(SceneObjectMesh&& mesh)
        : BaseSceneObject(SceneObjectType::Mesh),
          m_IndexArray(std::move(mesh.m_IndexArray)),
          m_VertexArray(std::move(mesh.m_VertexArray)),
          m_PrimitiveType(mesh.m_PrimitiveType) {}

    // Set some things
    void AddIndexArray(SceneObjectIndexArray&& array);
    void AddVertexArray(SceneObjectVertexArray&& array);
    void SetPrimitiveType(PrimitiveType type);
    void SetMaterial(const std::weak_ptr<SceneObjectMaterial>& material);

    // Get some things
    const SceneObjectVertexArray&              GetVertexByName(std::string_view name) const;
    size_t                                     GetVertexArraysCount() const;
    const std::vector<SceneObjectVertexArray>& GetVertexArrays() const;
    const SceneObjectIndexArray&               GetIndexArray() const;
    const PrimitiveType&                       GetPrimitiveType() const;
    std::weak_ptr<SceneObjectMaterial>         GetMaterial() const;

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectMesh& obj);
};

class SceneObjectGeometry : public BaseSceneObject {
protected:
    // LOD | meshes array
    //  0  | meshes array[0] include multiple meshes at 0 LOD
    // ... | ...
    std::vector<std::vector<std::unique_ptr<SceneObjectMesh>>> m_MeshesLOD;

    bool                     m_Visible    = true;
    bool                     m_Shadow     = false;
    bool                     m_MotionBlur = false;
    SceneObjectCollisionType m_CollisionType{SceneObjectCollisionType::None};
    std::array<float, 10>    m_CollisionParameters{};

public:
    SceneObjectGeometry()
        : BaseSceneObject(SceneObjectType::Geometry) {}
    void                                                 SetVisibility(bool visible);
    void                                                 SetIfCastShadow(bool shadow);
    void                                                 SetIfMotionBlur(bool motion_blur);
    void                                                 SetCollisionType(SceneObjectCollisionType collision_type);
    void                                                 SetCollisionParameters(const float* param, int32_t count);
    const bool                                           Visible() const;
    const bool                                           CastShadow() const;
    const bool                                           MotionBlur() const;
    const SceneObjectCollisionType                       CollisionType() const;
    const float*                                         CollisionParameters() const;
    void                                                 AddMesh(std::unique_ptr<SceneObjectMesh> mesh, size_t level = 0);
    const std::vector<std::unique_ptr<SceneObjectMesh>>& GetMeshes(size_t lod = 0) const;
    friend std::ostream&                                 operator<<(std::ostream& out, const SceneObjectGeometry& obj);
};

using AttenFunc = std::function<float(float, float)>;

float default_atten_func(float intensity, float distance);

class SceneObjectLight : public BaseSceneObject {
protected:
    SceneObjectLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f)
        : BaseSceneObject(SceneObjectType::Light),
          m_LightColor(color),
          m_Intensity(intensity),
          m_LightAttenuation(default_atten_func){};

    Color       m_LightColor;
    float       m_Intensity;
    AttenFunc   m_LightAttenuation;
    bool        m_CastShadows{false};
    std::string m_TextureRef;

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectLight& obj);

public:
    void         SetIfCastShadow(bool shadow);
    void         SetColor(std::string_view attrib, const vec4f& color);
    void         SetParam(std::string_view attrib, float param);
    void         SetTexture(std::string_view attrib, std::string_view texture_name);
    void         SetAttenuation(AttenFunc func);
    const Color& GetColor();
    float        GetIntensity();
};

class SceneObjectPointLight : public SceneObjectLight {
public:
    SceneObjectPointLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f)
        : SceneObjectLight(color, intensity) {}

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectPointLight& obj);
};

class SceneObjectSpotLight : public SceneObjectLight {
protected:
    vec3f m_Direction{};
    float m_InnerConeAngle;
    float m_OuterConeAngle;

public:
    SceneObjectSpotLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f,
                         const vec3f& direction = vec3f(0.0f), float inner_cone_angle = std::numbers::pi / 3.0f,
                         float outer_cone_angle = std::numbers::pi / 4.0f)
        : SceneObjectLight(color, intensity), m_InnerConeAngle(inner_cone_angle), m_OuterConeAngle(outer_cone_angle) {}

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectSpotLight& obj);
};

class SceneObjectInfiniteLight : public SceneObjectLight {
public:
    using SceneObjectLight::SceneObjectLight;
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectInfiniteLight& obj);
};

class SceneObjectCamera : public BaseSceneObject {
public:
    SceneObjectCamera(float aspect = 16.0f / 9.0f, float near_clip = 1.0f, float far_clip = 100.0f, float fov = std::numbers::pi / 4)
        : BaseSceneObject(SceneObjectType::Camera),
          m_Aspect(aspect),
          m_NearClipDistance(near_clip),
          m_FarClipDistance(far_clip),
          m_Fov(fov) {}

    void  SetColor(std::string_view attrib, const vec4f& color);
    void  SetParam(std::string_view attrib, float param);
    void  SetTexture(std::string_view attrib, std::string_view texture_name);
    float GetAspect() const;
    float GetNearClipDistance() const;
    float GetFarClipDistance() const;
    float GetFov() const;

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectCamera& obj);

protected:
    float m_Aspect;
    float m_NearClipDistance;
    float m_FarClipDistance;
    float m_Fov;
};

class SceneObjectTransform {
protected:
    mat4f m_Transform;
    bool  m_SceneObjectOnly{false};

public:
    SceneObjectTransform() : m_Transform(1.0f) {}
    SceneObjectTransform(const mat4f matrix, const bool object_only = false)
        : m_Transform(matrix), m_SceneObjectOnly(object_only) {}

    operator mat4f() { return m_Transform; }
    operator const mat4f() const { return m_Transform; }

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectTransform& obj);
};

class SceneObjectTranslation : public SceneObjectTransform {
public:
    SceneObjectTranslation(const char axis, const float amount);
    SceneObjectTranslation(const float x, const float y, const float z);
};

class SceneObjectRotation : public SceneObjectTransform {
public:
    SceneObjectRotation(const char axis, const float theta);
    SceneObjectRotation(vec3f& axis, const float theta);
    SceneObjectRotation(quatf quaternion);
};

class SceneObjectScale : public SceneObjectTransform {
public:
    SceneObjectScale(const char axis, const float amount);
    SceneObjectScale(const float x, const float y, const float z);
};

}  // namespace Hitagi::Asset
