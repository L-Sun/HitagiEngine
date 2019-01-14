#include "SceneObject.hpp"

namespace My {
// Type print
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
std::ostream& operator<<(std::ostream& out, VertexDataType type) {
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
            case IndexDataType::INT8:
                out << "0x"
                    << *(reinterpret_cast<const int8_t*>(obj.m_pData) + i)
                    << ' ';
                break;
            case IndexDataType::INT16:
                out << "0x"
                    << *(reinterpret_cast<const int16_t*>(obj.m_pData) + i)
                    << ' ';
                break;
            case IndexDataType::INT32:
                out << "0x"
                    << *(reinterpret_cast<const int32_t*>(obj.m_pData) + i)
                    << ' ';
                break;
            case IndexDataType::INT64:
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
