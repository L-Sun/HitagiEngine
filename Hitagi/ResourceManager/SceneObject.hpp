#pragma once
#include <crossguid/guid.hpp>

#include "HitagiMath.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

namespace Hitagi::Resource {
enum struct SceneObjectType : int32_t {
    MESH          = "MESH"_i32,
    MATERIAL      = "MATL"_i32,
    TEXTURE       = "TXTU"_i32,
    LIGHT         = "LGHT"_i32,
    CAMERA        = "CAMR"_i32,
    ANIMATOR      = "ANIM"_i32,
    CLIP          = "CLIP"_i32,
    GEOMETRY      = "GEOM"_i32,
    INDEX_ARRAY   = "VARR"_i32,
    VERETEX_ARRAY = "VARR"_i32,
};

enum struct SceneObjectCollisionType : int32_t {
    NONE         = "CNON"_i32,
    SPHERE       = "CSPH"_i32,
    BOX          = "CBOX"_i32,
    CYLINDER     = "CCYL"_i32,
    CAPSULE      = "CCAP"_i32,
    CONE         = "CCON"_i32,
    MULTI_SPHERE = "CMUL"_i32,
    CONVEX_HULL  = "CCVH"_i32,
    CONVEX_MESH  = "CCVM"_i32,
    BVH_MESH     = "CBVM"_i32,
    HEIGHTFIELD  = "CHIG"_i32,
    PLANE        = "CPLN"_i32,
};

enum struct VertexDataType : int32_t {
    FLOAT1  = "FLT1"_i32,
    FLOAT2  = "FLT2"_i32,
    FLOAT3  = "FLT3"_i32,
    FLOAT4  = "FLT4"_i32,
    DOUBLE1 = "DUB1"_i32,
    DOUBLE2 = "DUB2"_i32,
    DOUBLE3 = "DUB3"_i32,
    DOUBLE4 = "DUB4"_i32,
};

enum struct IndexDataType : int32_t {
    INT8  = "I8  "_i32,
    INT16 = "I16 "_i32,
    INT32 = "I32 "_i32,
    INT64 = "I64 "_i32,
};

enum struct PrimitiveType : int32_t {
    // clang-format off
    None                 = "NONE"_i32,  // No particular primitive type.
    POINT_LIST           = "PLST"_i32,  // For N>=0, vertex N renders a point.
    LINE_LIST            = "LLST"_i32,  // For N>=0, vertices [N*2+0, N*2+1] render a line.
    LINE_STRIP           = "LSTR"_i32,  // For N>=0, vertices [N, N+1] render a line.
    TRI_LIST             = "TLST"_i32,  // For N>=0, vertices [N*3+0, N*3+1, N*3+2] render a triangle.
    TRI_FAN              = "TFAN"_i32,  // For N>=0, vertices [0, (N+1)%M, (N+2)%M] render a triangle, where M is the vertex count.
    TRI_STRIP            = "TSTR"_i32,  // For N>=0, vertices [N*2+0, N*2+1, N*2+2] and [N*2+2, N*2+1, N*2+3] render triangles.
    PATCH                = "PACH"_i32,  // Used for tessellation.
    LINE_LIST_ADJACENCY  = "LLSA"_i32,  // For N>=0, vertices [N*4..N*4+3] render a line from [1, 2]. Lines [0, 1] and [2, 3] are adjacent to the rendered line.
    LINE_STRIP_ADJACENCY = "LSTA"_i32,  // For N>=0, vertices [N+1, N+2] render a line. Lines [N, N+1] and [N+2, N+3] are adjacent to the rendered line.
    TRI_LIST_ADJACENCY   = "TLSA"_i32,  // For N>=0, vertices [N*6..N*6+5] render a triangle from [0, 2, 4]. Triangles [0, 1, 2] [4, 2, 3] and [5, 0, 4] are adjacent to the rendered triangle.
    TRI_STRIP_ADJACENCY  = "TSTA"_i32,  // For N>=0, vertices [N*4..N*4+6] render a triangle from [0, 2, 4] and [4, 2, 6]. Odd vertices Nodd form adjacent triangles with indices min(Nodd+1,Nlast) and max(Nodd-3,Nfirst).
    RECT_LIST            = "RLST"_i32,  // For N>=0, vertices [N*3+0, N*3+1, N*3+2] render a screen-aligned rectangle. 0 is upper-left, 1 is upper-right, and 2 is the lower-left corner.
    LINE_LOOP            = "LLOP"_i32,  // Like <c>kPrimitiveTypeLineStrip</c>, but the first and last vertices also render a line.
    QUAD_LIST            = "QLST"_i32,  // For N>=0, vertices [N*4+0, N*4+1, N*4+2] and [N*4+0, N*4+2, N*4+3] render triangles.
    QUAD_STRIP           = "QSTR"_i32,  // For N>=0, vertices [N*2+0, N*2+1, N*2+3] and [N*2+0, N*2+3, N*2+2] render triangles.
    POLYGON              = "POLY"_i32,  // For N>=0, vertices [0, N+1, N+2] render a triangle.
    // clang-format on
};

class BaseSceneObject {
protected:
    xg::Guid        m_Guid;
    SceneObjectType m_Type;
    BaseSceneObject(SceneObjectType type);
    BaseSceneObject(const xg::Guid& guid, const SceneObjectType& type);
    BaseSceneObject(xg::Guid&& guid, const SceneObjectType& type);
    BaseSceneObject(BaseSceneObject&& obj);
    BaseSceneObject& operator=(BaseSceneObject&& obj);
    virtual ~BaseSceneObject() {}

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
    uint32_t               m_TexCoordIndex;
    std::string            m_Name;
    std::shared_ptr<Image> m_Image;
    std::vector<mat4f>     m_Transforms;

public:
    SceneObjectTexture() : BaseSceneObject(SceneObjectType::TEXTURE), m_TexCoordIndex(0) {}
    SceneObjectTexture(std::string_view name)
        : BaseSceneObject(SceneObjectType::TEXTURE), m_TexCoordIndex(0), m_Name(name) {}
    SceneObjectTexture(uint32_t coordIndex, std::shared_ptr<Image>& image)
        : BaseSceneObject(SceneObjectType::TEXTURE), m_TexCoordIndex(coordIndex), m_Image(image) {}
    SceneObjectTexture(uint32_t coordIndex, std::shared_ptr<Image>&& image)
        : BaseSceneObject(SceneObjectType::TEXTURE), m_TexCoordIndex(coordIndex), m_Image(std::move(image)) {}
    SceneObjectTexture(SceneObjectTexture&)  = default;
    SceneObjectTexture(SceneObjectTexture&&) = default;

    void                 AddTransform(mat4f& matrix);
    void                 SetName(const std::string& name);
    void                 SetName(std::string&& name);
    void                 LoadTexture();
    const std::string&   GetName() const;
    const Image&         GetTextureImage();
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectTexture& obj);
};

template <typename T>
struct ParameterValueMap {
    T                                   Value;
    std::shared_ptr<SceneObjectTexture> ValueMap;

    ParameterValueMap() = default;
    ParameterValueMap(const T value) : Value(value) {}
    ParameterValueMap(const std::shared_ptr<SceneObjectTexture>& value) : ValueMap(value) {}
    ParameterValueMap(const ParameterValueMap& rhs) = default;
    ParameterValueMap(ParameterValueMap&& rhs)      = default;
    ParameterValueMap& operator=(const ParameterValueMap& rhs) = default;
    ParameterValueMap& operator=(ParameterValueMap&& rhs) = default;
    ParameterValueMap& operator                           =(const std::shared_ptr<SceneObjectTexture>& rhs) {
        ValueMap = rhs;
        return *this;
    }
    ~ParameterValueMap() = default;

    friend std::ostream& operator<<(std::ostream& out, const ParameterValueMap& obj) {
        out << obj.Value;
        if (obj.ValueMap) {
            out << fmt::format("\n{:-^30}\n", "Parameter Map") << *(obj.ValueMap)
                << fmt::format("\n{:-^30}", "Parameter Map End");
        }
        return out;
    }
};

typedef ParameterValueMap<vec4f> Color;
typedef ParameterValueMap<vec3f> Normal;
typedef ParameterValueMap<float> Parameter;

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
        : BaseSceneObject(SceneObjectType::MATERIAL),
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
    SceneObjectMaterial(const std::string& name) : SceneObjectMaterial() { m_Name = name; };
    SceneObjectMaterial(std::string&& name) : SceneObjectMaterial() { m_Name = std::move(name); };

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
    void                 SetTexture(std::string_view attrib, std::string_view textureName);
    void                 SetTexture(std::string_view attrib, const std::shared_ptr<SceneObjectTexture>& texture);
    void                 LoadTextures();
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectMaterial& obj);
};

class SceneObjectVertexArray {
public:
    SceneObjectVertexArray(
        std::string_view attr        = "",
        VertexDataType   dataType    = VertexDataType::FLOAT3,
        const void*      data        = nullptr,
        size_t           vertexCount = 0,
        uint32_t         morphIndex  = 0);

    SceneObjectVertexArray(
        std::string_view attr,
        VertexDataType   dataType,
        Core::Buffer&&   buffer,
        uint32_t         morphIndex = 0);

    SceneObjectVertexArray(const SceneObjectVertexArray&) = default;
    SceneObjectVertexArray(SceneObjectVertexArray&&)      = default;
    SceneObjectVertexArray& operator=(const SceneObjectVertexArray&) = default;
    SceneObjectVertexArray& operator=(SceneObjectVertexArray&&) = default;
    ~SceneObjectVertexArray()                                   = default;

    const std::string&   GetAttributeName() const;
    VertexDataType       GetDataType() const;
    size_t               GetDataSize() const;
    const void*          GetData() const;
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

class SceneObjectIndexArray {
public:
    SceneObjectIndexArray(
        const IndexDataType dataType     = IndexDataType::INT16,
        const void*         data         = nullptr,
        const size_t        elementCount = 0,
        const uint32_t      restartIndex = 0);

    SceneObjectIndexArray(
        const IndexDataType dataType,
        Core::Buffer&&      data,
        const uint32_t      restartIndex = 0);

    SceneObjectIndexArray(const SceneObjectIndexArray&) = default;
    SceneObjectIndexArray(SceneObjectIndexArray&&)      = default;
    SceneObjectIndexArray& operator=(const SceneObjectIndexArray&) = default;
    SceneObjectIndexArray& operator=(SceneObjectIndexArray&&) = default;
    ~SceneObjectIndexArray()                                  = default;

    const IndexDataType  GetIndexType() const;
    const void*          GetData() const;
    size_t               GetIndexCount() const;
    size_t               GetDataSize() const;
    size_t               GetIndexSize() const;
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectIndexArray& obj);

private:
    IndexDataType m_DataType;
    size_t        m_IndexCount;
    Core::Buffer  m_Data;

    size_t m_ResetartIndex;
};

class SceneObjectMesh : public BaseSceneObject {
protected:
    // Different vector elements correspond to different attributes of
    // vertices, such as the first element is coorespond to the position of
    // vertices, the second element is coorrespond to the normal of
    // vertices, etc.
    std::unordered_map<std::string, SceneObjectVertexArray> m_VertexArray;
    SceneObjectIndexArray                                   m_IndexArray;
    std::weak_ptr<SceneObjectMaterial>                      m_Material;
    PrimitiveType                                           m_PrimitiveType;

    bool m_Visible;
    bool m_Shadow;
    bool m_MotionBlur;

public:
    SceneObjectMesh(bool visible = true, bool shadow = true, bool motionBlur = true)
        : BaseSceneObject(SceneObjectType::MESH) {}
    SceneObjectMesh(SceneObjectMesh&& mesh)
        : BaseSceneObject(SceneObjectType::MESH),
          m_IndexArray(std::move(mesh.m_IndexArray)),
          m_VertexArray(std::move(mesh.m_VertexArray)),
          m_PrimitiveType(mesh.m_PrimitiveType) {}

    // Set some things
    void AddIndexArray(SceneObjectIndexArray&& array);
    void AddVertexArray(SceneObjectVertexArray&& array);
    void SetPrimitiveType(PrimitiveType type);
    void SetMaterial(const std::weak_ptr<SceneObjectMaterial>& material);

    // Get some things
    size_t GetVertexCount() const;
    size_t GetVertexPropertiesCount() const;
    // TODO replace const string& with string_view in c++20
    bool                               HasProperty(const std::string& name) const;
    const SceneObjectVertexArray&      GetVertexPropertyArray(const std::string& attr) const;
    const SceneObjectIndexArray&       GetIndexArray() const;
    const PrimitiveType&               GetPrimitiveType();
    std::weak_ptr<SceneObjectMaterial> GetMaterial() const;

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectMesh& obj);
};

class SceneObjectGeometry : public BaseSceneObject {
protected:
    // LOD | meshes array
    //  0  | meshes array[0] include multiple meshes at 0 LOD
    // ... | ...
    std::vector<std::vector<std::unique_ptr<SceneObjectMesh>>> m_MeshesLOD;

    bool                     m_Visible;
    bool                     m_Shadow;
    bool                     m_MotionBlur;
    SceneObjectCollisionType m_CollisionType;
    float                    m_CollisionParameters[10];

public:
    SceneObjectGeometry()
        : BaseSceneObject(SceneObjectType::GEOMETRY), m_CollisionType(SceneObjectCollisionType::NONE) {}
    void                                                 SetVisibility(bool visible);
    void                                                 SetIfCastShadow(bool shadow);
    void                                                 SetIfMotionBlur(bool motionBlur);
    void                                                 SetCollisionType(SceneObjectCollisionType collisionType);
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

typedef float (*AttenFunc)(float /* Intensity */, float /* distance */);

float DefaultAttenFunc(float intensity, float distance);

class SceneObjectLight : public BaseSceneObject {
public:
    SceneObjectLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f)
        : BaseSceneObject(SceneObjectType::LIGHT),
          m_LightColor(color),
          m_Intensity(intensity),
          m_LightAttenuation(DefaultAttenFunc),
          m_CastShadows(false){};

protected:
    Color       m_LightColor;
    float       m_Intensity;
    AttenFunc   m_LightAttenuation;
    bool        m_CastShadows;
    std::string m_TextureRef;

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectLight& obj);

public:
    void         SetIfCastShadow(bool shadow);
    void         SetColor(std::string_view attrib, const vec4f& color);
    void         SetParam(std::string_view attrib, float param);
    void         SetTexture(std::string_view attrib, std::string_view textureName);
    void         SetAttenuation(AttenFunc func);
    const Color& GetColor();
    float        GetIntensity();
};

class SceneObjectPointLight : public SceneObjectLight {
public:
    using SceneObjectLight::SceneObjectLight;
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectPointLight& obj);
};

class SceneObjectSpotLight : public SceneObjectLight {
protected:
    vec3f m_Direction;
    float m_InnerConeAngle;
    float m_OuterConeAngle;

public:
    SceneObjectSpotLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f,
                         const vec3f& direction = vec3f(0.0f), float innerConeAngle = PI / 3.0f,
                         float outerConeAngle = PI / 4.0f)
        : SceneObjectLight(color, intensity), m_InnerConeAngle(innerConeAngle), m_OuterConeAngle(outerConeAngle) {}

    friend std::ostream& operator<<(std::ostream& out, const SceneObjectSpotLight& obj);
};

class SceneObjectInfiniteLight : public SceneObjectLight {
public:
    using SceneObjectLight::SceneObjectLight;
    friend std::ostream& operator<<(std::ostream& out, const SceneObjectInfiniteLight& obj);
};

class SceneObjectCamera : public BaseSceneObject {
public:
    SceneObjectCamera(float aspect = 16.0f / 9.0f, float nearClip = 1.0f, float farClip = 100.0f, float fov = PI / 4)
        : BaseSceneObject(SceneObjectType::CAMERA),
          m_Aspect(aspect),
          m_NearClipDistance(nearClip),
          m_FarClipDistance(farClip),
          m_Fov(fov) {}

    void  SetColor(std::string_view attrib, const vec4f& color);
    void  SetParam(std::string_view attrib, float param);
    void  SetTexture(std::string_view attrib, std::string_view textureName);
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
    bool  m_SceneObjectOnly;

public:
    SceneObjectTransform() : m_Transform(1.0f), m_SceneObjectOnly(false) {}
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

}  // namespace Hitagi::Resource
