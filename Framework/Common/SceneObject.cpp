#include "SceneObject.hpp"
#include "AssetLoader.hpp"
#include "JPEG.hpp"
#include "PNG.hpp"
#include "BMP.hpp"
#include "TGA.hpp"

namespace My {
// Class BaseSceneObject
BaseSceneObject::BaseSceneObject(SceneObjectType type) : m_Type(type) {
    m_Guid = xg::newGuid();
}
BaseSceneObject::BaseSceneObject(xg::Guid& guid, SceneObjectType type)
    : m_Guid(guid), m_Type(type) {}
BaseSceneObject::BaseSceneObject(xg::Guid&& guid, SceneObjectType type)
    : m_Guid(std::move(guid)), m_Type(type) {}
BaseSceneObject::BaseSceneObject(BaseSceneObject&& obj)
    : m_Guid(std::move(obj.m_Guid)), m_Type(obj.m_Type) {}
BaseSceneObject& BaseSceneObject::operator=(BaseSceneObject&& obj) {
    this->m_Guid = std::move(obj.m_Guid);
    this->m_Type = obj.m_Type;
    return *this;
}

// Class SceneObjectVertexArray
const std::string& SceneObjectVertexArray::GetAttributeName() const {
    return m_strAttribute;
}
VertexDataType SceneObjectVertexArray::GetDataType() const {
    return m_DataType;
}
size_t SceneObjectVertexArray::GetDataSize() const {
    size_t size = m_szData;

    switch (m_DataType) {
        case VertexDataType::kFLOAT1:
        case VertexDataType::kFLOAT2:
        case VertexDataType::kFLOAT3:
        case VertexDataType::kFLOAT4:
            size *= sizeof(float);
            break;
        case VertexDataType::kDOUBLE1:
        case VertexDataType::kDOUBLE2:
        case VertexDataType::kDOUBLE3:
        case VertexDataType::kDOUBLE4:
            size *= sizeof(double);
        default:
            size = 0;
            break;
    }
    return size;
}
const void* SceneObjectVertexArray::GetData() const { return m_pData; }
size_t      SceneObjectVertexArray::GetVertexCount() const {
    size_t size = m_szData;

    switch (m_DataType) {
        case VertexDataType::kFLOAT1:
        case VertexDataType::kDOUBLE1:
            size /= 1;
            break;
        case VertexDataType::kFLOAT2:
        case VertexDataType::kDOUBLE2:
            size /= 2;
            break;
        case VertexDataType::kFLOAT3:
        case VertexDataType::kDOUBLE3:
            size /= 3;
            break;
        case VertexDataType::kFLOAT4:
        case VertexDataType::kDOUBLE4:
            size /= 4;
            break;
        default:
            size = 0;
            break;
    }
    return size;
}

// Class SceneObjectIndexArray
const uint32_t SceneObjectIndexArray::GetMaterialIndex() const {
    return m_nMaterialIndex;
}
const IndexDataType SceneObjectIndexArray::GetIndexType() const {
    return m_DataType;
}
const void* SceneObjectIndexArray::GetData() const { return m_pData; }
size_t      SceneObjectIndexArray::GetIndexCount() const { return m_szData; }
size_t      SceneObjectIndexArray::GetDataSize() const {
    size_t size = m_szData;

    switch (m_DataType) {
        case IndexDataType::kINT8:
            size *= sizeof(int8_t);
            break;
        case IndexDataType::kINT16:
            size *= sizeof(int16_t);
            break;
        case IndexDataType::kINT32:
            size *= sizeof(int32_t);
            break;
        case IndexDataType::kINT64:
            size *= sizeof(int64_t);
            break;
        default:
            size = 0;
            break;
    }
    return size;
}

// Class SceneObjectMesh
void SceneObjectMesh::AddIndexArray(SceneObjectIndexArray&& array) {
    m_IndexArray.push_back(std::move(array));
}
void SceneObjectMesh::AddVertexArray(SceneObjectVertexArray&& array) {
    m_VertexArray.push_back(std::move(array));
}
void SceneObjectMesh::SetPrimitiveType(PrimitiveType type) {
    m_PrimitiveType = type;
}
size_t SceneObjectMesh::GetIndexGroupCount() const {
    return m_IndexArray.size();
}
size_t SceneObjectMesh::GetIndexCount(const size_t index) const {
    return m_IndexArray.empty() ? 0 : m_IndexArray[index].GetIndexCount();
}
size_t SceneObjectMesh::GetVertexCount() const {
    return m_VertexArray.empty() ? 0 : m_VertexArray[0].GetVertexCount();
}
size_t SceneObjectMesh::GetVertexPropertiesCount() const {
    return m_VertexArray.size();
}
const SceneObjectVertexArray& SceneObjectMesh::GetVertexPropertyArray(
    const size_t index) const {
    return m_VertexArray[index];
}
const SceneObjectIndexArray& SceneObjectMesh::GetIndexArray(
    const size_t index) const {
    return m_IndexArray[index];
}
const PrimitiveType& SceneObjectMesh::GetPrimitiveType() {
    return m_PrimitiveType;
}
BoundingBox SceneObjectMesh::GetBoundingBox() const {
    vec3 bbmin(std::numeric_limits<float>::max());
    vec3 bbmax(std::numeric_limits<float>::min());
    auto count = m_VertexArray.size();

    for (auto n = 0; n < count; n++) {
        if (m_VertexArray[n].GetAttributeName() == "position") {
            auto data_type      = m_VertexArray[n].GetDataType();
            auto vertices_count = m_VertexArray[n].GetVertexCount();
            auto data           = m_VertexArray[n].GetData();

            for (auto i = 0; i < vertices_count; i++) {
                switch (data_type) {
                    case VertexDataType::kFLOAT3: {
                        const vec3 vertex =
                            reinterpret_cast<const float*>(data) + 3 * i;
                        bbmin.x = (bbmin.x < vertex.x) ? bbmin.x : vertex.x;
                        bbmin.y = (bbmin.y < vertex.y) ? bbmin.y : vertex.y;
                        bbmin.z = (bbmin.z < vertex.z) ? bbmin.z : vertex.z;
                        bbmax.x = (bbmax.x > vertex.x) ? bbmax.x : vertex.x;
                        bbmax.y = (bbmax.y > vertex.y) ? bbmax.y : vertex.y;
                        bbmax.z = (bbmax.z > vertex.z) ? bbmax.z : vertex.z;
                    } break;
                    case VertexDataType::kDOUBLE3: {
                        const Vector<double, 3> vertex =
                            reinterpret_cast<const double*>(data) + 3 * i;
                        bbmin.x = (bbmin.x < vertex.x) ? bbmin.x : vertex.x;
                        bbmin.y = (bbmin.y < vertex.y) ? bbmin.y : vertex.y;
                        bbmin.z = (bbmin.z < vertex.z) ? bbmin.z : vertex.z;
                        bbmax.x = (bbmax.x > vertex.x) ? bbmax.x : vertex.x;
                        bbmax.y = (bbmax.y > vertex.y) ? bbmax.y : vertex.y;
                        bbmax.z = (bbmax.z > vertex.z) ? bbmax.z : vertex.z;
                    } break;
                    default:
                        assert(0);
                }
            }
        }
    }
    BoundingBox result;
    result.extent   = (bbmax - bbmin) * 0.5f;
    result.centroid = (bbmax + bbmin) * 0.5;
    return result;
}

// Class SceneObjectTexture
void SceneObjectTexture::AddTransform(mat4& matrix) {
    m_Transforms.push_back(matrix);
}
void SceneObjectTexture::SetName(const std::string& name) { m_Name = name; }
void SceneObjectTexture::SetName(std::string&& name) {
    m_Name = std::move(name);
}
void SceneObjectTexture::LoadTexture() {
    if (!m_pImage) {
        Buffer      buf = g_pAssetLoader->SyncOpenAndReadBinary(m_Name);
        std::string ext = m_Name.substr(m_Name.find_last_of("."));
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
const Color& SceneObjectMaterial::GetBaseColor() const { return m_BaseColor; }
const Color& SceneObjectMaterial::GetSpecularColor() const {
    return m_Specular;
}
const Parameter& SceneObjectMaterial::GetSpecularPower() const {
    return m_SpecularPower;
}
void SceneObjectMaterial::SetName(const std::string& name) { m_Name = name; }
void SceneObjectMaterial::SetName(std::string&& name) {
    m_Name = std::move(name);
}
void SceneObjectMaterial::SetColor(std::string_view attrib, const vec4& color) {
    if (attrib == "diffuse") m_BaseColor = Color(color);
    if (attrib == "specular") m_Specular = Color(color);
    if (attrib == "emission") m_Emission = Color(color);
    if (attrib == "opacity") m_Opacity = Color(color);
    if (attrib == "transparency") m_Transparency = Color(color);
}
void SceneObjectMaterial::SetParam(std::string_view attrib, const float param) {
    if (attrib == "specular_power") m_SpecularPower = Parameter(param);
}
void SceneObjectMaterial::SetTexture(std::string_view attrib,
                                     std::string_view textureName) {
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
void SceneObjectMaterial::SetTexture(
    std::string_view                           attrib,
    const std::shared_ptr<SceneObjectTexture>& texture) {
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
void SceneObjectGeometry::SetIfMotionBlur(bool motion_blur) {
    m_bMotionBlur = motion_blur;
}
void SceneObjectGeometry::SetCollisionType(
    SceneObjectCollisionType collision_type) {
    m_CollisionType = collision_type;
}
void SceneObjectGeometry::SetCollisionParameters(const float* param,
                                                 int32_t      count) {
    assert(count > 0 && count < 10);
    memcpy(m_CollisionParameters, param, sizeof(float) * count);
}
const bool SceneObjectGeometry::Visible() { return m_bVisible; }
const bool SceneObjectGeometry::CastShadow() { return m_bShadow; }
const bool SceneObjectGeometry::MotionBlur() { return m_bMotionBlur; }
const SceneObjectCollisionType SceneObjectGeometry::CollisionType() const {
    return m_CollisionType;
}
const float* SceneObjectGeometry::CollisionParameters() const {
    return m_CollisionParameters;
}

void SceneObjectGeometry::AddMesh(std::shared_ptr<SceneObjectMesh>& mesh) {
    m_Mesh.push_back(std::move(mesh));
}
const std::weak_ptr<SceneObjectMesh> SceneObjectGeometry::GetMesh() {
    return (m_Mesh.empty() ? nullptr : m_Mesh[0]);
}
const std::weak_ptr<SceneObjectMesh> SceneObjectGeometry::GetMeshLOD(
    size_t lod) {
    return (lod < m_Mesh.size() ? m_Mesh[lod] : nullptr);
}
BoundingBox SceneObjectGeometry::GetBoundingBox() const {
    return m_Mesh.empty() ? BoundingBox() : m_Mesh[0]->GetBoundingBox();
}

// Class SceneObjectLight
void SceneObjectLight::SetIfCastShadow(bool shadow) { m_bCastShadows = shadow; }
void SceneObjectLight::SetColor(std::string_view attrib, vec4& color) {
    if (attrib == "light") {
        m_LightColor = Color(color);
    }
}
void SceneObjectLight::SetParam(std::string_view attrib, float param) {
    if (attrib == "intensity") {
        m_fIntensity = param;
    }
}
void SceneObjectLight::SetTexture(std::string_view attrib,
                                  std::string_view textureName) {
    if (attrib == "projection") {
        m_strTexture = textureName;
    }
}
void SceneObjectLight::SetAttenuation(AttenFunc func) {
    m_LightAttenuation = func;
}
const Color& SceneObjectLight::GetColor() { return m_LightColor; }
float        SceneObjectLight::GetIntensity() { return m_fIntensity; }

// Class SceneObjectOmniLight
// Class SceneObjectSpotLight
// Class SceneObjectInfiniteLight

// Class SceneObjectCamera
void SceneObjectCamera::SetColor(std::string_view attrib, vec4& color) {
    // TODO: extension
}
void SceneObjectCamera::SetParam(std::string_view attrib, float param) {
    if (attrib == "near")
        m_fNearClipDistance = param;
    else if (attrib == "far")
        m_fFarClipDistance = param;
}
void SceneObjectCamera::SetTexture(std::string_view attrib,
                                   std::string_view textureName) {
    // TODO: extension
}
float SceneObjectCamera::GetNearClipDistance() const {
    return m_fNearClipDistance;
}
float SceneObjectCamera::GetFarClipDistance() const {
    return m_fFarClipDistance;
}

// Class SceneObjectOrthogonalCamera
// Class SceneObjectPerspectiveCamera
void SceneObjectPerspectiveCamera::SetParam(std::string_view attrib,
                                            float            param) {
    // TODO: handle fovs, fovy
    if (attrib == "fov") {
        m_fFov = param;
    }
    SceneObjectCamera::SetParam(attrib, param);
}
float SceneObjectPerspectiveCamera::GetFov() const { return m_fFov; }

// Class SceneObjectTransform
// Class SceneObjectTranslation
SceneObjectTranslation::SceneObjectTranslation(const char  axis,
                                               const float amount) {
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
SceneObjectTranslation::SceneObjectTranslation(const float x, const float y,
                                               const float z) {
    m_matrix = translate(m_matrix, vec3(x, y, z));
}

// Class SceneObjectRotation
SceneObjectRotation::SceneObjectRotation(const char axis, const float theta) {
    switch (axis) {
        case 'x':
            m_matrix = rotate(m_matrix, radians(theta), vec3(1.0f, 0.0f, 0.0f));
            break;
        case 'y':
            m_matrix = rotate(m_matrix, radians(theta), vec3(0.0f, 1.0f, 0.0f));
            break;
        case 'z':
            m_matrix = rotate(m_matrix, radians(theta), vec3(0.0f, 0.0f, 1.0f));
            break;
        default:
            break;
    }
}
SceneObjectRotation::SceneObjectRotation(vec3& axis, const float theta) {
    axis     = normalize(axis);
    m_matrix = rotate(m_matrix, theta, axis);
}
SceneObjectRotation::SceneObjectRotation(quat quaternion) {
    m_matrix = mat4(quaternion) * m_matrix;
}

// Class SceneObjectScale
SceneObjectScale::SceneObjectScale(const char axis, const float amount) {
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
SceneObjectScale::SceneObjectScale(const float x, const float y,
                                   const float z) {
    m_matrix = scale(m_matrix, vec3(x, y, z));
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
    out << "SceneObject" << std::endl;
    out << "-----------" << std::endl;
    out << "GUID: " << obj.m_Guid << std::endl;
    out << "Type: " << obj.m_Type << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectVertexArray& obj) {
    out << "Attribute: " << obj.m_strAttribute << std::endl;
    out << "Morph Target Index: 0x" << obj.m_MorphTargetIndex << std::endl;
    out << "Data Type: " << obj.m_DataType << std::endl;
    out << "Data Size: 0x" << obj.m_szData << std::endl;
    out << "Data: ";
    for (size_t i = 0; i < obj.m_szData; i++) {
        out << *(reinterpret_cast<const float*>(obj.m_pData) + i) << ' ';
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectIndexArray& obj) {
    out << "Material: " << obj.m_nMaterialIndex << std::endl;
    out << "Restart Index: 0x" << obj.m_szResetartIndex << std::endl;
    out << "Index Data Type: " << obj.m_DataType << std::endl;
    out << "Data Size: 0x" << obj.m_szData << std::endl;
    out << "Data: ";
    for (size_t i = 0; i < obj.m_szData; i++) {
        switch (obj.m_DataType) {
            case IndexDataType::kINT8:
                out << "0x"
                    << *(reinterpret_cast<const int8_t*>(obj.m_pData) + i)
                    << ' ';
                break;
            case IndexDataType::kINT16:
                out << "0x"
                    << *(reinterpret_cast<const int16_t*>(obj.m_pData) + i)
                    << ' ';
                break;
            case IndexDataType::kINT32:
                out << "0x"
                    << *(reinterpret_cast<const int32_t*>(obj.m_pData) + i)
                    << ' ';
                break;
            case IndexDataType::kINT64:
                out << "0x"
                    << *(reinterpret_cast<const int64_t*>(obj.m_pData) + i)
                    << ' ';
                break;
            default:
                break;
        }
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectMesh& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Primitive Type: " << obj.m_PrimitiveType << std::endl;
    out << "This mesh contains 0x" << obj.m_VertexArray.size()
        << " vertex properties." << std::endl;
    out << std::endl;
    for (size_t i = 0; i < obj.m_VertexArray.size(); i++) {
        out << obj.m_VertexArray[i] << std::endl;
    }
    out << "This mesh catains 0x" << obj.m_IndexArray.size()
        << " index properties." << std::endl;
    out << std::endl;
    for (size_t i = 0; i < obj.m_IndexArray.size(); i++) {
        out << obj.m_IndexArray[i] << std::endl;
    }

    out << "Visible: " << obj.m_bVisible << std::endl;
    out << "Shadow: " << obj.m_bShadow << std::endl;
    out << "Motion Blur: " << obj.m_bMotionBlur << std::endl;

    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectTexture& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Coord Index: " << obj.m_nTexCoordIndex << std::endl;
    out << "Name: " << obj.m_Name << std::endl;
    if (obj.m_pImage) out << "Image: " << *obj.m_pImage << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectMaterial& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Name: " << obj.m_Name << std::endl;
    out << "Albedo: " << obj.m_BaseColor << std::endl;
    out << "Metallic: " << obj.m_Metallic << std::endl;
    out << "Roughness: " << obj.m_Roughness << std::endl;
    out << "Normal: " << obj.m_Normal << std::endl;
    out << "Specular: " << obj.m_Specular << std::endl;
    out << "Ambient Occlusion: " << obj.m_AmbientOcclusion << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectGeometry& obj) {
    auto count = obj.m_Mesh.size();
    for (decltype(count) i = 0; i < count; i++) {
        out << "Mesh: " << *obj.m_Mesh[i] << std::endl;
    }
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectLight& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Color: " << obj.m_LightColor << std::endl;
    out << "Intensity: " << obj.m_fIntensity << std::endl;
    out << "Cast Shadows: " << obj.m_bCastShadows << std::endl;
    out << "Texture: " << obj.m_strTexture << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectOmniLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << std::endl;
    out << "Light Type: Omni" << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectSpotLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << std::endl;
    out << "Light Type: Spot" << std::endl;
    out << "Cone Angle" << obj.m_fConeAngle << std::endl;
    out << "Penumbra Angle" << obj.m_fPenumbraAngle << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream&                   out,
                         const SceneObjectInfiniteLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << std::endl;
    out << "Light Type: Infinite" << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectCamera& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << std::endl;
    out << "Aspcet: " << obj.m_fAspect << std::endl;
    out << "Near Clip Distance: " << obj.m_fNearClipDistance << std::endl;
    out << "Far Clip Distance: " << obj.m_fFarClipDistance << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream&                      out,
                         const SceneObjectOrthogonalCamera& obj) {
    out << static_cast<const SceneObjectCamera&>(obj) << std::endl;
    out << "Camera Type: Orthogonal" << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream&                       out,
                         const SceneObjectPerspectiveCamera& obj) {
    out << static_cast<const SceneObjectCamera&>(obj) << std::endl;
    out << "Camera Type: Perspective" << std::endl;
    out << "FOV: " << obj.m_fFov << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SceneObjectTransform& obj) {
    out << "Transform Matrix: " << obj.m_matrix << std::endl;
    out << "Is Object Local: " << obj.m_bSceneObjectOnly << std::endl;
    return out;
}

float DefaultAttenFunc(float intensity, float distance) {
    return intensity / pow(1 + distance, 2.0f);
}

}  // namespace My
