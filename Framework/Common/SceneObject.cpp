#include "SceneObject.hpp"
#include "AssetLoader.hpp"
#include "JPEG.hpp"
#include "PNG.hpp"
#include "BMP.hpp"
#include "TGA.hpp"

namespace My {
// Class BaseSceneObject
BaseSceneObject::BaseSceneObject(SceneObjectType type) : m_Type(type) { m_Guid = xg::newGuid(); }
BaseSceneObject::BaseSceneObject(const xg::Guid& guid, const SceneObjectType& type) : m_Guid(guid), m_Type(type) {}
BaseSceneObject::BaseSceneObject(xg::Guid&& guid, const SceneObjectType& type)
    : m_Guid(std::move(guid)), m_Type(type) {}
BaseSceneObject::BaseSceneObject(BaseSceneObject&& obj) : m_Guid(std::move(obj.m_Guid)), m_Type(obj.m_Type) {}
BaseSceneObject& BaseSceneObject::operator=(BaseSceneObject&& obj) {
    this->m_Guid = std::move(obj.m_Guid);
    this->m_Type = obj.m_Type;
    return *this;
}

// Class SceneObjectVertexArray
SceneObjectVertexArray::SceneObjectVertexArray(std::string_view attr, uint32_t morph_index,
                                               const VertexDataType data_type, const void* data, size_t vertexCount)
    : m_strAttribute(attr), m_MorphTargetIndex(morph_index), m_DataType(data_type), m_vertexCount(vertexCount) {
    m_pData = g_pMemoryManager->Allocate(GetDataSize());
    std::memcpy(m_pData, data, GetDataSize());
}
SceneObjectVertexArray::SceneObjectVertexArray(const SceneObjectVertexArray& rhs) { this->operator=(rhs); }
SceneObjectVertexArray::SceneObjectVertexArray(SceneObjectVertexArray&& rhs) { this->operator=(rhs); }
SceneObjectVertexArray& SceneObjectVertexArray::operator=(const SceneObjectVertexArray& rhs) {
    if (this != &rhs) {
        if (m_pData) g_pMemoryManager->Free(m_pData, GetDataSize());
        m_strAttribute     = rhs.m_strAttribute;
        m_MorphTargetIndex = rhs.m_MorphTargetIndex;
        m_DataType         = rhs.m_DataType;
        m_vertexCount      = rhs.m_vertexCount;
        m_pData            = g_pMemoryManager->Allocate(rhs.GetDataSize());
        std::memcpy(m_pData, rhs.m_pData, rhs.GetDataSize());
    }
    return *this;
}
SceneObjectVertexArray& SceneObjectVertexArray::operator=(SceneObjectVertexArray&& rhs) {
    if (this != &rhs) {
        if (m_pData) g_pMemoryManager->Free(m_pData, GetDataSize());
        m_strAttribute     = std::move(rhs.m_strAttribute);
        m_MorphTargetIndex = rhs.m_MorphTargetIndex;
        m_DataType         = rhs.m_DataType;
        m_vertexCount      = rhs.m_vertexCount;
        m_pData            = rhs.m_pData;
        rhs.m_pData        = nullptr;
    }
    return *this;
}
SceneObjectVertexArray::~SceneObjectVertexArray() {
    if (m_pData) g_pMemoryManager->Free(m_pData, GetDataSize());
}
const std::string& SceneObjectVertexArray::GetAttributeName() const { return m_strAttribute; }
VertexDataType     SceneObjectVertexArray::GetDataType() const { return m_DataType; }
size_t             SceneObjectVertexArray::GetDataSize() const {
    size_t vertexSize;
    switch (m_DataType) {
        case VertexDataType::kFLOAT1:
            vertexSize = sizeof(float) * 1;
            break;
        case VertexDataType::kFLOAT2:
            vertexSize = sizeof(float) * 2;
            break;
        case VertexDataType::kFLOAT3:
            vertexSize = sizeof(float) * 3;
            break;
        case VertexDataType::kFLOAT4:
            vertexSize = sizeof(float) * 4;
            break;
        case VertexDataType::kDOUBLE1:
            vertexSize = sizeof(double) * 1;
            break;
        case VertexDataType::kDOUBLE2:
            vertexSize = sizeof(double) * 2;
            break;
        case VertexDataType::kDOUBLE3:
            vertexSize = sizeof(double) * 3;
            break;
        case VertexDataType::kDOUBLE4:
            vertexSize = sizeof(double) * 4;
            break;
        default:
            vertexSize = 0;
            break;
    }
    return vertexSize * GetVertexCount();
}
const void* SceneObjectVertexArray::GetData() const { return m_pData; }
size_t      SceneObjectVertexArray::GetVertexCount() const { return m_vertexCount; }

// Class SceneObjectIndexArray
SceneObjectIndexArray::SceneObjectIndexArray(const uint32_t restart_index, const IndexDataType data_type,
                                             const void* data, const size_t indexCount)
    : m_szResetartIndex(restart_index), m_DataType(data_type), m_indexCount(indexCount) {
    m_pData = g_pMemoryManager->Allocate(GetDataSize());
    std::memcpy(m_pData, data, GetDataSize());
}
SceneObjectIndexArray::SceneObjectIndexArray(const SceneObjectIndexArray& rhs) { this->operator=(rhs); }
SceneObjectIndexArray::SceneObjectIndexArray(SceneObjectIndexArray&& rhs) { this->operator=(std::move(rhs)); }
SceneObjectIndexArray& SceneObjectIndexArray::operator=(const SceneObjectIndexArray& rhs) {
    if (this != &rhs) {
        if (m_pData) g_pMemoryManager->Free(m_pData, GetDataSize());
        m_szResetartIndex = rhs.m_szResetartIndex;
        m_DataType        = rhs.m_DataType;
        m_indexCount      = rhs.m_indexCount;
        m_pData           = g_pMemoryManager->Allocate(rhs.GetDataSize());
        std::memcpy(m_pData, rhs.m_pData, rhs.GetDataSize());
    }
    return *this;
}
SceneObjectIndexArray& SceneObjectIndexArray::operator=(SceneObjectIndexArray&& rhs) {
    if (this != &rhs) {
        if (m_pData) g_pMemoryManager->Free(m_pData, GetDataSize());
        m_szResetartIndex = rhs.m_szResetartIndex;
        m_DataType        = rhs.m_DataType;
        m_indexCount      = rhs.m_indexCount;
        m_pData           = rhs.m_pData;
        rhs.m_pData       = nullptr;
    }
    return *this;
}
SceneObjectIndexArray::~SceneObjectIndexArray() {
    if (m_pData) g_pMemoryManager->Free(m_pData, GetDataSize());
}
const IndexDataType SceneObjectIndexArray::GetIndexType() const { return m_DataType; }
const void*         SceneObjectIndexArray::GetData() const { return m_pData; }
size_t              SceneObjectIndexArray::GetIndexCount() const { return m_indexCount; }
size_t              SceneObjectIndexArray::GetDataSize() const {
    size_t size;

    switch (m_DataType) {
        case IndexDataType::kINT8:
            size = m_indexCount * sizeof(int8_t);
            break;
        case IndexDataType::kINT16:
            size = m_indexCount * sizeof(int16_t);
            break;
        case IndexDataType::kINT32:
            size = m_indexCount * sizeof(int32_t);
            break;
        case IndexDataType::kINT64:
            size = m_indexCount * sizeof(int64_t);
            break;
        default:
            size = 0;
            break;
    }
    return size;
}

// Class SceneObjectMesh
void SceneObjectMesh::AddIndexArray(SceneObjectIndexArray&& array) { m_IndexArray = std::move(array); }
void SceneObjectMesh::AddVertexArray(SceneObjectVertexArray&& array) {
    m_VertexArray.insert({array.GetAttributeName(), std::move(array)});
}
void   SceneObjectMesh::SetPrimitiveType(PrimitiveType type) { m_PrimitiveType = type; }
void   SceneObjectMesh::SetMaterial(const std::weak_ptr<SceneObjectMaterial>& material) { m_Material = material; }
size_t SceneObjectMesh::GetVertexCount() const {
    return m_VertexArray.empty() ? 0 : m_VertexArray.cbegin()->second.GetVertexCount();
}
size_t                        SceneObjectMesh::GetVertexPropertiesCount() const { return m_VertexArray.size(); }
const SceneObjectVertexArray& SceneObjectMesh::GetVertexPropertyArray(const std::string& attr) const {
    return m_VertexArray.at(attr);
}

const SceneObjectIndexArray&       SceneObjectMesh::GetIndexArray() const { return m_IndexArray; }
const PrimitiveType&               SceneObjectMesh::GetPrimitiveType() { return m_PrimitiveType; }
std::weak_ptr<SceneObjectMaterial> SceneObjectMesh::GetMaterial() const { return m_Material; }
BoundingBox                        SceneObjectMesh::GetBoundingBox() const {
    vec3f bbmin(std::numeric_limits<float>::max());
    vec3f bbmax(std::numeric_limits<float>::min());
    auto  positions    = m_VertexArray.at("POSITION");
    auto  data_type    = positions.GetDataType();
    auto  vertex_count = positions.GetVertexCount();
    auto  data         = positions.GetData();

    switch (data_type) {
        case VertexDataType::kFLOAT3: {
            auto vertex = reinterpret_cast<const vec3f*>(data);
            for (auto i = 0; i < vertex_count; i++, vertex++) {
                bbmin.x = std::min(bbmin.x, vertex->x);
                bbmin.y = std::min(bbmin.y, vertex->y);
                bbmin.z = std::min(bbmin.z, vertex->z);
                bbmax.x = std::max(bbmax.x, vertex->x);
                bbmax.y = std::max(bbmax.y, vertex->y);
                bbmax.z = std::max(bbmax.z, vertex->z);
            }
        } break;
        case VertexDataType::kDOUBLE3: {
            auto vertex = reinterpret_cast<const vec3d*>(data);
            for (auto i = 0; i < vertex_count; i++, vertex++) {
                bbmin.x = std::min(static_cast<double>(bbmin.x), vertex->x);
                bbmin.y = std::min(static_cast<double>(bbmin.y), vertex->y);
                bbmin.z = std::min(static_cast<double>(bbmin.z), vertex->z);
                bbmax.x = std::max(static_cast<double>(bbmax.x), vertex->x);
                bbmax.y = std::max(static_cast<double>(bbmax.y), vertex->y);
                bbmax.z = std::max(static_cast<double>(bbmax.z), vertex->z);
            }
        } break;
        default:
            assert(0);
    }

    BoundingBox result;
    result.extent   = (bbmax - bbmin) * 0.5f;
    result.centroid = (bbmax + bbmin) * 0.5;
    return result;
}

// Class SceneObjectTexture
void SceneObjectTexture::AddTransform(mat4f& matrix) { m_Transforms.push_back(matrix); }
void SceneObjectTexture::SetName(const std::string& name) { m_Name = name; }
void SceneObjectTexture::SetName(std::string&& name) { m_Name = std::move(name); }
void SceneObjectTexture::LoadTexture() {
    if (!m_pImage) {
        Buffer buf = g_pAssetLoader->SyncOpenAndReadBinary(m_Name);
        auto   ext = std::filesystem::path(m_Name).extension();
        if (ext == ".jpg" || ext == ".jpeg") {
            JpegParser jfif_parser;
            m_pImage = std::make_shared<Image>(jfif_parser.Parse(buf));
        } else if (ext == ".png") {
            PngParser png_parser;
            m_pImage = std::make_shared<Image>(png_parser.Parse(buf));
        } else if (ext == ".bmp") {
            BmpParser bmp_parser;
            m_pImage = std::make_shared<Image>(bmp_parser.Parse(buf));
        } else if (ext == ".tga") {
            TgaParser tga_parser;
            m_pImage = std::make_shared<Image>(tga_parser.Parse(buf));
        }
    }
}
const std::string& SceneObjectTexture::GetName() const { return m_Name; }
const Image&       SceneObjectTexture::GetTextureImage() {
    if (!m_pImage) {
        LoadTexture();
    }

    return *m_pImage;
}

// Class SceneObjectMaterial
const std::string& SceneObjectMaterial::GetName() const { return m_Name; }
const Color&       SceneObjectMaterial::GetBaseColor() const { return m_BaseColor; }
const Color&       SceneObjectMaterial::GetSpecularColor() const { return m_Specular; }
const Parameter&   SceneObjectMaterial::GetSpecularPower() const { return m_SpecularPower; }
void               SceneObjectMaterial::SetName(const std::string& name) { m_Name = name; }
void               SceneObjectMaterial::SetName(std::string&& name) { m_Name = std::move(name); }
void               SceneObjectMaterial::SetColor(std::string_view attrib, const vec4f& color) {
    if (attrib == "diffuse") m_BaseColor = Color(color);
    if (attrib == "specular") m_Specular = Color(color);
    if (attrib == "emission") m_Emission = Color(color);
    if (attrib == "transparency") m_Transparency = Color(color);
}
void SceneObjectMaterial::SetParam(std::string_view attrib, const float param) {
    if (attrib == "specular_power") m_SpecularPower = Parameter(param);
    if (attrib == "opacity") m_Opacity = Parameter(param);
}
void SceneObjectMaterial::SetTexture(std::string_view attrib, std::string_view textureName) {
    if (attrib == "diffuse") m_BaseColor = std::make_shared<SceneObjectTexture>(textureName);
    if (attrib == "specular") m_Specular = std::make_shared<SceneObjectTexture>(textureName);
    if (attrib == "specular_power") m_SpecularPower = std::make_shared<SceneObjectTexture>(textureName);
    if (attrib == "emission") m_Emission = std::make_shared<SceneObjectTexture>(textureName);
    if (attrib == "opacity") m_Opacity = std::make_shared<SceneObjectTexture>(textureName);
    if (attrib == "transparency") m_Transparency = std::make_shared<SceneObjectTexture>(textureName);
    if (attrib == "normal") m_Normal = std::make_shared<SceneObjectTexture>(textureName);
}
void SceneObjectMaterial::SetTexture(std::string_view attrib, const std::shared_ptr<SceneObjectTexture>& texture) {
    if (attrib == "diffuse") m_BaseColor = texture;
    if (attrib == "specular") m_Specular = texture;
    if (attrib == "specular_power") m_SpecularPower = texture;
    if (attrib == "emission") m_Emission = texture;
    if (attrib == "opacity") m_Opacity = texture;
    if (attrib == "transparency") m_Transparency = texture;
    if (attrib == "normal") m_Normal = texture;
}
void SceneObjectMaterial::LoadTextures() {
    if (m_BaseColor.ValueMap) m_BaseColor.ValueMap->LoadTexture();
}

// Class SceneObjectGeometry
void SceneObjectGeometry::SetVisibility(bool visible) { m_bVisible = visible; }
void SceneObjectGeometry::SetIfCastShadow(bool shadow) { m_bShadow = shadow; }
void SceneObjectGeometry::SetIfMotionBlur(bool motion_blur) { m_bMotionBlur = motion_blur; }
void SceneObjectGeometry::SetCollisionType(SceneObjectCollisionType collision_type) {
    m_CollisionType = collision_type;
}
void SceneObjectGeometry::SetCollisionParameters(const float* param, int32_t count) {
    assert(count > 0 && count < 10);
    memcpy(m_CollisionParameters, param, sizeof(float) * count);
}
const bool                     SceneObjectGeometry::Visible() const { return m_bVisible; }
const bool                     SceneObjectGeometry::CastShadow() const { return m_bShadow; }
const bool                     SceneObjectGeometry::MotionBlur() const { return m_bMotionBlur; }
const SceneObjectCollisionType SceneObjectGeometry::CollisionType() const { return m_CollisionType; }
const float*                   SceneObjectGeometry::CollisionParameters() const { return m_CollisionParameters; }

void SceneObjectGeometry::AddMesh(const std::weak_ptr<SceneObjectMesh>& mesh, size_t level) {
    if (level >= m_MeshesLOD.size()) m_MeshesLOD.resize(level + 1);
    m_MeshesLOD[level].push_back(mesh);
}
std::vector<std::weak_ptr<SceneObjectMesh>> SceneObjectGeometry::GetMeshes(size_t lod) const {
    return (lod < m_MeshesLOD.size() ? m_MeshesLOD[lod] : std::vector<std::weak_ptr<SceneObjectMesh>>());
}
BoundingBox SceneObjectGeometry::GetBoundingBox() const {
    BoundingBox ret;
    if (!m_MeshesLOD.empty() && !m_MeshesLOD[0].empty()) {
        vec3f bbmin(std::numeric_limits<float>::max());
        vec3f bbmax(std::numeric_limits<float>::min());
        for (auto&& _mesh : m_MeshesLOD[0]) {
            if (auto mesh = _mesh.lock()) {
                auto box    = mesh->GetBoundingBox();
                auto _bbmin = box.centroid - box.extent;
                auto _bbmax = box.centroid + box.extent;
                bbmin.x     = std::min(bbmin.x, _bbmin.x);
                bbmin.y     = std::min(bbmin.y, _bbmin.y);
                bbmin.z     = std::min(bbmin.z, _bbmin.z);
                bbmax.x     = std::max(bbmax.x, _bbmax.x);
                bbmax.y     = std::max(bbmax.y, _bbmax.y);
                bbmax.z     = std::max(bbmax.z, _bbmax.z);
            }
        }

        ret.centroid = (bbmax + bbmin) * 0.5f;
        ret.extent   = (bbmax - bbmin) * 0.5f;
    }
    return ret;
}

// Class SceneObjectLight
void SceneObjectLight::SetIfCastShadow(bool shadow) { m_bCastShadows = shadow; }
void SceneObjectLight::SetColor(std::string_view attrib, const vec4f& color) {
    if (attrib == "light") {
        m_LightColor = Color(color);
    }
}
void SceneObjectLight::SetParam(std::string_view attrib, float param) {
    if (attrib == "intensity") {
        m_fIntensity = param;
    }
}
void SceneObjectLight::SetTexture(std::string_view attrib, std::string_view textureName) {
    if (attrib == "projection") {
        m_strTexture = textureName;
    }
}
void         SceneObjectLight::SetAttenuation(AttenFunc func) { m_LightAttenuation = func; }
const Color& SceneObjectLight::GetColor() { return m_LightColor; }
float        SceneObjectLight::GetIntensity() { return m_fIntensity; }

// Class SceneObjectOmniLight
// Class SceneObjectSpotLight
// Class SceneObjectInfiniteLight

// Class SceneObjectCamera
void SceneObjectCamera::SetColor(std::string_view attrib, const vec4f& color) {
    // TODO: extension
}
void SceneObjectCamera::SetParam(std::string_view attrib, float param) {
    if (attrib == "near")
        m_fNearClipDistance = param;
    else if (attrib == "far")
        m_fFarClipDistance = param;
}
void SceneObjectCamera::SetTexture(std::string_view attrib, std::string_view textureName) {
    // TODO: extension
}
float SceneObjectCamera::GetNearClipDistance() const { return m_fNearClipDistance; }
float SceneObjectCamera::GetFarClipDistance() const { return m_fFarClipDistance; }

// Class SceneObjectOrthogonalCamera
// Class SceneObjectPerspectiveCamera
void SceneObjectPerspectiveCamera::SetParam(std::string_view attrib, float param) {
    // TODO: handle fovs, fovy
    if (attrib == "fov") {
        m_fFov = param;
    }
    SceneObjectCamera::SetParam(attrib, param);
}
float SceneObjectPerspectiveCamera::GetFov() const { return m_fFov; }

// Class SceneObjectTransform
// Class SceneObjectTranslation
SceneObjectTranslation::SceneObjectTranslation(const char axis, const float amount) {
    switch (axis) {
        case 'x':
            m_matrix = translate(m_matrix, vec3f(amount, 0.0f, 0.0f));
            break;
        case 'y':
            m_matrix = translate(m_matrix, vec3f(0.0f, amount, 0.0f));
            break;
        case 'z':
            m_matrix = translate(m_matrix, vec3f(0.0f, 0.0f, amount));
        default:
            break;
    }
}
SceneObjectTranslation::SceneObjectTranslation(const float x, const float y, const float z) {
    m_matrix = translate(m_matrix, vec3f(x, y, z));
}

// Class SceneObjectRotation
SceneObjectRotation::SceneObjectRotation(const char axis, const float theta) {
    switch (axis) {
        case 'x':
            m_matrix = rotate(m_matrix, radians(theta), vec3f(1.0f, 0.0f, 0.0f));
            break;
        case 'y':
            m_matrix = rotate(m_matrix, radians(theta), vec3f(0.0f, 1.0f, 0.0f));
            break;
        case 'z':
            m_matrix = rotate(m_matrix, radians(theta), vec3f(0.0f, 0.0f, 1.0f));
            break;
        default:
            break;
    }
}
SceneObjectRotation::SceneObjectRotation(vec3f& axis, const float theta) {
    axis     = normalize(axis);
    m_matrix = rotate(m_matrix, theta, axis);
}
SceneObjectRotation::SceneObjectRotation(quatf quaternion) { m_matrix = mat4f(quaternion) * m_matrix; }

// Class SceneObjectScale
SceneObjectScale::SceneObjectScale(const char axis, const float amount) {
    switch (axis) {
        case 'x':
            m_matrix = scale(m_matrix, vec3f(amount, 0.0f, 0.0f));
            break;
        case 'y':
            m_matrix = scale(m_matrix, vec3f(0.0f, amount, 0.0f));
            break;
        case 'z':
            m_matrix = scale(m_matrix, vec3f(0.0f, 0.0f, amount));
            break;
        default:
            break;
    }
}
SceneObjectScale::SceneObjectScale(const float x, const float y, const float z) {
    m_matrix = scale(m_matrix, vec3f(x, y, z));
}

// Type print
std::ostream& operator<<(std::ostream& out, const SceneObjectType& type) {
    int32_t n = static_cast<int32_t>(type);
    // little endian, read bit from end;
    n       = endian_net_unsigned_int<int32_t>(n);
    char* c = reinterpret_cast<char*>(&n);

    for (int i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }

    return out;
}
std::ostream& operator<<(std::ostream& out, const VertexDataType& type) {
    int32_t n = static_cast<int32_t>(type);
    n         = endian_net_unsigned_int<int32_t>(n);
    char* c   = reinterpret_cast<char*>(&n);

    for (size_t i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }
    return out;
}
std::ostream& operator<<(std::ostream& out, const IndexDataType& type) {
    int32_t n = static_cast<int32_t>(type);
    n         = endian_net_unsigned_int<int32_t>(n);
    char* c   = reinterpret_cast<char*>(&n);

    for (size_t i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }
    return out;
}
std::ostream& operator<<(std::ostream& out, const PrimitiveType& type) {
    int32_t n = static_cast<int32_t>(type);
    n         = endian_net_unsigned_int(n);
    char* c   = reinterpret_cast<char*>(&n);

    for (size_t i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }
    return out;
}

// Object print
std::ostream& operator<<(std::ostream& out, const BaseSceneObject& obj) {
    out << "SceneObject\n"
        << "-----------\n"
        << "GUID: " << obj.m_Guid << std::dec << '\n'
        << "Type: " << obj.m_Type << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectVertexArray& obj) {
    out << "Attribute: " << obj.m_strAttribute << '\n';
    out << "Morph Target Index: " << obj.m_MorphTargetIndex << '\n';
    out << "Data Type: " << obj.m_DataType << '\n';
    out << "Data Size: " << obj.GetDataSize() << " bytes.\n";
    out << "Data Count: " << obj.GetVertexCount() << '\n';
    out << "Data: ";
    for (size_t i = 0; i < obj.GetVertexCount(); i++) {
        switch (obj.m_DataType) {
            case VertexDataType::kFLOAT1:
                std::cout << *(reinterpret_cast<const float*>(obj.m_pData) + i) << ' ';
                break;
            case VertexDataType::kFLOAT2:
                std::cout << *(reinterpret_cast<const vec2f*>(obj.m_pData) + i) << ' ';
                break;
            case VertexDataType::kFLOAT3:
                std::cout << *(reinterpret_cast<const vec3f*>(obj.m_pData) + i) << ' ';
                break;
            case VertexDataType::kFLOAT4:
                std::cout << *(reinterpret_cast<const vec4f*>(obj.m_pData) + i) << ' ';
                break;
            case VertexDataType::kDOUBLE1:
                std::cout << *(reinterpret_cast<const double*>(obj.m_pData) + i) << ' ';
                break;
            case VertexDataType::kDOUBLE2:
                std::cout << *(reinterpret_cast<const Vector<double, 2>*>(obj.m_pData) + i) << ' ';
                break;
            case VertexDataType::kDOUBLE3:
                std::cout << *(reinterpret_cast<const Vector<double, 3>*>(obj.m_pData) + i) << ' ';
                break;
            case VertexDataType::kDOUBLE4:
                std::cout << *(reinterpret_cast<const Vector<double, 4>*>(obj.m_pData) + i) << ' ';
                break;
            default:
                break;
        }
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectIndexArray& obj) {
    out << "Restart Index: " << obj.m_szResetartIndex << '\n';
    out << "Index Data Type: " << obj.m_DataType << '\n';
    out << "Data Size: " << obj.GetDataSize() << '\n';
    out << "Data: ";
    for (size_t i = 0; i < obj.GetIndexCount(); i++) {
        switch (obj.m_DataType) {
            case IndexDataType::kINT8:
                out << reinterpret_cast<const int8_t*>(obj.m_pData)[i] << ' ';
                break;
            case IndexDataType::kINT16:
                out << reinterpret_cast<const int16_t*>(obj.m_pData)[i] << ' ';
                break;
            case IndexDataType::kINT32:
                out << reinterpret_cast<const int32_t*>(obj.m_pData)[i] << ' ';
                break;
            case IndexDataType::kINT64:
                out << reinterpret_cast<const int64_t*>(obj.m_pData)[i] << ' ';
                break;
            default:
                break;
        }
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectMesh& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << '\n';
    out << "Primitive Type: " << obj.m_PrimitiveType << '\n';
    if (auto material = obj.m_Material.lock()) out << "Material: " << *material << '\n';
    out << "This mesh contains " << obj.m_VertexArray.size() << " vertex properties." << '\n';
    for (auto&& [key, v] : obj.m_VertexArray) {
        out << v << '\n';
    }
    out << "Indices index:" << '\n' << obj.m_IndexArray << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectTexture& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << '\n';
    out << "Coord Index: " << obj.m_nTexCoordIndex << '\n';
    out << "Name: " << obj.m_Name << '\n';
    if (obj.m_pImage) out << "Image: " << *obj.m_pImage << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectMaterial& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << '\n';
    out << "Name: " << obj.m_Name << '\n';
    out << "Albedo: " << obj.m_BaseColor << '\n';
    out << "Metallic: " << obj.m_Metallic << '\n';
    out << "Roughness: " << obj.m_Roughness << '\n';
    out << "Normal: " << obj.m_Normal << '\n';
    out << "Specular: " << obj.m_Specular << '\n';
    out << "Ambient Occlusion: " << obj.m_AmbientOcclusion << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectGeometry& obj) {
    for (size_t i = 0; i < obj.m_MeshesLOD.size(); i++) {
        out << "Level: " << i << '\n';
        for (size_t j = 0; j < obj.m_MeshesLOD[i].size(); j++)
            if (auto mesh = obj.m_MeshesLOD[i][j].lock()) out << "Mesh[" << j << "]:" << *mesh << '\n';
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectLight& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << '\n';
    out << "Color: " << obj.m_LightColor << '\n';
    out << "Intensity: " << obj.m_fIntensity << '\n';
    out << "Cast Shadows: " << obj.m_bCastShadows << '\n';
    out << "Texture: " << obj.m_strTexture << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectPointLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << '\n';
    out << "Light Type: Omni" << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectSpotLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << '\n';
    out << "Light Type: Spot" << '\n';
    out << "Inner Cone Angle" << obj.m_fInnerConeAngle << '\n';
    out << "Outer Cone Angle" << obj.m_fOuterConeAngle << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectInfiniteLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << '\n';
    out << "Light Type: Infinite" << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectCamera& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << '\n';
    out << "Aspcet: " << obj.m_fAspect << '\n';
    out << "Near Clip Distance: " << obj.m_fNearClipDistance << '\n';
    out << "Far Clip Distance: " << obj.m_fFarClipDistance << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectOrthogonalCamera& obj) {
    out << static_cast<const SceneObjectCamera&>(obj) << '\n';
    out << "Camera Type: Orthogonal" << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectPerspectiveCamera& obj) {
    out << static_cast<const SceneObjectCamera&>(obj) << '\n';
    out << "Camera Type: Perspective" << '\n';
    out << "FOV: " << obj.m_fFov << '\n';
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectTransform& obj) {
    out << "Transform Matrix: " << obj.m_matrix << '\n';
    out << "Is Object Local: " << obj.m_bSceneObjectOnly << '\n';
    return out;
}

float DefaultAttenFunc(float intensity, float distance) { return intensity / pow(1 + distance, 2.0f); }
}  // namespace My