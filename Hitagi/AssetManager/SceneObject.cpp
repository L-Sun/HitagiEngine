#include "SceneObject.hpp"
#include "MemoryManager.hpp"
#include "AssetManager.hpp"

#include <magic_enum.hpp>

namespace Hitagi::Asset {
// Class BaseSceneObject
BaseSceneObject::BaseSceneObject(SceneObjectType type) : m_Type(type) { m_Guid = xg::newGuid(); }
BaseSceneObject::BaseSceneObject(xg::Guid guid, SceneObjectType type) : m_Guid(std::move(guid)), m_Type(type) {}
BaseSceneObject::BaseSceneObject(BaseSceneObject&& obj) : m_Guid(std::move(obj.m_Guid)), m_Type(obj.m_Type) {}
BaseSceneObject& BaseSceneObject::operator=(BaseSceneObject&& obj) {
    if (this != &obj) {
        m_Guid = std::move(obj.m_Guid);
        m_Type = obj.m_Type;
    }
    return *this;
}

// Class SceneObjectVertexArray
SceneObjectVertexArray::SceneObjectVertexArray(std::string_view attr,
                                               VertexDataType   data_type,
                                               Core::Buffer&&   buffer,
                                               uint32_t         morph_index)
    : BaseSceneObject(SceneObjectType::VertexArray),
      m_Attribute(attr),
      m_DataType(data_type),
      m_VertexCount(buffer.GetDataSize() / GetVertexSize()),
      m_Data(std::move(buffer)),
      m_MorphTargetIndex(morph_index) {}

const std::string& SceneObjectVertexArray::GetAttributeName() const { return m_Attribute; }
VertexDataType     SceneObjectVertexArray::GetDataType() const { return m_DataType; }
size_t             SceneObjectVertexArray::GetDataSize() const { return m_Data.GetDataSize(); }
const uint8_t*     SceneObjectVertexArray::GetData() const { return m_Data.GetData(); }
size_t             SceneObjectVertexArray::GetVertexCount() const { return m_VertexCount; }
size_t             SceneObjectVertexArray::GetVertexSize() const {
    switch (m_DataType) {
        case VertexDataType::Float1:
            return sizeof(float) * 1;
        case VertexDataType::Float2:
            return sizeof(float) * 2;
        case VertexDataType::Float3:
            return sizeof(float) * 3;
        case VertexDataType::Float4:
            return sizeof(float) * 4;
        case VertexDataType::Double1:
            return sizeof(double) * 1;
        case VertexDataType::Double2:
            return sizeof(double) * 2;
        case VertexDataType::Double3:
            return sizeof(double) * 3;
        case VertexDataType::Double4:
            return sizeof(double) * 4;
    }
    return 0;
}

// Class SceneObjectIndexArray
SceneObjectIndexArray::SceneObjectIndexArray(
    const IndexDataType data_type,
    Core::Buffer&&      data,
    const uint32_t      restart_index)
    : BaseSceneObject(SceneObjectType::IndexArray),
      m_DataType(data_type),
      m_IndexCount(data.GetDataSize() / GetIndexSize()),
      m_Data(std::move(data)),
      m_ResetartIndex(restart_index) {}

const IndexDataType SceneObjectIndexArray::GetIndexType() const { return m_DataType; }
const uint8_t*      SceneObjectIndexArray::GetData() const { return m_Data.GetData(); }
size_t              SceneObjectIndexArray::GetIndexCount() const { return m_IndexCount; }
size_t              SceneObjectIndexArray::GetDataSize() const { return m_Data.GetDataSize(); }
size_t              SceneObjectIndexArray::GetIndexSize() const {
    switch (m_DataType) {
        case IndexDataType::Int8:
            return sizeof(int8_t);
        case IndexDataType::Int16:
            return sizeof(int16_t);
        case IndexDataType::Int32:
            return sizeof(int32_t);
        case IndexDataType::Int64:
            return sizeof(int64_t);
    }
    return 0;
}

// Class SceneObjectMesh
void SceneObjectMesh::SetIndexArray(SceneObjectIndexArray&& array) { m_IndexArray = std::move(array); }
void SceneObjectMesh::AddVertexArray(SceneObjectVertexArray&& array) { m_VertexArray.emplace_back(std::move(array)); }
void SceneObjectMesh::SetPrimitiveType(PrimitiveType type) { m_PrimitiveType = type; }
void SceneObjectMesh::SetMaterial(const std::weak_ptr<SceneObjectMaterial>& material) { m_Material = material; }

const SceneObjectVertexArray& SceneObjectMesh::GetVertexByName(std::string_view name) const {
    for (auto&& vertex : m_VertexArray)
        if (vertex.GetAttributeName() == name)
            return vertex;
    throw std::range_error(fmt::format("No name called{}", name));
}
size_t                                     SceneObjectMesh::GetVertexArraysCount() const { return m_VertexArray.size(); }
const std::vector<SceneObjectVertexArray>& SceneObjectMesh::GetVertexArrays() const { return m_VertexArray; }

const SceneObjectIndexArray&       SceneObjectMesh::GetIndexArray() const { return m_IndexArray; }
const PrimitiveType&               SceneObjectMesh::GetPrimitiveType() const { return m_PrimitiveType; }
std::weak_ptr<SceneObjectMaterial> SceneObjectMesh::GetMaterial() const { return m_Material; }

// Class SceneObjectTexture
void                   SceneObjectTexture::SetName(const std::string& name) { m_Name = name; }
void                   SceneObjectTexture::SetName(std::string&& name) { m_Name = std::move(name); }
void                   SceneObjectTexture::LoadTexture() { m_Image = g_AssetManager->ImportImage(m_TexturePath); }
const std::string&     SceneObjectTexture::GetName() const { return m_Name; }
std::shared_ptr<Image> SceneObjectTexture::GetTextureImage() const { return m_Image; }

// Class SceneObjectMaterial
const std::string& SceneObjectMaterial::GetName() const { return m_Name; }
const Color&       SceneObjectMaterial::GetAmbientColor() const { return m_AmbientColor; }
const Color&       SceneObjectMaterial::GetDiffuseColor() const { return m_DiffuseColor; }
const Color&       SceneObjectMaterial::GetSpecularColor() const { return m_Specular; }
const Parameter&   SceneObjectMaterial::GetSpecularPower() const { return m_SpecularPower; }
const Color&       SceneObjectMaterial::GetEmission() const { return m_Emission; }
void               SceneObjectMaterial::SetName(const std::string& name) { m_Name = name; }
void               SceneObjectMaterial::SetName(std::string&& name) { m_Name = std::move(name); }
void               SceneObjectMaterial::SetColor(std::string_view attrib, const vec4f& color) {
    if (attrib == "ambient") m_AmbientColor = Color(color);
    if (attrib == "diffuse") m_DiffuseColor = Color(color);
    if (attrib == "specular") m_Specular = Color(color);
    if (attrib == "emission") m_Emission = Color(color);
    if (attrib == "transparency") m_Transparency = Color(color);
}
void SceneObjectMaterial::SetParam(std::string_view attrib, const float param) {
    if (attrib == "specular_power") m_SpecularPower = Parameter(param);
    if (attrib == "opacity") m_Opacity = Parameter(param);
}
void SceneObjectMaterial::SetTexture(std::string_view attrib, const std::shared_ptr<SceneObjectTexture>& texture) {
    if (attrib == "ambient") m_DiffuseColor = texture;
    if (attrib == "diffuse") m_DiffuseColor = texture;
    if (attrib == "specular") m_Specular = texture;
    if (attrib == "specular_power") m_SpecularPower = texture;
    if (attrib == "emission") m_Emission = texture;
    if (attrib == "opacity") m_Opacity = texture;
    if (attrib == "transparency") m_Transparency = texture;
    if (attrib == "normal") m_Normal = texture;
}
void SceneObjectMaterial::LoadTextures() {
    if (m_DiffuseColor.value_map) m_DiffuseColor.value_map->LoadTexture();
}

// Class SceneObjectGeometry
void SceneObjectGeometry::SetVisibility(bool visible) { m_Visible = visible; }
void SceneObjectGeometry::SetIfCastShadow(bool shadow) { m_Shadow = shadow; }
void SceneObjectGeometry::SetIfMotionBlur(bool motion_blur) { m_MotionBlur = motion_blur; }
void SceneObjectGeometry::SetCollisionType(SceneObjectCollisionType collision_type) { m_CollisionType = collision_type; }
void SceneObjectGeometry::SetCollisionParameters(const float* param, int32_t count) {
    assert(count > 0 && count < 10);
    memcpy(m_CollisionParameters.data(), param, sizeof(float) * count);
}
const bool                     SceneObjectGeometry::Visible() const { return m_Visible; }
const bool                     SceneObjectGeometry::CastShadow() const { return m_Shadow; }
const bool                     SceneObjectGeometry::MotionBlur() const { return m_MotionBlur; }
const SceneObjectCollisionType SceneObjectGeometry::CollisionType() const { return m_CollisionType; }
const float*                   SceneObjectGeometry::CollisionParameters() const { return m_CollisionParameters.data(); }

void SceneObjectGeometry::AddMesh(std::unique_ptr<SceneObjectMesh> mesh, size_t level) {
    if (level >= m_MeshesLOD.size()) m_MeshesLOD.resize(level + 1);
    m_MeshesLOD[level].emplace_back(mesh.release());
}
const std::vector<std::unique_ptr<SceneObjectMesh>>& SceneObjectGeometry::GetMeshes(size_t lod) const {
    return m_MeshesLOD[lod];
}

// Class SceneObjectLight
void SceneObjectLight::SetIfCastShadow(bool shadow) { m_CastShadows = shadow; }
void SceneObjectLight::SetColor(std::string_view attrib, const vec4f& color) {
    if (attrib == "light") {
        m_LightColor = Color(color);
    }
}
void SceneObjectLight::SetParam(std::string_view attrib, float param) {
    if (attrib == "intensity") {
        m_Intensity = param;
    }
}
void SceneObjectLight::SetTexture(std::string_view attrib, std::string_view texture_name) {
    if (attrib == "projection") {
        m_TextureRef = texture_name;
    }
}
void         SceneObjectLight::SetAttenuation(AttenFunc func) { m_LightAttenuation = func; }
const Color& SceneObjectLight::GetColor() { return m_LightColor; }
float        SceneObjectLight::GetIntensity() { return m_Intensity; }

// Class SceneObjectOmniLight
// Class SceneObjectSpotLight
// Class SceneObjectInfiniteLight

// Class SceneObjectCamera
void SceneObjectCamera::SetColor(std::string_view attrib, const vec4f& color) {
    // TODO: extension
}
void SceneObjectCamera::SetParam(std::string_view attrib, float param) {
    if (attrib == "near")
        m_NearClipDistance = param;
    else if (attrib == "far")
        m_FarClipDistance = param;
    else if (attrib == "fov")
        m_Fov = param;
}
void SceneObjectCamera::SetTexture(std::string_view attrib, std::string_view texture_name) {
    // TODO: extension
}
float SceneObjectCamera::GetAspect() const { return m_Aspect; }
float SceneObjectCamera::GetNearClipDistance() const { return m_NearClipDistance; }
float SceneObjectCamera::GetFarClipDistance() const { return m_FarClipDistance; }
float SceneObjectCamera::GetFov() const { return m_Fov; }

// Class SceneObjectTransform
// Class SceneObjectTranslation
SceneObjectTranslation::SceneObjectTranslation(const char axis, const float amount) {
    switch (axis) {
        case 'x':
            m_Transform = translate(m_Transform, vec3f(amount, 0.0f, 0.0f));
            break;
        case 'y':
            m_Transform = translate(m_Transform, vec3f(0.0f, amount, 0.0f));
            break;
        case 'z':
            m_Transform = translate(m_Transform, vec3f(0.0f, 0.0f, amount));
        default:
            break;
    }
}
SceneObjectTranslation::SceneObjectTranslation(const float x, const float y, const float z) {
    m_Transform = translate(m_Transform, vec3f(x, y, z));
}

// Class SceneObjectRotation
SceneObjectRotation::SceneObjectRotation(const char axis, const float theta) {
    switch (axis) {
        case 'x':
            m_Transform = rotate(m_Transform, radians(theta), vec3f(1.0f, 0.0f, 0.0f));
            break;
        case 'y':
            m_Transform = rotate(m_Transform, radians(theta), vec3f(0.0f, 1.0f, 0.0f));
            break;
        case 'z':
            m_Transform = rotate(m_Transform, radians(theta), vec3f(0.0f, 0.0f, 1.0f));
            break;
        default:
            break;
    }
}
SceneObjectRotation::SceneObjectRotation(vec3f& axis, const float theta) {
    axis        = normalize(axis);
    m_Transform = rotate(m_Transform, theta, axis);
}
SceneObjectRotation::SceneObjectRotation(quatf quaternion) { m_Transform = rotate(m_Transform, quaternion); }

// Class SceneObjectScale
SceneObjectScale::SceneObjectScale(const char axis, const float amount) {
    switch (axis) {
        case 'x':
            m_Transform = scale(m_Transform, vec3f(amount, 0.0f, 0.0f));
            break;
        case 'y':
            m_Transform = scale(m_Transform, vec3f(0.0f, amount, 0.0f));
            break;
        case 'z':
            m_Transform = scale(m_Transform, vec3f(0.0f, 0.0f, amount));
            break;
        default:
            break;
    }
}
SceneObjectScale::SceneObjectScale(const float x, const float y, const float z) {
    m_Transform = scale(m_Transform, vec3f(x, y, z));
}

// Object print
std::ostream& operator<<(std::ostream& out, const BaseSceneObject& obj) {
    out << "GUID: " << obj.m_Guid << std::dec << std::endl;
    out << "Type: " << magic_enum::enum_name(obj.m_Type);
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectVertexArray& obj) {
    out << "Attribute:          " << obj.m_Attribute << std::endl;
    out << "Morph Target Index: " << obj.m_MorphTargetIndex << std::endl;
    out << "Data Type:          " << magic_enum::enum_name(obj.m_DataType) << std::endl;
    out << "Data Size:          " << obj.GetDataSize() << " bytes" << std::endl;
    out << "Data Count:         " << obj.GetVertexCount() << std::endl;
    out << "Data:               ";
    const uint8_t* data = obj.m_Data.GetData();
    for (size_t i = 0; i < obj.GetVertexCount(); i++) {
        switch (obj.m_DataType) {
            case VertexDataType::Float1:
                std::cout << *(reinterpret_cast<const float*>(data) + i) << " ";
                break;
            case VertexDataType::Float2:
                std::cout << *(reinterpret_cast<const vec2f*>(data) + i) << " ";
                break;
            case VertexDataType::Float3:
                std::cout << *(reinterpret_cast<const vec3f*>(data) + i) << " ";
                break;
            case VertexDataType::Float4:
                std::cout << *(reinterpret_cast<const vec4f*>(data) + i) << " ";
                break;
            case VertexDataType::Double1:
                std::cout << *(reinterpret_cast<const double*>(data) + i) << " ";
                break;
            case VertexDataType::Double2:
                std::cout << *(reinterpret_cast<const Vector<double, 2>*>(data) + i) << " ";
                break;
            case VertexDataType::Double3:
                std::cout << *(reinterpret_cast<const Vector<double, 3>*>(data) + i) << " ";
                break;
            case VertexDataType::Double4:
                std::cout << *(reinterpret_cast<const Vector<double, 4>*>(data) + i) << " ";
                break;
            default:
                break;
        }
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectIndexArray& obj) {
    out << "Restart Index:   " << obj.m_ResetartIndex << std::endl;
    out << "Index Data Type: " << magic_enum::enum_name(obj.m_DataType) << std::endl;
    out << "Data Size:       " << obj.GetDataSize() << std::endl;
    out << "Data:            ";
    auto data = obj.GetData();
    for (size_t i = 0; i < obj.GetIndexCount(); i++) {
        switch (obj.m_DataType) {
            case IndexDataType::Int8:
                out << reinterpret_cast<const int8_t*>(data)[i] << ' ';
                break;
            case IndexDataType::Int16:
                out << reinterpret_cast<const int16_t*>(data)[i] << ' ';
                break;
            case IndexDataType::Int32:
                out << reinterpret_cast<const int32_t*>(data)[i] << ' ';
                break;
            case IndexDataType::Int64:
                out << reinterpret_cast<const int64_t*>(data)[i] << ' ';
                break;
            default:
                break;
        }
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectMesh& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Primitive Type: " << magic_enum::enum_name(obj.m_PrimitiveType) << std::endl;
    if (auto material = obj.m_Material.lock()) {
        out << "Material: " << *material << std::endl;
    }
    out << "This mesh contains " << obj.m_VertexArray.size() << " vertex properties." << std::endl;
    for (auto&& v : obj.m_VertexArray) {
        out << v << std::endl;
    }
    out << "Indices index:" << std::endl
        << obj.m_IndexArray;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectTexture& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Coord Index: " << obj.m_TexCoordIndex << std::endl;
    out << "Name:        " << obj.m_Name << std::endl;
    if (!obj.m_Image) out << "Image:\n"
                          << obj.m_Image;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectMaterial& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Name:              " << obj.m_Name << std::endl;
    out << "Albedo:            " << obj.m_DiffuseColor << std::endl;
    out << "Metallic:          " << obj.m_Metallic << std::endl;
    out << "Roughness:         " << obj.m_Roughness << std::endl;
    out << "Normal:            " << obj.m_Normal << std::endl;
    out << "Specular:          " << obj.m_Specular << std::endl;
    out << "Ambient Occlusion: " << obj.m_AmbientOcclusion;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectGeometry& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    for (size_t i = 0; i < obj.m_MeshesLOD.size(); i++) {
        out << "Level: " << i << std::endl;
        for (size_t j = 0; j < obj.m_MeshesLOD[i].size(); j++)
            out << fmt::format("Mesh[{}]:\n", i) << *obj.m_MeshesLOD[i][j] << std::endl;
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectLight& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Color:        " << obj.m_LightColor << std::endl;
    out << "Intensity:    " << obj.m_Intensity << std::endl;
    out << "Cast Shadows: " << obj.m_CastShadows << std::endl;
    out << "Texture:      " << obj.m_TextureRef;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectPointLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << std::endl;
    out << "Light Type: Omni" << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectSpotLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << std::endl;
    out << "Light Type:       Spot" << std::endl;
    out << "Inner Cone Angle: " << obj.m_InnerConeAngle << std::endl;
    out << "Outer Cone Angle: " << obj.m_OuterConeAngle;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectInfiniteLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << std::endl;
    out << "Light Type: Infinite" << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectCamera& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Aspcet:             " << obj.m_Aspect << std::endl;
    out << "Fov:                " << obj.m_Fov << std::endl;
    out << "Near Clip Distance: " << obj.m_NearClipDistance << std::endl;
    out << "Far Clip Distance:  " << obj.m_FarClipDistance << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectTransform& obj) {
    out << "Transform Matrix: " << obj.m_Transform << std::endl;
    out << "Is Object Local:  " << obj.m_SceneObjectOnly;
    return out;
}

float default_atten_func(float intensity, float distance) { return intensity / pow(1 + distance, 2.0f); }
}  // namespace Hitagi::Asset