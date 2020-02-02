#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <crossguid/guid.hpp>
#include "portable.hpp"
#include "geommath.hpp"
#include "Image.hpp"

namespace My {
enum struct SceneObjectType : int32_t {
    kMESH          = "MESH"_i32,
    kMATERIAL      = "MATL"_i32,
    kTEXTURE       = "TXTU"_i32,
    kLIGHT         = "LGHT"_i32,
    kCAMERA        = "CAMR"_i32,
    kANIMATOR      = "ANIM"_i32,
    kCLIP          = "CLIP"_i32,
    kGEOMETRY      = "GEOM"_i32,
    kINDEX_ARRAY   = "VARR"_i32,
    kVERETEX_ARRAY = "VARR"_i32,
};

enum struct SceneObjectCollisionType : int32_t {
    kNONE         = "CNON"_i32,
    kSPHERE       = "CSPH"_i32,
    kBOX          = "CBOX"_i32,
    kCYLINDER     = "CCYL"_i32,
    kCAPSULE      = "CCAP"_i32,
    kCONE         = "CCON"_i32,
    kMULTI_SPHERE = "CMUL"_i32,
    kCONVEX_HULL  = "CCVH"_i32,
    kCONVEX_MESH  = "CCVM"_i32,
    kBVH_MESH     = "CBVM"_i32,
    kHEIGHTFIELD  = "CHIG"_i32,
    kPLANE        = "CPLN"_i32,
};

enum struct VertexDataType : int32_t {
    kFLOAT1  = "FLT1"_i32,
    kFLOAT2  = "FLT2"_i32,
    kFLOAT3  = "FLT3"_i32,
    kFLOAT4  = "FLT4"_i32,
    kDOUBLE1 = "DUB1"_i32,
    kDOUBLE2 = "DUB2"_i32,
    kDOUBLE3 = "DUB3"_i32,
    kDOUBLE4 = "DUB4"_i32,
};

enum struct IndexDataType : int32_t {
    kINT8  = "I8  "_i32,
    kINT16 = "I16 "_i32,
    kINT32 = "I32 "_i32,
    kINT64 = "I64 "_i32,
};

enum struct PrimitiveType : int32_t {
    // clang-format off
    kNone                 = "NONE"_i32,  // No particular primitive type.
    kPOINT_LIST           = "PLST"_i32,  // For N>=0, vertex N renders a point.
    kLINE_LIST            = "LLST"_i32,  // For N>=0, vertices [N*2+0, N*2+1] render a line.
    kLINE_STRIP           = "LSTR"_i32,  // For N>=0, vertices [N, N+1] render a line.
    kTRI_LIST             = "TLST"_i32,  // For N>=0, vertices [N*3+0, N*3+1, N*3+2] render a triangle.
    kTRI_FAN              = "TFAN"_i32,  // For N>=0, vertices [0, (N+1)%M, (N+2)%M] render a triangle, where M is the vertex count.
    kTRI_STRIP            = "TSTR"_i32,  // For N>=0, vertices [N*2+0, N*2+1, N*2+2] and [N*2+2, N*2+1, N*2+3] render triangles.
    kPATCH                = "PACH"_i32,  // Used for tessellation.
    kLINE_LIST_ADJACENCY  = "LLSA"_i32,  // For N>=0, vertices [N*4..N*4+3] render a line from [1, 2]. Lines [0, 1] and [2, 3] are adjacent to the rendered line.
    kLINE_STRIP_ADJACENCY = "LSTA"_i32,  // For N>=0, vertices [N+1, N+2] render a line. Lines [N, N+1] and [N+2, N+3] are adjacent to the rendered line.
    kTRI_LIST_ADJACENCY   = "TLSA"_i32,  // For N>=0, vertices [N*6..N*6+5] render a triangle from [0, 2, 4]. Triangles [0, 1, 2] [4, 2, 3] and [5, 0, 4] are adjacent to the rendered triangle.
    kTRI_STRIP_ADJACENCY  = "TSTA"_i32,  // For N>=0, vertices [N*4..N*4+6] render a triangle from [0, 2, 4] and [4, 2, 6]. Odd vertices Nodd form adjacent triangles with indices min(Nodd+1,Nlast) and max(Nodd-3,Nfirst).
    kRECT_LIST            = "RLST"_i32,  // For N>=0, vertices [N*3+0, N*3+1, N*3+2] render a screen-aligned rectangle. 0 is upper-left, 1 is upper-right, and 2 is the lower-left corner.
    kLINE_LOOP            = "LLOP"_i32,  // Like <c>kPrimitiveTypeLineStrip</c>, but the first and last vertices also render a line.
    kQUAD_LIST            = "QLST"_i32,  // For N>=0, vertices [N*4+0, N*4+1, N*4+2] and [N*4+0, N*4+2, N*4+3] render triangles.
    kQUAD_STRIP           = "QSTR"_i32,  // For N>=0, vertices [N*2+0, N*2+1, N*2+3] and [N*2+0, N*2+3, N*2+2] render triangles.
    kPOLYGON              = "POLY"_i32,  // For N>=0, vertices [0, N+1, N+2] render a triangle.
    // clang-format on
};

std::ostream& operator<<(std::ostream& out, const SceneObjectType& type);
std::ostream& operator<<(std::ostream& out, const VertexDataType& type);
std::ostream& operator<<(std::ostream& out, const IndexDataType& type);
std::ostream& operator<<(std::ostream& out, const PrimitiveType& type);

class BaseSceneObject {
protected:
    xg::Guid        m_Guid;
    SceneObjectType m_Type;
    BaseSceneObject(SceneObjectType type);
    BaseSceneObject(xg::Guid& guid, SceneObjectType type);
    BaseSceneObject(xg::Guid&& guid, SceneObjectType type);
    BaseSceneObject(BaseSceneObject&& obj);
    BaseSceneObject& operator=(BaseSceneObject&& obj);
    virtual ~BaseSceneObject() {}

    BaseSceneObject()                     = default;
    BaseSceneObject(BaseSceneObject& obj) = default;
    BaseSceneObject& operator=(BaseSceneObject& obj) = default;

public:
    const xg::Guid&       GetGuid() const { return m_Guid; }
    const SceneObjectType GetType() const { return m_Type; }

    friend std::ostream& operator<<(std::ostream&          out,
                                    const BaseSceneObject& obj);
};

class SceneObjectVertexArray {
protected:
    const std::string    m_strAttribute;
    const uint32_t       m_MorphTargetIndex;
    const VertexDataType m_DataType;
    const void*          m_pData;
    const size_t         m_szData;

public:
    SceneObjectVertexArray(
        std::string_view attr = "", uint32_t morph_index = 0,
        const VertexDataType data_type = VertexDataType::kFLOAT3,
        const void* data = nullptr, size_t data_size = 0)
        : m_strAttribute(attr),
          m_MorphTargetIndex(morph_index),
          m_DataType(data_type),
          m_pData(data),
          m_szData(data_size) {}
    SceneObjectVertexArray(SceneObjectVertexArray& arr)  = default;
    SceneObjectVertexArray(SceneObjectVertexArray&& arr) = default;

    const std::string&   GetAttributeName() const;
    VertexDataType       GetDataType() const;
    size_t               GetDataSize() const;
    const void*          GetData() const;
    size_t               GetVertexCount() const;
    friend std::ostream& operator<<(std::ostream&                 out,
                                    const SceneObjectVertexArray& obj);
};

class SceneObjectIndexArray {
protected:
    const uint32_t      m_nMaterialIndex;
    const size_t        m_szResetartIndex;
    const IndexDataType m_DataType;
    const void*         m_pData;
    const size_t        m_szData;

public:
    SceneObjectIndexArray(const uint32_t      material_index = 0,
                          const uint32_t      restart_index  = 0,
                          const IndexDataType data_type = IndexDataType::kINT16,
                          const void*         data      = nullptr,
                          const size_t        data_size = 0)
        : m_nMaterialIndex(material_index),
          m_szResetartIndex(restart_index),
          m_DataType(data_type),
          m_pData(data),
          m_szData(data_size) {}
    SceneObjectIndexArray(SceneObjectIndexArray&)  = default;
    SceneObjectIndexArray(SceneObjectIndexArray&&) = default;

    const uint32_t       GetMaterialIndex() const;
    const IndexDataType  GetIndexType() const;
    const void*          GetData() const;
    size_t               GetIndexCount() const;
    size_t               GetDataSize() const;
    friend std::ostream& operator<<(std::ostream&                out,
                                    const SceneObjectIndexArray& obj);
};

struct BoundingBox {
    vec3 centroid;
    vec3 extent;
};

class SceneObjectMesh : public BaseSceneObject {
protected:
    std::vector<SceneObjectIndexArray> m_IndexArray;
    // Different vector elements correspond to different attributes of
    // vertices, such as the first element is coorespond to the position of
    // vertices, the second element is coorrespond to the normal of
    // vertices, etc.
    std::vector<SceneObjectVertexArray> m_VertexArray;

    PrimitiveType m_PrimitiveType;

    bool m_bVisible;
    bool m_bShadow;
    bool m_bMotionBlur;

public:
    SceneObjectMesh(bool visible = true, bool shadow = true,
                    bool motion_blur = true)
        : BaseSceneObject(SceneObjectType::kMESH) {}
    SceneObjectMesh(SceneObjectMesh&& mesh)
        : BaseSceneObject(SceneObjectType::kMESH),
          m_IndexArray(std::move(mesh.m_IndexArray)),
          m_VertexArray(std::move(mesh.m_VertexArray)),
          m_PrimitiveType(mesh.m_PrimitiveType) {}

    // Set some things
    void AddIndexArray(SceneObjectIndexArray&& array);
    void AddVertexArray(SceneObjectVertexArray&& array);
    void SetPrimitiveType(PrimitiveType type);

    // Get some things
    size_t                        GetIndexGroupCount() const;
    size_t                        GetIndexCount(const size_t index) const;
    size_t                        GetVertexCount() const;
    size_t                        GetVertexPropertiesCount() const;
    const SceneObjectVertexArray& GetVertexPropertyArray(
        const size_t index) const;
    const SceneObjectIndexArray& GetIndexArray(const size_t index) const;
    const PrimitiveType&         GetPrimitiveType();
    BoundingBox                  GetBoundingBox() const;
    friend std::ostream&         operator<<(std::ostream&          out,
                                    const SceneObjectMesh& obj);
};

class SceneObjectTexture : public BaseSceneObject {
protected:
    uint32_t               m_nTexCoordIndex;
    std::string            m_Name;
    std::shared_ptr<Image> m_pImage;
    std::vector<mat4>      m_Transforms;

public:
    SceneObjectTexture()
        : BaseSceneObject(SceneObjectType::kTEXTURE), m_nTexCoordIndex(0) {}
    SceneObjectTexture(std::string_view name)
        : BaseSceneObject(SceneObjectType::kTEXTURE),
          m_nTexCoordIndex(0),
          m_Name(name) {}
    SceneObjectTexture(uint32_t coord_index, std::shared_ptr<Image>& image)
        : BaseSceneObject(SceneObjectType::kTEXTURE),
          m_nTexCoordIndex(coord_index),
          m_pImage(image) {}
    SceneObjectTexture(uint32_t coord_index, std::shared_ptr<Image>&& image)
        : BaseSceneObject(SceneObjectType::kTEXTURE),
          m_nTexCoordIndex(coord_index),
          m_pImage(std::move(image)) {}
    SceneObjectTexture(SceneObjectTexture&)  = default;
    SceneObjectTexture(SceneObjectTexture&&) = default;

    void                 AddTransform(mat4& matrix);
    void                 SetName(const std::string& name);
    void                 SetName(std::string&& name);
    void                 LoadTexture();
    const std::string&   GetName() const;
    const Image&         GetTextureImage();
    friend std::ostream& operator<<(std::ostream&             out,
                                    const SceneObjectTexture& obj);
};

template <typename T>
struct ParameterValueMap {
    T                                   Value;
    std::shared_ptr<SceneObjectTexture> ValueMap;

    ParameterValueMap() = default;
    ParameterValueMap(const T value) : Value(value) {}
    ParameterValueMap(const std::shared_ptr<SceneObjectTexture>& value)
        : ValueMap(value) {}
    ParameterValueMap(const ParameterValueMap& rhs) = default;
    ParameterValueMap(ParameterValueMap&& rhs)      = default;
    ParameterValueMap& operator=(const ParameterValueMap& rhs) = default;
    ParameterValueMap& operator=(ParameterValueMap&& rhs) = default;
    ParameterValueMap& operator                           =(
        const std::shared_ptr<SceneObjectTexture>& rhs) {
        ValueMap = rhs;
        return *this;
    }
    ~ParameterValueMap() = default;

    friend std::ostream& operator<<(std::ostream&            out,
                                    const ParameterValueMap& obj) {
        out << "Parameter Value:" << obj.Value;
        if (obj.ValueMap) {
            out << std::endl;
            out << "Parameter Map: \n" << *(obj.ValueMap) << std::endl;
        }
        return out;
    }
};

typedef ParameterValueMap<vec4>  Color;
typedef ParameterValueMap<vec3>  Normal;
typedef ParameterValueMap<float> Parameter;

class SceneObjectMaterial : public BaseSceneObject {
protected:
    std::string m_Name;
    Color       m_BaseColor;
    Parameter   m_Metallic;
    Parameter   m_Roughness;
    Normal      m_Normal;
    Color       m_Specular;
    Parameter   m_SpecularPower;
    Parameter   m_AmbientOcclusion;
    Color       m_Opacity;
    Color       m_Transparency;
    Color       m_Emission;

public:
    SceneObjectMaterial(void)
        : BaseSceneObject(SceneObjectType::kMATERIAL),
          m_Name(""),
          m_BaseColor(vec4(1.0f)),
          m_Metallic(0.0f),
          m_Roughness(0.0f),
          m_Normal(vec3(0.0f, 0.0f, 1.0f)),
          m_Specular(0.0f),
          m_SpecularPower(1.0f),
          m_AmbientOcclusion(1.0f),
          m_Opacity(1.0f),
          m_Transparency(0.0f),
          m_Emission(0.0f){};
    SceneObjectMaterial(const std::string& name) : SceneObjectMaterial() {
        m_Name = name;
    };
    SceneObjectMaterial(std::string&& name) : SceneObjectMaterial() {
        m_Name = std::move(name);
    };

    const std::string& GetName() const;
    const Color&       GetBaseColor() const;
    const Color&       GetSpecularColor() const;
    const Parameter&   GetSpecularPower() const;
    void               SetName(const std::string& name);
    void               SetName(std::string&& name);
    void               SetColor(std::string_view attrib, const vec4& color);
    void               SetParam(std::string_view attrib, const float param);
    void SetTexture(std::string_view attrib, std::string_view textureName);
    void SetTexture(std::string_view                           attrib,
                    const std::shared_ptr<SceneObjectTexture>& texture);
    void LoadTextures();
    friend std::ostream& operator<<(std::ostream&              out,
                                    const SceneObjectMaterial& obj);
};

class SceneObjectGeometry : public BaseSceneObject {
protected:
    std::vector<std::shared_ptr<SceneObjectMesh>> m_Mesh;

    bool                     m_bVisible;
    bool                     m_bShadow;
    bool                     m_bMotionBlur;
    SceneObjectCollisionType m_CollisionType;
    float                    m_CollisionParameters[10];

public:
    SceneObjectGeometry()
        : BaseSceneObject(SceneObjectType::kGEOMETRY),
          m_CollisionType(SceneObjectCollisionType::kNONE) {}
    void       SetVisibility(bool visible);
    void       SetIfCastShadow(bool shadow);
    void       SetIfMotionBlur(bool motion_blur);
    void       SetCollisionType(SceneObjectCollisionType collision_type);
    void       SetCollisionParameters(const float* param, int32_t count);
    const bool Visible();
    const bool CastShadow();
    const bool MotionBlur();
    const SceneObjectCollisionType CollisionType() const;
    const float*                   CollisionParameters() const;
    void AddMesh(std::shared_ptr<SceneObjectMesh>&& mesh);
    const std::weak_ptr<SceneObjectMesh> GetMesh();
    const std::weak_ptr<SceneObjectMesh> GetMeshLOD(size_t lod);
    BoundingBox                          GetBoundingBox() const;
    friend std::ostream&                 operator<<(std::ostream&              out,
                                    const SceneObjectGeometry& obj);
};

typedef float (*AttenFunc)(float /* Intensity */, float /* distance */);

float DefaultAttenFunc(float intensity, float distance);

class SceneObjectLight : public BaseSceneObject {
protected:
    Color       m_LightColor;
    float       m_fIntensity;
    AttenFunc   m_LightAttenuation;
    bool        m_bCastShadows;
    std::string m_strTexture;

    SceneObjectLight(void)
        : BaseSceneObject(SceneObjectType::kLIGHT),
          m_LightColor(vec4(1.0f)),
          m_fIntensity(100.0f),
          m_LightAttenuation(DefaultAttenFunc),
          m_bCastShadows(false){};

    friend std::ostream& operator<<(std::ostream&           out,
                                    const SceneObjectLight& obj);

public:
    void SetIfCastShadow(bool shadow);
    void SetColor(std::string_view attrib, const vec4& color);
    void SetParam(std::string_view attrib, float param);
    void SetTexture(std::string_view attrib, std::string_view textureName);
    void SetAttenuation(AttenFunc func);
    const Color& GetColor();
    float        GetIntensity();
};

class SceneObjectOmniLight : public SceneObjectLight {
public:
    using SceneObjectLight::SceneObjectLight;
    friend std::ostream& operator<<(std::ostream&               out,
                                    const SceneObjectOmniLight& obj);
};

class SceneObjectSpotLight : public SceneObjectLight {
protected:
    float m_fConeAngle;
    float m_fPenumbraAngle;

public:
    SceneObjectSpotLight(void)
        : SceneObjectLight(),
          m_fConeAngle(PI / 4.0f),
          m_fPenumbraAngle(PI / 3.0f) {}

    friend std::ostream& operator<<(std::ostream&               out,
                                    const SceneObjectSpotLight& obj);
};

class SceneObjectInfiniteLight : public SceneObjectLight {
public:
    using SceneObjectLight::SceneObjectLight;
    friend std::ostream& operator<<(std::ostream&                   out,
                                    const SceneObjectInfiniteLight& obj);
};

class SceneObjectCamera : public BaseSceneObject {
protected:
    float m_fAspect;
    float m_fNearClipDistance;
    float m_fFarClipDistance;

    // can only be used as base class
    SceneObjectCamera(float aspect = 16.0f / 9.0f, float near_clip = 1.0f,
                      float far_clip = 100.0f)
        : BaseSceneObject(SceneObjectType::kCAMERA),
          m_fAspect(aspect),
          m_fNearClipDistance(near_clip),
          m_fFarClipDistance(far_clip) {}

    friend std::ostream& operator<<(std::ostream&            out,
                                    const SceneObjectCamera& obj);

public:
    void  SetColor(std::string_view attrib, const vec4& color);
    void  SetParam(std::string_view attrib, float param);
    void  SetTexture(std::string_view attrib, std::string_view textureName);
    float GetNearClipDistance() const;
    float GetFarClipDistance() const;
};

class SceneObjectOrthogonalCamera : public SceneObjectCamera {
public:
    using SceneObjectCamera::SceneObjectCamera;
    friend std::ostream& operator<<(std::ostream&                      out,
                                    const SceneObjectOrthogonalCamera& obj);
};

class SceneObjectPerspectiveCamera : public SceneObjectCamera {
protected:
    float m_fFov;

public:
    SceneObjectPerspectiveCamera(float fov = PI / 2.0)
        : SceneObjectCamera(), m_fFov(fov) {}

    void                 SetParam(std::string_view attrib, float param);
    float                GetFov() const;
    friend std::ostream& operator<<(std::ostream&                       out,
                                    const SceneObjectPerspectiveCamera& obj);
};

class SceneObjectTransform {
protected:
    mat4 m_matrix;
    bool m_bSceneObjectOnly;

public:
    SceneObjectTransform() : m_matrix(1.0f), m_bSceneObjectOnly(false) {}
    SceneObjectTransform(const mat4 matrix, const bool object_only = false)
        : m_matrix(matrix), m_bSceneObjectOnly(object_only) {}

    operator mat4() { return m_matrix; }
    operator const mat4() const { return m_matrix; }

    friend std::ostream& operator<<(std::ostream&               out,
                                    const SceneObjectTransform& obj);
};

class SceneObjectTranslation : public SceneObjectTransform {
public:
    SceneObjectTranslation(const char axis, const float amount);
    SceneObjectTranslation(const float x, const float y, const float z);
};

class SceneObjectRotation : public SceneObjectTransform {
public:
    SceneObjectRotation(const char axis, const float theta);
    SceneObjectRotation(vec3& axis, const float theta);
    SceneObjectRotation(quat quaternion);
};

class SceneObjectScale : public SceneObjectTransform {
public:
    SceneObjectScale(const char axis, const float amount);
    SceneObjectScale(const float x, const float y, const float z);
};

}  // namespace My
