#include "SceneObject.hpp"
#include "MemoryManager.hpp"
#include "AssetManager.hpp"

#include <magic_enum.hpp>

namespace Hitagi::Asset {

// Class Vertices
Vertices::Vertices(std::string_view attr,
                   VertexDataType   data_type,
                   Core::Buffer&&   buffer,
                   uint32_t         morph_index)
    : SceneObject(SceneObjectType::VertexArray),
      m_Attribute(attr),
      m_DataType(data_type),
      m_VertexCount(buffer.GetDataSize() / GetVertexSize()),
      m_Data(std::move(buffer)),
      m_MorphTargetIndex(morph_index) {}

const std::string& Vertices::GetAttributeName() const { return m_Attribute; }
VertexDataType     Vertices::GetDataType() const { return m_DataType; }
size_t             Vertices::GetDataSize() const { return m_Data.GetDataSize(); }
const uint8_t*     Vertices::GetData() const { return m_Data.GetData(); }
size_t             Vertices::GetVertexCount() const { return m_VertexCount; }
size_t             Vertices::GetVertexSize() const {
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

// Class Indices
Indices::Indices(
    const IndexDataType data_type,
    Core::Buffer&&      data,
    const uint32_t      restart_index)
    : SceneObject(SceneObjectType::IndexArray),
      m_DataType(data_type),
      m_IndexCount(data.GetDataSize() / GetIndexSize()),
      m_Data(std::move(data)),
      m_ResetartIndex(restart_index) {}

const IndexDataType Indices::GetIndexType() const { return m_DataType; }
const uint8_t*      Indices::GetData() const { return m_Data.GetData(); }
size_t              Indices::GetIndexCount() const { return m_IndexCount; }
size_t              Indices::GetDataSize() const { return m_Data.GetDataSize(); }
size_t              Indices::GetIndexSize() const {
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

// Class Mesh
void Mesh::SetIndexArray(Indices&& array) { m_IndexArray = std::move(array); }
void Mesh::AddVertexArray(Vertices&& array) { m_VertexArray.emplace_back(std::move(array)); }
void Mesh::SetPrimitiveType(PrimitiveType type) { m_PrimitiveType = type; }
void Mesh::SetMaterial(const std::weak_ptr<Material>& material) { m_Material = material; }

const Vertices& Mesh::GetVertexByName(std::string_view name) const {
    for (auto&& vertex : m_VertexArray)
        if (vertex.GetAttributeName() == name)
            return vertex;
    throw std::range_error(fmt::format("No name called{}", name));
}
size_t                       Mesh::GetVertexArraysCount() const { return m_VertexArray.size(); }
const std::vector<Vertices>& Mesh::GetVertexArrays() const { return m_VertexArray; }

const Indices&          Mesh::GetIndexArray() const { return m_IndexArray; }
const PrimitiveType&    Mesh::GetPrimitiveType() const { return m_PrimitiveType; }
std::weak_ptr<Material> Mesh::GetMaterial() const { return m_Material; }

// Class Texture
void                   Texture::LoadTexture() { m_Image = g_AssetManager->ImportImage(m_TexturePath); }
std::shared_ptr<Image> Texture::GetTextureImage() const { return m_Image; }

// Class Material
void Material::SetColor(std::string_view attrib, const vec4f& color) {
    if (attrib == "ambient") m_AmbientColor = color;
    if (attrib == "diffuse") m_DiffuseColor = color;
    if (attrib == "specular") m_Specular = color;
    if (attrib == "emission") m_Emission = color;
    if (attrib == "transparency") m_Transparency = color;
}
void Material::SetParam(std::string_view attrib, const float param) {
    if (attrib == "specular_power") m_SpecularPower = param;
    if (attrib == "opacity") m_Opacity = param;
}
void Material::SetTexture(std::string_view attrib, const std::shared_ptr<Texture>& texture) {
    if (attrib == "ambient") m_DiffuseColor = texture;
    if (attrib == "diffuse") m_DiffuseColor = texture;
    if (attrib == "specular") m_Specular = texture;
    if (attrib == "specular_power") m_SpecularPower = texture;
    if (attrib == "emission") m_Emission = texture;
    if (attrib == "opacity") m_Opacity = texture;
    if (attrib == "transparency") m_Transparency = texture;
    if (attrib == "normal") m_Normal = texture;
}
void Material::LoadTextures() {
    if (std::holds_alternative<std::shared_ptr<Texture>>(m_DiffuseColor))
        std::get<std::shared_ptr<Texture>>(m_DiffuseColor)->LoadTexture();
}

// Class Geometry
void       Geometry::SetVisibility(bool visible) { m_Visible = visible; }
const bool Geometry::Visible() const { return m_Visible; }

void Geometry::AddMesh(std::unique_ptr<Mesh> mesh, size_t level) {
    if (level >= m_MeshesLOD.size()) m_MeshesLOD.resize(level + 1);
    m_MeshesLOD[level].emplace_back(mesh.release());
}
const std::vector<std::unique_ptr<Mesh>>& Geometry::GetMeshes(size_t lod) const {
    return m_MeshesLOD[lod];
}

// Class Camera
void Camera::SetParam(std::string_view attrib, float param) {
    if (attrib == "near")
        m_NearClipDistance = param;
    else if (attrib == "far")
        m_FarClipDistance = param;
    else if (attrib == "fov")
        m_Fov = param;
}
void Camera::SetTexture(std::string_view attrib, std::string_view texture_name) {
    // TODO: extension
}
float Camera::GetAspect() const { return m_Aspect; }
float Camera::GetNearClipDistance() const { return m_NearClipDistance; }
float Camera::GetFarClipDistance() const { return m_FarClipDistance; }
float Camera::GetFov() const { return m_Fov; }

// Class SceneObjectTransform
// Class SceneObjectTranslation

// Object print
std::ostream& operator<<(std::ostream& out, const SceneObject& obj) {
    out << "GUID: " << obj.m_Guid << std::dec << std::endl;
    if (!obj.m_Name.empty())
        out << "Name: " << obj.m_Name << std::dec << std::endl;
    out << "Type: " << magic_enum::enum_name(obj.m_Type);
    return out;
}

std::ostream& operator<<(std::ostream& out, const Vertices& obj) {
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
std::ostream& operator<<(std::ostream& out, const Indices& obj) {
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
std::ostream& operator<<(std::ostream& out, const Mesh& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
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
std::ostream& operator<<(std::ostream& out, const Texture& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
    out << "Coord Index: " << obj.m_TexCoordIndex << std::endl;
    out << "Name:        " << obj.m_Name << std::endl;
    if (!obj.m_Image) out << "Image:\n"
                          << obj.m_Image;
    return out;
}

std::ostream& operator<<(std::ostream& out, const Material& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
    out << "Name:              " << obj.m_Name << std::endl;
    out << "Albedo:            " << obj.m_DiffuseColor << std::endl;
    out << "Metallic:          " << obj.m_Metallic << std::endl;
    out << "Roughness:         " << obj.m_Roughness << std::endl;
    out << "Normal:            " << obj.m_Normal << std::endl;
    out << "Specular:          " << obj.m_Specular << std::endl;
    out << "Ambient Occlusion: " << obj.m_AmbientOcclusion;
    return out;
}
std::ostream& operator<<(std::ostream& out, const Geometry& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
    for (size_t i = 0; i < obj.m_MeshesLOD.size(); i++) {
        out << "Level: " << i << std::endl;
        for (size_t j = 0; j < obj.m_MeshesLOD[i].size(); j++)
            out << fmt::format("Mesh[{}]:\n", i) << *obj.m_MeshesLOD[i][j] << std::endl;
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const Light& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
    out << "Color:        " << obj.m_LightColor << std::endl;
    out << "Intensity:    " << obj.m_Intensity << std::endl;
    out << "Texture:      " << obj.m_TextureRef;
    return out;
}
std::ostream& operator<<(std::ostream& out, const PointLight& obj) {
    out << static_cast<const Light&>(obj) << std::endl;
    out << "Light Type: Omni" << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SpotLight& obj) {
    out << static_cast<const Light&>(obj) << std::endl;
    out << "Light Type:       Spot" << std::endl;
    out << "Inner Cone Angle: " << obj.m_InnerConeAngle << std::endl;
    out << "Outer Cone Angle: " << obj.m_OuterConeAngle;
    return out;
}
std::ostream& operator<<(std::ostream& out, const InfiniteLight& obj) {
    out << static_cast<const Light&>(obj) << std::endl;
    out << "Light Type: Infinite" << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const Camera& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
    out << "Aspcet:             " << obj.m_Aspect << std::endl;
    out << "Fov:                " << obj.m_Fov << std::endl;
    out << "Near Clip Distance: " << obj.m_NearClipDistance << std::endl;
    out << "Far Clip Distance:  " << obj.m_FarClipDistance << std::endl;
    return out;
}

float default_atten_func(float intensity, float distance) { return intensity / pow(1 + distance, 2.0f); }
}  // namespace Hitagi::Asset