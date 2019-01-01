#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <type_traits>

#include "geommath.hpp"

#include "Guid.hpp"
#include "Image.hpp"
#include "portable.hpp"
#include "AssetLoader.hpp"
#include "JPEG.hpp"

namespace My {
enum SceneObjectType {
    SCENE_OBJECT_TYPE_MESH          = "MESH"_i32,
    SCENE_OBJECT_TYPE_MATERIAL      = "MATL"_i32,
    SCENE_OBJECT_TYPE_TEXTURE       = "TXTU"_i32,
    SCENE_OBJECT_TYPE_LIGHT         = "LGHT"_i32,
    SCENE_OBJECT_TYPE_CAMERA        = "CAMR"_i32,
    SCENE_OBJECT_TYPE_ANIMATOR      = "ANIM"_i32,
    SCENE_OBJECT_TYPE_CLIP          = "CLIP"_i32,
    SCENE_OBJECT_TYPE_GEOMETRY      = "GEOM"_i32,
    SCENE_OBJECT_TYPE_INDEX_ARRAY   = "VARR"_i32,
    SCENE_OBJECT_TYPE_VERETEX_ARRAY = "VARR"_i32,
};

std::ostream& operator<<(std::ostream& out, SceneObjectType type);

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
    virtual ~BaseSceneObject() {}

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

std::ostream& operator<<(std::ostream& out, VertexDataType type);

class SceneObjectVertexArray {
protected:
    const std::string    m_strAttribute;
    const uint32_t       m_MorphTargetIndex;
    const VertexDataType m_DataType;
    const void*          m_pData;
    const size_t         m_szData;

public:
    SceneObjectVertexArray(const char* attr = "", uint32_t morph_index = 0,
                           const VertexDataType data_type =
                               VertexDataType::VERTEX_DATA_TYPE_FLOAT3,
                           const void* data = nullptr, size_t data_size = 0)
        : m_strAttribute(attr),
          m_MorphTargetIndex(morph_index),
          m_DataType(data_type),
          m_pData(data),
          m_szData(data_size) {}
    SceneObjectVertexArray(SceneObjectVertexArray& arr)  = default;
    SceneObjectVertexArray(SceneObjectVertexArray&& arr) = default;

    const std::string& GetAttributeName() const { return m_strAttribute; };
    VertexDataType     GetDataType() const { return m_DataType; }
    size_t             GetDataSize() const {
        size_t size = m_szData;

        switch (m_DataType) {
            case VertexDataType::VERTEX_DATA_TYPE_FLOAT1:
            case VertexDataType::VERTEX_DATA_TYPE_FLOAT2:
            case VertexDataType::VERTEX_DATA_TYPE_FLOAT3:
            case VertexDataType::VERTEX_DATA_TYPE_FLOAT4:
                size *= sizeof(float);
                break;
            case VertexDataType::VERTEX_DATA_TYPE_DOUBLE1:
            case VertexDataType::VERTEX_DATA_TYPE_DOUBLE2:
            case VertexDataType::VERTEX_DATA_TYPE_DOUBLE3:
            case VertexDataType::VERTEX_DATA_TYPE_DOUBLE4:
                size *= sizeof(double);
            default:
                size = 0;
                break;
        }
        return size;
    }
    const void* GetData() const { return m_pData; }
    size_t      GetVertexCount() const {
        size_t size = m_szData;

        switch (m_DataType) {
            case VERTEX_DATA_TYPE_FLOAT1:
            case VERTEX_DATA_TYPE_DOUBLE1:
                size /= 1;
                break;
            case VERTEX_DATA_TYPE_FLOAT2:
            case VERTEX_DATA_TYPE_DOUBLE2:
                size /= 2;
                break;
            case VERTEX_DATA_TYPE_FLOAT3:
            case VERTEX_DATA_TYPE_DOUBLE3:
                size /= 3;
                break;
            case VERTEX_DATA_TYPE_FLOAT4:
            case VERTEX_DATA_TYPE_DOUBLE4:
                size /= 4;
                break;
            default:
                size = 0;
                break;
        }
        return size;
    }

    friend std::ostream& operator<<(std::ostream&                 out,
                                    const SceneObjectVertexArray& obj);
};

enum IndexDataType {
    INDEX_DATA_TYPE_INT8  = "I8  "_i32,
    INDEX_DATA_TYPE_INT16 = "I16 "_i32,
    INDEX_DATA_TYPE_INT32 = "I32 "_i32,
    INDEX_DATA_TYPE_INT64 = "I64 "_i32,
};

std::ostream& operator<<(std::ostream& out, const IndexDataType& type);

class SceneObjectIndexArray {
protected:
    const uint32_t      m_nMaterialIndex;
    const size_t        m_szResetartIndex;
    const IndexDataType m_DataType;
    const void*         m_pData;
    const size_t        m_szData;

public:
    SceneObjectIndexArray(
        const uint32_t material_index = 0, const uint32_t restart_index = 0,
        const IndexDataType data_type = IndexDataType::INDEX_DATA_TYPE_INT16,
        const void* data = nullptr, const size_t data_size = 0)
        : m_nMaterialIndex(material_index),
          m_szResetartIndex(restart_index),
          m_DataType(data_type),
          m_pData(data),
          m_szData(data_size) {}
    SceneObjectIndexArray(SceneObjectIndexArray&)  = default;
    SceneObjectIndexArray(SceneObjectIndexArray&&) = default;

    const uint32_t      GetMaterialIndex() const { return m_nMaterialIndex; }
    const IndexDataType GetIndexType() const { return m_DataType; }
    const void*         GetData() const { return m_pData; }
    size_t              GetIndexCount() const { return m_szData; }
    size_t              GetDataSize() const {
        size_t size = m_szData;

        switch (m_DataType) {
            case IndexDataType::INDEX_DATA_TYPE_INT8:
                size *= sizeof(int8_t);
                break;
            case IndexDataType::INDEX_DATA_TYPE_INT16:
                size *= sizeof(int16_t);
                break;
            case IndexDataType::INDEX_DATA_TYPE_INT32:
                size *= sizeof(int32_t);
                break;
            case IndexDataType::INDEX_DATA_TYPE_INT64:
                size *= sizeof(int64_t);
                break;
            default:
                size = 0;
                break;
        }
        return size;
    }

    friend std::ostream& operator<<(std::ostream&                out,
                                    const SceneObjectIndexArray& obj);
};

typedef enum _PrimitiveType : int32_t {
    // clang-format off
    PRIMITIVE_TYPE_None                 = "NONE"_i32,  ///< No particular primitive type.
    PRIMITIVE_TYPE_POINT_LIST           = "PLST"_i32,  ///< For N>=0, vertex N renders a point.
    PRIMITIVE_TYPE_LINE_LIST            = "LLST"_i32,  ///< For N>=0, vertices [N*2+0, N*2+1] render a line.
    PRIMITIVE_TYPE_LINE_STRIP           = "LSTR"_i32,  ///< For N>=0, vertices [N, N+1] render a line.
    PRIMITIVE_TYPE_TRI_LIST             = "TLST"_i32,  ///< For N>=0, vertices [N*3+0, N*3+1, N*3+2] render a triangle.
    PRIMITIVE_TYPE_TRI_FAN              = "TFAN"_i32,  ///< For N>=0, vertices [0, (N+1)%M, (N+2)%M] render a triangle, where M is the vertex count.
    PRIMITIVE_TYPE_TRI_STRIP            = "TSTR"_i32,  ///< For N>=0, vertices [N*2+0, N*2+1, N*2+2] and [N*2+2, N*2+1, N*2+3] render triangles.
    PRIMITIVE_TYPE_PATCH                = "PACH"_i32,  ///< Used for tessellation.
    PRIMITIVE_TYPE_LINE_LIST_ADJACENCY  = "LLSA"_i32,  ///< For N>=0, vertices [N*4..N*4+3] render a line from [1, 2]. Lines [0, 1] and [2, 3] are adjacent to the rendered line.
    PRIMITIVE_TYPE_LINE_STRIP_ADJACENCY = "LSTA"_i32,  ///< For N>=0, vertices [N+1, N+2] render a line. Lines [N, N+1] and [N+2, N+3] are adjacent to the rendered line.
    PRIMITIVE_TYPE_TRI_LIST_ADJACENCY   = "TLSA"_i32,  ///< For N>=0, vertices [N*6..N*6+5] render a triangle from [0, 2, 4]. Triangles [0, 1, 2] [4, 2, 3] and [5, 0, 4] are adjacent to the rendered triangle.
    PRIMITIVE_TYPE_TRI_STRIP_ADJACENCY  = "TSTA"_i32,  ///< For N>=0, vertices [N*4..N*4+6] render a triangle from [0, 2, 4] and [4, 2, 6]. Odd vertices Nodd form adjacent triangles with indices min(Nodd+1,Nlast) and max(Nodd-3,Nfirst).
    PRIMITIVE_TYPE_RECT_LIST            = "RLST"_i32,  ///< For N>=0, vertices [N*3+0, N*3+1, N*3+2] render a screen-aligned rectangle. 0 is upper-left, 1 is upper-right, and 2 is the lower-left corner.
    PRIMITIVE_TYPE_LINE_LOOP            = "LLOP"_i32,  ///< Like <c>kPrimitiveTypeLineStrip</c>, but the first and last vertices also render a line.
    PRIMITIVE_TYPE_QUAD_LIST            = "QLST"_i32,  ///< For N>=0, vertices [N*4+0, N*4+1, N*4+2] and [N*4+0, N*4+2, N*4+3] render triangles.
    PRIMITIVE_TYPE_QUAD_STRIP           = "QSTR"_i32,  ///< For N>=0, vertices [N*2+0, N*2+1, N*2+3] and [N*2+0, N*2+3, N*2+2] render triangles.
    PRIMITIVE_TYPE_POLYGON              = "POLY"_i32,  ///< For N>=0, vertices [0, N+1, N+2] render a triangle.
    // clang-format on
} PrimitiveType;

std::ostream& operator<<(std::ostream& out, const PrimitiveType& type);

class SceneObjectMesh : public BaseSceneObject {
protected:
    std::vector<SceneObjectIndexArray>  m_IndexArray;
    std::vector<SceneObjectVertexArray> m_VertexArray;

    PrimitiveType m_PrimitiveType;

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
    SceneObjectMesh(SceneObjectMesh&& mesh)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_MESH),
          m_IndexArray(std::move(mesh.m_IndexArray)),
          m_VertexArray(std::move(mesh.m_VertexArray)),
          m_bVisible(mesh.m_bVisible),
          m_bShadow(mesh.m_bShadow),
          m_bMotionBlur(mesh.m_bMotionBlur) {}

    // Set some things
    void AddIndexArray(SceneObjectIndexArray&& array) {
        m_IndexArray.push_back(std::move(array));
    }
    void AddVertexArray(SceneObjectVertexArray&& array) {
        m_VertexArray.push_back(std::move(array));
    }
    void SetPrimitiveType(PrimitiveType type) { m_PrimitiveType = type; }

    // Get some things
    size_t GetIndexGroupCount() const { return m_IndexArray.size(); }
    size_t GetIndexCount(const size_t index) const {
        return m_IndexArray.empty() ? 0 : m_IndexArray[index].GetIndexCount();
    }
    size_t GetVertexCount() const {
        return m_VertexArray.empty() ? 0 : m_VertexArray[0].GetVertexCount();
    }
    size_t GetVertexPropertiesCount() const { return m_VertexArray.size(); }
    const SceneObjectVertexArray& GetVertexPropertyArray(
        const size_t index) const {
        return m_VertexArray[index];
    }
    const SceneObjectIndexArray& GetIndexArray(const size_t index) const {
        return m_IndexArray[index];
    }
    const PrimitiveType& GetPrimitiveType() { return m_PrimitiveType; }

    friend std::ostream& operator<<(std::ostream&          out,
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
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_TEXTURE),
          m_nTexCoordIndex(0) {}
    SceneObjectTexture(const std::string& name)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_TEXTURE),
          m_nTexCoordIndex(0),
          m_Name(name) {}
    SceneObjectTexture(uint32_t coord_index, std::shared_ptr<Image>& image)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_TEXTURE),
          m_nTexCoordIndex(coord_index),
          m_pImage(image) {}
    SceneObjectTexture(uint32_t coord_index, std::shared_ptr<Image>&& image)
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_TEXTURE),
          m_nTexCoordIndex(coord_index),
          m_pImage(std::move(image)) {}
    SceneObjectTexture(SceneObjectTexture&)  = default;
    SceneObjectTexture(SceneObjectTexture&&) = default;

    void AddTransform(mat4& matrix) { m_Transforms.push_back(matrix); }
    void SetName(const std::string& name) { m_Name = name; }
    void SetName(std::string&& name) { m_Name = std::move(name); }
    void LoadTexture() {
        if (!m_pImage) {
            Buffer     buf = g_pAssetLoader->SyncOpenAndRead(m_Name);
            JfifParser jfif_parser;
            m_pImage = std::make_shared<Image>(jfif_parser.Parse(buf));
        }
    }
    const std::string& GetName() const { return m_Name; }
    const Image&       GetTextureImage() {
        if (!m_pImage) {
            Buffer     buf = g_pAssetLoader->SyncOpenAndRead(m_Name);
            JfifParser jfif_parser;
            m_pImage = std::make_shared<Image>(jfif_parser.Parse(buf));
        }

        return *m_pImage;
    }

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
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_MATERIAL),
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

    const std::string& GetName() const { return m_Name; }
    const Color&       GetBaseColor() const { return m_BaseColor; }
    const Color&       GetSpecularColor() const { return m_Specular; }
    const Parameter&   GetSpecularPower() const { return m_SpecularPower; }

    void SetName(const std::string& name) { m_Name = name; }
    void SetName(std::string&& name) { m_Name = std::move(name); }
    void SetColor(const std::string& attrib, const vec4& color) {
        if (attrib == "diffuse") m_BaseColor = Color(color);
        if (attrib == "specular") m_Specular = Color(color);
        if (attrib == "emission") m_Emission = Color(color);
        if (attrib == "opacity") m_Opacity = Color(color);
        if (attrib == "transparency") m_Transparency = Color(color);
    }
    void SetParam(const std::string& attrib, const float param) {
        if (attrib == "specular_power") m_SpecularPower = Parameter(param);
    }
    void SetTexture(const std::string& attrib, const std::string& textureName) {
        if (attrib == "diffuse")
            m_BaseColor = std::make_shared<SceneObjectTexture>(textureName);
        if (attrib == "specular")
            m_Specular = std::make_shared<SceneObjectTexture>(textureName);
        if (attrib == "specular_power")
            m_SpecularPower = std::make_shared<SceneObjectTexture>(textureName);
        if (attrib == "emission")
            m_Emission = std::make_shared<SceneObjectTexture>(textureName);
        if (attrib == "opacity")
            m_Opacity = std::make_shared<SceneObjectTexture>(textureName);
        if (attrib == "transparency")
            m_Transparency = std::make_shared<SceneObjectTexture>(textureName);
        if (attrib == "normal")
            m_Normal = std::make_shared<SceneObjectTexture>(textureName);
    }
    void SetTexture(const std::string&                         attrib,
                    const std::shared_ptr<SceneObjectTexture>& texture) {
        if (attrib == "diffuse") m_BaseColor = texture;
        if (attrib == "specular") m_Specular = texture;
        if (attrib == "specular_power") m_SpecularPower = texture;
        if (attrib == "emission") m_Emission = texture;
        if (attrib == "opacity") m_Opacity = texture;
        if (attrib == "transparency") m_Transparency = texture;
        if (attrib == "normal") m_Normal = texture;
    }

    void LoadTextures() {
        if (m_BaseColor.ValueMap) m_BaseColor.ValueMap->LoadTexture();
    }

    friend std::ostream& operator<<(std::ostream&              out,
                                    const SceneObjectMaterial& obj);
};

class SceneObjectGeometry : public BaseSceneObject {
protected:
    std::vector<std::shared_ptr<SceneObjectMesh>> m_Mesh;

    bool m_bVisible;
    bool m_bShadow;
    bool m_bMotionBlur;

public:
    SceneObjectGeometry()
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_GEOMETRY) {}
    void SetVisibility(bool visible) { m_bVisible = visible; }
    void SetIfCastShadow(bool shadow) { m_bShadow = shadow; }
    void SetIfMotionBlur(bool motion_blur) { m_bMotionBlur = motion_blur; }
    const bool Visible() { return m_bVisible; }
    const bool CastShadow() { return m_bShadow; }
    const bool MotionBlur() { return m_bMotionBlur; }

    void AddMesh(std::shared_ptr<SceneObjectMesh>& mesh) {
        m_Mesh.push_back(std::move(mesh));
    }
    const std::weak_ptr<SceneObjectMesh> GetMesh() {
        return (m_Mesh.empty() ? nullptr : m_Mesh[0]);
    }
    const std::weak_ptr<SceneObjectMesh> GetMeshLOD(size_t lod) {
        return (lod < m_Mesh.size() ? m_Mesh[lod] : nullptr);
    }

    friend std::ostream& operator<<(std::ostream&              out,
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
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_LIGHT),
          m_LightColor(vec4(1.0f)),
          m_fIntensity(100.0f),
          m_LightAttenuation(DefaultAttenFunc),
          m_bCastShadows(false){};

    friend std::ostream& operator<<(std::ostream&           out,
                                    const SceneObjectLight& obj);

public:
    void SetIfCastShadow(bool shadow) { m_bCastShadows = shadow; }
    void SetColor(std::string& attrib, vec4& color) {
        if (attrib == "light") {
            m_LightColor = Color(color);
        }
    }
    void SetParam(std::string& attrib, float param) {
        if (attrib == "intensity") {
            m_fIntensity = param;
        }
    }
    void SetTexture(std::string& attrib, std::string& textureName) {
        if (attrib == "projection") {
            m_strTexture = textureName;
        }
    }
    void         SetAttenuation(AttenFunc func) { m_LightAttenuation = func; }
    const Color& GetColor() { return m_LightColor; }
    float        GetIntensity() { return m_fIntensity; }
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
        : BaseSceneObject(SceneObjectType::SCENE_OBJECT_TYPE_CAMERA),
          m_fAspect(aspect),
          m_fNearClipDistance(near_clip),
          m_fFarClipDistance(far_clip) {}

    friend std::ostream& operator<<(std::ostream&            out,
                                    const SceneObjectCamera& obj);

public:
    void SetColor(std::string& attrib, vec4& color) {
        // TODO: extension
    }
    void SetParam(std::string& attrib, float param) {
        if (attrib == "near")
            m_fNearClipDistance = param;
        else if (attrib == "far")
            m_fFarClipDistance = param;
    }
    void SetTexture(std::string& attrib, std::string& textureName) {
        // TODO: extension
    }

    float GetNearClipDistance() const { return m_fNearClipDistance; }
    float GetFarClipDistance() const { return m_fFarClipDistance; }
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

    void SetParam(std::string& attrib, float param) {
        // TODO: handle fovs, fovy
        if (attrib == "fov") {
            m_fFov = param;
        }
        SceneObjectCamera::SetParam(attrib, param);
    }
    float GetFov() const { return m_fFov; }

    friend std::ostream& operator<<(std::ostream&                       out,
                                    const SceneObjectPerspectiveCamera& obj);
};

class SceneObjectTransform {
protected:
    mat4 m_matrix;
    bool m_bSceneObjectOnly;

public:
    SceneObjectTransform() {
        m_matrix           = mat4(1.0f);
        m_bSceneObjectOnly = false;
    }
    SceneObjectTransform(const mat4 matrix, const bool object_only = false) {
        m_matrix           = matrix;
        m_bSceneObjectOnly = object_only;
    }

    operator mat4() { return m_matrix; }
    operator const mat4() const { return m_matrix; }

    friend std::ostream& operator<<(std::ostream&               out,
                                    const SceneObjectTransform& obj);
};

class SceneObjectTranslation : public SceneObjectTransform {
public:
    SceneObjectTranslation(const char axis, const float amount) {
        switch (axis) {
            case 'x':
                m_matrix = translate(m_matrix, vec3(amount, 0.0f, 0.0f));
                break;
            case 'y':
                m_matrix = translate(m_matrix, vec3(0.0f, amount, 0.0f));
                break;
            case 'z':
                m_matrix = translate(m_matrix, vec3(0.0f, 0.0f, amount));
            default:
                break;
        }
    }
    SceneObjectTranslation(const float x, const float y, const float z) {
        m_matrix = translate(m_matrix, vec3(x, y, z));
    }
};

class SceneObjectRotation : public SceneObjectTransform {
public:
    SceneObjectRotation(const char axis, const float theta) {
        switch (axis) {
            case 'x':
                m_matrix =
                    rotate(m_matrix, radians(theta), vec3(1.0f, 0.0f, 0.0f));
                break;
            case 'y':
                m_matrix =
                    rotate(m_matrix, radians(theta), vec3(0.0f, 1.0f, 0.0f));
                break;
            case 'z':
                m_matrix =
                    rotate(m_matrix, radians(theta), vec3(0.0f, 0.0f, 1.0f));
                break;
            default:
                break;
        }
    }
    SceneObjectRotation(vec3& axis, const float theta) {
        axis     = normalize(axis);
        m_matrix = rotate(m_matrix, theta, axis);
    }
    SceneObjectRotation(quat quaternion) {
        m_matrix = mat4(quaternion) * m_matrix;
    }
};

class SceneObjectScale : public SceneObjectTransform {
public:
    SceneObjectScale(const char axis, const float amount) {
        switch (axis) {
            case 'x':
                m_matrix = scale(m_matrix, vec3(amount, 0.0f, 0.0f));
                break;
            case 'y':
                m_matrix = scale(m_matrix, vec3(0.0f, amount, 0.0f));
                break;
            case 'z':
                m_matrix = scale(m_matrix, vec3(0.0f, 0.0f, amount));
                break;
            default:
                break;
        }
    }
    SceneObjectScale(const float x, const float y, const float z) {
        m_matrix = scale(m_matrix, vec3(x, y, z));
    }
};

}  // namespace My
