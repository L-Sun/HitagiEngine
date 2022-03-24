#include "Mesh.hpp"

#include <magic_enum.hpp>

using namespace Hitagi::Math;

namespace Hitagi::Asset {

// Class VertexArray
size_t VertexArray::GetVertexSize() const {
    switch (m_DataType) {
        case DataType::Float1:
            return sizeof(float) * 1;
        case DataType::Float2:
            return sizeof(float) * 2;
        case DataType::Float3:
            return sizeof(float) * 3;
        case DataType::Float4:
            return sizeof(float) * 4;
        case DataType::Double1:
            return sizeof(double) * 1;
        case DataType::Double2:
            return sizeof(double) * 2;
        case DataType::Double3:
            return sizeof(double) * 3;
        case DataType::Double4:
            return sizeof(double) * 4;
    }
    return 0;
}

// Class Indices
size_t IndexArray::GetIndexSize() const {
    switch (m_DataType) {
        case DataType::Int8:
            return sizeof(int8_t);
        case DataType::Int16:
            return sizeof(int16_t);
        case DataType::Int32:
            return sizeof(int32_t);
        case DataType::Int64:
            return sizeof(int64_t);
    }
    return 0;
}

const VertexArray& Mesh::GetVertexByName(std::string_view name) const {
    for (auto&& vertex : m_VertexArray)
        if (vertex.GetAttributeName() == name)
            return vertex;
    throw std::range_error(fmt::format("No name called {}", name));
}

std::ostream& operator<<(std::ostream& out, const VertexArray& obj) {
    out << "Attribute:          " << obj.m_Attribute << std::endl;
    out << "Data Type:          " << magic_enum::enum_name(obj.m_DataType) << std::endl;
    out << "Data Size:          " << obj.GetDataSize() << " bytes" << std::endl;
    out << "Data Count:         " << obj.GetVertexCount() << std::endl;
    out << "Data:               ";
    const uint8_t* data = obj.m_Data.GetData();
    for (size_t i = 0; i < obj.GetVertexCount(); i++) {
        switch (obj.m_DataType) {
            case VertexArray::DataType::Float1:
                std::cout << *(reinterpret_cast<const float*>(data) + i) << " ";
                break;
            case VertexArray::DataType::Float2:
                std::cout << *(reinterpret_cast<const vec2f*>(data) + i) << " ";
                break;
            case VertexArray::DataType::Float3:
                std::cout << *(reinterpret_cast<const vec3f*>(data) + i) << " ";
                break;
            case VertexArray::DataType::Float4:
                std::cout << *(reinterpret_cast<const vec4f*>(data) + i) << " ";
                break;
            case VertexArray::DataType::Double1:
                std::cout << *(reinterpret_cast<const double*>(data) + i) << " ";
                break;
            case VertexArray::DataType::Double2:
                std::cout << *(reinterpret_cast<const Vector<double, 2>*>(data) + i) << " ";
                break;
            case VertexArray::DataType::Double3:
                std::cout << *(reinterpret_cast<const Vector<double, 3>*>(data) + i) << " ";
                break;
            case VertexArray::DataType::Double4:
                std::cout << *(reinterpret_cast<const Vector<double, 4>*>(data) + i) << " ";
                break;
            default:
                break;
        }
    }
    return out << std::endl;
}
std::ostream& operator<<(std::ostream& out, const IndexArray& obj) {
    out << "Index Data Type: " << magic_enum::enum_name(obj.m_DataType) << std::endl;
    out << "Data Size:       " << obj.GetDataSize() << std::endl;
    out << "Data:            ";
    auto data = obj.GetData();
    for (size_t i = 0; i < obj.GetIndexCount(); i++) {
        switch (obj.m_DataType) {
            case IndexArray::DataType::Int8:
                out << reinterpret_cast<const int8_t*>(data)[i] << ' ';
                break;
            case IndexArray::DataType::Int16:
                out << reinterpret_cast<const int16_t*>(data)[i] << ' ';
                break;
            case IndexArray::DataType::Int32:
                out << reinterpret_cast<const int32_t*>(data)[i] << ' ';
                break;
            case IndexArray::DataType::Int64:
                out << reinterpret_cast<const int64_t*>(data)[i] << ' ';
                break;
            default:
                break;
        }
    }
    return out << std::endl;
}

std::shared_ptr<Bone> Mesh::CreateNewBone(std::string name) {
    auto bone = std::make_shared<Bone>();
    bone->SetName(std::move(name));
    m_Bones.emplace_back(bone);
    return bone;
}

std::ostream& operator<<(std::ostream& out, const Mesh& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
    out << "Primitive Type: " << magic_enum::enum_name(obj.m_PrimitiveType) << std::endl;
    if (auto material = obj.m_MaterialRef.lock()) {
        out << fmt::format("Material Ref: {}\n", material->GetGuid().str());
    }
    out << "This mesh contains " << obj.m_VertexArray.size() << " vertex properties." << std::endl;
    for (auto&& v : obj.m_VertexArray) {
        out << v << std::endl;
    }
    out << "Indices index:\n"
        << obj.m_IndexArray;
    return out;
}

}  // namespace Hitagi::Asset