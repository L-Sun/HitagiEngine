#include "SceneObject.hpp"
#include "portable.hpp"

using namespace std;

namespace My {
// Type print
ostream& operator<<(ostream& out, SceneObjectType type) {
    int32_t n = static_cast<int32_t>(type);
    // little endian, read bit from end;
    n       = endian_net_unsigned_int<int32_t>(n);
    char* c = reinterpret_cast<char*>(&n);

    for (int i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }

    return out;
}
ostream& operator<<(ostream& out, VertexDataType type) {
    int32_t n = static_cast<int32_t>(type);
    n         = endian_net_unsigned_int<int32_t>(n);
    char* c   = reinterpret_cast<char*>(&n);

    for (size_t i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }
    return out;
}
ostream& operator<<(ostream& out, const IndexDataType& type) {
    int32_t n = static_cast<int32_t>(type);
    n         = endian_net_unsigned_int<int32_t>(n);
    char* c   = reinterpret_cast<char*>(&n);

    for (size_t i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }
    return out;
}
ostream& operator<<(ostream& out, const PrimitiveType& type) {
    int32_t n = static_cast<int32_t>(type);
    n         = endian_net_unsigned_int(n);
    char* c   = reinterpret_cast<char*>(&n);

    for (size_t i = 0; i < sizeof(int32_t); i++) {
        out << *c++;
    }
    return out;
}

// Object print
ostream& operator<<(ostream& out, const SceneObjectVertexArray& obj) {
    out << "Attribute: " << obj.m_strAttribute << endl;
    out << "Morph Target Index: 0x" << obj.m_MorphTargetIndex << endl;
    out << "Data Type: " << obj.m_DataType << endl;
    out << "Data Size: 0x" << obj.m_szData << endl;
    out << "Data: ";
    for (size_t i = 0; i < obj.m_szData; i++) {
        out << *(reinterpret_cast<const float*>(obj.m_pData) + i) << ' ';
    }
    return out << endl;
}
ostream& operator<<(ostream& out, const SceneObjectIndexArray& obj) {
    out << "Material: " << obj.m_nMaterialIndex << endl;
    out << "Restart Index: 0x" << obj.m_szResetartIndex << endl;
    out << "Index Data Type: " << obj.m_DataType << endl;
    out << "Data Size: 0x" << obj.m_szData << endl;
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
    return out << endl;
}
ostream& operator<<(ostream& out, const SceneObjectMesh& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << endl;
    out << "Primitive Type: " << obj.m_PrimitiveType << endl;
    out << "This mesh contains 0x" << obj.m_VertexArray.size()
        << " vertex properties." << endl;
    out << endl;
    for (size_t i = 0; i < obj.m_VertexArray.size(); i++) {
        out << obj.m_VertexArray[i] << endl;
    }
    out << "This mesh catains 0x" << obj.m_IndexArray.size()
        << " index properties." << endl;
    out << endl;
    for (size_t i = 0; i < obj.m_IndexArray.size(); i++) {
        out << obj.m_IndexArray[i] << endl;
    }

    out << "Visible: " << obj.m_bVisible << endl;
    out << "Shadow: " << obj.m_bShadow << endl;
    out << "Motion Blur: " << obj.m_bMotionBlur << endl;

    return out;
}
ostream& operator<<(ostream& out, const SceneObjectTexture& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << endl;
    out << "Coord Index: " << obj.m_nTexCoordIndex << endl;
    out << "Name: " << obj.m_Name << endl;
    if (obj.m_pImage) out << "Image: " << *obj.m_pImage << endl;
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectMaterial& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << endl;
    out << "Name: " << obj.m_Name << endl;
    out << "Albedo: " << obj.m_BaseColor << endl;
    out << "Metallic: " << obj.m_Metallic << endl;
    out << "Roughness: " << obj.m_Roughness << endl;
    out << "Normal: " << obj.m_Normal << endl;
    out << "Specular: " << obj.m_Specular << endl;
    out << "Ambient Occlusion: " << obj.m_AmbientOcclusion << endl;
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectGeometry& obj) {
    auto count = obj.m_Mesh.size();
    for (decltype(count) i = 0; i < count; i++) {
        out << "Mesh: " << *obj.m_Mesh[i] << endl;
    }
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectLight& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << endl;
    out << "Color: " << obj.m_LightColor << endl;
    out << "Intensity: " << obj.m_fIntensity << endl;
    out << "Cast Shadows: " << obj.m_bCastShadows << endl;
    out << "Texture: " << obj.m_strTexture << endl;
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectSpotLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << endl;
    out << "Light Type: Spot" << endl;
    out << "Cone Angle" << obj.m_fConeAngle << endl;
    out << "Penumbra Angle" << obj.m_fPenumbraAngle << endl;
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectInfiniteLight& obj) {
    out << static_cast<const SceneObjectLight&>(obj) << endl;
    out << "Light Type: Infinite" << endl;
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectCamera& obj) {
    out << static_cast<const BaseSceneObject&>(obj) << endl;
    out << "Aspcet: " << obj.m_fAspect << endl;
    out << "Near Clip Distance: " << obj.m_fNearClipDistance << endl;
    out << "Far Clip Distance: " << obj.m_fFarClipDistance << endl;
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectOrthogonalCamera& obj) {
    out << static_cast<const SceneObjectCamera&>(obj) << endl;
    out << "Camera Type: Orthogonal" << endl;
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectPerspectiveCamera& obj) {
    out << static_cast<const SceneObjectCamera&>(obj) << endl;
    out << "Camera Type: Perspective" << endl;
    out << "FOV: " << obj.m_fFov << endl;
    return out;
}
ostream& operator<<(ostream& out, const SceneObjectTransform& obj) {
    out << "Transform Matrix: " << obj.m_matrix << endl;
    out << "Is Object Local: " << obj.m_bSceneObjectOnly << endl;
    return out;
}

float DefaultAttenFunc(float intensity, float distance) {
    return intensity / pow(1 + distance, 2.0f);
}

BoundingBox SceneObjectMesh::GetBoundingBox() const {
    vec3 bbmin(numeric_limits<float>::max());
    vec3 bbmax(numeric_limits<float>::min());
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
                        const Vector3<double> vertex =
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
}  // namespace My
